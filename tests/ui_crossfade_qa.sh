#!/usr/bin/env bash
# C3 compositor checks; controlled special-mode flags are private QA only.
set -euo pipefail
if (( $# != 3 )); then
    echo "Usage: $0 ROM PALLET_9_7 READY_BATTLE" >&2
    exit 2
fi
project=$(cd -- "$(dirname -- "$0")/.." && pwd)
rom=$(realpath -- "$1")
world=$(realpath -- "$2")
battle=$(realpath -- "$3")
cmake --build "$project/build" --target all pallet_render_smoke --parallel 4
ctest --test-dir "$project/build" --output-on-failure
qa_dir=$(mktemp -d "$project/build/qa/ui-crossfade-XXXXXX")
mkdir -p "$qa_dir/roms" "$qa_dir/logs"
cp -- "$rom" "$qa_dir/roms/pokeyellow.gbc"
cp -- "$world" "$qa_dir/world.state"
cp -- "$battle" "$qa_dir/battle.state"
cp -- "$project/build/pallet_render_smoke" "$qa_dir/pallet_render_smoke"
cd -- "$qa_dir"
trap 'echo "QA interrupted; inspect $qa_dir" >&2' ERR
sha256sum roms/pokeyellow.gbc world.state battle.state > logs/inputs.sha256
export SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy
echo "QA output: $qa_dir"
for scene in world battle warp; do
    for camera in ortho fp; do
        mode=crossfade
        fixture=$scene
        if [[ $scene == warp ]]; then mode=crossfade-warp; fixture=world; fi
        if [[ $camera == fp ]]; then mode=$mode-fp; fi
        name=$scene-$camera
        mkdir -p "$name/logs"
        echo "Checking $name"
        (cd "$name"; ../pallet_render_smoke ../roms/pokeyellow.gbc "../$fixture.state" "$mode" > logs/run.log 2>&1)
    done
done
sha256sum --check logs/inputs.sha256
printf 'PASS: complete-frame crossfades, pixel checks, reversal, focus/Esc, world/menus/battle/warp and unsupported modes; evidence in %s\n' "$qa_dir" | tee logs/result.txt
