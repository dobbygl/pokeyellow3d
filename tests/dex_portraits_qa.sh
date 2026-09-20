#!/usr/bin/env bash
# Uses private inputs and original UI actions; never invokes battery callbacks.
set -euo pipefail
if (( $# < 2 || $# > 3 )); then echo "Usage: $0 ROM WORLD_WITH_POKEDEX [dex-data]" >&2; exit 2; fi
project=$(cd -- "$(dirname -- "$0")/.." && pwd)
rom=$(realpath -- "$1")
state=$(realpath -- "$2")
mode=${3:-dex-portraits}
if [[ $mode != dex-portraits && $mode != dex-data ]]; then echo "Invalid QA mode" >&2; exit 2; fi
cmake --build "$project/build" --target all pallet_render_smoke --parallel 4
qa_dir=$(mktemp -d "$project/build/qa/dex-portraits-XXXXXX")
mkdir -p "$qa_dir/roms" "$qa_dir/logs"
cp -- "$rom" "$qa_dir/roms/pokeyellow.gbc"
cp -- "$state" "$qa_dir/world.state"
cp -- "$project/build/pallet_render_smoke" "$qa_dir/pallet_render_smoke"
cd -- "$qa_dir"
sha256sum roms/pokeyellow.gbc world.state > logs/inputs.sha256
echo "QA output: $qa_dir"
SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy ./pallet_render_smoke roms/pokeyellow.gbc world.state "$mode" > logs/run.log 2>&1
sha256sum --check logs/inputs.sha256
"$project/build/mon_pic_test" roms/pokeyellow.gbc logs/front-vram.bin
mkdir -p "$project/build/qa/dex"
cp -- logs/front-vram.bin "$project/build/qa/dex/front-vram.bin"
ctest --test-dir "$project/build" --output-on-failure -R mon_pic
echo "PASS: original-engine portrait evidence in $qa_dir"
