#!/usr/bin/env bash
# All fixtures are user-owned copies. The NPC script is prepared only inside
# the smoke helper; the renderer reads live engine state without writing it.
set -euo pipefail
if (( $# != 4 )); then
    echo "Usage: $0 ROM PALLET_9_7 ROUTE1_10_28 PALLET_SURF_6_14" >&2
    exit 2
fi
project=$(cd -- "$(dirname -- "$0")/.." && pwd)
rom=$(realpath -- "$1")
pallet=$(realpath -- "$2")
route=$(realpath -- "$3")
surf=$(realpath -- "$4")
cmake --build "$project/build" --target all pallet_render_smoke --parallel 4
qa_dir=$(mktemp -d "$project/build/qa/world-animation-XXXXXX")
mkdir -p "$qa_dir/roms" "$qa_dir/logs"
cp -- "$rom" "$qa_dir/roms/pokeyellow.gbc"
cp -- "$pallet" "$qa_dir/pallet.state"
cp -- "$route" "$qa_dir/route.state"
cp -- "$surf" "$qa_dir/surf.state"
cp -- "$project/build/pallet_render_smoke" "$qa_dir/pallet_render_smoke"
cd -- "$qa_dir"
trap 'echo "QA interrupted; inspect $qa_dir" >&2' ERR
sha256sum roms/pokeyellow.gbc pallet.state route.state surf.state > logs/inputs.sha256
export SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy
echo "QA output: $qa_dir"
for camera in ortho fp; do
    suffix=""
    if [[ $camera == fp ]]; then suffix=-fp; fi
    for kind in npc grass surf; do
        state=pallet
        if [[ $kind == grass ]]; then state=route; fi
        if [[ $kind == surf ]]; then state=surf; fi
        name="$kind-$camera"
        mkdir -p "$name/logs"
        echo "Checking $name"
        (cd -- "$name"; ../pallet_render_smoke ../roms/pokeyellow.gbc "../$state.state" \
            "world-animation-$kind$suffix" > logs/run.log 2>&1)
        if [[ $kind != npc ]]; then
            cmp "$name/logs/effects-on.state" "$name/logs/effects-off.state"
        fi
    done
done
sha256sum --check logs/inputs.sha256
printf 'PASS: original NPC frames outside LCD, bounded wind/grass/surf effects, pause/load/map reset, 1200 complete engine snapshots identical on/off; evidence in %s\n' "$qa_dir" | tee logs/result.txt
