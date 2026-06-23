/**
 * \file ap-game.h
 * \brief Angband-side Archipelago glue: wires AP events to game state.
 *
 * Unlike apinterface.c (which owns the APCc/glib/winsock dependencies), this
 * lives on the normal Angband side and may freely use the game's headers.  It
 * registers handlers with apinterface so that AP item/location events act on the
 * world (e.g. keeping killed uniques dead, stocking the home).
 */
#ifndef AP_GAME_H
#define AP_GAME_H

struct object;

/** Register the Archipelago game-side handlers.  Idempotent; call at game start. */
void ap_game_setup(void);

/**
 * Report that the player picked up \p obj.  When artifacts-as-checks mode is on
 * and the object is an artifact, this sends the matching location check.  No-op
 * otherwise.
 */
void ap_game_item_picked_up(const struct object *obj);

#endif /* AP_GAME_H */
