/**
 * \file cmd-core.c
 * \brief Handles the queueing of game commands.
 *
 * Copyright (c) 2008-9 Antony Sidwell
 * Copyright (c) 2014 Andi Sidwell
 *
 * This work is free software; you can redistribute it and/or modify it
 * under the terms of either:
 *
 * a) the GNU General Public License as published by the Free Software
 *    Foundation, version 2, or
 *
 * b) the "Angband licence":
 *    This software may be copied and distributed for educational, research,
 *    and not for profit purposes provided that this copyright and statement
 *    are included in all such copies.  Other copyrights may also apply.
 */

#include "angband.h"
#include "cmds.h"
#include "cmd-core.h"
#include "effects-info.h"
#include "game-input.h"
#include "game-world.h"
#include "obj-chest.h"
#include "obj-desc.h"
#include "obj-tval.h"
#include "obj-util.h"
#include "player-attack.h"
#include "player-birth.h"
#include "player-calcs.h"
#include "player-spell.h"
#include "player-timed.h"
#include "player-util.h"
#include "store.h"
#include "target.h"

errr (*cmd_get_hook)(cmd_context c);

/**
 * ------------------------------------------------------------------------
 * A simple list of commands and their handling functions.
 * ------------------------------------------------------------------------ */
struct command_info
{
	cmd_code cmd;
	const char *verb;
	cmd_handler_fn fn;
	bool repeat_allowed;
	bool can_use_energy;
	int auto_repeat_n;
};

