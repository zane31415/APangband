/**
 * \file ap-game.c
 * \brief Angband-side Archipelago glue: wires AP events to game state.
 *
 * See ap-game.h.  This file may use the game's headers freely; it talks to the
 * APCc client only through the dependency-free apinterface.h API.
 */
#include "angband.h"
#include "init.h"
#include "monster.h"
#include "mon-util.h"
#include "object.h"
#include "obj-desc.h"
#include "obj-gear.h"
#include "obj-knowledge.h"
#include "obj-make.h"
#include "obj-pile.h"
#include "obj-tval.h"
#include "obj-util.h"
#include "player.h"
#include "player-calcs.h"
#include "player-spell.h"
#include "player-util.h"
#include "store.h"
#include "apinterface.h"
#include "ap-game.h"

/* Set while killing the player in response to a received DeathLink, so we don't
 * bounce that same death back out as another DeathLink. */
static bool ap_dying_from_deathlink = false;

/*
 * Per-artifact "its location has been checked" flags, indexed by aidx (lazily
 * allocated, z_info->a_max entries).  Populated by ap_check_confirmed -- which
 * fires for both live checks and the on-connect replay -- so it reflects the
 * server's view of which artifact locations are already found.  Drives the Black
 * Market "buy a missed location" offer (ap_find_missed_location).  Not saved: a
 * reconnect's replay rebuilds it.
 */
static bool *ap_artifact_checked = NULL;

/** Ensure the checked-artifact table is allocated.  Returns false if it can't. */
static bool ap_ensure_artifact_table(void)
{
	if (!ap_artifact_checked && z_info && z_info->a_max)
		ap_artifact_checked = mem_zalloc(z_info->a_max * sizeof(bool));
	return ap_artifact_checked != NULL;
}

/*
 * Proper-named artifacts are stored quoted in artifact.txt ('Narthanc'), but the
 * Archipelago location names drop the quotes ("Narthanc").  Copy art->name into
 * buf with any wrapping single quotes stripped, so it matches the apworld.
 */
static void ap_artifact_bare_name(const struct artifact *art, char *buf, size_t len)
{
	size_t n;

	if (!len) return;
	my_strcpy(buf, art->name ? art->name : "", len);

	n = strlen(buf);
	if (n >= 2 && buf[0] == '\'' && buf[n - 1] == '\'') {
		memmove(buf, buf + 1, n - 2);
		buf[n - 2] = '\0';
	}
}

/*
 * Clean singular base-kind name ("Phial", "Bastard Sword") for an artifact's
 * base object -- the form suffix artifacts use in their apworld location name.
 * This must go through object_kind_name(), NOT raw kind->name, because kind names
 * carry article/plural markers ("& Phial~") that object_desc strips; the apworld
 * used the stripped form (it equals the artifact.txt base-object sval name).
 */
static void ap_artifact_base_name(const struct artifact *art, char *buf, size_t len)
{
	struct object_kind *kind = lookup_kind(art->tval, art->sval);

	if (kind && kind->name)
		object_kind_name(buf, len, kind, true);
	else if (len)
		buf[0] = '\0';
}

/*
 * Send the location check for an artifact.  Its apworld location name is one of
 * two forms and we can't tell which from here, so send both: the bare (de-
 * quoted) name -- "Narthanc", "Angrist" -- and base-kind + name -- "Phial of
 * Galadriel".  ap_send_check() drops whichever isn't a real location.
 */
static void ap_send_artifact_check(const struct artifact *art)
{
	char bare[120], base[120], full[120];

	if (!art || !art->name) return;

	ap_artifact_bare_name(art, bare, sizeof(bare));
	ap_send_check(bare);

	ap_artifact_base_name(art, base, sizeof(base));
	if (base[0]) {
		strnfmt(full, sizeof(full), "%s %s", base, bare);
		ap_send_check(full);
	}
}

/**
 * Does \p name match \p art's Archipelago location name?  Compares against both
 * naming forms (see ap_send_artifact_check).
 */
static bool ap_artifact_loc_matches(const struct artifact *art, const char *name)
{
	char bare[120], base[120], full[120];

	if (!art->name) return false;

	ap_artifact_bare_name(art, bare, sizeof(bare));
	if (streq(name, bare)) return true;

	ap_artifact_base_name(art, base, sizeof(base));
	if (base[0]) {
		strnfmt(full, sizeof(full), "%s %s", base, bare);
		if (streq(name, full)) return true;
	}

	return false;
}

