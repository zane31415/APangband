/**
 * \file cmd-obj.c
 * \brief Handle objects in various ways
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
 * b) the "Angband licence":
 *    This software may be copied and distributed for educational, research,
 *    and not for profit purposes provided that this copyright and statement
 *    are included in all such copies.  Other copyrights may also apply.
 */

#include "angband.h"
#include "cave.h"
#include "cmd-core.h"
#include "cmds.h"
#include "effects.h"
#include "game-input.h"
#include "init.h"
#include "obj-desc.h"
#include "obj-gear.h"
#include "obj-ignore.h"
#include "obj-info.h"
#include "obj-knowledge.h"
#include "obj-make.h"
#include "obj-pile.h"
#include "obj-tval.h"
#include "obj-util.h"
#include "player-attack.h"
#include "player-calcs.h"
#include "player-spell.h"
#include "player-timed.h"
#include "player-util.h"
#include "target.h"
#include "trap.h"

/**
 * ------------------------------------------------------------------------
 * Utility bits and bobs
 * ------------------------------------------------------------------------
 */

/**
 * Check to see if the player can use a rod/wand/staff/activatable object.
 *
 * \return a positive value if the given object can be used; return zero if
 * the object cannot be used but might succeed on repetition (i.e. device's
 * failure check did not pass but the failure rate is less than 100%); return
 * a negative value if the object cannot be used and repetition won't help
 * (no charges, requires recharge, or failure rate is 100% or more).
 */
static int check_devices(struct object *obj)
{
	int fail;
	const char *action;
	const char *what = NULL;
	bool activated = false;

	/* Get the right string */
	if (tval_is_rod(obj)) {
		action = "zap the rod";
	} else if (tval_is_wand(obj)) {
		action = "use the wand";
		what = "wand";
	} else if (tval_is_staff(obj)) {
		action = "use the staff";
		what = "staff";
	} else {
		action = "activate it";
		activated = true;
	}

	/* Notice empty staffs */
	if (what && obj->pval <= 0) {
		event_signal(EVENT_INPUT_FLUSH);
		msg("The %s has no charges left.", what);
		return -1;
	}

	/* Figure out how hard the item is to use */
	fail = get_use_device_chance(obj);

	/* Roll for usage */
	if (randint1(1000) < fail) {
		event_signal(EVENT_INPUT_FLUSH);
		msg("You failed to %s properly.", action);
		return (fail < 1001) ? 0 : -1;
	}

	/* Notice activations */
	if (activated) {
		if (obj->effect)
			obj->known->effect = obj->effect;
		else if (obj->activation)
			obj->known->activation = obj->activation;
	}

	return 1;
}


/**
 * Return the chance of an effect beaming, given a tval.
 */
static int beam_chance(int tval)
{
	switch (tval)
	{
		case TV_WAND: return 20;
		case TV_ROD:  return 10;
	}

	return 0;
}


/**
 * Print an artifact activation message.
 */
static void activation_message(struct object *obj, const struct player *p)
{
	const char *message;

	/* See if we have a message, then print it */
	if (!obj->activation) return;
	if (!obj->activation->message) return;
	if (obj->artifact && obj->artifact->alt_msg) {
		message = obj->artifact->alt_msg;
	} else {
		message = obj->activation->message;
	}
	print_custom_message(obj, message, MSG_GENERIC, p);
}



/**
 * ------------------------------------------------------------------------
 * Inscriptions
 * ------------------------------------------------------------------------
 */

/**
 * Remove inscription
 */
void do_cmd_uninscribe(struct command *cmd)
{
	struct object *obj;

	if (!player_get_resume_normal_shape(player, cmd)) {
		return;
	}

	/* Get arguments */
	if (cmd_get_item(cmd, "item", &obj,
			/* Prompt */ "Uninscribe which item?",
			/* Error  */ "You have nothing you can uninscribe.",
			/* Filter */ obj_has_inscrip,
			/* Choice */ USE_EQUIP | USE_INVEN | USE_QUIVER | USE_FLOOR) != CMD_OK)
		return;

	obj->note = 0;
	msg("Inscription removed.");

	player->upkeep->notice |= (PN_COMBINE | PN_IGNORE);
	player->upkeep->redraw |= (PR_INVEN | PR_EQUIP);
}

/**
 * Add inscription
 */