static const struct command_info game_cmds[] =
{
	{ CMD_LOADFILE, "load a savefile", NULL, false, false, 0 },
	{ CMD_NEWGAME, "start a new game", NULL, false, false, 0 },

	{ CMD_BIRTH_INIT, "start the character birth process", do_cmd_birth_init, false, false, 0 },
	{ CMD_BIRTH_RESET, "go back to the beginning", do_cmd_birth_reset, false, false, 0 },
	{ CMD_CHOOSE_RACE, "select race", do_cmd_choose_race, false, false, 0 },
	{ CMD_CHOOSE_CLASS, "select class", do_cmd_choose_class, false, false, 0 },
	{ CMD_BUY_STAT, "buy points in a stat", do_cmd_buy_stat, false, false, 0 },
	{ CMD_SELL_STAT, "sell points in a stat", do_cmd_sell_stat, false, false, 0 },
	{ CMD_RESET_STATS, "reset stats", do_cmd_reset_stats, false, false, 0 },
	{ CMD_REFRESH_STATS, "refresh stats", do_cmd_refresh_stats, false, false,  0 },
	{ CMD_ROLL_STATS, "roll new stats", do_cmd_roll_stats, false, false, 0 },
	{ CMD_PREV_STATS, "use previously rolled stats", do_cmd_prev_stats, false, false, 0 },
	{ CMD_NAME_CHOICE, "choose name", do_cmd_choose_name, false, false, 0 },
	{ CMD_SERVER_CHOICE, "choose server", do_cmd_choose_server, false, false, 0 },
	{ CMD_SLOTNAME_CHOICE, "choose slotname", do_cmd_choose_slotname, false, false, 0 },
	{ CMD_HISTORY_CHOICE, "write history", do_cmd_choose_history, false, false, 0 },
	{ CMD_ACCEPT_CHARACTER, "accept character", do_cmd_accept_character, false, false, 0 },

	{ CMD_GO_UP, "go up stairs", do_cmd_go_up, false, true, 0 },
	{ CMD_GO_DOWN, "go down stairs", do_cmd_go_down, false, true, 0 },
	{ CMD_WALK, "walk", do_cmd_walk, true, true, 0 },
	{ CMD_RUN, "run", do_cmd_run, true, true, 0 },
	{ CMD_EXPLORE, "explore", do_cmd_explore, false, true, 0 },
	{ CMD_NAVIGATE_UP, "navigate up", do_cmd_navigate_up, false, true, 0 },
	{ CMD_NAVIGATE_DOWN, "navigate down", do_cmd_navigate_down, false, true, 0 },
	{ CMD_JUMP, "jump", do_cmd_jump, false, true, 0 },
	{ CMD_OPEN, "open", do_cmd_open, true, true, 99 },
	{ CMD_CLOSE, "close", do_cmd_close, true, true, 99 },
	{ CMD_TUNNEL, "tunnel", do_cmd_tunnel, true, true, 99 },
	{ CMD_HOLD, "stay still", do_cmd_hold, true, true, 0 },
	{ CMD_DISARM, "disarm", do_cmd_disarm, true, true, 99 },
	{ CMD_ALTER, "alter", do_cmd_alter, true, true, 99 },
	{ CMD_STEAL, "steal", do_cmd_steal, false, true, 0 },
	{ CMD_REST, "rest", do_cmd_rest, false, true, 0 },
	{ CMD_SLEEP, "sleep", do_cmd_sleep, false, true, 0 },
	{ CMD_PATHFIND, "walk", do_cmd_pathfind, false, true, 0 },
	{ CMD_PICKUP, "pickup", do_cmd_pickup, false, true, 0 },
	{ CMD_AUTOPICKUP, "autopickup", do_cmd_autopickup, false, true, 0 },
	{ CMD_WIELD, "wear or wield", do_cmd_wield, false, true, 0 },
	{ CMD_TAKEOFF, "take off", do_cmd_takeoff, false, true, 0 },
	{ CMD_DROP, "drop", do_cmd_drop, false, true, 0 },
	{ CMD_UNINSCRIBE, "un-inscribe", do_cmd_uninscribe, false, false, 0 },
	{ CMD_AUTOINSCRIBE, "autoinscribe", do_cmd_autoinscribe, false, false, 0 },
	{ CMD_EAT, "eat", do_cmd_eat_food, false, true, 0 },
	{ CMD_QUAFF, "quaff", do_cmd_quaff_potion, false, true, 0 },
	{ CMD_USE_ROD, "zap", do_cmd_zap_rod, true, true, 99 },
	{ CMD_USE_STAFF, "use", do_cmd_use_staff, true, true, 99 },
	{ CMD_USE_WAND, "aim", do_cmd_aim_wand, true, true, 99 },
	{ CMD_READ_SCROLL, "read", do_cmd_read_scroll, false, true, 0 },
	{ CMD_ACTIVATE, "activate", do_cmd_activate, true, true, 99 },
	{ CMD_REFILL, "refuel with", do_cmd_refill, false, true, 0 },
	{ CMD_FIRE, "fire", do_cmd_fire, false, true, 0 },
	{ CMD_THROW, "throw", do_cmd_throw, false, true, 0 },
	{ CMD_INSCRIBE, "inscribe", do_cmd_inscribe, false, false, 0 },
	{ CMD_STUDY, "study", do_cmd_study, false, true, 0 },
	{ CMD_CAST, "cast", do_cmd_cast, false, true, 0 },
	{ CMD_SELL, "sell", do_cmd_sell, false, false, 0 },
	{ CMD_STASH, "stash", do_cmd_stash, false, false, 0 },
	{ CMD_BUY, "buy", do_cmd_buy, false, false, 0 },
	{ CMD_RETRIEVE, "retrieve", do_cmd_retrieve, false, false, 0 },
	{ CMD_USE, "use", do_cmd_use, true, true, 99 },
	{ CMD_RETIRE, "retire character", do_cmd_retire, false, false, 0 },
	{ CMD_HELP, "help", NULL, false, false, 0 },
	{ CMD_REPEAT, "repeat", NULL, false, false, 0 },

	{ CMD_COMMAND_MONSTER, "make a monster act", do_cmd_mon_command, false, true, 0 },

	{ CMD_SPOIL_ARTIFACT, "generate spoiler file for artifacts", do_cmd_spoil_artifact, false, false, 0 },
	{ CMD_SPOIL_MON, "generate spoiler file for monsters", do_cmd_spoil_monster, false, false, 0 },
	{ CMD_SPOIL_MON_BRIEF, "generate brief spoiler file for monsters", do_cmd_spoil_monster_brief, false, false, 0 },
	{ CMD_SPOIL_OBJ, "generate spoiler file for objects", do_cmd_spoil_obj, false, false, 0 },

	{ CMD_WIZ_ACQUIRE, "acquire objects", do_cmd_wiz_acquire, false, false, 0 },
	{ CMD_WIZ_ADVANCE, "make character powerful", do_cmd_wiz_advance, false, false, 0 },
	{ CMD_WIZ_BANISH, "banish nearby monsters", do_cmd_wiz_banish, false, false, 0 },
	{ CMD_WIZ_CHANGE_ITEM_QUANTITY, "change number of an item", do_cmd_wiz_change_item_quantity, false, false, 0 },
	{ CMD_WIZ_COLLECT_DISCONNECT_STATS, "collect statistics about disconnected levels", do_cmd_wiz_collect_disconnect_stats, false, false, 0 },
	{ CMD_WIZ_COLLECT_OBJ_MON_STATS, "collect object/monster statistics", do_cmd_wiz_collect_obj_mon_stats, false, false, 0 },
	{ CMD_WIZ_COLLECT_PIT_STATS, "collect pit statistics", do_cmd_wiz_collect_pit_stats, false, false, 0 },
	{ CMD_WIZ_CREATE_ALL_ARTIFACT, "create all artifacts", do_cmd_wiz_create_all_artifact, false, false, 0 },
	{ CMD_WIZ_CREATE_ALL_ARTIFACT_FROM_TVAL, "create all artifacts of a tval", do_cmd_wiz_create_all_artifact_from_tval, false, false, 0 },
	{ CMD_WIZ_CREATE_ALL_OBJ, "create all objects", do_cmd_wiz_create_all_obj, false, false, 0 },
	{ CMD_WIZ_CREATE_ALL_OBJ_FROM_TVAL, "create all objects of a tval", do_cmd_wiz_create_all_obj_from_tval, false, false, 0 },
	{ CMD_WIZ_CREATE_ARTIFACT, "create artifact", do_cmd_wiz_create_artifact, false, false, 0 },
	{ CMD_WIZ_CREATE_OBJ, "create object", do_cmd_wiz_create_obj, false, false, 0 },
	{ CMD_WIZ_CREATE_TRAP, "create trap", do_cmd_wiz_create_trap, false, false, 0 },
	{ CMD_WIZ_CURE_ALL, "cure everything", do_cmd_wiz_cure_all, false, false, 0 },
	{ CMD_WIZ_CURSE_ITEM, "change a curse on an item", do_cmd_wiz_curse_item, false, false, 0 },
	{ CMD_WIZ_DETECT_ALL_LOCAL, "detect everything nearby", do_cmd_wiz_detect_all_local, false, false, 0 },
	{ CMD_WIZ_DETECT_ALL_MONSTERS, "detect all monsters", do_cmd_wiz_detect_all_monsters, false, false, 0 },
	{ CMD_WIZ_DISPLAY_KEYLOG, "display keystroke log", do_cmd_wiz_display_keylog, false, false, 0 },
	{ CMD_WIZ_DUMP_LEVEL_MAP, "write map of level", do_cmd_wiz_dump_level_map, false, false, 0 },
	{ CMD_WIZ_EDIT_PLAYER_EXP, "change the player's experience", do_cmd_wiz_edit_player_exp, false, false, 0 },
	{ CMD_WIZ_EDIT_PLAYER_GOLD, "change the player's gold", do_cmd_wiz_edit_player_gold, false, false, 0 },
	{ CMD_WIZ_EDIT_PLAYER_START, "start editing the player", do_cmd_wiz_edit_player_start, false, false, 0 },
	{ CMD_WIZ_EDIT_PLAYER_STAT, "edit one of the player's stats", do_cmd_wiz_edit_player_stat, false, false, 0 },
	{ CMD_WIZ_HIT_ALL_LOS, "hit all monsters in LOS", do_cmd_wiz_hit_all_los, false, false, 0 },
	{ CMD_WIZ_INCREASE_EXP, "increase experience", do_cmd_wiz_increase_exp, false, false, 0 },
	{ CMD_WIZ_JUMP_LEVEL, "jump to a level", do_cmd_wiz_jump_level, false, true, 0 },
	{ CMD_WIZ_LEARN_OBJECT_KINDS, "learn about kinds of objects", do_cmd_wiz_learn_object_kinds, false, false, 0 },
	{ CMD_WIZ_MAGIC_MAP, "map local area", do_cmd_wiz_magic_map, false, false, 0 },
	{ CMD_WIZ_PEEK_NOISE_SCENT, "peek at noise and scent", do_cmd_wiz_peek_noise_scent, false, false, 0 },
	{ CMD_WIZ_PERFORM_EFFECT, "perform an effect", do_cmd_wiz_perform_effect, false, false, 0 },
	{ CMD_WIZ_PLAY_ITEM, "play with item", do_cmd_wiz_play_item, false, false, 0 },
	{ CMD_WIZ_PUSH_OBJECT, "push objects from square", do_cmd_wiz_push_object, false, false, 0 },
	{ CMD_WIZ_QUERY_FEATURE, "highlight specific feature", do_cmd_wiz_query_feature, false, false, 0 },
	{ CMD_WIZ_QUERY_SQUARE_FLAG, "query square flag", do_cmd_wiz_query_square_flag, false, false, 0 },
	{ CMD_WIZ_QUIT_NO_SAVE, "quit without saving", do_cmd_wiz_quit_no_save, false, false, 0 },
	{ CMD_WIZ_RECALL_MONSTER, "recall monster", do_cmd_wiz_recall_monster, false, false, 0 },
	{ CMD_WIZ_RERATE, "rerate hitpoints", do_cmd_wiz_rerate, false, false, 0 },
	{ CMD_WIZ_REROLL_ITEM, "reroll an item", do_cmd_wiz_reroll_item, false, false, 0 },
	{ CMD_WIZ_STAT_ITEM, "get statistics for an item", do_cmd_wiz_stat_item, false, false, 0 },
	{ CMD_WIZ_SUMMON_NAMED, "summon specific monster", do_cmd_wiz_summon_named, false, false, 0 },
	{ CMD_WIZ_SUMMON_RANDOM, "summon random monsters", do_cmd_wiz_summon_random, false, false, 0 },
	{ CMD_WIZ_TELEPORT_RANDOM, "teleport", do_cmd_wiz_teleport_random, false, false, 0 },
	{ CMD_WIZ_TELEPORT_TO, "teleport to location", do_cmd_wiz_teleport_to, false, false, 0 },
	{ CMD_WIZ_TWEAK_ITEM, "modify item attributes", do_cmd_wiz_tweak_item, false, false, 0 },
	{ CMD_WIZ_WIPE_RECALL, "erase monster recall", do_cmd_wiz_wipe_recall, false, false, 0 },
	{ CMD_WIZ_WIZARD_LIGHT, "wizard light the level", do_cmd_wiz_wizard_light, false, false, 0 },
};

