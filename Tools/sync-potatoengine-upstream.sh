#!/usr/bin/env bash
# sync-potatoengine-upstream.sh <upstream-checkout-dir>
#
# Copies the canonical engine modules from THIS repo (MingGoRTS —
# source of truth) into a PotatoEngine upstream checkout. The
# upstream-mirror workflow runs this and fails if the mirror drifts.
#
# Game-layer content (Gameplay/, Campaign/, Game/, MingGoRTS_IDE/,
# _bmad-output/, assets/) never crosses — the mirror is engine-only.

set -euo pipefail

DEST="${1:?usage: $0 <path-to-PotatoEngine-checkout>}"
SRC="$(cd "$(dirname "$0")/.." && pwd)"

if [ ! -d "$DEST/.git" ]; then
    echo "error: $DEST is not a git checkout" >&2
    exit 1
fi

# Engine leaf dirs — keep aligned with upstream-mirror.yml path
# filters so CI watches exactly what this script moves.
DIRS=(
    Core Security Events FileSystem Logging MathUtils Memory Platform
    Scene Serialization Time Rendering Physics Audio Input Resources
    ECS GameObject GUI AI Quantum Simulation Networking Media Examples
)
EXTS=(
    external/glad_gen
    external/tinygltf
    external/miniaudio
)

sync_dir() {
    local rel="$1"
    if [ ! -d "$SRC/$rel" ]; then
        echo "skip  $rel (not present in source)"
        return
    fi
    rm -rf "${DEST:?}/$rel"
    mkdir -p "$DEST/$rel"
    cp -a "$SRC/$rel/." "$DEST/$rel/"
    echo "sync  $rel"
}

for d in "${DIRS[@]}"; do sync_dir "$d"; done
for d in "${EXTS[@]}"; do sync_dir "$d"; done

echo "done — review 'git status' in $DEST and commit upstream."