void do_cmd_inscribe(struct command *cmd)
{
	struct object *obj;
	const char *str;

	char prompt[1024];
	char o_name[80];

	if (!player_get_resume_normal_shape(player, cmd)) {
		return;
	}

	/* Get arguments */
	if (cmd_get_item(cmd, "item", &obj,
			/* Prompt */ "Inscribe which item?",
			/* Error  */ "You have nothing to inscribe.",
			/* Filter */ NULL,
			/* Choice */ USE_EQUIP | USE_INVEN | USE_QUIVER | USE_FLOOR | IS_HARMLESS) != CMD_OK)
		return;

	/* Form prompt */
	object_desc(o_name, sizeof(o_name), obj, ODESC_PREFIX | ODESC_FULL,
		player);
	strnfmt(prompt, sizeof prompt, "Inscribing %s.", o_name);

	if (cmd_get_string(cmd, "inscription", &str,
			quark_str(obj->note) /* Default */,
			prompt, "Inscribe with what? ") != CMD_OK)
		return;

	obj->note = quark_add(str);

	player->upkeep->notice |= (PN_COMBINE | PN_IGNORE);
	player->upkeep->redraw |= (PR_INVEN | PR_EQUIP);
}


/**
 * Autoinscribe all appropriate objects
 */
void do_cmd_autoinscribe(struct command *cmd)
{
	if (player_is_shapechanged(player)) return;

	autoinscribe_ground(player);
	autoinscribe_pack(player);

	player->upkeep->redraw |= (PR_INVEN | PR_EQUIP);
}


/**
 * ------------------------------------------------------------------------
 * Taking off/putting on
 * ------------------------------------------------------------------------
 */

/**
 * Take off an item
 */
void do_cmd_takeoff(struct command *cmd)
{
	struct object *obj;

	if (!player_get_resume_normal_shape(player, cmd)) {
		return;
	}

	/* Get arguments */
	if (cmd_get_item(cmd, "item", &obj,
			/* Prompt */ "Take off or unwield which item?",
			/* Error  */ "You have nothing to take off or unwield.",
			/* Filter */ obj_can_takeoff,
			/* Choice */ USE_EQUIP) != CMD_OK)
		return;

	inven_takeoff(obj);
	combine_pack(player);
	pack_overflow(obj);
	player->upkeep->energy_use = z_info->move_energy / 2;
}


/**
 * Wield or wear an item
 */
void do_cmd_wield(struct command *cmd)
{
	struct object *equip_obj;
	char o_name[80];
	const char *act;

	unsigned n;

	int slot;
	struct object *obj;

	if (!player_get_resume_normal_shape(player, cmd)) {
		return;
	}

	/* Get arguments */
	if (cmd_get_item(cmd, "item", &obj,
			/* Prompt */ "Wear or wield which item?",
			/* Error  */ "You have nothing to wear or wield.",
			/* Filter */ obj_can_wear,
			/* Choice */ USE_INVEN | USE_FLOOR | USE_QUIVER) != CMD_OK)
		return;

	/* Get the slot the object wants to go in, and the item currently there */
	slot = wield_slot(obj);
	equip_obj = slot_object(player, slot);

	/* If the slot is open, wield and be done */
	if (!equip_obj) {
		inven_wield(obj, slot);
		return;
	}

	/* Usually if the slot is taken we'll just replace the item in the slot,
	 * but for rings we need to ask the user which slot they actually
	 * want to replace */
	if (tval_is_ring(obj)) {
		if (cmd_get_item(cmd, "replace", &equip_obj,
						 /* Prompt */ "Replace which ring? ",
						 /* Error  */ "Error in do_cmd_wield(), please report.",
						 /* Filter */ tval_is_ring,
						 /* Choice */ USE_EQUIP) != CMD_OK)
			return;

		/* Change slot if necessary */
		slot = equipped_item_slot(player->body, equip_obj);
	}

	/* Prevent wielding into a stickied slot */
	if (!obj_can_takeoff(equip_obj)) {
		object_desc(o_name, sizeof(o_name), equip_obj, ODESC_BASE,
			player);
		msg("You cannot remove the %s you are %s.", o_name,
			equip_describe(player, slot));
		return;
	}

	/* "!t" checks for taking off */
	n = check_for_inscrip(equip_obj, "!t");
	while (n--) {
		/* Prompt */
		object_desc(o_name, sizeof(o_name), equip_obj,
			ODESC_PREFIX | ODESC_FULL, player);
		
		/* Forget it */
		if (!get_check(format("Really take off %s? ", o_name))) return;
	}

	/* Describe the object */
	object_desc(o_name, sizeof(o_name), equip_obj,
		ODESC_PREFIX | ODESC_FULL, player);

	/* Took off weapon */
	if (slot_type_is(player, slot, EQUIP_WEAPON))
		act = "You were wielding";
	/* Took off bow */
	else if (slot_type_is(player, slot, EQUIP_BOW))
		act = "You were holding";
	/* Took off light */
	else if (slot_type_is(player, slot, EQUIP_LIGHT))
		act = "You were holding";
	/* Took off something else */
	else
		act = "You were wearing";

	inven_wield(obj, slot);

	/* Message */
	msgt(MSG_WIELD, "%s %s (%c).", act, o_name,
		gear_to_label(player, equip_obj));
}

