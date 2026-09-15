#!/bin/sh
# Builds Clippy Facts' one screen with no device and taps it.
#
#   host-tests/clippy/run.sh
#
# Its own suite rather than lines in host-tests/ui, which already compiles
# thirty screen files: this app's rule is that the rect Clippy is DRAWN in is
# the rect a tap is measured against, and that is worth a file somebody can read
# top to bottom. Same include path as that suite, and the same reason for it --
# no src/, no lib/, no Arduino, so a screen builder reaching for GfxRenderer or
# the SD card fails here loudly instead of quietly becoming untestable.
#
# lib/GfxRenderer is not on the include path either. The test includes
# RevealedInteractions.h and PaintClock.h by relative path (both are
# header-only, and PaintClock is what decides whether a tap is allowed to
# route at all); nothing else from lib/ is reachable.
set -e
cd "$(dirname "$0")"
# Keyed to this checkout, not just the suite name: two worktrees sharing one
# build dir means one tree can run -- and pass -- a binary the other built.
BUILD_DIR="${TMPDIR:-/tmp}/$(basename "${CXX:-c++}")-clippy-tests-$(cd ../.. && pwd | cksum | cut -d" " -f1)"
mkdir -p "$BUILD_DIR"
SDK=../../freeink-sdk/libs/ui/FreeInkUI
ICONS=../../freeink-sdk/libs/assets/Icons
# -Wno-comment: the fork's comments wrap, and a line ending in a backslash
# inside one is not a continuation anybody meant.
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -Wno-comment \
  -I"$SDK/include" -I"$ICONS/include" \
  "$SDK/src/FreeInkUI.cpp" \
  ../../src/apps_local/clippy/ClippyFactsCore.cpp \
  ../../src/apps_local/clippy/ClippyFactsScreens.cpp \
  test_clippy.cpp -o "$BUILD_DIR/test_clippy"
"$BUILD_DIR/test_clippy"
