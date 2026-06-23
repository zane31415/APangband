/**
 * \file borg-messages-react.c
 * \brief React to messages that have been received and parsed
 *
 * Copyright (c) 1997 Ben Harrison, James E. Wilson, Robert A. Koeneke
 * Copyright (c) 2007-9 Andi Sidwell, Chris Carr, Ed Graham, Erik Osheim
 *
 * This work is free software; you can redistribute it and/or modify it
 * under the terms of either:
 *
 * a) the GNU General Public License as published by the Free Software
 *    Foundation, version 2, or
 *
 * b) the "Angband License":
 *    This software may be copied and distributed for educational, research,
 *    and not for profit purposes provided that this copyright and statement
 *    are included in all such copies.  Other copyrights may also apply.
 */

#include "borg-messages-react.h"

#ifdef ALLOW_BORG

#include "../cmds.h"

#include "borg-io.h"
#include "borg-log.h"
#include "borg-messages.h"
#include "borg-reincarnate.h"
#include "borg-think.h"
#include "borg-trait.h"
#include "borg.h"

bool borg_dont_react = false;

/*
 * Bounded recovery for the "unexpected request for direction" desync.
 *
 * The borg sometimes queues a fixed-length key sequence for a menu whose
 * real key count is state-dependent (e.g. borg_destroy_floor()'s "k - a a"
 * ignore sequence, where the starting item list and the dynamic ignore menu
 * change how many keys are actually consumed).  Leftover keys then execute as
 * top-level commands; one of them (e.g. 'c' close) raises a "Direction?"
 * prompt the borg never queued a target for.  Upstream simply borg_oops()es
 * here, halting the borg so a human can debug -- which wedges an unattended
 * run forever.  Instead we ESCAPE the prompt and flush the stray keys so the
 * borg re-plans, but cap how many times this happens without progress so a
 * genuinely deterministic desync still halts rather than spinning forever.
 */
#define BORG_MAX_UNEXPECTED_DIRECTION 5
static int     borg_unexpected_direction_count  = 0;
static int16_t borg_unexpected_direction_last_t = 0;

/*
 * Handle various "important" messages
 *
 * Actually, we simply "queue" them for later analysis
 */
void borg_react(const char *msg, const char *buf)
{
    int len;

    if (borg_dont_react || borg.trait[BI_ISPARALYZED]) {
        borg_note("# Ignoring messages.");
        return;
    }

    /* Note actual message */
    if (borg_cfg[BORG_VERBOSE])
        borg_note(format("> Reacting Msg (%s)", msg));

    /* Extract length of parsed message */
    len = strlen(buf);

    /* trim off trailing , if there is one, seems to have been introduced to
     * some uniques messages */
    if (len && buf[len - 1] == ',') {
        char *tmp  = (char *)buf; /* cast away const */
        tmp[--len] = 0;
    }

    /* Verify space */
    if (borg_msg_num + 1 > borg_msg_max) {
        borg_note("too many messages");
        return;
    }

    /* Verify space */
    if (borg_msg_len + len + 1 > borg_msg_siz) {
        borg_note("too much messages");
        return;
    }

    /* Assume not used yet */
    borg_msg_use[borg_msg_num] = 0;

    /* Save the message position */
    borg_msg_pos[borg_msg_num] = borg_msg_len;

    /* Save the message text */
    my_strcpy(borg_msg_buf + borg_msg_len, buf, borg_msg_siz - borg_msg_len);

    /* Advance the buf */
    borg_msg_len += len + 1;

    /* Advance the pos */
    borg_msg_num++;
}

/*
 * Handle various messages that need response
 */
