#!/usr/bin/env bash
# Private fixtures and losslessly compressed per-frame engine snapshots.
set -euo pipefail
if (( $# != 4 )); then
    echo "Usage: $0 ROM PALLET_9_7 ROUTE1_10_28 VIRIDIAN" >&2
    exit 2
fi
project=$(cd -- "$(dirname -- "$0")/.." && pwd)
rom=$(realpath -- "$1")
pallet=$(realpath -- "$2")
route=$(realpath -- "$3")
city=$(realpath -- "$4")
command -v gzip >/dev/null
cmake --build "$project/build" --target all pallet_render_smoke --parallel 4
qa_dir=$(mktemp -d "$project/build/qa/daylight-XXXXXX")
mkdir -p "$qa_dir/roms" "$qa_dir/logs"
cp -- "$rom" "$qa_dir/roms/pokeyellow.gbc"
cp -- "$pallet" "$qa_dir/pallet.state"
cp -- "$route" "$qa_dir/route.state"
cp -- "$city" "$qa_dir/city.state"
cp -- "$project/build/pallet_render_smoke" "$qa_dir/pallet_render_smoke"
cd -- "$qa_dir"
trap 'echo "QA interrupted; inspect $qa_dir" >&2' ERR
sha256sum roms/pokeyellow.gbc pallet.state route.state city.state > logs/inputs.sha256
export SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy
echo "QA output: $qa_dir"
for camera in ortho fp; do
    suffix=""
    if [[ $camera == fp ]]; then suffix=-fp; fi
    for place in pallet route city; do
        name="$place-$camera"
        mkdir -p "$name/logs"
        (cd -- "$name"; ../pallet_render_smoke ../roms/pokeyellow.gbc "../$place.state" \
            "daylight$suffix" > logs/run.log 2>&1)
        cmp "$name/logs/disabled-before.ppm" "$name/logs/disabled-after.ppm"
        ./pallet_render_smoke roms/pokeyellow.gbc "$place.state" daylight-reload \
            "$name/logs/lighting.cfg" > "$name/logs/reload.log" 2>&1
        echo "PASS captures $name and fresh-process preferences"
    done
    for hour in -1 6 12 18 0; do
        mkdir -p "replay-$camera/hour-$hour/logs"
        (cd -- "replay-$camera/hour-$hour"; ../../pallet_render_smoke ../../roms/pokeyellow.gbc \
            ../../route.state "daylight-replay$suffix" "$hour" > logs/run.log 2>&1)
        if (( hour >= 0 )); then
            cmp "replay-$camera/hour--1/logs/engine-final.state" \
                "replay-$camera/hour-$hour/logs/engine-final.state"
        fi
        echo "PASS replay $camera hour=$hour"
    done
done
sha256sum --check logs/inputs.sha256
printf 'PASS: 24 lighting views, both cameras, exact disabled restoration, fresh-process preferences and complete engine replay equality; evidence in %s\n' "$qa_dir" | tee logs/result.txt
