/**
 * \file apinterface.c
 * \brief Glue between Angband's C code and the vendored APCc Archipelago client.
 *
 * Keeps all of the APCc / glib / libwebsockets / winsock surface confined to
 * this one translation unit.  The rest of the game talks to Archipelago only
 * through the small API declared in apinterface.h, and passes connection
 * details in as plain strings so that this file never has to include player.h
 * (whose PF_* player flags collide with winsock's PF_* protocol families).
 */

/*
 * Pull in the Archipelago client first.  APCc.h includes <glib.h>, which
 * defines MAX/MIN/ABS as function-like macros that collide with Angband's
 * definitions in h-basic.h.  We don't use glib's versions here, so undefine
 * them before the Angband headers so Angband's win cleanly.
 */
#include "apcc/APCc.h"

#undef MAX
#undef MIN
#undef ABS

#include "message.h"
#include "z-util.h"

#define AP_GAME_NAME "Angband"

/*
 * APCc's AP_Init() stores the ip/player-name/password *pointers* (not copies)
 * and dereferences them later from AP_WebService().  Keep our own static
 * storage so the strings outlive the call that starts the connection.
 */
static char ap_host[128];
static char ap_slot[64];
static char ap_password[1];           /* empty for now: no password support */

static bool ap_started = false;       /* AP_Init/AP_Start have been called */
static bool ap_announced = false;     /* "connected" message already shown */
static bool ap_failed = false;        /* setup failed; don't keep retrying */

/* --- Required APCc callbacks ------------------------------------------------
 *
 * These are intentionally thin for now.  Wiring received items and location
 * checks into actual Angband gameplay is the next milestone; for the moment we
 * just surface activity so a connection can be observed working.
 */

static void ap_cb_item_clear(void)
{
	/* Reset local item state on (re)sync.  Nothing to do yet. */
}

static void ap_cb_item_recv(uint64_t item_id, int sending_player, bool notify)
{
	char buf[64];

	(void)sending_player;
	if (!notify) return;

	/* Angband's msg() formatter lacks %llu, so render the id with libc. */
	snprintf(buf, sizeof(buf), "%llu", (unsigned long long)item_id);
	msg("AP: received item %s.", buf);
}

static void ap_cb_location_checked(uint64_t loc_id)
{
	(void)loc_id;
}

/* --- Helpers --------------------------------------------------------------- */

/**
 * Split "host:port" (e.g. "archipelago.gg:32000") into host and port.
 *
 * \return true on success.
 */
static bool ap_parse_server(const char *src, char *host, size_t hostlen,
		int *port)
{
	const char *colon = strrchr(src, ':');
	size_t hlen;

	if (!colon || colon == src) return false;

	hlen = (size_t)(colon - src);
	if (hlen >= hostlen) hlen = hostlen - 1;
	memcpy(host, src, hlen);
	host[hlen] = '\0';

	*port = atoi(colon + 1);
	return (*port > 0);
}

/**
 * Initialise the APCc client and begin connecting using the server/slotname
 * captured during character birth.
 */
static void ap_begin(const char *server, const char *slotname)
{
	int port = 0;

	if (!server || server[0] == '\0') {
		/* No AP server configured: Archipelago is simply disabled. */
		ap_failed = true;
		return;
	}

	if (!ap_parse_server(server, ap_host, sizeof(ap_host), &port)) {
		msg("AP: could not parse server '%s' (expected host:port).",
			server);
		ap_failed = true;
		return;
	}

	my_strcpy(ap_slot, slotname ? slotname : "", sizeof(ap_slot));
	ap_password[0] = '\0';

	AP_SetItemClearCallback(ap_cb_item_clear);
	AP_SetItemRecvCallback(ap_cb_item_recv);
	AP_SetLocationCheckedCallback(ap_cb_location_checked);

	AP_Init(ap_host, port, AP_GAME_NAME, ap_slot, ap_password);
	AP_Start();

	ap_started = true;
	msg("AP: connecting to %s:%d as '%s'...", ap_host, port, ap_slot);
}

/* --- Public API ------------------------------------------------------------ */

void ap_service(const char *server, const char *slotname)
{
	if (ap_failed) return;
	if (!ap_started) ap_begin(server, slotname);
	if (!ap_started) return;

	AP_WebService();

	if (!ap_announced && AP_GetConnectionStatus() == Authenticated) {
		ap_announced = true;
		msg("AP: connected to Archipelago as '%s'.", ap_slot);
	}
}

bool ap_is_connected(void)
{
	return ap_started && AP_GetConnectionStatus() == Authenticated;
}

void ap_shutdown(void)
{
	if (ap_started) {
		AP_Shutdown();
		ap_started = false;
		ap_announced = false;
	}
}
