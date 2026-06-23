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
#include "player.h"
#include "player-util.h"
#include "store.h"
#include "apinterface.h"
#include "ap-game.h"

/* Set while killing the player in response to a received DeathLink, so we don't
 * bounce that same death back out as another DeathLink. */
static bool ap_dying_from_deathlink = false;

/**
 * A location check was confirmed for our slot.  This fires both for live checks
 * and -- crucially -- for the replay of every previously-checked location right
 * after connecting.  Locations are named after unique monsters (and artifacts);
 * for a unique we zero its max_num so it stays dead, including for a brand-new
 * character reincarnating into the same AP slot.
 *
 * Non-monster location names (artifacts) simply don't resolve to a race and are
 * ignored here.
 */
static void ap_check_confirmed(const char *name)
{
	struct monster_race *race = lookup_monster(name);

	if (!race) return;
	if (!rf_has(race->flags, RF_UNIQUE)) return;

	/* Killed for good: prevents this unique from being generated again. */
	race->max_num = 0;
}

/**
 * An item was granted to our slot.  Fires for live grants and for the replay of
 * all previously-received items on connect, so it is the hook for stocking the
 * player's home with Archipelago loot.
 *
 * TODO: create the corresponding object(s) and place them in the home store.
 * This needs: name -> object mapping (artifacts via lookup_artifact_name();
 * consumables like "Rod of Recall"/"Scroll of Acquirement" via kind lookup;
 * "Progressive ... Artifact" via per-category ordering), plus de-duplication so
 * reconnecting the same character doesn't re-stock items already in the home
 * (track a high-water mark of granted items in the save file).
 */
/**
 * Turn an Archipelago item name into a game object to place in the home.
 *
 * TODO: the actual name->object mapping.  Cases to handle:
 *   - specific artifacts (e.g. "Pick of Erebor") via lookup_artifact_name();
 *   - consumables ("Rod of Recall", "Scroll of Acquirement", potions...) via
 *     object kind, with any leading quantity in the name;
 *   - "Progressive <Category> Artifact": the Nth artifact of that category,
 *     categories ordered by artifact.txt 'level' (depth);
 *   - non-object boons ("+5 levels of Experience", "Triple Starting Gold",
 *     "Expanded Starting Shop Books") need separate, non-home handling.
 * Returns NULL until implemented, so nothing is placed yet.
 */
static struct object *ap_make_item(const char *name)
{
	(void)name;
	return NULL;
}

/** Deliver a single granted item into the player's home. */
static void ap_deliver_item(const char *name)
{
	struct object *obj = ap_make_item(name);

	if (!obj) {
		msg("AP: '%s' has no home delivery mapping yet.", name);
		return;
	}

	home_carry(obj);
	msg("AP: '%s' delivered to your home.", name);
}

/**
 * An item was granted to our slot, with a stable per-slot sequence index.
 *
 * De-duplication is per character via a high-water mark in the save file: a
 * fresh character starts at 0 and receives the full replay (restocking its empty
 * home), while the same character reconnecting has its mark past the replay and
 * adds nothing.  This is why we can't use APCc's server-side notify flag.
 */
static void ap_item_granted(const char *item_name, uint64_t index)
{
	if (!player) return;

	/* Already delivered to this character. */
	if (index < player->ap_items_received) return;

	ap_deliver_item(item_name);

	/* Advance the contiguous high-water mark past this item. */
	player->ap_items_received = (uint32_t)(index + 1);
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
	const struct artifact *art;
	char full[120];

	if (!obj || !obj->artifact) return;
	if (!ap_artifacts_as_checks()) return;

	art = obj->artifact;

	/*
	 * Artifact location names don't follow one rule: proper-named artifacts
	 * are just the name ("Angrist"), while suffix artifacts are base + name
	 * ("Phial" + " of Galadriel").  Send both forms; ap_send_check() ignores
	 * the one that isn't a real location (the service thread resolves names).
	 */
	ap_send_check(art->name);
	if (obj->kind && obj->kind->name) {
		strnfmt(full, sizeof(full), "%s %s", obj->kind->name, art->name);
		ap_send_check(full);
	}
}

void ap_game_setup(void)
{
	ap_set_check_handler(ap_check_confirmed);
	ap_set_item_handler(ap_item_granted);
	ap_set_connect_handler(ap_on_connected);
	ap_set_deathlink_handler(ap_deathlink_received);
}
