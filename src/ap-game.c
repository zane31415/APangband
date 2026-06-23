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
static void ap_item_granted(const char *item_name)
{
	msg("AP: received '%s' (home delivery not yet implemented).", item_name);
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

void ap_game_setup(void)
{
	ap_set_check_handler(ap_check_confirmed);
	ap_set_item_handler(ap_item_granted);
	ap_set_connect_handler(ap_on_connected);
}