/**
 * Make a deep copy of a command and its arguments.
 */
void cmd_copy(struct command *dest, const struct command *src)
{
	int i;

	*dest = *src;
	/* String arguments require a deep copy. */
	for (i = 0; i < CMD_MAX_ARGS; ++i) {
		if (dest->arg[i].type == arg_STRING) {
			dest->arg[i].data.string =
				string_make(src->arg[i].data.string);
		}
	}
}

/**
 * Release any resources for the command's arguments.
 */
void cmd_release(struct command *cmd)
{
	int i;

	for (i = 0; i < CMD_MAX_ARGS; ++i) {
		if (cmd->arg[i].type == arg_STRING) {
			string_free((char*)(cmd->arg[i].data.string));
			cmd->arg[i].name[0] = '\0';
			cmd->arg[i].type = arg_NONE;
		}
	}
}

const char *cmd_verb(cmd_code cmd)
{
	size_t i;
	for (i = 0; i < N_ELEMENTS(game_cmds); i++) {
		if (game_cmds[i].cmd == cmd)
			return game_cmds[i].verb;
	}
	return NULL;
}

/**
 * Return the index of the given command in the command array.
 */
static int cmd_idx(cmd_code code)
{
	size_t i;

	for (i = 0; i < N_ELEMENTS(game_cmds); i++)
		if (game_cmds[i].cmd == code)
			return i;

	return CMD_ARG_NOT_PRESENT;
}


