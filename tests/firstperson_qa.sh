#!/usr/bin/env bash
# Integration runs use private copies, never the user's battery save.
set -euo pipefail
if (( $# != 4 )); then
    echo "Usage: $0 ROM PALLET_9_7_WITH_PARTY PALLET_9_8 ROUTE1_10_28" >&2
    exit 2
fi
project=$(cd -- "$(dirname -- "$0")/.." && pwd)
rom=$(realpath -- "$1")
pallet=$(realpath -- "$2")
house=$(realpath -- "$3")
route=$(realpath -- "$4")
cmake --build "$project/build" --target pokeyellow_launcher interior_test interior_audit battle_state_test pallet_render_smoke firstperson_test pallet_state_test kanto_rom_audit kanto_geometry_audit --parallel 4
ctest --test-dir "$project/build" --output-on-failure
qa_dir=$(mktemp -d "$project/build/qa/firstperson-XXXXXX")
mkdir -p "$qa_dir/roms" "$qa_dir/logs"
cp -- "$rom" "$qa_dir/roms/pokeyellow.gbc"
cp -- "$pallet" "$qa_dir/pallet.state"
cp -- "$house" "$qa_dir/house.state"
cp -- "$route" "$qa_dir/route.state"
cp -- "$project/build/pallet_render_smoke" "$qa_dir/pallet_render_smoke"
cd -- "$qa_dir"
trap 'echo "QA interrupted; inspect $qa_dir/logs" >&2' ERR
echo "QA output: $qa_dir"
sha256sum roms/pokeyellow.gbc pallet.state house.state route.state > logs/inputs.sha256
export SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy FP_CAPTURE_DIR=logs
run() {
    local name=$1
    shift
    echo "Checking $name"
    ./pallet_render_smoke roms/pokeyellow.gbc "$@" > "logs/$name.log" 2>&1
}
run journey-ortho pallet.state journey
run journey-fp pallet.state firstperson
diff <(rg 'final_wram=|PASS: Pallet' logs/journey-ortho.log) <(rg 'final_wram=|PASS: Pallet' logs/journey-fp.log)
FP_RECORD=logs/relative-input.txt run controls pallet.state fp-controls
for direction in U D L R; do
    rg -q "c[0-9]+:$direction:[0-9]+" logs/relative-input.txt
done
run house house.state fp-house
run pallet-views pallet.state fp-views
run route-views route.state fp-views
rg -q '\[SMOKE\] view=3 map=12 xy=10,28' logs/route-views.log
# Only fixture setup requests a scripted engine warp; benchmark rendering is
# read-only and measures the loaded Saffron scene plus its four neighbors.
run five-maps pallet.state warp 10 0 saffron.state
SMOKE_CAPTURE=logs/five-maps-fp.ppm run benchmark-fp saffron.state fp-benchmark
SMOKE_CAPTURE=logs/five-maps-ortho.ppm run benchmark-ortho saffron.state ortho-benchmark
sha256sum --check logs/inputs.sha256
printf 'PASS: first-person journey, script parity, SDL controls, house, eight views and five-map benchmark; evidence in %s\n' "$qa_dir" | tee logs/result.txt
