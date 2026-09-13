#!/usr/bin/env python3
"""The fallback that drew ClippyArt.h, for a machine with no librsvg.

WHAT THIS IS NOT: a general SVG renderer, or a replacement for anything. The
canonical generator is tools_local/clippy/gen_clippy_art.sh, which runs the
SDK's own gen_icons.py -- one asset pipeline for the whole fork, and this does
not become a second one. Run that instead the moment `rsvg-convert` and `uv`
exist on the machine, and commit its output over this.

WHY IT EXISTS: the machine Clippy Facts was written on has neither, and an app
whose only drawing is a TODO is not an app. It reads assets_local/clippy/clippy.svg
-- the same file, so there is still exactly one drawing -- and emits the same
header in the same 1-bpp format, from the same numbers.

WHAT IT UNDERSTANDS, and nothing else: <rect> with rx, <circle>, <line> with
round caps, `fill`, `stroke` and `stroke-width` as presentation attributes ON
THE ELEMENT. No paths, no transforms, no groups, no inheritance, no CSS. It
fails loudly on anything else rather than silently dropping it, because a
dropped element is an eyebrow that is simply not there.

TWO DELIBERATE DIFFERENCES from gen_icons.py, both of which mean regenerating
through the real pipeline will shift the low bits:

  * Antialiasing is a 4x4 box supersample thresholded at gen_icons.py's own
    THRESHOLD, rather than librsvg's analytic coverage. Same rule, coarser.

  * MINIMUM STROKE HINTING. A 4-unit stroke in a 96-unit drawing is 1.3px at
    the 32px shelf cut, and 1.3px of coverage thresholded at 57% is a dotted
    line or nothing at all. Strokes are widened to at least kMinStrokePx of
    output before rasterising, which is what keeps the brows on the shelf icon.
    librsvg will draw them lighter and truer; look at the shelf row after you
    regenerate.

Usage:
    py -3 tools_local/clippy/rasterize_clippy.py \
        --svg assets_local/clippy/clippy.svg \
        --alias clippy --sizes 32,240 \
        --out src/apps_local/clippy/ClippyArt.h
"""
import argparse
import math
import re
import sys

# Luminance below this (composited on white) counts as black/drawn. gen_icons.py's
# number, quoted rather than re-chosen: the two rasterisers have to agree about
# what "drawn" means or the same SVG gives two different silhouettes.
THRESHOLD = 110

# Box supersampling factor per axis. 4 gives 16 coverage levels, which is finer
# than the threshold can distinguish anyway.
SUPERSAMPLE = 4

# The floor a stroke is widened to, in OUTPUT pixels. See the module docstring.
MIN_STROKE_PX = 1.8

# clang-format's ColumnLimit for this repo, so the emitted byte tables do not
# come back reflowed by the first person to run ./bin/clang-format-fix.
COLUMN_LIMIT = 120

TAG_RE = re.compile(r"<(rect|circle|line|svg)\b([^>]*)>")
# Digits belong in an attribute NAME, which is not obvious until x1 and y1 both
# read as absent, every <line> collapses to the origin, and the drawing gains one
# stray black pixel in its top-left corner that looks like a packing bug.
ATTR_RE = re.compile(r"([A-Za-z][A-Za-z0-9-]*)\s*=\s*\"([^\"]*)\"")

# What each element must state. Nothing is inherited and nothing defaults to a
# useful value, so a missing coordinate is an error rather than a zero: see above
# for what a silent zero looks like.
REQUIRED = {
    "rect": ("x", "y", "width", "height"),
    "circle": ("cx", "cy", "r"),
    "line": ("x1", "y1", "x2", "y2"),
}


def parse_svg(path):
    """(viewBox size, [(tag, attrs)]) in document order. Square viewBox only."""
    text = open(path, encoding="utf-8").read()
    # Comments first: the SVG carries a long one, and it mentions <g stroke="#000">.
    text = re.sub(r"<!--.*?-->", "", text, flags=re.S)
    if "<g" in text or "<path" in text or "transform=" in text:
        sys.exit("rasterize_clippy.py: groups, paths and transforms are not supported; "
                 "regenerate with tools_local/clippy/gen_clippy_art.sh")
    view = None
    elements = []
    for match in TAG_RE.finditer(text):
        tag = match.group(1)
        attrs = dict(ATTR_RE.findall(match.group(2)))
        if tag == "svg":
            box = [float(v) for v in attrs.get("viewBox", "").replace(",", " ").split()]
            if len(box) != 4 or box[0] != 0 or box[1] != 0 or box[2] != box[3]:
                sys.exit("rasterize_clippy.py: need a square viewBox at the origin")
            view = box[2]
            continue
        elements.append((tag, attrs))
    if view is None:
        sys.exit("rasterize_clippy.py: no <svg viewBox>")
    if not elements:
        sys.exit("rasterize_clippy.py: nothing to draw")
    return view, elements


