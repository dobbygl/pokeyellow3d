#!/usr/bin/env bash
# Private original-engine inventory and transaction journeys in both cameras.
set -euo pipefail
if (( $# != 2 )); then
    echo "Usage: $0 ROM WORLD_AFTER_PARCEL" >&2
    exit 2
fi
project=$(cd -- "$(dirname -- "$0")/.." && pwd)
rom=$(realpath -- "$1")
source_state=$(realpath -- "$2")
cmake --build "$project/build" --target all pallet_render_smoke --parallel 4
ctest --test-dir "$project/build" --output-on-failure
qa_dir=$(mktemp -d "$project/build/qa/ui-full-items-XXXXXX")
mkdir -p "$qa_dir/roms" "$qa_dir/logs"
cp -- "$rom" "$qa_dir/roms/pokeyellow.gbc"
cp -- "$source_state" "$qa_dir/world.state"
cp -- "$project/build/pallet_render_smoke" "$qa_dir/pallet_render_smoke"
cd -- "$qa_dir"
trap 'echo "QA interrupted; inspect $qa_dir" >&2' ERR
sha256sum roms/pokeyellow.gbc world.state > logs/inputs.sha256
export SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy QA_FULL_NEGATIVES=1
echo "QA output: $qa_dir"
./pallet_render_smoke roms/pokeyellow.gbc world.state warp 42 0 shop.state > logs/prepare-shop.log 2>&1
for area in bag shop; do
    for variant in short long empty; do
        for camera in ortho fp; do
            mode=items-$area
            if [[ $variant != short ]]; then mode=$mode-$variant; fi
            if [[ $camera == fp ]]; then mode=$mode-fp; fi
            mkdir -p "$mode/logs"
            requested=$mode
            if [[ ${QA_MENU_STYLE:-classic} == integrated ]]; then requested=$mode-styled; fi
            fixture=world.state
            if [[ $area == shop ]]; then fixture=shop.state; fi
            (cd "$mode"; ../pallet_render_smoke ../roms/pokeyellow.gbc "../$fixture" "$requested" > logs/run.log 2>&1)
        done
    done
done
python3 - "$qa_dir" "${QA_MENU_STYLE:-classic}" <<'CHECK'
import json
import re
import sys
from pathlib import Path

root = Path(sys.argv[1])
rows = []
for path in sorted(root.glob('items-*/logs/run.log')):
    content = path.read_text()
    assert 'PASS: original ' in content and 'FAIL' not in content, path
    captures = sorted(p.name for p in path.parent.glob('*.tiles'))
    assert captures, path
    for name in captures:
        assert path.with_name(name).with_suffix('.state').is_file()
        assert path.with_name(name).with_suffix('.ppm').is_file()
    row = dict(mode=path.parent.parent.name, captures=captures)
    if sys.argv[2] == 'integrated':
        match = re.search(r'\[UI-FULL\] frames=(\d+) glyphs=(\d+) bits=(\d+) graphics=(\d+) fallback=(\d+) cursor=(\d+) pending_cursor=(\d+) kinds=([\d,]+)', content)
        assert match, path
        row.update(zip(('frames', 'glyphs', 'bits', 'graphics', 'fallback', 'cursors', 'pending_cursor'), map(int, match.groups()[:7])))
        row['kinds'] = list(map(int, match[8].split(',')))
        assert all(row[key] > 0 for key in ('frames', 'glyphs', 'bits', 'fallback', 'cursors')), row
        assert '[ITEM-MOTION] PASS focus, Esc and open-menu load' in content, path
        for control in ('selection', 'border', 'unknown-tile', 'font'):
            assert f'[UI-FULL-NEGATIVE] {control} full LCD verified' in content, path
        assert not re.search(r'default_glyph_frames=[1-9]', content), path
    rows.append(row)
assert len(rows) == 12
(root / 'logs/items.json').write_text(json.dumps(rows, indent=2) + '\n')
CHECK
sha256sum --check logs/inputs.sha256
printf 'PASS: twelve original bag/mart journeys, empty/short/long lists, swaps, Use/Toss, buy/sell and both cameras; evidence in %s\n' "$qa_dir" | tee logs/result.txt
