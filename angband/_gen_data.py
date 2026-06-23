"""
One-shot generator: reads the manual_angband425_broney JSON data and emits
data.py for the Angband apworld.  Run from the repo root:

    python apworld/_gen_data.py

The names produced here must byte-match what the native game (the APCc client)
sends/receives, which is why we copy them verbatim from the manual data that was
already validated against the game's monster.txt / artifact.txt.
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))
MANUAL = os.path.join(HERE, "..", "manual_angband425_broney", "data")

# Stable ID bases.  Item and location IDs live in separate namespaces in AP, so
# overlap is harmless, but distinct bases keep logs readable.
ITEM_ID_BASE = 4250000
LOCATION_ID_BASE = 4251000


def load(name):
    with open(os.path.join(MANUAL, name), encoding="utf-8") as fh:
        return json.load(fh)


def pyrepr(obj, indent=0):
    """Deterministic, readable repr for the literal dump."""
    pad = "    " * indent
    pad2 = "    " * (indent + 1)
    if isinstance(obj, dict):
        if not obj:
            return "{}"
        lines = ["{"]
        for k, v in obj.items():
            lines.append(f"{pad2}{k!r}: {pyrepr(v, indent + 1)},")
        lines.append(pad + "}")
        return "\n".join(lines)
    if isinstance(obj, (list, tuple)):
        if not obj:
            return "[]"
        # keep short lists inline
        inline = "[" + ", ".join(pyrepr(x) for x in obj) + "]"
        if len(inline) <= 96:
            return inline
        lines = ["["]
        for x in obj:
            lines.append(f"{pad2}{pyrepr(x, indent + 1)},")
        lines.append(pad + "]")
        return "\n".join(lines)
    return repr(obj)


def parse_requires(req):
    """
    Turn the manual requirement DSL into a list of (item_name, count) tuples.
    The manual only ever uses the form
        |Item A:count| AND |Item B:count| AND ...
    so we just split on AND and parse each |...| token.
    """
    if not req:
        return []
    out = []
    for tok in req.split("AND"):
        tok = tok.strip()
        if not tok:
            continue
        assert tok.startswith("|") and tok.endswith("|"), tok
        inner = tok[1:-1]
        name, _, count = inner.rpartition(":")
        out.append([name.strip(), int(count)])
    return out


def main():
    items = load("items.json")
    locations = load("locations.json")
    regions = load("regions.json")
    game = load("game.json")

    # ---- Items -------------------------------------------------------------
    # Each distinct item NAME gets one ID.  Count/classification come alongside.
    item_name_to_id = {}
    item_meta = {}  # name -> {count, classification, is_artifact}
    next_id = ITEM_ID_BASE + 1
    for it in items:
        name = it["name"]
        assert name not in item_name_to_id, f"duplicate item {name}"
        item_name_to_id[name] = next_id
        next_id += 1
        if it.get("progression"):
            cls = "progression"
        elif it.get("useful"):
            cls = "useful"
        elif it.get("trap"):
            cls = "trap"
        else:
            cls = "filler"
        cats = it.get("category", [])
        item_meta[name] = {
            "count": it.get("count", 1),
            "classification": cls,
            "is_artifact": "Artifact" in cats,
        }

    # The dedicated filler item (from game.json) is its own item, not in the
    # itempool list above.  Give it an ID and filler classification.
    filler_name = game.get("filler_item_name", "Filler")
    if filler_name not in item_name_to_id:
        item_name_to_id[filler_name] = next_id
        next_id += 1
        item_meta[filler_name] = {
            "count": 0,            # created on demand only
            "classification": "filler",
            "is_artifact": False,
        }

    # ---- Locations ---------------------------------------------------------
    # Unique-kill locations first (category starts with "Unique"), then the
    # artifact-pickup locations.  Stable order -> stable IDs.
    def is_artifact_loc(loc):
        return any("Artifact" in c for c in loc.get("category", []))

    ordered = [l for l in locations if not is_artifact_loc(l)] + \
              [l for l in locations if is_artifact_loc(l)]

    location_name_to_id = {}
    location_meta = {}  # name -> {region, category, is_artifact}
    next_loc = LOCATION_ID_BASE + 1
    for loc in ordered:
        name = loc["name"]
        assert name not in location_name_to_id, f"duplicate location {name}"
        location_name_to_id[name] = next_loc
        next_loc += 1
        location_meta[name] = {
            "region": loc["region"],
            "is_artifact": is_artifact_loc(loc),
        }

    # ---- Regions -----------------------------------------------------------
    # Preserve insertion order; convert requires DSL to (item, count) lists.
    region_data = {}
    for rname, rinfo in regions.items():
        region_data[rname] = {
            "connects_to": list(rinfo.get("connects_to", [])),
            "requires": parse_requires(rinfo.get("requires", [])),
            "starting": bool(rinfo.get("starting", False)),
        }

    # The deepest region (no outgoing connections) is where Victory lives.
    deepest = [r for r, d in region_data.items() if not d["connects_to"]]
    assert len(deepest) == 1, f"expected one deepest region, got {deepest}"
    victory_region = deepest[0]

    # ---- Emit --------------------------------------------------------------
    header = '''"""
Generated by _gen_data.py from the manual_angband425_broney data.  Do not edit
by hand; re-run the generator instead.

These tables are the single source of truth shared between generation (this
apworld) and the native game's APCc client.  Every location name is exactly the
string the client sends when the corresponding unique is killed / artifact is
picked up, and every item name is exactly the string the client receives.
"""

ITEM_ID_BASE = {item_base}
LOCATION_ID_BASE = {loc_base}

# The dedicated, infinitely-repeatable filler item.
FILLER_ITEM_NAME = {filler!r}

# The region the Victory event is placed in (the deepest dungeon band).
VICTORY_REGION = {victory!r}
'''.format(item_base=ITEM_ID_BASE, loc_base=LOCATION_ID_BASE,
           filler=filler_name, victory=victory_region)

    body = []
    body.append("# name -> AP item id")
    body.append("ITEM_NAME_TO_ID = " + pyrepr(item_name_to_id))
    body.append("")
    body.append("# name -> {count, classification, is_artifact}")
    body.append("ITEM_TABLE = " + pyrepr(item_meta))
    body.append("")
    body.append("# name -> AP location id")
    body.append("LOCATION_NAME_TO_ID = " + pyrepr(location_name_to_id))
    body.append("")
    body.append("# name -> {region, is_artifact}")
    body.append("LOCATION_TABLE = " + pyrepr(location_meta))
    body.append("")
    body.append("# region name -> {connects_to, requires:[[item,count]...], starting}")
    body.append("REGION_TABLE = " + pyrepr(region_data))
    body.append("")

    out = header + "\n" + "\n".join(body)
    with open(os.path.join(HERE, "data.py"), "w", encoding="utf-8") as fh:
        fh.write(out)

    # Console summary for sanity.
    n_items = sum(m["count"] for m in item_meta.values())
    n_unique = sum(1 for m in location_meta.values() if not m["is_artifact"])
    n_art = sum(1 for m in location_meta.values() if m["is_artifact"])
    print(f"items: {len(item_name_to_id)} names, {n_items} copies")
    print(f"locations: {len(location_name_to_id)} ({n_unique} unique-kill, {n_art} artifact)")
    print(f"regions: {len(region_data)}; victory region: {victory_region}")
    print("wrote data.py")


if __name__ == "__main__":
    main()