/**
 * ------------------------------------------------------------------------
 * The command queue.
 * ------------------------------------------------------------------------ */

#define CMD_QUEUE_SIZE 20
#define prev_cmd_idx(idx) ((idx + CMD_QUEUE_SIZE - 1) % CMD_QUEUE_SIZE)

static int cmd_head = 0;
static int cmd_tail = 0;
static struct command cmd_queue[CMD_QUEUE_SIZE];

/*
 * Remember last non-background command executed for use by CMD_REPEAT; either
 * still in the queue at last_command_idx or copied out of the queue into
 * last_command
 */
int last_command_idx = -1;
static struct command last_command = {
	.context = CTX_INIT,
	.code = CMD_NULL,
	.nrepeats = 0,
	.background_command = 0,
	.arg = { { 0 } }
};

static bool repeat_prev_allowed = false;
static bool repeating = false;


struct command *cmdq_peek(void)
{
	return &cmd_queue[prev_cmd_idx(cmd_head)];
}


/**
 * Insert the given command into the command queue.
 *
 * Makes a shallow copy of the command.  If the command has string arguments,
 * the queue assumes ownership of those arguments.
 */
errr cmdq_push_copy(struct command *cmd)
{
	/* If queue full, return error */
	if (cmd_head + 1 == cmd_tail) return 1;
	if (cmd_head + 1 == CMD_QUEUE_SIZE && cmd_tail == 0) return 1;

	/* Insert command into queue. */
	if (cmd->code != CMD_REPEAT) {
		if (last_command_idx == cmd_head) {
			/*
			 * The last non-background command will be overwritten.
			 * Copy out of the queue in case it is needed for
			 * CMD_REPEAT.
			 */
			cmd_release(&last_command);
			cmd_copy(&last_command, &cmd_queue[cmd_head]);
			last_command_idx = -1;
		}
		cmd_release(&cmd_queue[cmd_head]);
		cmd_queue[cmd_head] = *cmd;
	} else if (!repeat_prev_allowed) {
		return 1;
	} else {
		/*
		 * If we're repeating a command, we duplicate the previous
		 * command in the next command "slot".
		 */
		if (last_command_idx >= 0) {
			assert(last_command_idx < CMD_QUEUE_SIZE);
			if (last_command_idx != cmd_head) {
				cmd_release(&cmd_queue[cmd_head]);
				cmd_copy(&cmd_queue[cmd_head],
					&cmd_queue[last_command_idx]);
			}
		} else if (last_command.code != CMD_NULL) {
			cmd_release(&cmd_queue[cmd_head]);
			cmd_copy(&cmd_queue[cmd_head], &last_command);
		} else {
			return 1;
		}
	}

	/* Advance point in queue, wrapping around at the end */
	cmd_head++;
	if (cmd_head == CMD_QUEUE_SIZE) cmd_head = 0;

	return 0;	
}

/**
 * Process a game command from the UI or the command queue and carry out
 * whatever actions go along with it.
 */
