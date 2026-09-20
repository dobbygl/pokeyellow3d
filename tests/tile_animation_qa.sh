#!/usr/bin/env bash
# User ROM and saves stay private. Only fixture setup requests scripted warps;
# all animation comes from the original engine, with per-frame memory guards.
set -euo pipefail
if (( $# != 2 )); then
    echo "Usage: $0 ROM PALLET_STATE" >&2
    exit 2
fi
project=$(cd -- "$(dirname -- "$0")/.." && pwd)
rom=$(realpath -- "$1")
state=$(realpath -- "$2")
cmake --build "$project/build" --target all pallet_render_smoke --parallel 4
qa_dir=$(mktemp -d "$project/build/qa/tile-animation-XXXXXX")
mkdir -p "$qa_dir/roms" "$qa_dir/logs"
cp -- "$rom" "$qa_dir/roms/pokeyellow.gbc"
cp -- "$state" "$qa_dir/pallet.state"
cp -- "$project/build/pallet_render_smoke" "$qa_dir/pallet_render_smoke"
cd -- "$qa_dir"
trap 'echo "QA interrupted; inspect $qa_dir" >&2' ERR
sha256sum roms/pokeyellow.gbc pallet.state > logs/inputs.sha256
export SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy
echo "QA output: $qa_dir"
# Every animated tileset and one disabled interior; no licensed fixture is tracked.
for map in 0 51 40 65 95 94 59 83 9 37; do
    if (( map == 0 )); then
        cp pallet.state map-0.state
    else
        ./pallet_render_smoke roms/pokeyellow.gbc pallet.state warp "$map" 0 "map-$map.state" > "logs/warp-$map.log" 2>&1
    fi
    for camera in tile-animation tile-animation-fp; do
        mkdir -p "$map-$camera/logs"
        echo "Checking map=$map camera=$camera"
        (cd "$map-$camera"; ../pallet_render_smoke ../roms/pokeyellow.gbc "../map-$map.state" "$camera" > logs/run.log 2>&1)
    done
done
sha256sum --check logs/inputs.sha256
printf 'PASS: all animated tilesets and static interior, original cadence, VRAM, GPU, pause and reload in both cameras; evidence in %s\n' "$qa_dir" | tee logs/result.txt
