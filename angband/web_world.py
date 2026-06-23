from BaseClasses import Tutorial
from worlds.AutoWorld import WebWorld

from .options import option_groups


class AngbandWebWorld(WebWorld):
    game = "Angband"

    # Visual theme for the game's WebHost pages.  Options: dirt, grass,
    # grassFlowers, ice, jungle, ocean, partyTime, stone.
    theme = "stone"

    setup_en = Tutorial(
        "Multiworld Setup Guide",
        "A guide to setting up Angband for Archipelago multiworld.",
        "English",
        "setup_en.md",
        "setup/en",
        ["Broney"],
    )

    tutorials = [setup_en]

    option_groups = option_groups
