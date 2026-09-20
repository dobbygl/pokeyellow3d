#!/usr/bin/env bash
set -euo pipefail
if (( $# != 3 )); then
    echo "Usage: $0 ROM CENTER_ENTRANCE_STATE WORLD_STATE" >&2
    exit 2
fi
project=$(cd -- "$(dirname -- "$0")/.." && pwd)
rom=$(realpath -- "$1")
center=$(realpath -- "$2")
world=$(realpath -- "$3")
cmake --build "$project/build" --target all pallet_render_smoke --parallel 4
ctest --test-dir "$project/build" --output-on-failure
qa_dir=$(mktemp -d "$project/build/qa/pc-focus-XXXXXX")
mkdir -p "$qa_dir/roms" "$qa_dir/logs"
cp -- "$rom" "$qa_dir/roms/pokeyellow.gbc"
cp -- "$center" "$qa_dir/center.state"
cp -- "$world" "$qa_dir/world.state"
cp -- "$project/build/pallet_render_smoke" "$qa_dir/pallet_render_smoke"
cd -- "$qa_dir"
trap 'echo "QA interrupted; inspect $qa_dir" >&2' ERR
sha256sum roms/pokeyellow.gbc center.state world.state > logs/inputs.sha256
export SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy
echo "QA output: $qa_dir"
./pallet_render_smoke roms/pokeyellow.gbc world.state warp 38 0 bedroom.state > logs/prepare-bedroom.log 2>&1
for fixture in center bedroom; do
    for suffix in '' '-fp'; do
        name="$fixture$suffix"
        mkdir -p "$name/logs"
        echo "Checking $name"
        (cd "$name"; ../pallet_render_smoke ../roms/pokeyellow.gbc "../$fixture.state" "pc-focus$suffix" > logs/run.log 2>&1)
    done
done
for suffix in '' '-fp'; do
    name="menus$suffix"
    mkdir -p "$name/logs"
    echo "Checking $name"
    (cd "$name"; ../pallet_render_smoke ../roms/pokeyellow.gbc ../center.state "menus-center$suffix" > logs/run.log 2>&1)
done
sha256sum --check logs/inputs.sha256
printf 'PASS: Center/bedroom focus and return, original menus/deposit/withdraw, both cameras; evidence in %s\n' "$qa_dir" | tee logs/result.txt