bool borg_react_prompted(const char* buf, struct keypress *key, int x, int y)
{
    /* Mega-Hack -- Catch "Die? [y/n]" messages */
    /* If there is text on the first line... */
    /* And the game does not want a command... */
    /* And the cursor is on the top line... */
    /* And the text acquired above is "Die?" */
    if (y == 0 && x >= 4 && prefix(buf, "Die?") && borg_cheat_death) {
        /* Flush messages */
        borg_parse(NULL);

        /* flush the buffer */
        borg_flush();

        /* Take note */
        borg_note("# Death -- reincarnating to try again...");

        /* Dump the Character Map*/
        if (borg.trait[BI_CLEVEL] >= borg_cfg[BORG_DUMP_LEVEL]
            || strstr(player->died_from, "starvation"))
            borg_write_map(false);

        /* Log the death */
        borg_log_death();
        borg_log_death_data();

#if 0
        /* Note the score */
        borg_enter_score();
#endif

        /*
         * Roll up a fresh character and try again rather than god-mode healing
         * the dead one: the old character's progress is genuinely lost, which is
         * what we want for Archipelago test runs (reincarnate_borg() also
         * reconnects the slot so the new life gets its item replay).  Note this
         * still uses the engine's cheat_live intercept, so it fires before
         * is_dead is set -- no DeathLink is broadcast for a reincarnated death.
         */
        reincarnate_borg();
        borg_respawning = 7;

        key->code = 'n';
        return true;
    }

    /* with 292, there is a flush(0, 0, 0) introduced as it asks for
     * confirmation. This flush is messing up the borg.  This will allow the
     * borg to work around the flush Attempt to catch "Attempt it anyway? [y/n]"
     */
    if (y == 0 && x >= 4 && prefix(buf, "Atte")) {
        /* Return the confirmation */
        if (borg_cfg[BORG_VERBOSE])
            borg_note("# Confirming use of Spell/Prayer when low on mana.");
        key->code = 'y';
        return true;
    }

    /* Wearing two rings.  Place this on the left hand */
    if (y == 0 && x >= 12 && (prefix(buf, "(Equip: c-d,"))) {
        /* Left hand */
        key->code = 'c';
        if (borg_cfg[BORG_VERBOSE])
            borg_note("# Putting ring on the left hand.");
        return true;
    }
    /*
     * This frequently catches when the key queue has become messed up
     * The borg will press a key and get a "Direction?" type prompt and 
     * not be expecting it. The borg should halt in that case so the bad 
     * code can be found and cleaned up.
     */
    if (y == 0 && !borg_inkey(false)
        && (x >= 10) && strncmp(buf, "Direction", 9) == 0) {
        if (borg_confirm_target) {
            if (borg_cfg[BORG_VERBOSE])
                borg_note("# Expected request for Direction.");
            /* reset the flag */
            borg_confirm_target = false;
            /* Return queued target */
            key->code = borg_get_queued_direction();
            return key;
        }
        else {
            borg_note("** UNEXPECTED REQUEST FOR DIRECTION Dumping keypress history ***");
            borg_note(format("** line starting <%s> ***", buf));
            borg_dump_recent_keys(20);

            /*
             * If the borg has made progress since the last occurrence,
             * treat this as a fresh incident rather than accumulating
             * toward the halt threshold.  borg_t advances roughly once per
             * think cycle, so a real desync loop (which makes no progress)
             * keeps firing with borg_t barely moved, while genuinely
             * unrelated incidents spread out in time reset the counter.
             */
            if (borg_t - borg_unexpected_direction_last_t > 50)
                borg_unexpected_direction_count = 0;
            borg_unexpected_direction_last_t = borg_t;

            if (++borg_unexpected_direction_count
                <= BORG_MAX_UNEXPECTED_DIRECTION) {
                borg_note(format("# Recovering from unexpected direction prompt "
                                 "(attempt %d/%d): escaping and flushing keys.",
                    borg_unexpected_direction_count,
                    BORG_MAX_UNEXPECTED_DIRECTION));
                /* Drop every stray queued key so the borg re-plans cleanly */
                borg_flush();
                /* Dismiss the unexpected prompt */
                key->code = ESCAPE;
                return true;
            }

            /* Repeated desync with no progress -- halt for debugging */
            borg_unexpected_direction_count = 0;
            borg_oops("unexpected request for direction");
            /* Hack -- Escape */
            key->code = ESCAPE;
            return true;
        }
    }

    /* Stepping on a stack when the inventory is full gives a message */
    /* and the keypress when given this message is requeued so the borg */
    /* thinks it is a user keypress when it isn't */
    if (y == 0 && x >= 12 && (prefix(buf, "You have no room"))) {
        if (borg_cfg[BORG_VERBOSE])
            borg_note("# 'You have no room' is a no-op");
        /* key code 0 seems to be a no-op and ignored as a user keypress */
        key->code = 0;
        return true;
    }

    /* ***MEGA-HACK***  */
    /* This will be hit if the borg uses an unidentified effect that has */
    /* EF_SELECT/multiple effects. Always pick "one of the following at */
    /* random" when the item is used post ID, an effect will be selected */
    if (!borg_inkey(false) && y == 1 && prefix(buf, "Which effect?")) {
        if (borg_cfg[BORG_VERBOSE])
            borg_note("# Use of unknown object with multiple effects");
        /* the first selection (a) is random */
        key->code = 'a';
        return key;
    }

    /* This will be hit if the borg uses am item with the WONDER effect and */
    /* it picks banishment.  Always just escape out. Banishment is too */
    /* dangerous to randomly pick */
    if (!borg_inkey(false)
        && prefix(buf, "Choose a monster race (by symbol) to banish")) {
        if (borg_cfg[BORG_VERBOSE])
            borg_note("# Use of Wonder selected BANISHMENT");
        key->code = ESCAPE;
        return key;
    }

    /* prompt for stepping in lava.  This should be avoided but */
    /* if the borg is stuck, give him a pass */
    if (y == 0 && x >= 12
        && (prefix(buf, "The lava will") || prefix(buf, "Lava blocks y"))) {
        if (borg_cfg[BORG_VERBOSE])
            borg_note("# ignoring Lava warning");
        /* yes step in */
        key->code = 'y';
        return true;
    }

    /* prompt for recall depth */
    if (!borg_inkey(false) && y == 0 && x >= 12
        && prefix(buf, "Set recall depth")) {
        if (borg_cfg[BORG_VERBOSE])
            borg_note("# Use of unknown object with recall");
        /* the first selection (a) is random */
        key->code = 'n';
        return true;
    }

    /* make sure inventory is used for throwing */
    if (y == 1 && x >= 12 && (prefix(buf, "Throw which item? (Throw"))) {
        if (borg_cfg[BORG_VERBOSE])
            borg_note("# switching to inventory for throws");
        /* yes step in */
        key->code = '/';
        return true;
    }

    /* Sometimes the borg will overshoot the range limit of his shooter */
    if (x >= 12 && (prefix(buf, "Target out of range"))) {
        /* Fire Anyway? [Y/N] */
        /* yes step in */
        key->code = 'y';
        return true;
    }


    return false;
}

/*
 * Clear saved messages
 */
void borg_clear_reactions(void)
{
    borg_msg_len = 0;
    borg_msg_num = 0;
}

#endif
