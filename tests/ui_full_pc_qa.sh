#!/usr/bin/env bash
# Full-screen PC journeys; the same original inputs run in both menu styles.
set -euo pipefail
if (( $# != 2 )); then
    echo "Usage: $0 ROM NATURAL_PIDGEY_WORLD_WITH_EMPTY_BOX" >&2
    exit 2
fi
project=$(cd -- "$(dirname -- "$0")/.." && pwd)
rom=$(realpath -- "$1")
world=$(realpath -- "$2")
cmake --build "$project/build" --target all pallet_render_smoke --parallel 4
qa_dir=$(mktemp -d "$project/build/qa/ui-full-pc-XXXXXX")
mkdir -p "$qa_dir/roms" "$qa_dir/logs"
cp -- "$rom" "$qa_dir/roms/pokeyellow.gbc"
cp -- "$world" "$qa_dir/world.state"
cp -- "$project/build/pallet_render_smoke" "$qa_dir/pallet_render_smoke"
cd -- "$qa_dir"
trap 'echo "QA interrupted; inspect $qa_dir" >&2' ERR
sha256sum roms/pokeyellow.gbc world.state > logs/inputs.sha256
export SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy
if [[ ${QA_MENU_STYLE:-classic} == integrated ]]; then export QA_FULL_NEGATIVES=1; fi
echo "QA output: $qa_dir"
./pallet_render_smoke roms/pokeyellow.gbc world.state warp 41 0 center.state > logs/prepare-center.log 2>&1
./pallet_render_smoke roms/pokeyellow.gbc world.state warp 38 0 bedroom.state > logs/prepare-bedroom.log 2>&1
for scenario in storage storage-stress details items-scroll focus-center focus-bedroom; do
    for suffix in '' '-fp'; do
        name="$scenario$suffix"
        fixture=center
        mode="pc-$scenario$suffix"
        if [[ $scenario == focus-* ]]; then
            fixture=${scenario#focus-}
            mode="pc-focus$suffix"
        fi
        if [[ ${QA_MENU_STYLE:-classic} == integrated ]]; then mode=$mode-styled; fi
        mkdir -p "$name/logs"
        echo "Checking $name"
        (cd "$name"; ../pallet_render_smoke ../roms/pokeyellow.gbc "../$fixture.state" "$mode" > logs/run.log 2>&1)
    done
done
python3 - "$qa_dir" "${QA_MENU_STYLE:-classic}" <<'CHECK'
import json
import re
import sys
from pathlib import Path

root = Path(sys.argv[1])
report = {}
for path in sorted(root.glob('*/logs/run.log')):
    content = path.read_text()
    assert 'FAIL' not in content and 'PASS:' in content, path
    sample = dict(log=str(path))
    if sys.argv[2] == 'integrated':
        fields = re.findall(r'\[UI-FULL\] frames=(\d+) glyphs=(\d+) bits=(\d+) graphics=(\d+) fallback=(\d+) cursor=(\d+) pending_cursor=(\d+) kinds=([\d,]+)', content)
        assert fields, path
        values = fields[-1]
        sample.update(zip(('frames', 'glyphs', 'bits', 'graphics', 'fallback', 'cursor', 'pending_cursor'), map(int, values[:7])))
        sample['kinds'] = list(map(int, values[7].split(',')))
        assert sample['glyphs'] > 0 and sample['cursor'] > 0, path
        assert '[UI-FULL-CAPTURE]' in content, path
        assert '[UI-FULL-NEGATIVE] border' in content, path
        assert '[UI-FULL-NEGATIVE] font' in content, path
        assert '[UI-FULL-NEGATIVE] selection' in content, path
    report[path.parent.parent.name] = sample
assert len(report) == 12
(root / 'logs/full-pc.json').write_text(json.dumps(report, indent=2) + '\n')
print(json.dumps(report, indent=2))
CHECK
sha256sum --check logs/inputs.sha256
printf 'PASS: Bill, player and Oak PC, twenty-mon and fifty-item scroll, quantities, center/bedroom and both cameras; evidence in %s\n' "$qa_dir" | tee logs/result.txt
