#!/usr/bin/env bash
# C1: private real-engine door/stair/elevator/white-fade and load regressions.
set -euo pipefail
if (( $# != 4 )); then
    echo "Usage: $0 ROM PALLET_9_8 MART_1F_2_7 ROUTE1_10_28" >&2
    exit 2
fi
project=$(cd -- "$(dirname -- "$0")/.." && pwd)
rom=$(realpath -- "$1")
house=$(realpath -- "$2")
mart=$(realpath -- "$3")
route=$(realpath -- "$4")
cmake --build "$project/build" --target all pallet_render_smoke --parallel 4
ctest --test-dir "$project/build" --output-on-failure
qa_dir=$(mktemp -d "$project/build/qa/ui-transitions-XXXXXX")
mkdir -p "$qa_dir/roms" "$qa_dir/logs"
cp -- "$rom" "$qa_dir/roms/pokeyellow.gbc"
cp -- "$house" "$qa_dir/house.state"
cp -- "$mart" "$qa_dir/mart.state"
cp -- "$route" "$qa_dir/route.state"
cp -- "$project/build/pallet_render_smoke" "$qa_dir/pallet_render_smoke"
cd -- "$qa_dir"
trap 'echo "QA interrupted; inspect $qa_dir" >&2' ERR
sha256sum roms/pokeyellow.gbc house.state mart.state route.state > logs/inputs.sha256
export SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy
echo "QA output: $qa_dir"
run() {
    local name=$1 fixture=$2 mode=$3
    shift 3
    mkdir -p "$name/logs"
    echo "Checking $name"
    (cd "$name"; UI_TRACE=logs/bgp.csv UI_VIDEO=1 INTERIOR_CAPTURE_DIR=logs \
        ../pallet_render_smoke ../roms/pokeyellow.gbc "../$fixture" "$mode" "$@" > logs/run.log 2>&1)
}
run house house.state transitions
run house-fp house.state transitions-fp
run mart mart.state transitions
run mart-fp mart.state transitions-fp
run white route.state transitions-white
run reload house.state transition-reload ../house/logs/reds-house-1f.state
run reload-fp house.state transition-reload-fp ../house/logs/reds-house-1f.state
python3 "$project/tests/check_ui_traces.py" {house,house-fp,mart,mart-fp,white}/logs/bgp.csv | tee logs/traces.txt
sha256sum --check logs/inputs.sha256
printf 'PASS: C1 doors, stairs, elevator, white return and savestate fade; evidence in %s\n' "$qa_dir" | tee logs/result.txt
