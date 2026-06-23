from __future__ import annotations

from typing import TYPE_CHECKING

from BaseClasses import Region

from . import data

if TYPE_CHECKING:
    from .world import AngbandWorld

# Angband's regions are dungeon depth bands: Town, then Depths 1-20, 21-30, ...,
# up to 91-100, connected in a single linear descent.  Reaching a deeper band
# requires the artifact items the player has received (see rules.py); modelling
# the bands as regions lets that descent logic live on the entrances between them.


def entrance_name(src: str, dst: str) -> str:
    return f"{src} -> {dst}"


def create_and_connect_regions(world: AngbandWorld) -> None:
    create_all_regions(world)
    connect_regions(world)


def create_all_regions(world: AngbandWorld) -> None:
    regions = [
        Region(name, world.player, world.multiworld) for name in data.REGION_TABLE
    ]
    world.multiworld.regions += regions


def connect_regions(world: AngbandWorld) -> None:
    # Just wire the topology here; the per-entrance descent rules are applied in
    # rules.py (so all rule logic lives in one place).
    for src, info in data.REGION_TABLE.items():
        source_region = world.get_region(src)
        for dst in info["connects_to"]:
            source_region.connect(world.get_region(dst), entrance_name(src, dst))