static void process_command(cmd_context ctx, struct command *cmd)
{
	int oldrepeats = cmd->nrepeats;
	/* Hack - command a monster */
	int idx = cmd_idx(player->timed[TMD_COMMAND] ?
		CMD_COMMAND_MONSTER : cmd->code);

	/* Reset so that when selecting items, we look in the default location */
	player->upkeep->command_wrk = 0;

	if (idx == -1) return;

	/* Command repetition */
	if (game_cmds[idx].repeat_allowed) {
		/* Auto-repeat only if there isn't already a repeat length. */
		if (game_cmds[idx].auto_repeat_n > 0 && cmd->nrepeats == 0)
			cmd_set_repeat(game_cmds[idx].auto_repeat_n);
	} else {
		cmd->nrepeats = 0;
		repeating = false;
	}

	/* The command gets to unset this if it isn't appropriate for
	 * the user to repeat it. */
	repeat_prev_allowed = true;

	cmd->context = ctx;

	/* Actually execute the command function */
	if (game_cmds[idx].fn) {
		/* Occasional attack instead for bloodlust-affected characters */
		if (cmd->background_command > 1) {
			/*
			 * Some background commands do not trigger bloodlust.
			 * If they can take energy, they also don't reset
			 * whether the player's next command skips the
			 * bloodlust check.
			 */
			if (player->skip_cmd_coercion
					&& game_cmds[idx].can_use_energy) {
				player->skip_cmd_coercion = 2;
			}
		} else if (game_cmds[idx].can_use_energy
				&& !player->skip_cmd_coercion) {
			if (randint0(200) < player->timed[TMD_BLOODLUST]) {
				if (player_attack_random_monster(player)) return;
			} else if (player->timed[TMD_BLOODLUST]) {
				/*
				 * In case this command is canceled by the
				 * user, tentatively mark the player as
				 * immune from bloodlust check on the next
				 * command that can take energy and is not
				 * a background command.  Will update that
				 * tentative determination in
				 * process_player_cleanup().
				 */
				player->skip_cmd_coercion = 1;
			}
		}
		game_cmds[idx].fn(cmd);
	}

	/* If the command hasn't changed nrepeats, count this execution. */
	if (cmd->nrepeats > 0 && oldrepeats == cmd_get_nrepeats())
		cmd_set_repeat(oldrepeats - 1);
}

/**
 * Get the next game command from the queue and process it.
 */
bool cmdq_pop(cmd_context c)
{
	struct command *cmd;

	/* If we're repeating, just pull the last command again. */
	if (repeating) {
		cmd = &cmd_queue[prev_cmd_idx(cmd_tail)];
	} else if (cmd_head != cmd_tail) {
		/* If we have a command ready, set it. */
		cmd = &cmd_queue[cmd_tail++];
		if (cmd_tail == CMD_QUEUE_SIZE)
			cmd_tail = 0;
	} else {
		/* Failure to get a command. */
		return false;
	}

	/* Now process it */
	if (!cmd->background_command) {
		last_command_idx = prev_cmd_idx(cmd_tail);
	}
	process_command(c, cmd);
	return true;
}

/**
 * Inserts a command in the queue to be carried out, with the given
 * number of repeats.
 */
errr cmdq_push_repeat(cmd_code c, int nrepeats)
{
	struct command cmd = {
		.context = CTX_INIT,
		.code = CMD_NULL,
		.nrepeats = 0,
		.background_command = 0,
		.arg = { { 0 } }
	};

	if (cmd_idx(c) == -1)
		return 1;

	cmd.code = c;
	cmd.nrepeats = nrepeats;

	return cmdq_push_copy(&cmd);
}

/**
 * Inserts a command in the queue to be carried out. 
 */
errr cmdq_push(cmd_code c)
{
	return cmdq_push_repeat(c, 0);
}


/**
 * Shorthand to execute all commands in the queue right now, no waiting
 * for input.
 */
void cmdq_execute(cmd_context ctx)
{
	while (cmdq_pop(ctx)) ;
}

/**
 * Remove all commands from the queue.
 */
void cmdq_flush(void)
{
	cmd_tail = cmd_head;
}

/**
 * Remove all commands from the queue and release any allocated resources.
 */
void cmdq_release(void)
{
	int i;

	cmdq_flush();
	for (i = 0; i < CMD_QUEUE_SIZE; ++i) {
		cmd_release(cmd_queue + i);
	}
	cmd_release(&last_command);
	last_command.code = CMD_NULL;
	last_command_idx = -1;
}

/**
 * ------------------------------------------------------------------------
 * Handling of repeated commands
 * ------------------------------------------------------------------------ */

/**
 * Remove any pending repeats from the current command.
 */
void cmd_cancel_repeat(void)
{
	struct command *cmd = &cmd_queue[prev_cmd_idx(cmd_tail)];

	if (cmd->nrepeats || repeating) {
		/* Cancel */
		cmd->nrepeats = 0;
		repeating = false;

		/* Redraw the state (later) */
		player->upkeep->redraw |= (PR_STATE);
	}
}

/**
 * Update the number of repeats pending for the current command.
 */
void cmd_set_repeat(int nrepeats)
{
	struct command *cmd = &cmd_queue[prev_cmd_idx(cmd_tail)];

	cmd->nrepeats = nrepeats;
	if (nrepeats) repeating = true;
	else repeating = false;

	/* Redraw the state (later) */
	player->upkeep->redraw |= (PR_STATE);
}

/**
 * Return the number of repeats pending for the current command.
 */
int cmd_get_nrepeats(void)
{
	struct command *cmd = &cmd_queue[prev_cmd_idx(cmd_tail)];
	return cmd->nrepeats;
}

/**
 * Do not allow the current command to be repeated by the user using the
 * "repeat last command" command.
 */
void cmd_disable_repeat(void)
{
	repeat_prev_allowed = false;
}

/**
 * Do not allow the current command to be repeated by the user using the
 * "repeat last command" command if that command used an item from the floor.
 */
