#!/usr/bin/env bash
# Real capture and trainer chain; controlled move fixtures stay in this copy.
set -euo pipefail
if (( $# != 3 )); then
    echo "Usage: $0 ROM ROUTE1_10_4_WITH_BOUGHT_BALLS ROUTE1_10_28" >&2
    exit 2
fi
project=$(cd -- "$(dirname -- "$0")/.." && pwd)
rom=$(realpath -- "$1")
town=$(realpath -- "$2")
route=$(realpath -- "$3")
cmake --build "$project/build" --target pokeyellow_launcher interior_test interior_audit battle_state_test firstperson_test pallet_state_test kanto_rom_audit kanto_geometry_audit pallet_render_smoke --parallel 4
ctest --test-dir "$project/build" --output-on-failure
qa_dir=$(mktemp -d "$project/build/qa/battles-XXXXXX")
mkdir -p "$qa_dir/roms" "$qa_dir/logs"
cp -- "$rom" "$qa_dir/roms/pokeyellow.gbc"
cp -- "$town" "$qa_dir/town.state"
cp -- "$route" "$qa_dir/route.state"
cp -- "$project/build/pallet_render_smoke" "$qa_dir/pallet_render_smoke"
cd -- "$qa_dir"
trap 'echo "QA interrupted; inspect $qa_dir/logs" >&2' ERR
echo "QA output: $qa_dir"
sha256sum roms/pokeyellow.gbc town.state route.state > logs/inputs.sha256
export SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy
./pallet_render_smoke roms/pokeyellow.gbc route.state battle-menus >logs/menus.log 2>&1
./pallet_render_smoke roms/pokeyellow.gbc town.state battle3d >logs/battle3d.log 2>&1
sha256sum --check logs/inputs.sha256
printf 'PASS: B1/B2 battle integration; evidence in %s\n' "$qa_dir" | tee logs/result.txt
