/**
 * \file apinterface.h
 * \brief C interface between Angband and the vendored APCc Archipelago client.
 *
 * This header is deliberately free of the APCc/glib/libwebsockets includes so
 * that ordinary Angband translation units can call into the Archipelago client
 * without dragging in (and clashing with) those dependencies.  Notably,
 * libwebsockets pulls in winsock, whose PF_* protocol-family constants collide
 * with Angband's PF_* player flags in player.h -- so callers pass the connection
 * details in as plain strings rather than apinterface.c including player.h.
 * Only apinterface.c includes apcc/APCc.h.
 *
 * Game-side logic that needs Angband headers (marking uniques dead, stocking the
 * home, etc.) lives in ap-game.c and registers callbacks here via the
 * ap_set_*_handler() functions; this file translates AP item/location ids to
 * names and routes them to those handlers.
 */
#ifndef APINTERFACE_H
#define APINTERFACE_H

#include <stdbool.h>
#include <stdint.h>

/**
 * Service the Archipelago connection.
 *
 * On the first call, if \p server is non-empty, this initialises the APCc
 * client and begins connecting to \p server (expected form "host:port") as slot
 * \p slotname.  Subsequent calls pump websocket events; \p server and
 * \p slotname are ignored after the initial setup.  Safe to call every game
 * turn -- it is non-blocking.  A no-op when \p server is empty.
 */
void ap_service(const char *server, const char *slotname);

/** \return true once the slot is connected and authenticated. */
bool ap_is_connected(void);

/** Tear down the Archipelago connection (call on game exit). */
void ap_shutdown(void);

/**
 * Send the location check named \p name (e.g. a unique monster's name) to the
 * server.  No-op if not connected, the data package isn't loaded yet, or the
 * name isn't a known AP location.  The server de-duplicates already-sent checks.
 */
void ap_send_check(const char *name);

/**
 * Register the handler called when a location is confirmed checked for this slot
 * -- including replays of all previously-checked locations right after
 * connecting (used to keep killed uniques dead across character deaths).  The
 * handler receives the location's name.
 */
void ap_set_check_handler(void (*fn)(const char *name));

/**
 * Register the handler called when an item is granted to this slot -- including
 * replays of all previously-received items on connect (used to stock the home).
 * The handler receives the item's name and its stable 0-based sequence index in
 * this slot's received-items stream (the same item always has the same index
 * across reconnects), so the game can persist a per-character high-water mark
 * and avoid re-delivering items it has already placed.
 */
void ap_set_item_handler(void (*fn)(const char *item_name, uint64_t index));

/**
 * \return true if \p name is a location the connected game knows about (its data
 * package is loaded and contains it).  Lets the game pick among candidate names
 * (e.g. artifact naming variants) before sending a check.
 */
bool ap_location_known(const char *name);

/**
 * \return true if the server's slot_data enabled the "artifacts as checks" mode
 * (slot_data key "artifacts_as_checks"): artifacts become AP location checks and
 * the real artifacts are granted as items, instead of spawning normally.
 * Defaults to false until/unless the server sends the option.
 */
bool ap_artifacts_as_checks(void);

/**
 * Send a DeathLink to other linked players (call when the local player dies of
 * something other than a received DeathLink).  No-op if not connected.
 */
void ap_send_deathlink(void);

/**
 * Register the handler called when a DeathLink is received from another player.
 * The handler should kill the local player.
 */
void ap_set_deathlink_handler(void (*fn)(void));

/**
 * Register the handler called once when the slot finishes authenticating.  Fires
 * after the initial checked-location replay, so it is a good place to reconcile
 * game state with the server (e.g. send checks for uniques already dead in the
 * save file from a kill made while offline).
 */
void ap_set_connect_handler(void (*fn)(void));

#endif /* APINTERFACE_H */
