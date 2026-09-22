#!/usr/bin/env bash
# Private engine-driven party/summary journeys. The fixture needs two Pokemon
# and the Pokedex. Generated six-member parties exist only in disposable copies.
set -euo pipefail
if (( $# != 2 )); then
    echo "Usage: $0 ROM TWO_POKEMON_WORLD" >&2
    exit 2
fi
project=$(cd -- "$(dirname -- "$0")/.." && pwd)
rom=$(realpath -- "$1")
source_state=$(realpath -- "$2")
cmake --build "$project/build" --target all pallet_render_smoke --parallel 4
ctest --test-dir "$project/build" --output-on-failure
qa_dir=$(mktemp -d "$project/build/qa/ui-full-pokemon-XXXXXX")
mkdir -p "$qa_dir/roms" "$qa_dir/logs"
cp -- "$rom" "$qa_dir/roms/pokeyellow.gbc"
cp -- "$source_state" "$qa_dir/source.state"
cp -- "$project/build/pallet_render_smoke" "$qa_dir/pallet_render_smoke"
cd -- "$qa_dir"
trap 'echo "QA interrupted; inspect $qa_dir" >&2' ERR
sha256sum roms/pokeyellow.gbc source.state > logs/inputs.sha256
export SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy
echo "QA output: $qa_dir"
./pallet_render_smoke roms/pokeyellow.gbc source.state warp 41 0 center.state > logs/prepare-center.log 2>&1
for mode in pokemon pokemon-fp pokemon-six pokemon-six-fp pokemon-pc pokemon-pc-fp; do
    mkdir -p "$mode/logs"
    requested=$mode
    if [[ ${QA_MENU_STYLE:-classic} == integrated ]]; then requested=$mode-styled; fi
    fixture=source.state
    if [[ $mode == pokemon-pc* ]]; then fixture=center.state; fi
    (cd "$mode"; ../pallet_render_smoke ../roms/pokeyellow.gbc "../$fixture" "$requested" > logs/run.log 2>&1)
done
python3 - "$qa_dir" "${QA_MENU_STYLE:-classic}" <<'CHECK'
import json
import re
import sys
from pathlib import Path

root = Path(sys.argv[1])
rows = []
for mode in ('pokemon', 'pokemon-fp', 'pokemon-six', 'pokemon-six-fp', 'pokemon-pc', 'pokemon-pc-fp'):
    text = (root / mode / 'logs/run.log').read_text()
    if '-pc' in mode:
        assert 'PASS: original PC party/box summaries' in text and 'FAIL' not in text, mode
        for source in ('party', 'box'):
            for page in ('stats', 'moves'):
                assert (root / mode / 'logs' / f'pokemon-pc-{source}-{page}.state').is_file()
        row = {'mode': mode, 'sources': ['party', 'box']}
        if sys.argv[2] == 'integrated':
            values = re.search(r'\[UI-POKEMON\] frames=(\d+) glyphs=(\d+) bits=(\d+) cursors=(\d+) fallback=(\d+) party=(\d+) actions=(\d+) stats=(\d+) moves=(\d+)', text)
            assert values, mode
            row.update(zip(('frames', 'glyphs', 'bits', 'cursors', 'fallback', 'party', 'actions', 'stats', 'moves'), map(int, values.groups())))
            assert all(row[key] > 0 for key in ('frames', 'glyphs', 'bits', 'stats', 'moves')), row
        rows.append(row)
        continue
    assert 'PASS: original party' in text and 'FAIL' not in text, mode
    expected = 6 if '-six' in mode else 2
    for slot in range(expected):
        for page in ('party', 'actions', 'stats', 'moves'):
            assert (root / mode / 'logs' / f'pokemon-{slot}-{page}.state').is_file()
    row = {'mode': mode, 'members': expected}
    if sys.argv[2] == 'integrated':
        values = re.search(r'\[UI-POKEMON\] frames=(\d+) glyphs=(\d+) bits=(\d+) cursors=(\d+) fallback=(\d+) party=(\d+) actions=(\d+) stats=(\d+) moves=(\d+)', text)
        assert values, mode
        row.update(zip(('frames', 'glyphs', 'bits', 'cursors', 'fallback', 'party', 'actions', 'stats', 'moves'), map(int, values.groups())))
        assert all(row[key] > 0 for key in ('frames', 'glyphs', 'bits', 'cursors', 'fallback', 'party', 'actions', 'stats', 'moves')), row
        for control in ('border', 'unknown-tile', 'portrait', 'number-graphic', 'font'):
            assert (root / mode / 'logs' / f'pokemon-negative-{control}.ppm').is_file()
    rows.append(row)
(root / 'logs/pokemon.json').write_text(json.dumps(rows, indent=2) + '\n')
CHECK
sha256sum --check logs/inputs.sha256
printf 'PASS: six party/summary journeys, six-member/status/HP/level100, PC sources and both cameras; evidence in %s\n' "$qa_dir" | tee logs/result.txt
