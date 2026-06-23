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
static bool ap_announced = false;     /* "connected" handling already done */
static bool ap_failed = false;        /* setup failed; don't keep retrying */

/* Handlers registered by the game-side glue (ap-game.c). */
static void (*ap_check_handler)(const char *name) = NULL;
static void (*ap_item_handler)(const char *item_name) = NULL;
static void (*ap_connect_handler)(void) = NULL;

/*
 * Checked-location ids can arrive (as a replay) before the data package is
 * loaded, so their names aren't resolvable yet.  Park them here and drain once
 * the data package is synced.
 */
#define AP_PENDING_MAX 1024
static uint64_t ap_pending_checks[AP_PENDING_MAX];
static int ap_pending_count = 0;

/* --- Internal helpers ------------------------------------------------------ */

/** Resolve a checked location id to its name and hand it to the game. */
static void ap_deliver_check(uint64_t loc_id)
{
	char *name = AP_GetLocationName(loc_id);
	if (ap_check_handler && name) ap_check_handler(name);
}

static void ap_pending_push(uint64_t loc_id)
{
	if (ap_pending_count < AP_PENDING_MAX)
		ap_pending_checks[ap_pending_count++] = loc_id;
	else
		ap_deliver_check(loc_id);   /* overflow: best effort, deliver now */
}

static void ap_pending_drain(void)
{
	int i;
	for (i = 0; i < ap_pending_count; i++)
		ap_deliver_check(ap_pending_checks[i]);
	ap_pending_count = 0;
}

/* --- Required APCc callbacks ----------------------------------------------- */

static void ap_cb_item_clear(void)
{
	/* Reset local item state on (re)sync.  Nothing to do yet. */
}

static void ap_cb_item_recv(uint64_t item_id, int sending_player, bool notify)
{
	char *name;

	(void)sending_player;
	(void)notify;       /* deliver replays too: a fresh character's home is empty */

	name = AP_GetItemName(item_id);
	if (ap_item_handler && name) ap_item_handler(name);
}

static void ap_cb_location_checked(uint64_t loc_id)
{
	/*
	 * If the data package is loaded we can resolve the name now; otherwise
	 * defer (this happens for the checked-location replay that arrives with
	 * the Connected packet, before the data package on a first connect).
	 */
	if (AP_GetDataPackageStatus() == Synced)
		ap_deliver_check(loc_id);
	else
		ap_pending_push(loc_id);
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

	/* Once the data package is up, flush any checks deferred during connect. */
	if (ap_pending_count > 0 && AP_GetDataPackageStatus() == Synced)
		ap_pending_drain();

	if (!ap_announced && AP_GetConnectionStatus() == Authenticated) {
		ap_announced = true;
		msg("AP: connected to Archipelago as '%s'.", ap_slot);
		if (ap_connect_handler) ap_connect_handler();
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

void ap_send_check(const char *name)
{
	int64_t loc;

	if (!ap_started || !name) return;
	loc = AP_GetLocationIdByName(name);
	if (loc < 0) return;            /* unknown location or data package not ready */
	AP_SendItem((uint64_t)loc);
}

void ap_set_check_handler(void (*fn)(const char *name))
{
	ap_check_handler = fn;
}

void ap_set_item_handler(void (*fn)(const char *item_name))
{
	ap_item_handler = fn;
}

void ap_set_connect_handler(void (*fn)(void))
{
	ap_connect_handler = fn;
}
