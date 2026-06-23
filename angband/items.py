from __future__ import annotations

from typing import TYPE_CHECKING

from BaseClasses import Item, ItemClassification

from . import data

if TYPE_CHECKING:
    from .world import AngbandWorld

# Every item must have a unique integer ID, present in this lookup even if the
# item doesn't exist under some option combination.  The full table (including
# the dedicated filler item) is generated in data.py from the manual data, so the
# names match exactly what the native game's APCc client receives.
ITEM_NAME_TO_ID = data.ITEM_NAME_TO_ID

# Map the generated classification strings to real ItemClassification values.
_CLASSIFICATION = {
    "progression": ItemClassification.progression,
    "useful": ItemClassification.useful,
    "filler": ItemClassification.filler,
    "trap": ItemClassification.trap,
}


class AngbandItem(Item):
    game = "Angband"


def _classification(name: str) -> ItemClassification:
    return _CLASSIFICATION[data.ITEM_TABLE[name]["classification"]]


def get_random_filler_item_name(world: AngbandWorld) -> str:
    # Angband has a single, infinitely-repeatable filler item.  We must define a
    # get_filler_item_name() (bound to this in world.py) so that core can request
    # arbitrary extra filler (item links, panic fill, etc.).
    return data.FILLER_ITEM_NAME


def create_item_with_correct_classification(world: AngbandWorld, name: str) -> AngbandItem:
    # The world's create_item() must be able to build any item by name at any
    # time.  Classifications are static for Angband, so this is a simple lookup.
    return AngbandItem(name, _classification(name), ITEM_NAME_TO_ID[name], world.player)


def create_all_items(world: AngbandWorld) -> None:
    # AP requires exactly as many items as there are unfilled (non-event)
    # locations.  Both of Angband's modes are balanced so this works out:
    #   * artifacts_as_checks ON : 232 locations, 231 real items + 1 filler.
    #   * artifacts_as_checks OFF: 96 unique-kill locations, 96 boon items.
    artifacts_as_checks = bool(world.options.artifacts_as_checks)

    itempool: list[Item] = []
    for name, meta in data.ITEM_TABLE.items():
        count = meta["count"]
        if count <= 0:
            continue  # e.g. the on-demand-only filler item
        # In OFF mode the artifact items (progressive + named) don't exist, since
        # the artifacts spawn normally instead of being multiworld items.
        if meta["is_artifact"] and not artifacts_as_checks:
            continue
        itempool += [world.create_item(name) for _ in range(count)]

    # Top up to match the unfilled location count using repeatable filler.
    # get_unfilled_locations correctly ignores our Victory *event* location.
    number_of_unfilled_locations = len(world.multiworld.get_unfilled_locations(world.player))
    needed_filler = number_of_unfilled_locations - len(itempool)
    if needed_filler < 0:
        raise Exception(
            f"Angband: more items ({len(itempool)}) than locations "
            f"({number_of_unfilled_locations}); item/location tables are out of sync."
        )
    itempool += [world.create_filler() for _ in range(needed_filler)]

    world.multiworld.itempool += itempool