void cmd_disable_repeat_floor_item(void)
{
	int cmd_prev;

	/*
	 * Repeat already disallowed so skip further checks (avoids access
	 * to dangling object references in the command structures).
	 */
	if (!repeat_prev_allowed) return;

	cmd_prev = cmd_head - 1;
	if (cmd_prev < 0) cmd_prev = CMD_QUEUE_SIZE - 1;
	if (cmd_queue[cmd_prev].code != CMD_NULL) {
		struct command *cmd = &cmd_queue[cmd_prev];
		int i = 0;

		while (1) {
			if (i >= CMD_MAX_ARGS) {
				break;
			}
			if (cmd->arg[i].type == arg_ITEM
					&& cmd->arg[i].data.obj
					&& (cmd->arg[i].data.obj->grid.x != 0
					|| cmd->arg[i].data.obj->grid.y != 0)) {
				repeat_prev_allowed = false;
				break;
			}
			++i;
		}
	}
}

/**
 * ------------------------------------------------------------------------
 * Argument setting/getting generics
 * ------------------------------------------------------------------------ */

/**
 * Set an argument of name 'arg' to data 'data'
 */
static void cmd_set_arg(struct command *cmd, const char *name,
						enum cmd_arg_type type, union cmd_arg_data data)
{
	size_t i;

	int first_empty = -1;
	int idx = -1;

	assert(name);
	assert(name[0]);

	/* Find an arg that either... */
	for (i = 0; i < CMD_MAX_ARGS; i++) {
		struct cmd_arg *arg = &cmd->arg[i];
		if (!arg->name[0] && first_empty == -1)
			first_empty = i;

		if (streq(arg->name, name)) {
			idx = i;
			break;
		}
	}

	assert(first_empty != -1 || idx != -1);

	if (idx == -1) {
		idx = first_empty;
	} else {
		/* Free allocated string if overwriting a string argument. */
		if (cmd->arg[idx].type == arg_STRING) {
			string_free((char*)(cmd->arg[idx].data.string));
		}
	}

	cmd->arg[idx].type = type;
	cmd->arg[idx].data = data;
	my_strcpy(cmd->arg[idx].name, name, sizeof cmd->arg[0].name);
}

/**
 * Get an argument with name 'arg'
 */
static int cmd_get_arg(struct command *cmd, const char *arg,
					   enum cmd_arg_type type, union cmd_arg_data *data)
{
	size_t i;

	for (i = 0; i < CMD_MAX_ARGS; i++) {
		if (streq(cmd->arg[i].name, arg)) {
			if (cmd->arg[i].type != type)
				return CMD_ARG_WRONG_TYPE;

			*data = cmd->arg[i].data;
			return CMD_OK;
		}
	}

	return CMD_ARG_NOT_PRESENT;
 }

 

/**
 * ------------------------------------------------------------------------
 * 'Choice' type
 * ------------------------------------------------------------------------ */

/**
 * XXX This type is a hack. The only places that use this are:
 * - resting
 * - birth choices
 * - store items
 * - spells
 * - selecting an effect for an item that activates for an EF_SELECT effect
 *   (dragon's breath wands or potions, dragon armor that has multiple breath
 *   types)
 * - several debugging commands for integer or boolean arguments that did not
 *   seem to be a good match for 'number' arguments
 *
 * Each of these should have its own type, which will allow for proper
 * validity checking of data.
 */

/**
 * Set arg 'n' to 'choice'
 */
void cmd_set_arg_choice(struct command *cmd, const char *arg, int choice)
{
	union cmd_arg_data data;
	data.choice = choice;
	cmd_set_arg(cmd, arg, arg_CHOICE, data);
}

/**
 * Retrive an argument 'n' if it's a choice
 */
int cmd_get_arg_choice(struct command *cmd, const char *arg, int *choice)
{
	union cmd_arg_data data;
	int err;

	if ((err = cmd_get_arg(cmd, arg, arg_CHOICE, &data)) == CMD_OK)
		*choice = data.choice;

	return err;
}


/**
 * Get a spell from the user, trying the command first but then prompting
 *
 * \param cmd is the command to use.
 * \param arg is the name of the command's argument that stores the spell's
 * index.
 * \param p is the player.
 * \param spell is dereferenced and set to the index of the spell selected.
 * \param verb is the string describing the action for which the spell is
 * requested.  It is typically "cast" or "study".
 * \param book_filter is the function (if any) to test that an object is
 * appropriate for use as a spellbook by the player.
 * \param book_error is the message to display if no valid book is available.
 * If NULL, no message will be displayed.
 * \param spell_filter is the function to call to test if a spell is a valid
 * selection for the request.
 * \param spell_error is the message to display if no valid spell is available.
 * If NULL, no message will be displayed.
 * \return CMD_OK if a valid spell index was assigned to *spell or
 * CMD_ARG_ABORTED if no valid spell index was assigned to *spell.
 */
int cmd_get_spell(struct command *cmd, const char *arg, struct player *p,
		int *spell,
		const char *verb, item_tester book_filter,
		const char *book_error,
		bool (*spell_filter)(const struct player *p, int spell),
		const char *spell_error)
{
	struct object *book;

	/* See if we've been provided with this one */
	if (cmd_get_arg_choice(cmd, arg, spell) == CMD_OK) {
		/* Ensure it passes the filter */
		if (!spell_filter || spell_filter(p, *spell) == true)
			return CMD_OK;
	}

	/* See if we've been given a book to look at */
	if (cmd_get_arg_item(cmd, "book", &book) == CMD_OK) {
		*spell = get_spell_from_book(p, verb, book, spell_error,
			spell_filter);
	} else {
		*spell = get_spell(p, verb, book_filter, cmd->code, book_error,
			spell_filter, spell_error, &book);
	}

	if (*spell >= 0) {
		cmd_set_arg_item(cmd, "book", book);
		cmd_set_arg_choice(cmd, arg, *spell);
		return CMD_OK;
	}

	return CMD_ARG_ABORTED;
}