def is_ink(value):
    """Whether a paint value is drawn at all, and whether it is black.

    Returns None for "not painted", True for black, False for white. Anything
    else is a colour a 1-bit panel cannot hold, so it is an error rather than a
    guess.
    """
    if value is None:
        return None
    value = value.strip().lower()
    if value in ("none", ""):
        return None
    if value in ("#000", "#000000", "black"):
        return True
    if value in ("#fff", "#ffffff", "white"):
        return False
    sys.exit("rasterize_clippy.py: %r is neither black, white nor none" % value)


def rounded_rect_sdf(u, v, cx, cy, hw, hh, r):
    qx = abs(u - cx) - (hw - r)
    qy = abs(v - cy) - (hh - r)
    outside = math.hypot(max(qx, 0.0), max(qy, 0.0))
    return outside + min(max(qx, qy), 0.0) - r


def segment_distance(u, v, x1, y1, x2, y2):
    dx, dy = x2 - x1, y2 - y1
    length2 = dx * dx + dy * dy
    if length2 <= 0.0:
        return math.hypot(u - x1, v - y1)
    t = ((u - x1) * dx + (v - y1) * dy) / length2
    t = 0.0 if t < 0.0 else (1.0 if t > 1.0 else t)
    return math.hypot(u - (x1 + t * dx), v - (y1 + t * dy))


def shape_for(tag, attrs, min_stroke):
    """(distance function, bbox in user units, fill ink, stroke ink, stroke width).

    The distance function is a signed distance to the shape's OUTLINE: negative
    inside. Fill is `d <= 0`, stroke is `abs(d) <= width / 2`, which is the one
    place the two are distinguished.
    """
    for name in REQUIRED.get(tag, ()):
        if name not in attrs:
            sys.exit("rasterize_clippy.py: <%s> is missing %s" % (tag, name))
    get = lambda name, default=0.0: float(attrs.get(name, default))
    fill = is_ink(attrs.get("fill"))
    stroke = is_ink(attrs.get("stroke"))
    width = max(get("stroke-width"), min_stroke) if stroke is not None else 0.0
    if tag == "rect":
        x, y, w, h = get("x"), get("y"), get("width"), get("height")
        r = min(get("rx", attrs.get("ry", 0.0)), w / 2.0, h / 2.0)
        cx, cy, hw, hh = x + w / 2.0, y + h / 2.0, w / 2.0, h / 2.0
        distance = lambda u, v: rounded_rect_sdf(u, v, cx, cy, hw, hh, r)
        bbox = (x, y, x + w, y + h)
    elif tag == "circle":
        cx, cy, r = get("cx"), get("cy"), get("r")
        distance = lambda u, v: math.hypot(u - cx, v - cy) - r
        bbox = (cx - r, cy - r, cx + r, cy + r)
    elif tag == "line":
        x1, y1, x2, y2 = get("x1"), get("y1"), get("x2"), get("y2")
        if attrs.get("stroke-linecap") != "round":
            sys.exit("rasterize_clippy.py: only round line caps are supported")
        distance = lambda u, v: segment_distance(u, v, x1, y1, x2, y2)
        bbox = (min(x1, x2), min(y1, y2), max(x1, x2), max(y1, y2))
        fill = None  # a line has no inside
    else:
        sys.exit("rasterize_clippy.py: unsupported element <%s>" % tag)
    pad = width / 2.0 + 1.0
    bbox = (bbox[0] - pad, bbox[1] - pad, bbox[2] + pad, bbox[3] + pad)
    return distance, bbox, fill, stroke, width