/**
 * Drop an item
 */
void do_cmd_drop(struct command *cmd)
{
	int amt;
	struct object *obj;

	if (!player_get_resume_normal_shape(player, cmd)) {
		return;
	}

	/* Get arguments */
	if (cmd_get_item(cmd, "item", &obj,
			/* Prompt */ "Drop which item?",
			/* Error  */ "You have nothing to drop.",
			/* Filter */ NULL,
			/* Choice */ USE_EQUIP | USE_INVEN | USE_QUIVER) != CMD_OK)
		return;

	/* Cannot remove stickied items */
	if (object_is_equipped(player->body, obj) && !obj_can_takeoff(obj)) {
		msg("Hmmm, it seems to be stuck.");
		return;
	}

	if (cmd_get_quantity(cmd, "quantity", &amt, obj->number) != CMD_OK)
		return;

	inven_drop(obj, amt);
	player->upkeep->energy_use = z_info->move_energy / 2;
}

/**
 * ------------------------------------------------------------------------
 * Using items the traditional way
 * ------------------------------------------------------------------------
 */

enum use {
	USE_TIMEOUT,
	USE_CHARGE,
	USE_SINGLE
};

/**
 * Use an object the right way.
 *
 * Returns true if repeated commands may continue.
 */
static bool use_aux(struct command *cmd, struct object *obj, enum use use,
					int snd)
{
	struct effect *effect = object_effect(obj);
	bool from_floor = !object_is_carried(player, obj);
	int can_use = 1;
	bool was_aware;
	bool known_aim = false;
	bool none_left = false;
	int dir = 5;
	struct trap_kind *rune = lookup_trap("glyph of warding");

	/* Get arguments */
	if (cmd_get_arg_item(cmd, "item", &obj) != CMD_OK) assert(0);

	was_aware = object_flavor_is_aware(obj);

	/* Determine whether we know an item needs to be be aimed */
	if (tval_is_wand(obj) || tval_is_rod(obj) || was_aware ||
		(obj->effect && (obj->known->effect == obj->effect)) ||
		(obj->activation && (obj->known->activation == obj->activation))) {
		known_aim = true;
	}

	if (obj_needs_aim(obj)) {
		/* Unknown things with no obvious aim get a random direction */
		if (!known_aim) {
			dir = ddd[randint0(8)];
		} else if (cmd_get_target(cmd, "target", &dir) != CMD_OK) {
			return false;
		}

		/* Confusion wrecks aim */
		player_confuse_dir(player, &dir, false);
	}

	/* track the object used */
	track_object(player->upkeep, obj);

	/* Verify effect */
	assert(effect);

	/* Check for use if necessary */
	if ((use == USE_CHARGE) || (use == USE_TIMEOUT)) {
		can_use = check_devices(obj);
	}

	/* Execute the effect */
	if (can_use > 0) {
		int beam = beam_chance(obj->tval);
		int boost, level, charges = 0;
		uint16_t number;
		bool ident = false, describe = false, deduct_before, used;
		struct object *work_obj;
		struct object *first_remainder = NULL;
		char label = '\0';

		if (from_floor) {
			number = obj->number;
		} else {
			label = gear_to_label(player, obj);
			/*
			 * Show an aggregate total if the description doesn't
			 * have a charge/recharging notice specific to the
			 * stack.
			 */
			if (use != USE_CHARGE && use != USE_TIMEOUT) {
				number = object_pack_total(player, obj, false,
					&first_remainder);
				if (first_remainder && first_remainder->number
						== number) {
					first_remainder = NULL;
				}
			} else {
				number = obj->number;
			}
		}

		/* Get the level */
		if (obj->artifact)
			level = obj->artifact->level;
		else if (obj->activation)
			level = obj->activation->level;
		else
			level = obj->kind->level;

		/* Sound and/or message */
		if (obj->activation) {
			msgt(snd, "You activate it.");
			activation_message(obj, player);
		} else if (obj->kind->effect_msg) {
			msgt(snd, "%s", obj->kind->effect_msg);
		} else if (obj->kind->vis_msg && !player->timed[TMD_BLIND]) {
			msgt(snd, "%s", obj->kind->vis_msg);
		} else {
			/* Make a noise! */
			sound(snd);
		}

		/* Boost damage effects if skill > difficulty */
		boost = MAX((player->state.skills[SKILL_DEVICE] - level) / 2, 0);

		/*
		 * If the object is on the floor, tentatively deduct the
		 * amount used - the effect could leave the object inaccessible
		 * making it difficult to do after a successful use.  For the
		 * same reason, get a copy of the object to use for propagating
		 * knowledge and messaging (also do so for items in the pack
		 * to keep later logic simpler).  Don't do the deduction for
		 * an object in the pack because the rearrangement of the
		 * pack, if using a stack of one single use item, can distract
		 * the player, see
		 * https://github.com/angband/angband/issues/5543 .
		 * If effects change so that the originating object can be
		 * destroyed even if in the pack, the deduction would have to
		 * be done here if the item is in the pack as well.
		 */
		if (from_floor) {
			if (use == USE_SINGLE) {
				deduct_before = true;
				work_obj = floor_object_for_use(player, obj, 1,
					false, &none_left);
			} else {
				if (use == USE_CHARGE) {
					deduct_before = true;
					charges = obj->pval;
					/* Use a single charge */
					obj->pval--;
				} else if (use == USE_TIMEOUT) {
					deduct_before = true;
					charges = obj->timeout;
					obj->timeout += randcalc(obj->time, 0,
						RANDOMISE);
				} else {
					deduct_before = false;
				}
				work_obj = object_new();
				object_copy(work_obj, obj);
				work_obj->oidx = 0;
				if (obj->known) {
					work_obj->known = object_new();
					object_copy(work_obj->known,
						obj->known);
					work_obj->known->oidx = 0;
				}
			}
		} else {
			deduct_before = false;
			work_obj = object_new();
			object_copy(work_obj, obj);
			work_obj->oidx = 0;
			if (obj->known) {
				work_obj->known = object_new();
				object_copy(work_obj->known, obj->known);
				work_obj->known->oidx = 0;
			}
		}

		/* Do effect; use original not copy (proj. effect handling) */
		target_fix();
		used = effect_do(effect,
							source_player(),
							obj,
							&ident,
							was_aware,
							dir,
							beam,
							boost,
							cmd);
		target_release();

		if (!used) {
			if (deduct_before) {
				/* Restore the tentative deduction. */
				if (use == USE_SINGLE) {
					/*
					 * Drop/stash copy to simplify
					 * subsequent logic.
					 */
					struct object *wcopy = object_new();

					object_copy(wcopy, work_obj);
					if (work_obj->known) {
						wcopy->known = object_new();
						object_copy(wcopy->known,
							work_obj->known);
					}
					if (from_floor) {
						drop_near(cave, &wcopy, 0,
							player->grid, false,
							true);
					} else {
						inven_carry(player, wcopy,
							true, false);
					}
				} else if (use == USE_CHARGE) {
					obj->pval = charges;
				} else if (use == USE_TIMEOUT) {
					obj->timeout = charges;
				}
			}

			/*
			 * Quit if the item wasn't used and no knowledge was
			 * gained
			 */
			if (was_aware || !ident) {
				if (work_obj->known) {
					object_delete(player->cave, NULL, &work_obj->known);
				}
				object_delete(cave, player->cave, &work_obj);
				/*
				 * Selection of effect's target may have
				 * triggered an update to windows while the
				 * tentative deduction was in effect; signal
				 * another update to remedy that.
				 */
				if (deduct_before) {
					assert(from_floor);
					player->upkeep->redraw |= (PR_OBJECT);
				}
				return false;
			}
		}

		/* Increase knowledge */
		if (use == USE_SINGLE) {
			/* Single use items are automatically learned */
			if (!was_aware) {
				object_learn_on_use(player, work_obj);
			}
			describe = true;
		} else {
			/* Wearables may need update, other things become known or tried */
			if (tval_is_wearable(work_obj)) {
				update_player_object_knowledge(player);
			} else if (!was_aware && ident) {
				object_learn_on_use(player, work_obj);
				describe = true;
			} else {
				object_flavor_tried(work_obj);
			}
		}

		/*
		 * Use up, deduct charge, or apply timeout if it wasn't
		 * done before.  For charges or timeouts, also have to change
		 * work_obj since it is used for messaging (for single use
		 * items, ODESC_ALTNUM means that the work_obj's number doesn't
		 * need to be adjusted).
		 */
		if (used && !deduct_before) {
			assert(!from_floor);
			if (use == USE_CHARGE) {
				obj->pval--;
				work_obj->pval--;
			} else if (use == USE_TIMEOUT) {
				int adj = randcalc(obj->time, 0, RANDOMISE);

				obj->timeout += adj;
				work_obj->timeout += adj;
			} else if (use == USE_SINGLE) {
				struct object *used_obj = gear_object_for_use(
					player, obj, 1, false, &none_left);

				if (used_obj->known) {
					object_delete(cave, player->cave,
						&used_obj->known);
				}
				object_delete(cave, player->cave, &used_obj);
			}
		}

		if (describe) {
			/*
			 * Describe what's left of single use items or newly
			 * identified items of all kinds.
			 */
			char name[80];

			object_desc(name, sizeof(name), work_obj,
				ODESC_PREFIX | ODESC_FULL | ODESC_ALTNUM |
				((number + ((used && use == USE_SINGLE) ?
				-1 : 0)) << 16), player);
			if (from_floor) {
				/* Print a message */
				msg("You see %s.", name);
			} else if (first_remainder) {
				label = gear_to_label(player, first_remainder);
				msg("You have %s (1st %c).", name, label);
			} else {
				msg("You have %s (%c).", name, label);
			}
		} else if (used && use == USE_CHARGE) {
			/* Describe charges */
			if (from_floor)
				floor_item_charges(work_obj);
			else
				inven_item_charges(work_obj);
		}

		/* Clean up created copy. */
		if (work_obj->known)
			object_delete(player->cave, NULL, &work_obj->known);
		object_delete(cave, player->cave, &work_obj);
	}

	/* Use the turn */
	player->upkeep->energy_use = z_info->move_energy;

	/* Autoinscribe if we are guaranteed to still have any */
	if (!none_left && !from_floor)
		apply_autoinscription(player, obj);

	/* Mark as tried and redisplay */
	player->upkeep->notice |= (PN_COMBINE);
	player->upkeep->redraw |= (PR_INVEN | PR_EQUIP | PR_OBJECT);

	/* Hack to make Glyph of Warding work properly */
	if (square_trap_specific(cave, player->grid, rune->tidx)) {
		/* Push objects off the grid */
		if (square_object(cave, player->grid))
			push_object(player->grid);
	}

	return can_use == 0;
}


