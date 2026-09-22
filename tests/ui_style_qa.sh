#!/usr/bin/env bash
# Reuse the engine-driven journeys in both cameras; enable the independent
# per-frame glyph observer in the smoke executable. No fixture is committed.
set -euo pipefail
if (( $# != 8 )); then
    echo "Usage: $0 ROM TWO_POKEMON_WORLD ROUTE1 ROUTE22 PALLET_9_7 READY_BATTLE NATURAL_PIDGEY_WORLD_WITH_EMPTY_BOX SUCCESSFUL_CAPTURE_THROW" >&2
    exit 2
fi
project=$(cd -- "$(dirname -- "$0")/.." && pwd)
rom=$(realpath -- "$1")
world=$(realpath -- "$2")
wild=$(realpath -- "$3")
trainer=$(realpath -- "$4")
pallet=$(realpath -- "$5")
battle=$(realpath -- "$6")
pc_world=$(realpath -- "$7")
capture_throw=$(realpath -- "$8")
qa_dir=$(mktemp -d "$project/build/qa/ui-style-XXXXXX")
mkdir -p "$qa_dir/logs"
# Inherit the renderer used by the other journey suites. Call with
# LIBGL_ALWAYS_SOFTWARE=1 when software rendering is explicitly wanted.
export QA_MENU_STYLE=integrated QA_GLYPH_ORACLE=1
echo "QA output: $qa_dir"
"$project/tests/ui_menus_qa.sh" "$rom" "$world" > "$qa_dir/logs/menus.log" 2>&1
"$project/tests/ui_battles_qa.sh" "$rom" "$wild" "$trainer" > "$qa_dir/logs/battles.log" 2>&1
"$project/tests/ui_crossfade_qa.sh" "$rom" "$pallet" "$battle" > "$qa_dir/logs/crossfade.log" 2>&1
"$project/tests/ui_full_pc_qa.sh" "$rom" "$pc_world" > "$qa_dir/logs/full-pc.log" 2>&1
"$project/tests/ui_full_pokemon_qa.sh" "$rom" "$world" "$battle" > "$qa_dir/logs/full-pokemon.log" 2>&1
"$project/tests/ui_full_items_qa.sh" "$rom" "$world" "$battle" > "$qa_dir/logs/full-items.log" 2>&1
"$project/tests/ui_full_dex_qa.sh" "$rom" "$world" > "$qa_dir/logs/full-dex.log" 2>&1
"$project/tests/ui_full_options_qa.sh" "$rom" "$world" > "$qa_dir/logs/full-options.log" 2>&1
"$project/tests/ui_full_trainer_qa.sh" "$rom" "$world" > "$qa_dir/logs/full-trainer.log" 2>&1
"$project/tests/ui_full_naming_qa.sh" "$rom" "$world" "$capture_throw" > "$qa_dir/logs/full-naming.log" 2>&1
for style in classic integrated; do
    for operation in write read; do
        SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy \
            "$project/build/pallet_render_smoke" "$rom" "$world" "preferences-$operation" \
            "$qa_dir/logs/preferences.cfg" "$style" > "$qa_dir/logs/preferences-$operation-$style.log" 2>&1
    done
done
for camera in menu-motion menu-motion-fp; do
    motion_dir="$qa_dir/$camera"
    mkdir -p "$motion_dir/logs"
    (
        cd "$motion_dir"
        SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy QA_MENU_MOTION_AUDIT=1 \
            "$project/build/pallet_render_smoke" "$rom" "$pallet" "$camera-styled" \
            > logs/run.log 2>&1
    )
done
python3 - "$qa_dir" <<'CHECK'
import json
import re
import sys
from pathlib import Path

root = Path(sys.argv[1])
report = {}
for suite in ('menus', 'battles', 'crossfade'):
    text = (root / 'logs' / f'{suite}.log').read_text()
    directory = Path(re.search(r'QA output: (\S+)', text)[1])
    samples = []
    for path in sorted(directory.rglob('run.log')):
        content = path.read_text()
        assert '[UI-STYLE] FAIL' not in content and '[QA-CURSOR] FAIL' not in content
        values = re.findall(r'\[UI-STYLE\] frames=(\d+) glyphs=(\d+) bits=(\d+) classic_fallback_tiles=(\d+)', content)
        assert values, path
        frames, glyphs, bits, fallback = map(int, values[-1])
        samples.append(dict(scenario=str(path.relative_to(directory)), frames=frames,
                            glyphs=glyphs, bits=bits, fallback_tiles=fallback))
    assert samples, suite
    report[suite] = dict(directory=str(directory), scenarios=samples)
assert sum(s['glyphs'] for s in report['menus']['scenarios']) > 0
full_pc_text = (root / 'logs/full-pc.log').read_text()
full_pc_root = Path(re.search(r'QA output: (\S+)', full_pc_text)[1])
report['full-pc'] = dict(directory=str(full_pc_root), scenarios=json.loads((full_pc_root / 'logs/full-pc.json').read_text()))
pokemon_text = (root / 'logs/full-pokemon.log').read_text()
pokemon_root = Path(re.search(r'QA output: (\S+)', pokemon_text)[1])
report['full-pokemon'] = dict(directory=str(pokemon_root), scenarios=json.loads((pokemon_root / 'logs/pokemon.json').read_text()))
items_text = (root / 'logs/full-items.log').read_text()
items_root = Path(re.search(r'QA output: (\S+)', items_text)[1])
report['full-items'] = dict(directory=str(items_root), scenarios=json.loads((items_root / 'logs/items.json').read_text()))
dex_text = (root / 'logs/full-dex.log').read_text()
dex_root = Path(re.search(r'QA output: (\S+)', dex_text)[1])
report['full-dex'] = dict(directory=str(dex_root), scenarios=json.loads((dex_root / 'logs/dex.json').read_text()))
options_text = (root / 'logs/full-options.log').read_text()
options_root = Path(re.search(r'QA output: (\S+)', options_text)[1])
report['full-options'] = dict(directory=str(options_root), scenarios=json.loads((options_root / 'logs/options.json').read_text()))
trainer_text = (root / 'logs/full-trainer.log').read_text()
trainer_root = Path(re.search(r'QA output: (\S+)', trainer_text)[1])
report['full-trainer'] = dict(directory=str(trainer_root), scenarios=json.loads((trainer_root / 'logs/trainer.json').read_text()))
naming_text = (root / 'logs/full-naming.log').read_text()
naming_root = Path(re.search(r'QA output: (\S+)', naming_text)[1])
report['full-naming'] = dict(directory=str(naming_root), scenarios=json.loads((naming_root / 'logs/naming.json').read_text()))
for camera in ('menu-motion', 'menu-motion-fp'):
    content = (root / camera / 'logs/run.log').read_text()
    assert 'PASS: real Start animation' in content, camera
    assert '[UI-MOTION] FAIL' not in content, camera
    report[camera] = dict(log=str(root / camera / 'logs/run.log'))
(root / 'logs' / 'glyphs.json').write_text(json.dumps(report, indent=2) + '\n')
print(json.dumps(report, indent=2))
CHECK
printf 'PASS: integrated journeys, per-frame glyph oracle and cursor; evidence in %s\n' "$qa_dir" | tee "$qa_dir/logs/result.txt"