/** Record that the artifact location named \p name (if any) is now checked. */
static void ap_mark_artifact_location_checked(const char *name)
{
	int i;

	if (!ap_ensure_artifact_table()) return;

	for (i = 1; i < z_info->a_max; i++) {
		const struct artifact *art = &a_info[i];
		if (art->name && ap_artifact_loc_matches(art, name)) {
			ap_artifact_checked[art->aidx] = true;
			return;
		}
	}
}

/**
 * A location check was confirmed for our slot.  This fires both for live checks
 * and -- crucially -- for the replay of every previously-checked location right
 * after connecting.  Locations are named after unique monsters and artifacts;
 * for a unique we zero its max_num so it stays dead (including for a brand-new
 * character reincarnating into the same AP slot), and for an artifact we note
 * its location as found so the Black Market won't re-offer it.
 */
static void ap_check_confirmed(const char *name)
{
	struct monster_race *race = lookup_monster(name);

	if (race) {
		/* Killed for good: prevents this unique from being generated again. */
		if (rf_has(race->flags, RF_UNIQUE))
			race->max_num = 0;
		return;
	}

	ap_mark_artifact_location_checked(name);
}

/*** Archipelago item delivery: name -> game effect ***/

/*
 * The 23 item names below are frozen: they are the data-package contents the
 * apworld generates (see angband/data.py).  They split into:
 *   - 13 "Progressive <Category> Artifact": the Nth grant of a category yields
 *     the Nth artifact of that category ordered by artifact.txt 'level';
 *   - 2 specific named artifacts (the diggers Pick of Erebor / Mattock of Náin);
 *   - 5 consumables placed in the home;
 *   - 3 non-object boons (gold, experience, expanded shop books).
 */

/** Progressive artifact categories, in the order their items are numbered. */
enum ap_prog_cat {
	AP_CAT_LIGHT, AP_CAT_BLADED, AP_CAT_BLUNT, AP_CAT_POLEARM, AP_CAT_RANGED,
	AP_CAT_BOOTS, AP_CAT_HELM, AP_CAT_ARMOR, AP_CAT_CLOAK, AP_CAT_GLOVES,
	AP_CAT_SHIELD, AP_CAT_RING, AP_CAT_AMULET, AP_CAT_MAX
};

/** Maps each progressive item name to the object tvals that make up the category. */
static const struct {
	const char *item_name;
	int tvals[3];	/* TV_NULL (0) terminated */
} ap_prog_cats[AP_CAT_MAX] = {
	{ "Progressive Light Artifact",         { TV_LIGHT } },
	{ "Progressive Bladed Weapon Artifact", { TV_SWORD } },
	{ "Progressive Blunt Weapon Artifact",  { TV_HAFTED } },
	{ "Progressive Polearm Weapon Artifact",{ TV_POLEARM } },
	{ "Progressive Ranged Weapon Artifact", { TV_BOW } },
	{ "Progressive Boots Artifact",         { TV_BOOTS } },
	{ "Progressive Helm Artifact",          { TV_HELM, TV_CROWN } },
	{ "Progressive Armor Artifact",         { TV_SOFT_ARMOR, TV_HARD_ARMOR, TV_DRAG_ARMOR } },
	{ "Progressive Cloak Artifact",         { TV_CLOAK } },
	{ "Progressive Gloves Artifact",        { TV_GLOVES } },
	{ "Progressive Shield Artifact",        { TV_SHIELD } },
	{ "Progressive Ring Artifact",          { TV_RING } },
	{ "Progressive Amulet Artifact",        { TV_AMULET } },
};

/** Consumable items, with the object kind and stack size to place in the home. */
static const struct {
	const char *name;
	int tval;
	const char *sval;	/* kind name, as looked up by lookup_sval() */
	int qty;
} ap_consumables[] = {
	{ "2 Potions of Augmentation", TV_POTION, "Augmentation",              2 },
	{ "Rod of Recall",             TV_ROD,    "Recall",                    1 },
	{ "Scroll of Acquirement",     TV_SCROLL, "Acquirement",               1 },
	{ "Scroll of Deep Descent",    TV_SCROLL, "Deep Descent",              1 },
	{ "Piece of Elvish Waybread",  TV_FOOD,   "Piece of Elvish Waybread",  1 },
};

/*
 * Transient, game-thread-only count of how many of each progressive category
 * we have seen so far in the current item stream.  Reset at the start of every
 * replay (index 0) so the Nth-artifact mapping is recomputed deterministically
 * from the top, even across the per-character de-duplication.
 */
