#!/bin/bash
# Regenerate src/apps_local/mylibrary/LibraryIcons.h from
# tools_local/mylibrary/icons.txt.
#
#   apt install librsvg2-bin      # rsvg-convert, the only external dependency
#   ./tools_local/mylibrary/gen_library_icons.sh
#
# The same generator the shelf icons use, pointed at this app's manifest and
# writing into this app's folder. The output is committed, because
# regenerating needs librsvg and a checkout should build without it.
set -euo pipefail
REPO="$(cd "$(dirname "$(readlink -f "${BASH_SOURCE[0]}")")/../.." && pwd)"
cd "$REPO"
python3 freeink-sdk/libs/assets/Icons/tools/gen_icons.py \
  --manifest tools_local/mylibrary/icons.txt \
  --svgdir freeink-sdk/libs/assets/Icons/lucide/icons \
  --sizes 24,32 \
  --out src/apps_local/mylibrary/LibraryIcons.h
if command -v clang-format-21 >/dev/null 2>&1; then
  clang-format-21 -style=file -i src/apps_local/mylibrary/LibraryIcons.h
fi
echo "wrote src/apps_local/mylibrary/LibraryIcons.h"
