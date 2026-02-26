#!/bin/bash
# Apply Luau VM patches for Lua 5.1 type constant compatibility.
#
# This script reorders LUA_TVECTOR in Luau's type enum so that
# NIL=0, BOOLEAN=1, LIGHTUSERDATA=2, NUMBER=3, STRING=4, TABLE=5,
# FUNCTION=6, USERDATA=7, THREAD=8 — exactly matching Lua 5.1.
#
# Must be run from the repository root (where luau/ directory exists).
# After running, rebuild Luau: make -C luau clean && make -C luau

set -euo pipefail

LUAU_DIR="${1:-luau}"

LUA_H="$LUAU_DIR/VM/include/lua.h"
LOBJECT_H="$LUAU_DIR/VM/src/lobject.h"
LTM_CPP="$LUAU_DIR/VM/src/ltm.cpp"

for f in "$LUA_H" "$LOBJECT_H" "$LTM_CPP"; do
    [ -f "$f" ] || { echo "ERROR: $f not found"; exit 1; }
done

# 1. lua.h: move LUA_TVECTOR from before LUA_TSTRING to after LUA_TBUFFER
if grep -q 'LUA_TVECTOR,' "$LUA_H" && grep -q 'LUA_TNUMBER,' "$LUA_H"; then
    sed -i '/^    LUA_TVECTOR,$/d' "$LUA_H"
    sed -i '/LUA_TBUFFER,/a\
\n    LUA_TVECTOR, // moved after BUFFER to match Lua 5.1 type constants; iscollectable excludes this explicitly' "$LUA_H"
    echo "Patched $LUA_H"
fi

# 2. lobject.h: fix iscollectable to exclude VECTOR
sed -i 's/#define iscollectable(o) (ttype(o) >= LUA_TSTRING)/#define iscollectable(o) (ttype(o) >= LUA_TSTRING \&\& ttype(o) != LUA_TVECTOR)/' "$LOBJECT_H"
echo "Patched $LOBJECT_H"

# 3. ltm.cpp: move "vector" entry in luaT_typenames to after "buffer"
sed -i '/"vector",/d' "$LTM_CPP"
sed -i '/"buffer",/a\\n    "vector",' "$LTM_CPP"
echo "Patched $LTM_CPP"

echo "Done. Rebuild Luau: make -C $LUAU_DIR clean && make -C $LUAU_DIR"
