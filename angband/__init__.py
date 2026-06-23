# The first file Archipelago looks at is the archipelago.json manifest, but the
# first Python file imported is this __init__.py.  Its only real job is to make
# the World subclass discoverable by importing it (which registers it with the
# AutoWorld machinery).
#
# Unlike APQuest, Angband's "client" is the native game itself (the patched
# angband.exe talking to the server via the vendored APCc client), so there is
# no in-apworld Python client component to register here.
from .world import AngbandWorld as AngbandWorld
