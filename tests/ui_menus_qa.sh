#!/usr/bin/env bash
# Private original-engine menus. The fixture must have the Pokedex, completed
# parcel quest, Pikachu plus a caught second Pokemon, and at least 200 money.
set -euo pipefail
if (( $# != 2 )); then
    echo "Usage: $0 ROM TWO_POKEMON_WORLD_STATE" >&2
    exit 2
fi
project=$(cd -- "$(dirname -- "$0")/.." && pwd)
rom=$(realpath -- "$1")
source_state=$(realpath -- "$2")
cmake --build "$project/build" --target all pallet_render_smoke --parallel 4
ctest --test-dir "$project/build" --output-on-failure
qa_dir=$(mktemp -d "$project/build/qa/ui-menus-XXXXXX")
mkdir -p "$qa_dir/roms" "$qa_dir/logs"
cp -- "$rom" "$qa_dir/roms/pokeyellow.gbc"
cp -- "$source_state" "$qa_dir/source.state"
cp -- "$project/build/pallet_render_smoke" "$qa_dir/pallet_render_smoke"
cd -- "$qa_dir"
trap 'echo "QA interrupted; inspect $qa_dir" >&2' ERR
sha256sum roms/pokeyellow.gbc source.state > logs/inputs.sha256
export SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy
echo "QA output: $qa_dir"
# Outdoor scripted warps preserve the source coordinates. Enter Pallet through
# the real house exit so the new fixture has a valid position/collision buffer.
./pallet_render_smoke roms/pokeyellow.gbc source.state warp 0 0 pallet-parent.state > logs/prepare-pallet-parent.log 2>&1
for entry in 'pallet 37' 'center 41' 'shop 42' 'name 229'; do
    read -r name map <<< "$entry"
    input_state=source.state
    if [[ "$name" == pallet ]]; then input_state=pallet-parent.state; fi
    ./pallet_render_smoke roms/pokeyellow.gbc "$input_state" warp "$map" 0 "$name.state" > "logs/prepare-$name.log" 2>&1
done
run() {
    local name=$1 fixture=$2 mode=$3
    if [[ ${QA_MENU_STYLE:-classic} == integrated ]]; then mode=$mode-styled; fi
    mkdir -p "$name/logs"
    echo "Checking $name"
    (cd "$name"; ../pallet_render_smoke ../roms/pokeyellow.gbc "../$fixture.state" "$mode" > logs/run.log 2>&1)
}
run menus pallet menus
run menus-fp pallet menus-fp
run center center menus-center
run center-fp center menus-center-fp
run shop shop menus-shop
run shop-fp shop menus-shop-fp
run name name menus-name
run name-fp name menus-name-fp
# All original tilemaps are private. Pin the expected class independently of
# the live compositor checks and keep unknown full screens explicitly covered.
for scenario in menus menus-fp center center-fp shop shop-fp name name-fp; do
    for tiles in "$scenario"/logs/*.tiles; do
        label=$(basename "$tiles" .tiles)
        case "$label" in
            start|save-*|center-question|pc-start|shop-*|name-question) kind=1 ;;
            *) kind=2 ;;
        esac
        "$project/build/menu_layout_test" "$tiles" "$kind" >> logs/layouts.txt
    done
done
sha256sum --check logs/inputs.sha256
printf 'PASS: Start, save, party, bag, card, options, Pokedex, healing, PC, shop and naming in both cameras; evidence in %s\n' "$qa_dir" | tee logs/result.txt
