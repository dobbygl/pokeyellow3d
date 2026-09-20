#!/usr/bin/env bash
set -euo pipefail
if (( $# != 2 )); then echo "Usage: $0 ROM WORLD_WITH_POKEDEX" >&2; exit 2; fi
project=$(cd -- "$(dirname -- "$0")/.." && pwd)
rom=$(realpath -- "$1")
state=$(realpath -- "$2")
cmake --build "$project/build" --target all pallet_render_smoke --parallel 4
ctest --test-dir "$project/build" --output-on-failure
qa_dir=$(mktemp -d "$project/build/qa/dex-area-XXXXXX")
mkdir -p "$qa_dir/roms" "$qa_dir/logs"
cp -- "$rom" "$qa_dir/roms/pokeyellow.gbc"
cp -- "$state" "$qa_dir/world.state"
cp -- "$project/build/pallet_render_smoke" "$qa_dir/pallet_render_smoke"
cd -- "$qa_dir"
sha256sum roms/pokeyellow.gbc world.state > logs/inputs.sha256
export SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy
echo "QA output: $qa_dir"
for camera in ortho fp; do
    mode=dex
    if [[ $camera == fp ]]; then mode=dex-fp; fi
    mkdir -p "$camera/logs"
    (cd "$camera"; ../pallet_render_smoke ../roms/pokeyellow.gbc ../world.state "$mode" > logs/run.log 2>&1)
done
for number in 016 041 129; do
    mkdir -p "cold-$number/logs"
    (cd "cold-$number"; ../pallet_render_smoke ../roms/pokeyellow.gbc "../ortho/logs/area-$number.state" dex-area-screen > logs/run.log 2>&1)
done
cmp ortho/logs/areas.bin fp/logs/areas.bin
mkdir -p "$project/build/qa/dex"
cp ortho/logs/areas.bin "$project/build/qa/dex/areas.bin"
sha256sum --check logs/inputs.sha256
printf 'PASS: original list/data/cry/AREA for Pidgey, Zubat, Magikarp in both cameras; cold loads, exact LCD text, blink, preserved world and read-only frames; %s\n' "$qa_dir" | tee logs/result.txt