static int ap_prog_count[AP_CAT_MAX];

/*
 * Transient count of "Expanded Starting Shop Books" grants seen this stream:
 * the Kth grant stocks the bookseller's Kth dungeon book (in sval order).  Like
 * the progressive counters this is reset at index 0 and re-counted every replay.
 */
static int ap_books_count;

/*
 * Set when a home/inventory item this stream couldn't be placed (everything
 * full): the contiguous high-water mark stops there and the remaining items are
 * left for a later replay, so nothing is silently dropped or delivered twice.
 */
static bool ap_blocked;

/** Return the progressive category for an item name, or -1 if it isn't one. */
static int ap_prog_cat_of(const char *name)
{
	int c;

	for (c = 0; c < AP_CAT_MAX; c++)
		if (streq(name, ap_prog_cats[c].item_name)) return c;
	return -1;
}

/** qsort comparator: order artifacts by depth 'level', then index for stability. */
static int ap_art_cmp(const void *a, const void *b)
{
	const struct artifact *x = *(const struct artifact * const *)a;
	const struct artifact *y = *(const struct artifact * const *)b;

	if (x->level != y->level) return x->level - y->level;
	return (int)x->aidx - (int)y->aidx;
}

/** The \p n-th (1-based) artifact of progressive category \p cat, or NULL. */
static const struct artifact *ap_nth_cat_artifact(int cat, int n)
{
	const struct artifact **list;
	const struct artifact *result = NULL;
	int count = 0, i, t;

	if (cat < 0 || cat >= AP_CAT_MAX || n < 1) return NULL;

	list = mem_zalloc(z_info->a_max * sizeof(*list));
	for (i = 0; i < z_info->a_max; i++) {
		const struct artifact *art = &a_info[i];

		if (!art->name) continue;
		for (t = 0; t < 3 && ap_prog_cats[cat].tvals[t]; t++) {
			if (art->tval == ap_prog_cats[cat].tvals[t]) {
				list[count++] = art;
				break;
			}
		}
	}

	sort(list, count, sizeof(*list), ap_art_cmp);
	if (n <= count) result = list[n - 1];
	mem_free(list);
	return result;
}

/** Set up an object's "known" twin the way stores do, ready to be carried. */
static void ap_object_know(struct object *obj)
{
	obj->known = object_new();
	obj->known->notice |= OBJ_NOTICE_ASSESSED;
	object_set_base_known(player, obj);
	obj->known->notice |= OBJ_NOTICE_ASSESSED;
	/*
	 * Mark the known twin as the artifact, exactly as object_touch() does on a
	 * normal pickup: object_is_known_artifact() (hence object_desc showing the
	 * artifact's name) keys off obj->known->artifact, not obj->artifact.  NULL
	 * for non-artifacts, so this is a no-op for ordinary granted items.
	 */
	obj->known->artifact = obj->artifact;
	player_know_object(player, obj);
	obj->origin = ORIGIN_NONE;
}

/** Build a known object of \p kind in a stack of \p qty. */
static struct object *ap_make_kind_object(struct object_kind *kind, int qty)
{
	struct object *obj;

	if (!kind) return NULL;

	obj = object_new();
	object_prep(obj, kind, 0, RANDOMISE);
	obj->number = MAX(1, qty);
	ap_object_know(obj);
	return obj;
}

/** Build a fully-realised artifact object (the real reward, not the check). */
static struct object *ap_make_artifact_object(const struct artifact *art)
{
	struct object_kind *kind;
	struct object *obj;

	if (!art) return NULL;
	kind = lookup_kind(art->tval, art->sval);
	if (!kind) return NULL;

	obj = object_new();
	object_prep(obj, kind, art->alloc_min, MAXIMISE);
	obj->artifact = art;
	copy_artifact_data(obj, art);
	/*
	 * Deliberately NOT mark_artifact_created(): in artifacts-as-checks mode
	 * the attributeless natural copy must still be allowed to spawn so it can
	 * be picked up as the location check.  This granted copy is the separate,
	 * real reward.
	 */
	ap_object_know(obj);
	return obj;
}

/**
 * Turn an Archipelago item name into a game object to place in the home, or
 * NULL if the name isn't a home-delivered item (a boon, or unmapped).  \p prog_n
 * is the 1-based progressive index for "Progressive <Category> Artifact" names.
 */