/**
 * Choose an effect from a list, first try the command but then prompt
 * \param cmd is the command to use.
 * \param arg is the name of the argument to consult in the command
 * \param choice When the return value is CMD_OK, *choice will be set to the
 * index in the list for the selected effect or to -2 if the user selected the
 * random option enabled by allow_random.
 * \param prompt Is the text for the prompt displayed when querying the user.
 * May be NULL to use a default prompt.
 * \param effect points to the first effect in the linked list of effects.
 * \param count is the number of effects from which to choose.  If count is -1,
 * all the effects in the list will be used.
 * \param allow_random when true, present the user an additional option which
 * will choose one of the effects at random; when false, only present the
 * options that correspond to the effects in the list.
 * \return CMD_OK if *choice was updated with a valid selection; otherwise
 * return CMD_ARG_ABORTED.
 */
int cmd_get_effect_from_list(struct command *cmd, const char *arg, int *choice,
	const char *prompt, struct effect *effect, int count,
	bool allow_random)
{
	int selection;

	if (count == -1) {
		struct effect *cursor = effect;

		count = 0;
		while (cursor) {
			++count;
			cursor = effect_next(cursor);
		}
	}

	if (cmd_get_arg_choice(cmd, arg, &selection) != CMD_OK ||
			((selection != -2 || !allow_random) &&
			(selection < 0 || selection >= count))) {
		/* It isn't in the command or is invalid; prompt. */
		selection = get_effect_from_list(prompt, effect, count,
			allow_random);
	}
	if ((selection == -2 && allow_random) ||
			(selection >= 0 && selection < count)) {
		/* Record the selection in the command. */
		cmd_set_arg_choice(cmd, arg, selection);
		*choice = selection;
		return CMD_OK;
	}
	return CMD_ARG_ABORTED;
}

/**
 * ------------------------------------------------------------------------
 * Strings
 * ------------------------------------------------------------------------ */

/**
 * Set arg 'n' to given string
 */
void cmd_set_arg_string(struct command *cmd, const char *arg, const char *str)
{
	union cmd_arg_data data;
	data.string = string_make(str);
	cmd_set_arg(cmd, arg, arg_STRING, data);
}

/**
 * Retrieve arg 'n' if a string
 *
 * Note that the command itself retains ownership of the pointer returned in
 * *str:  the caller should not free that pointer.
 */
int cmd_get_arg_string(struct command *cmd, const char *arg, const char **str)
{
	union cmd_arg_data data;
	int err;

	if ((err = cmd_get_arg(cmd, arg, arg_STRING, &data)) == CMD_OK)
		*str = data.string;

	return err;
}

/**
 * Get a string, first from the command or failing that prompt the user
 *
 * Note that the command itself retains ownership of the pointer returned in
 * *str:  the caller should not free that pointer.
 */
int cmd_get_string(struct command *cmd, const char *arg, const char **str,
				   const char *initial, const char *title, const char *prompt)
{
	char tmp[80] = "";

	if (cmd_get_arg_string(cmd, arg, str) == CMD_OK)
		return CMD_OK;

	/* Introduce */
	msg("%s", title);
	event_signal(EVENT_MESSAGE_FLUSH);

	/* Prompt properly */
	if (initial)
		my_strcpy(tmp, initial, sizeof tmp);

	if (get_string(prompt, tmp, sizeof tmp)) {
		cmd_set_arg_string(cmd, arg, tmp);
		if (cmd_get_arg_string(cmd, arg, str) == CMD_OK)
			return CMD_OK;
	}

	return CMD_ARG_ABORTED;
}

/**
 * ------------------------------------------------------------------------
 * Directions
 * ------------------------------------------------------------------------ */

/**
 * Set arg 'n' to given direction
 */
void cmd_set_arg_direction(struct command *cmd, const char *arg, int dir)
{
	union cmd_arg_data data;
	data.direction = dir;
	cmd_set_arg(cmd, arg, arg_DIRECTION, data);
}

/**
 * Retrieve arg 'n' if a direction
 */
int cmd_get_arg_direction(struct command *cmd, const char *arg, int *dir)
{
	union cmd_arg_data data;
	int err;

	if ((err = cmd_get_arg(cmd, arg, arg_DIRECTION, &data)) == CMD_OK)
		*dir = data.direction;

	return err;
}

/**
 * Get a direction, first from command or prompt otherwise
 */
int cmd_get_direction(struct command *cmd, const char *arg, int *dir,
					  bool allow_5)
{
	if (cmd_get_arg_direction(cmd, arg, dir) == CMD_OK) {
		/* Validity check */
		if (*dir != DIR_NONE)
			return CMD_OK;
	}

	/* We need to do extra work */
	if (get_rep_dir(dir, allow_5)) {
		cmd_set_arg_direction(cmd, arg, *dir);
		return CMD_OK;
	}

	cmd_cancel_repeat();
	return CMD_ARG_ABORTED;
}

