#!/usr/bin/env bash
# C2: real wild and trainer encounters, using isolated copies in both cameras.
set -euo pipefail
if (( $# != 3 )); then
    echo "Usage: $0 ROM ROUTE1_10_28 ROUTE22_30_5" >&2
    exit 2
fi
project=$(cd -- "$(dirname -- "$0")/.." && pwd)
rom=$(realpath -- "$1")
wild=$(realpath -- "$2")
trainer=$(realpath -- "$3")
cmake --build "$project/build" --target all pallet_render_smoke --parallel 4
ctest --test-dir "$project/build" --output-on-failure
qa_dir=$(mktemp -d "$project/build/qa/ui-battles-XXXXXX")
mkdir -p "$qa_dir/roms" "$qa_dir/logs"
cp -- "$rom" "$qa_dir/roms/pokeyellow.gbc"
cp -- "$wild" "$qa_dir/wild.state"
cp -- "$trainer" "$qa_dir/trainer.state"
cp -- "$project/build/pallet_render_smoke" "$qa_dir/pallet_render_smoke"
cd -- "$qa_dir"
trap 'echo "QA interrupted; inspect $qa_dir" >&2' ERR
sha256sum roms/pokeyellow.gbc wild.state trainer.state > logs/inputs.sha256
export SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy C2_VERIFY=1 UI_VIDEO=1
echo "QA output: $qa_dir"
for scenario in wild trainer; do
    for camera in ortho fp; do
        name=$scenario-$camera
        mode=battle-timing
        if [[ $scenario == trainer ]]; then mode=trainer-timing; fi
        if [[ $camera == fp ]]; then mode=$mode-fp; fi
        if [[ ${QA_MENU_STYLE:-classic} == integrated ]]; then mode=$mode-styled; fi
        mkdir -p "$name/logs"
        echo "Checking $name"
        (cd "$name"; ../pallet_render_smoke ../roms/pokeyellow.gbc "../$scenario.state" "$mode" > logs/run.log 2>&1)
    done
done
python3 "$project/tests/check_battle_timing.py" "$qa_dir" | tee logs/timing.txt
sha256sum --check logs/inputs.sha256
printf 'PASS: wild/trainer in both cameras, first HUD timing, coverage, GL and read-only memory; evidence in %s\n' "$qa_dir" | tee logs/result.txt