static struct object *ap_make_item(const char *name, int prog_n)
{
	const struct artifact *art;
	int cat = ap_prog_cat_of(name);
	size_t i;

	/* Progressive artifact: the Nth of its category by depth. */
	if (cat >= 0)
		return ap_make_artifact_object(ap_nth_cat_artifact(cat, prog_n));

	/* Consumables and the filler. */
	for (i = 0; i < N_ELEMENTS(ap_consumables); i++) {
		if (streq(name, ap_consumables[i].name)) {
			int tval = ap_consumables[i].tval;
			int sval = lookup_sval(tval, ap_consumables[i].sval);
			struct object_kind *kind = (sval >= 0) ?
				lookup_kind(tval, sval) : NULL;
			return ap_make_kind_object(kind, ap_consumables[i].qty);
		}
	}

	/* Any name that is exactly an artifact (the specific diggers). */
	art = lookup_artifact_name(name);
	if (art && art->name && streq(art->name, name))
		return ap_make_artifact_object(art);

	return NULL;
}

/**
 * Place a granted object: into the pack if the player is down in the dungeon
 * and there is room, otherwise into the home.  Returns false only if it could
 * go nowhere (the home was full too), leaving the caller to retry it later.
 */
static bool ap_place_object(struct object *obj, const char *name)
{
	struct store *home = &stores[f_info[FEAT_HOME].shopnum - 1];

	if (player->depth > 0 && inven_carry_okay(obj)) {
		inven_carry(player, obj, true, false);
		msg("AP: '%s' arrives in your pack.", name);
		return true;
	}

	if (store_check_num(home, obj)) {
		home_carry(obj);
		msg("AP: '%s' delivered to your home.", name);
		return true;
	}

	return false;
}

/**
 * Deliver a single granted home/inventory item.  Returns false if the item is
 * real but couldn't be placed anywhere (so the caller should not advance past
 * it); true otherwise (placed, or nothing to place).
 */
static bool ap_deliver_item(const char *name, int prog_n)
{
	struct object *obj = ap_make_item(name, prog_n);

	if (!obj) {
		msg("AP: '%s' has no delivery mapping.", name);
		return true;
	}

	if (ap_place_object(obj, name))
		return true;

	/* No room anywhere: drop this copy; the replay remakes it next connect. */
	if (obj->known) object_delete(NULL, NULL, &obj->known);
	obj->known = NULL;
	object_delete(NULL, NULL, &obj);
	return false;
}

/**
 * Triple-gold boon: multiply the current purse by three.  Multiplicative and
 * stacking, so successive grants give 3x, 9x, 27x, ... -- applied once per
 * character (gated by the high-water mark), so a fresh or respawned character
 * gets the full multiplier while a reconnecting one is not multiplied again.
 */
static void ap_boon_triple_gold(void)
{
	int32_t cap = (int32_t)((1UL << 31) - 1);
	int64_t total = (int64_t)player->au * 3;

	player->au = (total > cap) ? cap : (int32_t)total;
	player->upkeep->redraw |= PR_GOLD;
	msg("AP: Your gold is tripled!");
}

/** Experience boon: jump the character up \p levels from wherever it is now. */
static void ap_boon_extra_levels(int levels)
{
	int old_lev = player->lev;
	int target = old_lev + levels;
	int32_t need;

	if (target > PY_MAX_LEVEL) target = PY_MAX_LEVEL;
	if (target < 2 || target <= old_lev) return;

	/* Experience needed to sit at the start of the target level. */
	need = (int32_t)(player_exp[target - 2] * player->expfact / 100L);
	if (need > player->exp)
		player_exp_gain(player, need - player->exp);

	msg("AP: +%d level(s) of experience (now level %d).",
		player->lev - old_lev, player->lev);
}

/** The \p n-th (1-based) dungeon spellbook of a book \p tval, in sval order. */
static struct object_kind *ap_nth_dungeon_book(int tval, int n)
{
	struct object_base *base = &kb_info[tval];
	int sval, rank = 0;

	for (sval = 1; sval <= base->num_svals; sval++) {
		struct object_kind *kind = lookup_kind(tval, sval);
		const struct class_book *book;

		if (!kind) continue;
		book = object_kind_to_book(kind);
		if (!book || !book->dungeon) continue;
		if (++rank == n) return kind;
	}

	return NULL;
}

/**
 * Append \p kind to a store's always-stock list (idempotent) and put one in the
 * stock right now, so it shows up without waiting for the next day's turnover.
 * Returns true if the kind was newly added.
 */
