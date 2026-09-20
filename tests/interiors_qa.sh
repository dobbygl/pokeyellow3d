#!/usr/bin/env bash
# Reproducible A1/A2 gate. ROM and saves stay in a disposable private directory.
set -euo pipefail
if (( $# != 4 )); then
    echo "Usage: $0 ROM PALLET_9_7_WITH_PARTY PALLET_9_8 VIRIDIAN_20_33" >&2
    exit 2
fi
project=$(cd -- "$(dirname -- "$0")/.." && pwd)
rom=$(realpath -- "$1")
pallet=$(realpath -- "$2")
house=$(realpath -- "$3")
city=$(realpath -- "$4")
cmake --build "$project/build" --target pokeyellow_launcher interior_test interior_audit battle_state_test firstperson_test pallet_state_test kanto_rom_audit kanto_geometry_audit pallet_render_smoke --parallel 4
ctest --test-dir "$project/build" --output-on-failure
qa_dir=$(mktemp -d "$project/build/qa/interiors-XXXXXX")
mkdir -p "$qa_dir/roms" "$qa_dir/logs/catalog"
cp -- "$rom" "$qa_dir/roms/pokeyellow.gbc"
cp -- "$pallet" "$qa_dir/pallet.state"
cp -- "$house" "$qa_dir/house.state"
cp -- "$city" "$qa_dir/city.state"
cp -- "$project/build/pallet_render_smoke" "$qa_dir/pallet_render_smoke"
cd -- "$qa_dir"
trap 'echo "QA interrupted; inspect $qa_dir/logs" >&2' ERR
echo "QA output: $qa_dir"
sha256sum roms/pokeyellow.gbc pallet.state house.state city.state > logs/inputs.sha256
export SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy INTERIOR_CAPTURE_DIR=logs
run() {
    local name=$1
    shift
    echo "Checking $name"
    ./pallet_render_smoke roms/pokeyellow.gbc "$@" > "logs/$name.log" 2>&1
}
"$project/build/interior_audit" roms/pokeyellow.gbc > logs/interiors.csv
run house house.state interior
run house-fp house.state interior-fp
run town city.state town
run mart-prepare pallet.state warp 122 0 mart-start.state
run elevator mart-start.state interior-transitions
run cave-prepare pallet.state warp 46 0 cave-start.state
run cave cave-start.state interior-transitions
CATALOG_CAPTURE_DIR=logs/catalog run catalog pallet.state interior-catalog
sha256sum --check logs/inputs.sha256
printf 'PASS: A1/A2 interiors, town, stairs, elevator, cave and 179 meshes; evidence in %s\n' "$qa_dir" | tee logs/result.txt
