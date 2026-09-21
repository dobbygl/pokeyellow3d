#!/usr/bin/env bash
# B1: cold boot with/without a private battery save; original title/menu inputs.
set -euo pipefail
if (( $# != 2 )); then
    echo "Usage: $0 ROM BATTERY_SAVE" >&2
    exit 2
fi
project=$(cd -- "$(dirname -- "$0")/.." && pwd)
rom=$(realpath -- "$1")
battery=$(realpath -- "$2")
cmake --build "$project/build" --target all pallet_render_smoke --parallel 4
ctest --test-dir "$project/build" --output-on-failure
qa_dir=$(mktemp -d "$project/build/qa/title-XXXXXX")
mkdir -p "$qa_dir/roms" "$qa_dir/logs"
cp -- "$rom" "$qa_dir/roms/pokeyellow.gbc"
cp -- "$battery" "$qa_dir/battery.sav"
cp -- "$project/build/pallet_render_smoke" "$qa_dir/pallet_render_smoke"
cd -- "$qa_dir"
sha256sum roms/pokeyellow.gbc battery.sav > logs/inputs.sha256
export SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy
echo "QA output: $qa_dir"
for mode in new continue idle; do
    mkdir -p "$mode/logs"
    echo "Checking $mode"
    case "$mode" in
        new) frames=7200; args=() ;;
        continue) frames=2800; args=(../battery.sav) ;;
        idle) frames=7200; args=() ;;
    esac
    (cd "$mode"; ../pallet_render_smoke ../roms/pokeyellow.gbc --title-audit "$mode" "$frames" "${args[@]}" > logs/run.log 2>&1)
done
sha256sum --check logs/inputs.sha256
echo "PASS: title/new game/continue/idle, VRAM portrait oracle, per-frame read-only memory and neutral controls; $qa_dir" | tee logs/result.txt