/**
 * Read a scroll
 */
void do_cmd_read_scroll(struct command *cmd)
{
	struct object *obj;

	if (!player_get_resume_normal_shape(player, cmd)) {
		return;
	}

	/* Check player can use scroll */
	if (!player_can_read(player, true))
		return;

	/* Get the scroll */
	if (cmd_get_item(cmd, "item", &obj,
			"Read which scroll? ",
			"You have no scrolls to read.",
			tval_is_scroll,
			USE_INVEN | USE_FLOOR) != CMD_OK) return;

	(void)use_aux(cmd, obj, USE_SINGLE, MSG_GENERIC);
}

/**
 * Use a staff 
 */
void do_cmd_use_staff(struct command *cmd)
{
	struct object *obj;

	if (!player_get_resume_normal_shape(player, cmd)) {
		cmd_set_repeat(0);
		return;
	}

	/* Get an item */
	if (cmd_get_item(cmd, "item", &obj,
			"Use which staff? ",
			"You have no staves to use.",
			tval_is_staff,
			USE_INVEN | USE_FLOOR | SHOW_FAIL) != CMD_OK) {
		cmd_set_repeat(0);
		return;
	}

	if (!obj_has_charges(obj)) {
		msg("That staff has no charges.");
		cmd_set_repeat(0);
		return;
	}

	/* Disable autorepetition when successful. */
	if (!use_aux(cmd, obj, USE_CHARGE, MSG_USE_STAFF)) {
		cmd_set_repeat(0);
	}
}

