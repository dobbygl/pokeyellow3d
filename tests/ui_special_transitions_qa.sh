#!/usr/bin/env bash
# C3: original field moves, transport, load pause and stale battle backgrounds.
set -euo pipefail
if (( $# != 4 )); then
    echo "Usage: $0 ROM PALLET_9_7 VIRIDIAN_20_33 ROUTE1_10_28" >&2
    exit 2
fi
project=$(cd -- "$(dirname -- "$0")/.." && pwd)
rom=$(realpath -- "$1")
pallet=$(realpath -- "$2")
city=$(realpath -- "$3")
route=$(realpath -- "$4")
cmake --build "$project/build" --target all pallet_render_smoke --parallel 4
ctest --test-dir "$project/build" --output-on-failure
qa_dir=$(mktemp -d "$project/build/qa/ui-special-transitions-XXXXXX")
mkdir -p "$qa_dir/roms" "$qa_dir/logs"
cp -- "$rom" "$qa_dir/roms/pokeyellow.gbc"
cp -- "$pallet" "$qa_dir/pallet.state"
cp -- "$city" "$qa_dir/city.state"
cp -- "$route" "$qa_dir/route.state"
cp -- "$project/build/pallet_render_smoke" "$qa_dir/pallet_render_smoke"
cd -- "$qa_dir"
trap 'echo "QA interrupted; inspect $qa_dir" >&2' ERR
sha256sum roms/pokeyellow.gbc pallet.state city.state route.state > logs/inputs.sha256
export SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy
echo "QA output: $qa_dir"
# Only preparation grants field moves/items or requests engine-scripted warps.
# All observed travel, menu navigation and encounters use original inputs.
./pallet_render_smoke roms/pokeyellow.gbc pallet.state warp 46 0 cave.state > logs/prepare-cave.log 2>&1
./pallet_render_smoke roms/pokeyellow.gbc pallet.state warp 37 0 house.state > logs/prepare-house.log 2>&1
for mode in bike surf; do
    ./pallet_render_smoke roms/pokeyellow.gbc pallet.state prepare "$mode" "$mode.state" > "logs/prepare-$mode.log" 2>&1
done
run() {
    local name=$1 fixture=$2 mode=$3
    shift 3
    mkdir -p "$name/logs"
    echo "Checking $name"
    (cd "$name"; ../pallet_render_smoke ../roms/pokeyellow.gbc "../$fixture.state" "$mode" "$@" > logs/run.log 2>&1)
}
for camera in ortho fp; do
    suffix=""
    if [[ $camera == fp ]]; then suffix=-fp; fi
    run "fly-$camera" city "travel$suffix" fly
    run "teleport-$camera" city "travel$suffix" teleport
    run "dig-$camera" cave "travel$suffix" dig
    run "bike-$camera" bike "transport$suffix" bike
    run "surf-$camera" surf "transport$suffix" surf
    run "load-pause-$camera" pallet "load-pause$suffix" ../house.state
    run "stale-map-$camera" pallet "stale-battle$suffix"
    run "stale-position-$camera" route "stale-battle$suffix"
done
sha256sum --check logs/inputs.sha256
printf 'PASS: C3 Fly, Teleport, Dig, bike/surf, load pause and stale battle scenes in both cameras; evidence in %s\n' "$qa_dir" | tee logs/result.txt
