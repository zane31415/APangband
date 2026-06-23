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
#include "apinterface.h"
#include "ap-game.h"

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
static void ap_item_granted(const char *item_name, uint64_t index)
{
	/*
	 * TODO: stock the home with this item.  Use a per-character high-water
	 * mark saved in the player file: only deliver when index >= mark, then
	 * set mark = index + 1.  A fresh character starts at mark 0 and receives
	 * the full replay (empty home gets restocked); the same character
	 * reconnecting has mark past the replay and adds nothing.  Item->object
	 * mapping (specific artifacts, consumables, progressives, non-object
	 * boons) is still to be designed.
	 */
	msg("AP: received item #%lu '%s' (home delivery not yet implemented).",
		(unsigned long)index, item_name);
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
	 * ("Phial" + " of Galadriel").  Try both forms and send whichever the
	 * data package actually contains.
	 */
	if (ap_location_known(art->name)) {
		ap_send_check(art->name);
		return;
	}

	if (obj->kind && obj->kind->name) {
		strnfmt(full, sizeof(full), "%s %s", obj->kind->name, art->name);
		if (ap_location_known(full)) {
			ap_send_check(full);
			return;
		}
	}

	msg("AP: no location matched artifact '%s'.", art->name);
}

void ap_game_setup(void)
{
	ap_set_check_handler(ap_check_confirmed);
	ap_set_item_handler(ap_item_granted);
	ap_set_connect_handler(ap_on_connected);
}