/**
 * Aim a wand 
 */
void do_cmd_aim_wand(struct command *cmd)
{
	struct object *obj;

	if (!player_get_resume_normal_shape(player, cmd)) {
		cmd_set_repeat(0);
		return;
	}

	/* Get an item */
	if (cmd_get_item(cmd, "item", &obj,
			"Aim which wand? ",
			"You have no wands to aim.",
			tval_is_wand,
			USE_INVEN | USE_FLOOR | SHOW_FAIL) != CMD_OK) {
		cmd_set_repeat(0);
		return;
	}

	if (!obj_has_charges(obj)) {
		msg("That wand has no charges.");
		cmd_set_repeat(0);
		return;
	}

	/* Disable autorepetition when successful. */
	if (!use_aux(cmd, obj, USE_CHARGE, MSG_ZAP_ROD)) {
		cmd_set_repeat(0);
	}
}

/**
 * Zap a rod 
 */
void do_cmd_zap_rod(struct command *cmd)
{
	struct object *obj;

	if (!player_get_resume_normal_shape(player, cmd)) {
		cmd_set_repeat(0);
		return;
	}

	/* Get an item */
	if (cmd_get_item(cmd, "item", &obj,
			"Zap which rod? ",
			"You have no rods to zap.",
			tval_is_rod,
			USE_INVEN | USE_FLOOR | SHOW_FAIL) != CMD_OK) {
		cmd_set_repeat(0);
		return;
	}

	if (!obj_can_zap(obj)) {
		msg("That rod is still charging.");
		cmd_set_repeat(0);
		return;
	}

	/* Disable autorepetition when successful. */
	if (!use_aux(cmd, obj, USE_TIMEOUT, MSG_ZAP_ROD)) {
		cmd_set_repeat(0);
	}
}

