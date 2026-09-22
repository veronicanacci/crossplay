#!/bin/sh
# Builds MY LIBRARY's model with no device and drives it: search normalisation,
# grouping and filtering, tag parsing, the personal state file's round trip,
# BookBuddy's export read, and the two updates run against a library.
#
#   host-tests/mylibrary/run.sh
#
# The model is freestanding (<string>, <vector> and nothing else), so this
# needs no SDK, no src/ and no lib/ on the include path: a model reaching for
# the renderer or the card fails here loudly instead of quietly becoming
# untestable.
set -e
cd "$(dirname "$0")"
# Keyed to this checkout, not just the suite name: two worktrees sharing one
# build dir means one tree can run -- and pass -- a binary the other built.
BUILD_DIR="${TMPDIR:-/tmp}/$(basename "${CXX:-c++}")-mylibrary-tests-$(cd ../.. && pwd | cksum | cut -d" " -f1)"
mkdir -p "$BUILD_DIR"
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror \
  ../../src/apps_local/mylibrary/LibraryCore.cpp \
  test_mylibrary.cpp -o "$BUILD_DIR/test_mylibrary"
"$BUILD_DIR/test_mylibrary"
"${CXX:-c++}" -std=c++17 -Wall -Wextra -Werror \
  ../../src/apps_local/mylibrary/LibraryCore.cpp \
  ../../src/apps_local/mylibrary/BookBuddyImport.cpp \
  ../../src/apps_local/mylibrary/LibraryUpdate.cpp \
  test_import.cpp -o "$BUILD_DIR/test_import"
"$BUILD_DIR/test_import"
