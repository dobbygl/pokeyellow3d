#!/usr/bin/env bash
# Natural Pidgey capture plus original PC transactions. Stress cases explicitly
# clone that record in a separate private context; user saves are never written.
set -euo pipefail
if (( $# != 2 )); then
    echo "Usage: $0 ROM ROUTE1_10_4_WITH_BOUGHT_BALLS_STATE" >&2
    exit 2
fi
project=$(cd -- "$(dirname -- "$0")/.." && pwd)
rom=$(realpath -- "$1")
state=$(realpath -- "$2")
cmake --build "$project/build" --target all pallet_render_smoke --parallel 4
ctest --test-dir "$project/build" --output-on-failure
qa_dir=$(mktemp -d "$project/build/qa/pc-storage-XXXXXX")
mkdir -p "$qa_dir/roms" "$qa_dir/logs" "$qa_dir/capture/logs"
cp -- "$rom" "$qa_dir/roms/pokeyellow.gbc"
cp -- "$state" "$qa_dir/town.state"
cp -- "$project/build/pallet_render_smoke" "$qa_dir/pallet_render_smoke"
cd -- "$qa_dir"
trap 'echo "QA interrupted; inspect $qa_dir" >&2' ERR
sha256sum roms/pokeyellow.gbc town.state > logs/inputs.sha256
export SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy
echo "QA output: $qa_dir"
(cd capture; ../pallet_render_smoke ../roms/pokeyellow.gbc ../town.state capture-pidgey > logs/run.log 2>&1)
./pallet_render_smoke roms/pokeyellow.gbc capture/logs/capture-complete.state warp 41 0 center.state > logs/prepare-center.log 2>&1
for mode in pc-storage pc-storage-fp pc-storage-stress pc-storage-stress-fp; do
    mkdir -p "$mode/logs"
    echo "Checking $mode"
    (cd "$mode"; ../pallet_render_smoke ../roms/pokeyellow.gbc ../center.state "$mode" > logs/run.log 2>&1)
done
mkdir -p "$project/build/qa/pc"
cp pc-storage/logs/boxes.wram "$project/build/qa/pc/boxes.wram"
cp pc-storage/logs/boxes.sram "$project/build/qa/pc/boxes.sram"
ctest --test-dir "$project/build" --output-on-failure > logs/ctest-final.log
sha256sum --check logs/inputs.sha256
printf 'PASS: natural Pidgey capture, deposit/withdraw/release, verified SRAM boxes, twenty-entry scroll and corruption fallback in both cameras; evidence in %s\n' "$qa_dir" | tee logs/result.txt