/**
 * Activate an object 
 */
void do_cmd_activate(struct command *cmd)
{
	struct object *obj;

	if (!player_get_resume_normal_shape(player, cmd)) {
		cmd_set_repeat(0);
		return;
	}

	/* Get an item */
	if (cmd_get_item(cmd, "item", &obj,
			"Activate which item? ",
			"You have no items to activate.",
			obj_is_activatable,
			USE_EQUIP | SHOW_FAIL) != CMD_OK) {
		cmd_set_repeat(0);
		return;
	}

	if (!obj_can_activate(obj)) {
		msg("That item is still charging.");
		cmd_set_repeat(0);
		return;
	}

	/* Disable autorepetition when successful. */
	if (!use_aux(cmd, obj, USE_TIMEOUT, MSG_ACT_ARTIFACT)) {
		cmd_set_repeat(0);
	}
}

/**
 * Eat some food 
 */
void do_cmd_eat_food(struct command *cmd)
{
	struct object *obj;

	/* Get an item */
	if (cmd_get_item(cmd, "item", &obj,
			"Eat which food? ",
			"You have no food to eat.",
			tval_is_edible,
			USE_INVEN | USE_FLOOR) != CMD_OK) return;

	(void)use_aux(cmd, obj, USE_SINGLE, MSG_EAT);
}

/**
 * Quaff a potion 
 */
void do_cmd_quaff_potion(struct command *cmd)
{
	struct object *obj;

	if (!player_get_resume_normal_shape(player, cmd)) {
		return;
	}

	/* Get an item */
	if (cmd_get_item(cmd, "item", &obj,
			"Quaff which potion? ",
			"You have no potions from which to quaff.",
			tval_is_potion,
			USE_INVEN | USE_FLOOR) != CMD_OK) return;

	(void)use_aux(cmd, obj, USE_SINGLE, MSG_QUAFF);
}

/**
 * Use any usable item
 */
