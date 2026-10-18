#!/usr/bin/env bash
# sync-potatoengine-upstream.sh <upstream-checkout-dir>
#
# Copies the canonical engine modules from THIS repo (ChinaWarlords —
# source of truth) into a PotatoEngine upstream checkout. The
# upstream-mirror workflow runs this and fails if the mirror drifts.
#
# Game-layer content (Gameplay/, Campaign/, Game/, MingGoRTS_IDE/,
# Examples/MingGoRTS/, _bmad-output/, assets/) never crosses; the mirror
# copies only engine examples from Examples/.

set -euo pipefail

DEST="${1:?usage: $0 <path-to-PotatoEngine-checkout>}"
SRC="$(cd "$(dirname "$0")/.." && pwd)"

if ! git -C "$DEST" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
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
    local preserved_readme=""
    if [ ! -d "$SRC/$rel" ]; then
        echo "skip  $rel (not present in source)"
        return
    fi
    # This repository has historically tracked this project-owned note in the
    # mirror only. Preserve it when a source checkout (including CI) lacks it.
    if [ "$rel" = "external/glad_gen" ] &&
       [ ! -f "$SRC/$rel/README.md" ] &&
       [ -f "$DEST/$rel/README.md" ]; then
        preserved_readme="$(mktemp)"
        cp "$DEST/$rel/README.md" "$preserved_readme"
    fi
    rm -rf "${DEST:?}/$rel"
    mkdir -p "$DEST/$rel"
    cp -a "$SRC/$rel/." "$DEST/$rel/"
    if [ "$rel" = "Examples" ]; then
        # MingGoRTS-only tests stay in this repository, outside the engine mirror.
        rm -rf "${DEST:?}/Examples/MingGoRTS"
    fi
    if [ -n "$preserved_readme" ]; then
        cp "$preserved_readme" "$DEST/$rel/README.md"
        rm -f "$preserved_readme"
    fi
    echo "sync  $rel"
}

for d in "${DIRS[@]}"; do sync_dir "$d"; done
for d in "${EXTS[@]}"; do sync_dir "$d"; done

echo "done — review 'git status' in $DEST and commit upstream."