static bool ap_store_always_add(struct store *s, struct object_kind *kind)
{
	struct object *obj;
	size_t i;

	if (!s || !kind) return false;

	for (i = 0; i < s->always_num; i++)
		if (s->always_table[i] == kind) return false;	/* already stocked */

	if (!s->always_num) {
		s->always_size = 8;
		s->always_table = mem_zalloc(s->always_size * sizeof(*s->always_table));
	} else if (s->always_num >= s->always_size) {
		s->always_size += 8;
		s->always_table = mem_realloc(s->always_table,
			s->always_size * sizeof(*s->always_table));
	}
	s->always_table[s->always_num++] = kind;

	obj = ap_make_kind_object(kind, 1);
	if (obj && !store_carry(s, obj)) {
		object_delete(NULL, NULL, &obj->known);
		obj->known = NULL;
		object_delete(NULL, NULL, &obj);
	}
	return true;
}

/**
 * Expanded-shop-books boon: each grant adds one more tier of dungeon books to
 * the town bookseller -- the \p n-th dungeon book (sval order) of every realm.
 * The bookseller's stock is rebuilt from store.txt every launch, so this is
 * re-applied (idempotently) for every received copy on every connect/replay.
 */
static void ap_boon_expanded_books(int n)
{
	static const int book_tvals[] = {
		TV_MAGIC_BOOK, TV_PRAYER_BOOK, TV_NATURE_BOOK, TV_SHADOW_BOOK
	};
	struct store *book = &stores[f_info[FEAT_STORE_BOOK].shopnum - 1];
	bool any = false;
	size_t k;

	for (k = 0; k < N_ELEMENTS(book_tvals); k++) {
		struct object_kind *kind = ap_nth_dungeon_book(book_tvals[k], n);
		if (kind && ap_store_always_add(book, kind)) any = true;
	}

	if (any) msg("AP: The bookseller stocks deeper spellbooks (tier %d).", n);
}

/**
 * Boons that touch state rebuilt from scratch every launch (the town
 * bookseller).  Must be re-applied on every replay, not just first receipt;
 * each is idempotent.  Returns true if \p name was such a boon.
 */
static bool ap_apply_replay_boon(const char *name)
{
	if (streq(name, "Expanded Starting Shop Books")) {
		ap_boon_expanded_books(++ap_books_count);
		return true;
	}
	return false;
}

/**
 * Boons that mutate saved player state (gold, experience) and so must be
 * applied exactly once per character.  Returns true if \p name was such a boon.
 */
static bool ap_apply_oneshot_boon(const char *name)
{
	if (streq(name, "Triple Starting Gold")) {
		ap_boon_triple_gold();
		return true;
	}
	if (streq(name, "+5 levels of Experience")) {
		ap_boon_extra_levels(5);
		return true;
	}
	return false;
}

/**
 * An item was granted to our slot, with a stable per-slot sequence index.
 *
 * De-duplication is per character via a high-water mark in the save file: a
 * fresh character starts at 0 and receives the full replay (restocking its empty
 * home), while the same character reconnecting has its mark past the replay and
 * adds nothing.  This is why we can't use APCc's server-side notify flag.
 *
 * Counting runs for *every* occurrence regardless of the high-water mark: the
 * progressive-category counters (so the Nth-artifact mapping stays correct
 * across skipped items), the expanded-books tier counter, and re-applying boons
 * over launch-rebuilt state (the bookseller).  State that is saved -- home and
 * pack contents, gold, experience -- is applied once, gated by the mark.
 *
 * If a home/inventory item can't be placed (everything full), the mark is left
 * at that item and the rest of the stream is held back (ap_blocked) so the mark
 * stays contiguous; a later replay (after space frees up) retries from there.
 */
static void ap_item_granted(const char *item_name, uint64_t index)
{
	int prog_n = 0, cat;

	if (!player) return;

	/* A fresh replay restarts at index 0: recompute all transient counters. */
	if (index == 0) {
		memset(ap_prog_count, 0, sizeof(ap_prog_count));
		ap_books_count = 0;
		ap_blocked = false;
	}

	/* Advance the category counter even for items we won't re-deliver. */
	cat = ap_prog_cat_of(item_name);
	if (cat >= 0) prog_n = ++ap_prog_count[cat];

	/* Re-applied every replay (idempotent). */
	ap_apply_replay_boon(item_name);

	/* Once-per-character below: skip anything at/under the high-water mark. */
	if (index < player->ap_items_received) return;

	/* A prior item this stream is waiting for space: hold the mark contiguous. */
	if (ap_blocked) return;

	if (ap_apply_oneshot_boon(item_name)) {
		player->ap_items_received = (uint32_t)(index + 1);
	} else if (ap_deliver_item(item_name, prog_n)) {
		player->ap_items_received = (uint32_t)(index + 1);
	} else {
		/* No room anywhere: stop here and retry this item on a later connect. */
		ap_blocked = true;
	}
}

