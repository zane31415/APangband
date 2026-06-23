from dataclasses import dataclass

from Options import DeathLink, DefaultOnToggle, OptionGroup, PerGameCommonOptions

# Angband's player-facing options.  These end up in the template yaml and on the
# WebHost options page.  See the Options API doc:
# https://github.com/ArchipelagoMW/Archipelago/blob/main/docs/options%20api.md


class ArtifactsAsChecks(DefaultOnToggle):
    """
    Turn artifacts into Archipelago checks.

    When enabled (the default and the way the randomizer is balanced), picking up
    an artifact sends a location check and the real artifacts are instead handed
    out by the multiworld.  Descending past the early dungeon also requires having
    received enough "Progressive ... Artifact" items, so the artifacts become your
    progression gating.

    When disabled, artifacts spawn and behave normally, only unique-monster kills
    are checks, and the dungeon has no artifact gating.

    The connected game reads this from slot_data (key "artifacts_as_checks").
    """

    display_name = "Artifacts As Checks"


# DeathLink is a standard Archipelago option; we reuse the built-in class so it
# behaves consistently with every other game.  The native client supports it.
# (DeathLink defaults to off; players opt in from their yaml.)


@dataclass
class AngbandOptions(PerGameCommonOptions):
    artifacts_as_checks: ArtifactsAsChecks
    death_link: DeathLink


option_groups = [
    OptionGroup(
        "Gameplay Options",
        [ArtifactsAsChecks, DeathLink],
    ),
]
