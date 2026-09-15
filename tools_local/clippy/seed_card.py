#!/usr/bin/env python3
"""Put Clippy Facts' database on a card.

    ./tools_local/clippy/seed_card.py [card-root]

Copies assets_local/clippy/facts.txt to <card>/clippy/facts.txt, which is where
the app looks (clippy::kFactsPath in ClippyFactsCore.h). The default card is the
simulator's fs_agent, the same one sim-shot.sh drives; hand it the mount point
of a real SD card to install on the device.

Counts the lines the reader will accept, by the reader's own rules -- blank and
'#' lines are not facts, lines over 200 bytes are skipped, and only the first
1024 are indexed -- so what this prints is what the app's footer will say. If
the two numbers ever differ, one of the two parsers is wrong, and the C++ one
is the one on the device.
"""

import pathlib
import re
import shutil
import sys

REPO = pathlib.Path(__file__).resolve().parents[2]
SOURCE = REPO / "assets_local/clippy/facts.txt"
CORE = REPO / "src/apps_local/clippy/ClippyFactsCore.h"


def constant(name):
    match = re.search(rf"constexpr int {name} = (\d+);", CORE.read_text())
    if not match:
        sys.exit(f"seed_card: no {name} in {CORE.name}")
    return int(match.group(1))


def path_constant():
    match = re.search(r'constexpr const char\* kFactsPath = "([^"]+)";', CORE.read_text())
    if not match:
        sys.exit(f"seed_card: no kFactsPath in {CORE.name}")
    return match.group(1)


def count_facts(text, max_bytes, max_facts):
    """(kept, skipped), the way FactFile::open() would count them."""
    kept = 0
    skipped = 0
    for line in text.splitlines():
        line = line.strip(" \t\r")
        if not line or line.startswith("#"):
            continue
        if len(line.encode("utf-8")) > max_bytes or kept >= max_facts:
            skipped += 1
            continue
        kept += 1
    return kept, skipped


def main():
    card = pathlib.Path(sys.argv[1]) if len(sys.argv) > 1 else REPO / "fs_agent"
    target = card / path_constant().lstrip("/")
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(SOURCE, target)
    kept, skipped = count_facts(
        SOURCE.read_text(encoding="utf-8"), constant("kMaxFactBytes"), constant("kMaxFacts")
    )
    print(f"seeded {target}: {kept} facts, {skipped} skipped")


if __name__ == "__main__":
    main()