/** A DeathLink arrived: kill the player (without bouncing it back out). */
static void ap_deathlink_received(void)
{
	if (!player || player->is_dead) return;

	ap_dying_from_deathlink = true;
	take_hit(player, 32000, "an Archipelago death");
	ap_dying_from_deathlink = false;
}

void ap_game_player_died(void)
{
	/* Don't echo a received DeathLink back to the multiworld. */
	if (ap_dying_from_deathlink) return;
	ap_send_deathlink();
}

void ap_game_player_won(void)
{
	ap_send_victory();
}

const struct artifact *ap_find_missed_location(int *price_out)
{
	const struct artifact *best = NULL;
	int i;

	/* Only meaningful in artifacts-as-checks mode, once connected. */
	if (!ap_artifacts_as_checks() || !ap_is_connected()) return NULL;
	if (!ap_ensure_artifact_table() || !player) return NULL;

	for (i = 1; i < z_info->a_max; i++) {
		const struct artifact *art = &a_info[i];

		if (!art->name) continue;
		if (ap_artifact_checked[art->aidx]) continue;     /* already found */
		if (art->alloc_prob <= 0) continue;               /* not a normal drop
		                                                   * (story artifacts like
		                                                   * Grond aren't AP
		                                                   * locations) */
		if (player->max_depth < art->alloc_min) continue; /* not yet deep enough */
		if (art->cost <= 0) continue;                     /* need a price */

		/* Lowest spawn depth first; break ties by the cheaper artifact. */
		if (!best
				|| art->alloc_min < best->alloc_min
				|| (art->alloc_min == best->alloc_min && art->cost < best->cost))
			best = art;
	}

	if (!best) return NULL;
	if (price_out) *price_out = 3 * best->cost;
	return best;
}

void ap_buy_missed_location(const struct artifact *art)
{
	if (!art) return;

	/* Mark it found locally so we don't re-offer it before the server's
	 * confirmation (and replay) comes back and sets the same flag. */
	if (ap_ensure_artifact_table())
		ap_artifact_checked[art->aidx] = true;

	/*
	 * Send the location check.  Whatever item sits at this location is released
	 * by the server through the normal grant path -- we do not hand out the
	 * artifact here.
	 */
	ap_send_artifact_check(art);
}

void ap_game_reset_for_new_life(void)
{
	/* A fresh character must receive the full item replay again. */
	if (player) player->ap_items_received = 0;

	/*
	 * Drop the connection; process_player's next ap_service() call sees it is
	 * down and reconnects with the (preserved) server/slotname, which re-fires
	 * the Connected event -> checked-location replay (uniques stay dead) and
	 * item replay (restock the new home, re-apply boons).
	 */
	ap_shutdown();
}

/**
 * Authentication finished.  The checked-location replay has already run (marking
 * known-dead uniques), so reconcile the other direction: send checks for any
 * unique that is dead in this save file but that the server might not know about
 * (e.g. killed while disconnected).  The server de-duplicates already-known
 * checks, so this is safe to run on every connect.
 */
static void ap_on_connected(void)
{
	int i;

	for (i = 0; i < z_info->r_max; i++) {
		struct monster_race *race = &r_info[i];

		if (!race->name) continue;
		if (!rf_has(race->flags, RF_UNIQUE)) continue;
		if (race->max_num != 0) continue;       /* still alive */

		ap_send_check(race->name);
	}
}

void ap_game_item_picked_up(const struct object *obj)
{
	if (!obj || !obj->artifact) return;
	if (!ap_artifacts_as_checks()) return;

	ap_send_artifact_check(obj->artifact);
}

void ap_game_setup(void)
{
	ap_set_check_handler(ap_check_confirmed);
	ap_set_item_handler(ap_item_granted);
	ap_set_connect_handler(ap_on_connected);
	ap_set_deathlink_handler(ap_deathlink_received);
}