void do_cmd_use(struct command *cmd)
{
	struct object *obj;

	if (!player_get_resume_normal_shape(player, cmd)) {
		cmd_set_repeat(0);
		return;
	}

	/* Get an item */
	if (cmd_get_item(cmd, "item", &obj,
			"Use which item? ",
			"You have no items to use.",
			obj_is_useable,
			USE_EQUIP | USE_INVEN | USE_QUIVER | USE_FLOOR | SHOW_FAIL | QUIVER_TAGS | SHOW_FAIL) != CMD_OK) {
		cmd_set_repeat(0);
		return;
	}

	/*
	 * If this is not a staff, wand, rod, or activatable item, always
	 * disable autorepetition.  The functions for handling a staff, wand
	 * rod, or activatable item take care of autorepetition for those
	 * objects.
	 */
	if (tval_is_ammo(obj)) {
		do_cmd_fire(cmd);
		cmd_set_repeat(0);
	} else if (tval_is_potion(obj)) {
		do_cmd_quaff_potion(cmd);
		cmd_set_repeat(0);
	} else if (tval_is_edible(obj)) {
		do_cmd_eat_food(cmd);
		cmd_set_repeat(0);
	} else if (tval_is_rod(obj)) {
		do_cmd_zap_rod(cmd);
	} else if (tval_is_wand(obj)) {
		do_cmd_aim_wand(cmd);
	} else if (tval_is_staff(obj)) {
		do_cmd_use_staff(cmd);
	} else if (tval_is_scroll(obj)) {
		do_cmd_read_scroll(cmd);
		cmd_set_repeat(0);
	} else if (obj_can_refill(obj)) {
		do_cmd_refill(cmd);
		cmd_set_repeat(0);
	} else if (obj_is_activatable(obj)) {
		if (object_is_equipped(player->body, obj)) {
			do_cmd_activate(cmd);
		} else {
			msg("Equip the item to use it.");
			cmd_set_repeat(0);
		}
	} else {
		msg("The item cannot be used at the moment");
		cmd_set_repeat(0);
	}
}


/**
 * ------------------------------------------------------------------------
 * Refuelling
 * ------------------------------------------------------------------------
 */

static void refill_lamp(struct object *lamp, struct object *obj)
{
	/* Refuel */
	lamp->timeout += obj->timeout ? obj->timeout : obj->pval;

	/* Message */
	msg("You fuel your lamp.");

	/* Comment */
	if (lamp->timeout >= z_info->fuel_lamp) {
		lamp->timeout = z_info->fuel_lamp;
		msg("Your lamp is full.");
	}

	/* Refilled from a lantern */
	if (of_has(obj->flags, OF_TAKES_FUEL)) {
		/* Unstack if necessary */
		if (obj->number > 1) {
			/* Obtain a local object, split */
			struct object *used = object_split(obj, 1);

			/* Remove fuel */
			used->timeout = 0;

			/* Carry or drop */
			if (object_is_carried(player, obj) && inven_carry_okay(used))
				inven_carry(player, used, true, true);
			else
				drop_near(cave, &used, 0, player->grid, false, true);
		} else
			/* Empty a single lantern */
			obj->timeout = 0;

		/* Combine the pack (later) */
		player->upkeep->notice |= (PN_COMBINE);

		/* Redraw stuff */
		player->upkeep->redraw |= (PR_INVEN);
	} else { /* Refilled from a flask */
		struct object *used;
		bool none_left = false;

		/* Decrease the item from the pack or the floor */
		if (object_is_carried(player, obj)) {
			used = gear_object_for_use(player, obj, 1, true,
				&none_left);
		} else {
			used = floor_object_for_use(player, obj, 1, true,
				&none_left);
		}
		if (used->known)
			object_delete(player->cave, NULL, &used->known);
		object_delete(cave, player->cave, &used);
	}

	/* Recalculate torch */
	player->upkeep->update |= (PU_TORCH);

	/* Redraw stuff */
	player->upkeep->redraw |= (PR_EQUIP);
}


void do_cmd_refill(struct command *cmd)
{
	struct object *light = equipped_item_by_slot_name(player, "light");
	struct object *obj;

	if (!player_get_resume_normal_shape(player, cmd)) {
		return;
	}

	/* Check what we're wielding. */
	if (!light || !tval_is_light(light)) {
		msg("You are not wielding a light.");
		return;
	} else if (of_has(light->flags, OF_NO_FUEL)
			|| !of_has(light->flags, OF_TAKES_FUEL)) {
		msg("Your light cannot be refilled.");
		return;
	}

	/* Get an item */
	if (cmd_get_item(cmd, "item", &obj,
			"Refuel with with fuel source? ",
			"You have nothing you can refuel with.",
			obj_can_refill,
			USE_INVEN | USE_FLOOR | USE_QUIVER) != CMD_OK) return;

	refill_lamp(light, obj);

	player->upkeep->energy_use = z_info->move_energy / 2;
}



