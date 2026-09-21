#!/usr/bin/env bash
# Reuse the engine-driven journeys in both cameras; enable the independent
# per-frame glyph observer in the smoke executable. No fixture is committed.
set -euo pipefail
if (( $# != 6 )); then
    echo "Usage: $0 ROM TWO_POKEMON_WORLD ROUTE1 ROUTE22 PALLET_9_7 READY_BATTLE" >&2
    exit 2
fi
project=$(cd -- "$(dirname -- "$0")/.." && pwd)
rom=$(realpath -- "$1")
world=$(realpath -- "$2")
wild=$(realpath -- "$3")
trainer=$(realpath -- "$4")
pallet=$(realpath -- "$5")
battle=$(realpath -- "$6")
qa_dir=$(mktemp -d "$project/build/qa/ui-style-XXXXXX")
mkdir -p "$qa_dir/logs"
export QA_MENU_STYLE=integrated QA_GLYPH_ORACLE=1 LIBGL_ALWAYS_SOFTWARE=1
echo "QA output: $qa_dir"
"$project/tests/ui_menus_qa.sh" "$rom" "$world" > "$qa_dir/logs/menus.log" 2>&1
"$project/tests/ui_battles_qa.sh" "$rom" "$wild" "$trainer" > "$qa_dir/logs/battles.log" 2>&1
"$project/tests/ui_crossfade_qa.sh" "$rom" "$pallet" "$battle" > "$qa_dir/logs/crossfade.log" 2>&1
for style in classic integrated; do
    for operation in write read; do
        SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy \
            "$project/build/pallet_render_smoke" "$rom" "$world" "preferences-$operation" \
            "$qa_dir/logs/preferences.cfg" "$style" > "$qa_dir/logs/preferences-$operation-$style.log" 2>&1
    done
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
(root / 'logs' / 'glyphs.json').write_text(json.dumps(report, indent=2) + '\n')
print(json.dumps(report, indent=2))
CHECK
printf 'PASS: integrated journeys, per-frame glyph oracle and cursor; evidence in %s\n' "$qa_dir" | tee "$qa_dir/logs/result.txt"
