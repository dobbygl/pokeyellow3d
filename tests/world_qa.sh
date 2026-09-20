#!/usr/bin/env bash
# All engine runs happen beside a copied executable and private save fixtures.
set -euo pipefail
if (( $# < 2 )); then
    echo "Usage: $0 ROM PALLET_9_7_WITH_PARTY [PALLET_9_8]" >&2
    exit 2
fi
project=$(cd -- "$(dirname -- "$0")/.." && pwd)
rom=$(realpath -- "$1")
fixture=$(realpath -- "$2")
route_fixture=""
if (( $# > 2 )); then route_fixture=$(realpath -- "$3"); fi
cmake --build "$project/build" --target pokeyellow_launcher interior_test interior_audit battle_state_test fade_state_test firstperson_test pallet_render_smoke pallet_state_test kanto_rom_audit kanto_geometry_audit --parallel 4
ctest --test-dir "$project/build" --output-on-failure
mkdir -p "$project/build/qa"
qa_dir=$(mktemp -d "$project/build/qa/kanto-XXXXXX")
mkdir -p "$qa_dir/roms" "$qa_dir/logs/catalog"
cp -- "$rom" "$qa_dir/roms/pokeyellow.gbc"
cp -- "$fixture" "$qa_dir/progress.state"
cp -- "$project/build/pallet_render_smoke" "$qa_dir/pallet_render_smoke"
if [[ -n $route_fixture ]]; then cp -- "$route_fixture" "$qa_dir/house.state"; fi
cd -- "$qa_dir"
trap 'echo "QA interrupted; inspect $qa_dir/logs" >&2' ERR
echo "QA output: $qa_dir"
sha256sum roms/pokeyellow.gbc progress.state > logs/inputs.sha256
export SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy
run() {
    local name=$1
    shift
    echo "Checking $name"
    ./pallet_render_smoke roms/pokeyellow.gbc "$@" > "logs/$name.log" 2>&1
}
"$project/build/kanto_rom_audit" roms/pokeyellow.gbc > logs/maps.csv
"$project/build/kanto_geometry_audit" roms/pokeyellow.gbc > logs/geometry.csv
JOURNEY_CITY_STATE=viridian.state run journey progress.state viridian
SMOKE_CAPTURE=logs/viridian-camera.ppm run camera viridian.state camera
run horizontal viridian.state prepare horizontal horizontal.state
if [[ -n $route_fixture ]]; then run house house.state route; fi
run cut-prepare viridian.state prepare cut cut-ready.state
SMOKE_CAPTURE=logs/cut-after.ppm run cut-action cut-ready.state play '20:S:4,90:A:4,160:A:4,230:A:4,350:A:4,450:A:4,600:A:4' 750 cut-done.state
run cut-reload cut-ready.state reload cut-done.state
run bike-prepare progress.state prepare bike bike-ready.state
run bike-menu bike-ready.state play '20:S:4,200:D:4' 300 bike-menu.state
run bike-select bike-menu.state play '20:D:16,100:A:16' 250 bike-select.state
SMOKE_CAPTURE=logs/bike.ppm run bike-use bike-select.state play '20:A:16,150:A:16,300:A:16,450:B:16,550:B:16,700:U:40' 800 bike-active.state
run bike-movement bike-active.state prepare transport bike-verified.state
run surf-prepare progress.state prepare surf surf-ready.state
run surf-start surf-ready.state play '20:S:12' 110 surf-start.state
SMOKE_CAPTURE=logs/surf.ppm run surf-use surf-start.state play '20:U:12,80:U:12,140:U:12,200:U:12,260:U:12,350:A:12,500:A:12,650:A:12,820:A:12' 1000 surf-active.state
run surf-movement surf-active.state prepare transport surf-verified.state
run forest-setup progress.state warp 51 2 forest.state
run forest-return forest.state play '20:U:35,100:D:100,350:U:120' 550 forest-return.state
rg -q '51 -> 50' logs/forest-return.log
rg -q '50 -> 51' logs/forest-return.log
rg -q '\[PLAY\] map=51 ' logs/forest-return.log
run vermilion-setup progress.state warp 5 5 vermilion.state
run dock-setup vermilion.state warp 94 0 dock.state
run ticket dock.state prepare ticket dock-ticket.state
run dock-return dock-ticket.state play '20:D:8,100:U:100,350:D:80,500:A:4,600:A:4,700:A:4,800:D:100' 1000 ship.state
rg -q '94 -> 5' logs/dock-return.log
rg -q '5 -> 94' logs/dock-return.log
rg -q '94 -> 95' logs/dock-return.log
CATALOG_BENCH_FRAMES=30 CATALOG_CAPTURE_DIR=logs/catalog run catalog progress.state catalog
printf 'PASS: all Kanto QA scenarios; evidence in %s\n' "$qa_dir" | tee logs/result.txt
