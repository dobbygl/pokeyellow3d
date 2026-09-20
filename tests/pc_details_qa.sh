#!/usr/bin/env bash
# All writes belong to disposable private fixtures; rendering is checked read-only.
set -euo pipefail
if (( $# != 2 )); then
    echo "Usage: $0 ROM CENTER_ENTRANCE_STATE_WITH_TWO_POKEMON_AND_BALLS" >&2
    exit 2
fi
project=$(cd -- "$(dirname -- "$0")/.." && pwd)
rom=$(realpath -- "$1")
state=$(realpath -- "$2")
cmake --build "$project/build" --target all pallet_render_smoke --parallel 4
qa_dir=$(mktemp -d "$project/build/qa/pc-details-XXXXXX")
mkdir -p "$qa_dir/roms" "$qa_dir/logs"
cp -- "$rom" "$qa_dir/roms/pokeyellow.gbc"
cp -- "$state" "$qa_dir/center.state"
cp -- "$project/build/pallet_render_smoke" "$qa_dir/pallet_render_smoke"
cd -- "$qa_dir"
trap 'echo "QA interrupted; inspect $qa_dir" >&2' ERR
sha256sum roms/pokeyellow.gbc center.state > logs/inputs.sha256
export SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy
echo "QA output: $qa_dir"
for mode in pc-details pc-details-fp pc pc-fp; do
    mkdir -p "$mode/logs"
    echo "Checking $mode"
    (cd "$mode"; ../pallet_render_smoke ../roms/pokeyellow.gbc ../center.state "$mode" > logs/run.log 2>&1)
done
sha256sum --check logs/inputs.sha256
printf 'PASS: item deposit/withdraw/toss, Oak counters and complete PC with original-engine champion SRAM in both cameras; evidence in %s\n' "$qa_dir" | tee logs/result.txt
