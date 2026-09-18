#!/bin/sh
# Builds TO DO's model and both screens with no device, drives the model and
# taps the screens.
#
#   host-tests/todo/run.sh
#
# Its own suite rather than lines in host-tests/ui, the way host-tests/clippy
# is: this app's rules -- a list is complete only when its items are, lists
# keep their order when their neighbours change, the rect the "..." is drawn
# in is the rect a tap is measured against -- are worth a file somebody can
# read top to bottom. Same include path as that suite, and the same reason:
# no src/, no lib/, no Arduino, so a screen builder reaching for GfxRenderer
# or the SD card fails here loudly instead of quietly becoming untestable.
set -e
cd "$(dirname "$0")"
# Keyed to this checkout, not just the suite name: two worktrees sharing one
# build dir means one tree can run -- and pass -- a binary the other built.
BUILD_DIR="${TMPDIR:-/tmp}/$(basename "${CXX:-c++}")-todo-tests-$(cd ../.. && pwd | cksum | cut -d" " -f1)"
mkdir -p "$BUILD_DIR"
SDK=../../freeink-sdk/libs/ui/FreeInkUI
ICONS=../../freeink-sdk/libs/assets/Icons
# -Wno-comment: the fork's comments wrap, and a line ending in a backslash
# inside one is not a continuation anybody meant.
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror -Wno-comment \
  -I"$SDK/include" -I"$ICONS/include" \
  "$SDK/src/FreeInkUI.cpp" \
  ../../src/apps_local/todo/TodoCore.cpp \
  ../../src/apps_local/todo/TodoScreens.cpp \
  test_todo.cpp -o "$BUILD_DIR/test_todo"
"$BUILD_DIR/test_todo"