def rasterize(view, elements, px):
    """A px-by-px list of rows of booleans, True where the pixel is black."""
    ss = SUPERSAMPLE
    side = px * ss
    scale = view / float(side)  # user units per subsample
    min_stroke = MIN_STROKE_PX * view / float(px)
    # Subsample buffer, one byte per sample: 1 white, 0 black. Painted in
    # document order, so a white eyeball really does cover the wire behind it.
    buffer = bytearray(b"\x01" * (side * side))
    for tag, attrs in elements:
        distance, bbox, fill, stroke, width = shape_for(tag, attrs, min_stroke)
        if fill is None and stroke is None:
            continue
        lo_x = max(0, int(bbox[0] / scale) - 1)
        hi_x = min(side - 1, int(bbox[2] / scale) + 1)
        lo_y = max(0, int(bbox[1] / scale) - 1)
        hi_y = min(side - 1, int(bbox[3] / scale) + 1)
        half = width / 2.0
        for sy in range(lo_y, hi_y + 1):
            v = (sy + 0.5) * scale
            row = sy * side
            for sx in range(lo_x, hi_x + 1):
                d = distance((sx + 0.5) * scale, v)
                if stroke is not None and abs(d) <= half:
                    buffer[row + sx] = 0 if stroke else 1
                elif fill is not None and d <= 0.0:
                    buffer[row + sx] = 0 if fill else 1
    # Downsample with gen_icons.py's rule rather than a majority vote: coverage
    # composited on white, black where the result is darker than THRESHOLD.
    samples = ss * ss
    rows = []
    for y in range(px):
        row = []
        for x in range(px):
            black = 0
            for sy in range(y * ss, y * ss + ss):
                base = sy * side + x * ss
                for sx in range(base, base + ss):
                    if buffer[sx] == 0:
                        black += 1
            row.append(255.0 * (1.0 - black / float(samples)) < THRESHOLD)
        rows.append(row)
    return rows


def pack(rows, px):
    """Pack to 1-bpp (MSB-first, 1=transparent, 0=black). Returns (bytes, opticalCenterY)."""
    data = []
    sum_y = 0
    count = 0
    for y in range(px):
        for xb in range(0, px, 8):
            byte = 0
            for b in range(8):
                x = xb + b
                white = 1
                if x < px and rows[y][x]:
                    white = 0
                    sum_y += y
                    count += 1
                byte |= white << (7 - b)
            data.append(byte)
    center = round(sum_y / count) if count else px // 2
    return data, center


def wrapped(prefix, values, suffix):
    """`values` laid out the way clang-format lays out a long initialiser list."""
    lines = []
    line = prefix
    indent = "    "
    for index, value in enumerate(values):
        piece = value + ("" if index == len(values) - 1 else ",")
        candidate = line + ("" if line in (prefix, indent) else " ") + piece
        if len(candidate) > COLUMN_LIMIT - (len(suffix) if index == len(values) - 1 else 0):
            lines.append(line)
            line = indent + piece
        else:
            line = candidate
    return "\n".join(lines + [line + suffix])


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--svg", required=True)
    ap.add_argument("--alias", required=True)
    ap.add_argument("--sizes", default="32,240")
    ap.add_argument("--out", required=True)
    args = ap.parse_args()
    view, elements = parse_svg(args.svg)
    sizes = [int(s) for s in args.sizes.split(",")]

    out = [
        "#pragma once",
        "",
        '#include "Icon.h"',
        "",
        "// Generated by tools_local/clippy/rasterize_clippy.py from assets_local/clippy/clippy.svg.",
        "// Do not edit. NOT the canonical generator: see tools_local/clippy/gen_clippy_art.sh,",
        "// which uses the SDK's gen_icons.py and needs librsvg. Regenerate through that one.",
        f"// Sizes: {', '.join(str(s) for s in sizes)}px. Icons: 1.",
        "",
    ]
    for px in sizes:
        data, center = pack(rasterize(view, elements, px), px)
        arr = f"icon_{args.alias}_{px}_bits"
        out.append(wrapped(f"static const uint8_t {arr}[] = {{", [f"0x{b:02X}" for b in data], "};"))
        out.append(f"static const freeink::Icon icon_{args.alias}_{px} = {{{px}, {px}, {center}, {arr}}};")
    out.append("")
    open(args.out, "w", encoding="utf-8", newline="\n").write("\n".join(out))
    print(f"wrote {args.out}: 1 icon x {len(sizes)} sizes")


if __name__ == "__main__":
    main()
