#!/usr/bin/env bash
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TARGET_DIR="$REPO_ROOT/user/go/.tinygo-root"
TMP_DIR="/tmp/opencode/tinygo-root"
SYS_TINYGO="$(tinygo env TINYGOROOT)"

echo "[*] System TINYGOROOT: $SYS_TINYGO"

# 1. Recreate persistent shadow TINYGOROOT inside user/go/.tinygo-root
rm -rf "$TARGET_DIR"
mkdir -p "$(dirname "$TARGET_DIR")"

if [ -d "$TMP_DIR/src" ]; then
    echo "[*] Found existing shadow root in $TMP_DIR — migrating to $TARGET_DIR..."
    cp -r "$SYS_TINYGO" "$TARGET_DIR"
    # Overlay any modified files from /tmp/opencode/tinygo-root onto the fresh copy
    cp -rf "$TMP_DIR"/* "$TARGET_DIR"/ 2>/dev/null || true
else
    echo "[*] Cloning fresh shadow root from $SYS_TINYGO to $TARGET_DIR..."
    cp -r "$SYS_TINYGO" "$TARGET_DIR"
fi

# 2. Overlay any local DragoonOS Go patches if present in user/go/patches/
if [ -d "$REPO_ROOT/user/go/patches" ]; then
    echo "[*] Applying local patches from user/go/patches/..."
    cp -rf "$REPO_ROOT/user/go/patches/"* "$TARGET_DIR"/
fi

# 3. Ensure user/go/.tinygo-root/ is ignored by Git
GITIGNORE="$REPO_ROOT/.gitignore"
touch "$GITIGNORE"
if ! grep -qxF "user/go/.tinygo-root/" "$GITIGNORE"; then
    echo "user/go/.tinygo-root/" >> "$GITIGNORE"
    echo "[*] Added user/go/.tinygo-root/ to .gitignore"
fi

# 4. Update TINYGOROOT in Makefile to point to the persistent path
MAKEFILE="$REPO_ROOT/Makefile"
if [ -f "$MAKEFILE" ] && grep -q "/tmp/opencode/tinygo-root" "$MAKEFILE"; then
    sed -i 's|/tmp/opencode/tinygo-root|$(abspath user/go/.tinygo-root)|g' "$MAKEFILE"
    echo "[*] Updated TINYGOROOT in Makefile to \$(abspath user/go/.tinygo-root)"
fi

echo "[+] Done! Persistent TINYGOROOT is ready at: $TARGET_DIR"