/**
 * ------------------------------------------------------------------------
 * Targets
 * ------------------------------------------------------------------------ */

/**
 * XXX Should this be unified with the arg_DIRECTION type?
 *
 * XXX Should we abolish DIR_TARGET and instead pass a struct target which
 * contains all relevant info?
 */

/**
 * Set arg 'n' to target
 */
void cmd_set_arg_target(struct command *cmd, const char *arg, int target)
{
	union cmd_arg_data data;
	data.direction = target;
	cmd_set_arg(cmd, arg, arg_TARGET, data);
}

/**
 * Retrieve arg 'n' if it's a target
 */
int cmd_get_arg_target(struct command *cmd, const char *arg, int *target)
{
	union cmd_arg_data data;
	int err;

	if ((err = cmd_get_arg(cmd, arg, arg_TARGET, &data)) == CMD_OK)
		*target = data.direction;

	return err;
}

/**
 * Get a target, first from command or prompt otherwise
 */
int cmd_get_target(struct command *cmd, const char *arg, int *target)
{
	if (cmd_get_arg_target(cmd, arg, target) == CMD_OK) {
		if (*target != DIR_UNKNOWN &&
				(*target != DIR_TARGET || target_okay()))
			return CMD_OK;
	}

	if (get_aim_dir(target)) {
		cmd_set_arg_target(cmd, arg, *target);
		return CMD_OK;
	}

	return CMD_ARG_ABORTED;
}

/**
 * ------------------------------------------------------------------------
 * Points
 * ------------------------------------------------------------------------ */

/**
 * Set argument 'n' to point grid
 */
void cmd_set_arg_point(struct command *cmd, const char *arg, struct loc grid)
{
	union cmd_arg_data data;
	data.point = grid;
	cmd_set_arg(cmd, arg, arg_POINT, data);
}

/**
 * Retrieve argument 'n' if it's a point
 */
int cmd_get_arg_point(struct command *cmd, const char *arg, struct loc *grid)
{
	union cmd_arg_data data;
	int err;

	if ((err = cmd_get_arg(cmd, arg, arg_POINT, &data)) == CMD_OK) {
		*grid = data.point;
	}

	return err;
}

/**
 * ------------------------------------------------------------------------
 * Item arguments
 * ------------------------------------------------------------------------ */

/**
 * Set argument 'n' to 'item'
 */
void cmd_set_arg_item(struct command *cmd, const char *arg, struct object *obj)
{
	union cmd_arg_data data;
	data.obj = obj;
	cmd_set_arg(cmd, arg, arg_ITEM, data);
}

/**
 * Retrieve argument 'n' as an item
 */
int cmd_get_arg_item(struct command *cmd, const char *arg, struct object **obj)
{
	union cmd_arg_data data;
	int err;

	if ((err = cmd_get_arg(cmd, arg, arg_ITEM, &data)) == CMD_OK)
		*obj = data.obj;

	return err;
}

/**
 * Get an item, first from the command or try the UI otherwise
 */
int cmd_get_item(struct command *cmd, const char *arg, struct object **obj,
				 const char *prompt, const char *reject, item_tester filter,
				 int mode)
{
	if ((cmd_get_arg_item(cmd, arg, obj) == CMD_OK) && (!filter|| filter(*obj)))
		return CMD_OK;

	/* Shapechanged players can only access the floor */
	if (player_is_shapechanged(player)) {
		mode &= ~(USE_EQUIP | USE_INVEN | USE_QUIVER);
	}

	if (get_item(obj, prompt, reject, cmd->code, filter, mode)) {
		cmd_set_arg_item(cmd, arg, *obj);
		return CMD_OK;
	}

	return CMD_ARG_ABORTED;
}

/**
 * ------------------------------------------------------------------------
 * Numbers, quantities
 * ------------------------------------------------------------------------ */

/**
 * Set argument 'n' to 'number'
 */
void cmd_set_arg_number(struct command *cmd, const char *arg, int amt)
{
	union cmd_arg_data data;
	data.number = amt;
	cmd_set_arg(cmd, arg, arg_NUMBER, data);
}

/**
 * Get argument 'n' as a number
 */
int cmd_get_arg_number(struct command *cmd, const char *arg, int *amt)
{
	union cmd_arg_data data;
	int err;

	if ((err = cmd_get_arg(cmd, arg, arg_NUMBER, &data)) == CMD_OK)
		*amt = data.number;

	return err;
}

/**
 * Get argument 'n' as a number; failing that, prompt for input
 */
int cmd_get_quantity(struct command *cmd, const char *arg, int *amt, int max)
{
	if (cmd_get_arg_number(cmd, arg, amt) == CMD_OK)
		return CMD_OK;

	*amt = get_quantity(NULL, max);
	if (*amt > 0) {
		cmd_set_arg_number(cmd, arg, *amt);
		return CMD_OK;
	}

	return CMD_ARG_ABORTED;
}
