from __future__ import annotations

from typing import TYPE_CHECKING

from worlds.generic.Rules import set_rule

from . import data
from .regions import entrance_name

if TYPE_CHECKING:
    from .world import AngbandWorld


def set_all_rules(world: AngbandWorld) -> None:
    set_all_entrance_rules(world)
    set_completion_condition(world)


def set_all_entrance_rules(world: AngbandWorld) -> None:
    # Each deeper dungeon band lists the "Progressive ... Artifact" items required
    # to descend into it.  We put that requirement on the single entrance leading
    # into the band.  When artifacts aren't checks, those items don't exist, so we
    # leave the descent ungated (the dungeon is a straight unique hunt).
    if not world.options.artifacts_as_checks:
        return

    for src, info in data.REGION_TABLE.items():
        for dst in info["connects_to"]:
            requires = data.REGION_TABLE[dst]["requires"]
            if not requires:
                continue
            entrance = world.get_entrance(entrance_name(src, dst))
            # Bind the requirement list as a default arg so the lambda captures
            # this specific entrance's needs rather than the loop variable.
            set_rule(
                entrance,
                lambda state, reqs=tuple((n, c) for n, c in requires): all(
                    state.has(name, world.player, count) for name, count in reqs
                ),
            )


def set_completion_condition(world: AngbandWorld) -> None:
    # The goal is the Victory event placed in the deepest band (see
    # locations.create_events).  Reaching it already implies satisfying every
    # descent requirement, so this is all we need.
    world.multiworld.completion_condition[world.player] = (
        lambda state: state.has("Victory", world.player)
    )
