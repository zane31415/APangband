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
 */
#ifndef APINTERFACE_H
#define APINTERFACE_H

#include <stdbool.h>

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

#endif /* APINTERFACE_H */