/**
 * ------------------------------------------------------------------------
 * Spell casting
 * ------------------------------------------------------------------------
 */

/**
 * Cast a spell from a book
 */
void do_cmd_cast(struct command *cmd)
{
	int spell_index, dir = 0;
	const struct class_spell *spell;

	if (!player_get_resume_normal_shape(player, cmd)) {
		return;
	}

	/* Check the player can cast spells at all */
	if (!player_can_cast(player, true))
		return;

	/* Get arguments */
	if (cmd_get_spell(cmd, "spell", player, &spell_index,
			/* Verb */ "cast",
			/* Book */ obj_can_cast_from,
			/* Book error */ "There are no spells you can cast.",
			/* Filter */ spell_okay_to_cast,
			/* Spell error */ "That book has no spells that you can cast.") != CMD_OK) {
		return;
	}

	/* Get the spell */
	spell = spell_by_index(player, spell_index);

	/* Verify "dangerous" spells */
	if (spell->smana > player->csp) {
		const char *verb = spell->realm->verb;
		const char *noun = spell->realm->spell_noun;

		/* Warning */
		msg("You do not have enough mana to %s this %s.", verb, noun);

		/* Flush input */
		event_signal(EVENT_INPUT_FLUSH);

		/* Verify */
		if (!get_check("Attempt it anyway? ")) return;
	}

	if (spell_needs_aim(spell_index)) {
		if (cmd_get_target(cmd, "target", &dir) == CMD_OK)
			player_confuse_dir(player, &dir, false);
		else
			return;
	}

	/* Cast a spell */
	target_fix();
	if (spell_cast(spell_index, dir, cmd)) {
		if (player->timed[TMD_FASTCAST]) {
			player->upkeep->energy_use = (z_info->move_energy * 3) / 4;
		} else {
			player->upkeep->energy_use = z_info->move_energy;
		}
	}
	target_release();
}


/**
 * Gain a specific spell, specified by spell number (for mages).
 */
void do_cmd_study_spell(struct command *cmd)
{
	int spell_index;

	/* Check the player can study at all atm */
	if (!player_can_study(player, true))
		return;

	if (cmd_get_spell(cmd, "spell", player, &spell_index,
			/* Verb */ "study",
			/* Book */ obj_can_study,
			/* Book error */ "You cannot learn any new spells from the books you have.",
			/* Filter */ spell_okay_to_study,
			/* Spell error */ "That book has no spells that you can learn.") != CMD_OK)
		return;

	spell_learn(spell_index);
	player->upkeep->energy_use = z_info->move_energy;
}

/**
 * Gain a random spell from the given book (for priests)
 */
void do_cmd_study_book(struct command *cmd)
{
	struct object *book_obj;
	const struct class_book *book;
	int spell_index = -1;
	struct class_spell *spell;
	int i, k = 0;

	/* Check the player can study at all atm */
	if (!player_can_study(player, true))
		return;

	if (cmd_get_item(cmd, "item", &book_obj,
			/* Prompt */ "Study which book? ",
			/* Error  */ "You cannot learn any new spells from the books you have.",
			/* Filter */ obj_can_study,
			/* Choice */ USE_INVEN | USE_FLOOR) != CMD_OK)
		return;

	book = player_object_to_book(player, book_obj);
	track_object(player->upkeep, book_obj);
	handle_stuff(player);

	for (i = 0; i < book->num_spells; i++) {
		spell = &book->spells[i];
		if (!spell_okay_to_study(player, spell->sidx))
			continue;
		if ((++k > 1) && (randint0(k) != 0))
			continue;
		spell_index = spell->sidx;
	}

	if (spell_index < 0) {
		msg("You cannot learn any %ss in that book.", book->realm->spell_noun);
	} else {
		spell_learn(spell_index);
		player->upkeep->energy_use = z_info->move_energy;
	}
}

/**
 * Choose the way to study.  Choose life.  Choose a career.  Choose family.
 * Choose a fucking big monster, choose orc shamans, kobolds, dark elven
 * druids, and Mim, Betrayer of Turin.
 */
void do_cmd_study(struct command *cmd)
{
	if (!player_get_resume_normal_shape(player, cmd)) {
		return;
	}

	if (player_has(player, PF_CHOOSE_SPELLS))
		do_cmd_study_spell(cmd);
	else
		do_cmd_study_book(cmd);
}
