#!/usr/bin/env python3
"""Turn a pasted list of facts into assets_local/clippy/facts.txt.

    ./tools_local/clippy/import_facts.py <pasted.txt> [--out assets_local/clippy/facts.txt]

A list somebody pasted from the web is not yet a database the panel can show:
it has curly quotes and dashes the device's faces cannot draw, lines too long
for the fact box, lines with no full stop, and repeats. This makes the minimum
of changes to fix each of those, and prints every line it dropped, so the
person who owns the list can see what happened to it.

What it does, and only this:
  * plain ASCII: curly quotes and apostrophes, en/em dashes, ellipses and
    non-breaking spaces become their typewriter equivalents; accented letters
    lose their accents; anything else non-ASCII is dropped from the line
  * whitespace: runs of blanks collapse to one, ends are trimmed
  * one sentence ends with punctuation: a line ending in a letter, digit or
    closing bracket gets a full stop
  * duplicates: the first copy stays, later ones go
  * length: a line over kMaxFactBytes (200) after all that is dropped, because
    the reader on the device would skip it anyway and a fact nobody sees is
    the failure the footer count cannot show

Comments ('#' lines) and blank lines in the input are ignored. The header of
the output explains the format to whoever opens the file on the card.
"""
import argparse
import pathlib
import re
import sys
import unicodedata

REPO = pathlib.Path(__file__).resolve().parents[2]
DEFAULT_OUT = REPO / "assets_local/clippy/facts.txt"
CORE = REPO / "src/apps_local/clippy/ClippyFactsCore.h"

ASCII_FOR = {
    "‘": "'", "’": "'", "‚": "'", "‛": "'", "′": "'",
    "“": '"', "”": '"', "„": '"', "″": '"',
    "–": "-", "—": "-", "―": "-", "−": "-",
    "…": "...",
    " ": " ", " ": " ", "​": "",
    "°": " degrees",
    "×": "x",
}

HEADER = """# Clippy Facts: the database.
#
# Copy this file to /clippy/facts.txt on the device's SD card (a folder named
# clippy at the root of the card), or run
#   tools_local/clippy/seed_card.py <card-root>
# which does that and counts what the app will accept.
#
# One fact per line. Lines starting with # and blank lines are ignored. Lines
# longer than 200 characters are skipped, because they would not fit the panel.
# Plain ASCII draws best: the reading face on the device carries the Latin
# alphabet and common punctuation, and a character it lacks draws as nothing.
# Add your own below. Order does not matter; the app picks at random and never
# shows the same fact twice in a row.
#
# Imported by tools_local/clippy/import_facts.py, which is what makes a pasted
# list fit those rules. Edit this file directly for small changes; re-run the
# importer over a new list for large ones.

"""


def max_bytes():
    match = re.search(r"constexpr int kMaxFactBytes = (\d+);", CORE.read_text())
    return int(match.group(1)) if match else 200


def to_ascii(text):
    for bad, good in ASCII_FOR.items():
        text = text.replace(bad, good)
    # Accents off, then anything still outside printable ASCII goes.
    text = unicodedata.normalize("NFKD", text)
    return "".join(c for c in text if 32 <= ord(c) < 127)


def clean(line):
    line = to_ascii(line)
    line = re.sub(r"\s+", " ", line).strip()
    # " ." and " ," left behind by a stray space before punctuation.
    line = re.sub(r"\s+([.,;:!?])", r"\1", line)
    if line and (line[-1].isalnum() or line[-1] in ")\"'"):
        line += "."
    return line


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("source")
    ap.add_argument("--out", default=str(DEFAULT_OUT))
    args = ap.parse_args()

    limit = max_bytes()
    raw = pathlib.Path(args.source).read_text(encoding="utf-8-sig")
    kept, seen = [], set()
    dropped_long, dropped_dup, changed = [], 0, 0
    for original in raw.splitlines():
        stripped = original.strip()
        if not stripped or stripped.startswith("#"):
            continue
        line = clean(stripped)
        if not line:
            continue
        if line != stripped:
            changed += 1
        if len(line.encode("utf-8")) > limit:
            dropped_long.append(line)
            continue
        key = line.lower()
        if key in seen:
            dropped_dup += 1
            continue
        seen.add(key)
        kept.append(line)

    out = pathlib.Path(args.out)
    out.write_text(HEADER + "\n".join(kept) + "\n", encoding="utf-8", newline="\n")

    print(f"wrote {out}: {len(kept)} facts")
    print(f"  {changed} lines changed (ASCII, spacing or a final full stop)")
    print(f"  {dropped_dup} duplicates dropped")
    print(f"  {len(dropped_long)} dropped for being over {limit} bytes:")
    for line in dropped_long:
        print(f"    - {line[:100]}{'...' if len(line) > 100 else ''}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
