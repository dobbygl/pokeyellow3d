#!/usr/bin/env bash
# Complete original Pokedex list on its device, in both cameras.
set -euo pipefail
if (( $# != 2 )); then echo "Usage: $0 ROM WORLD_WITH_POKEDEX" >&2; exit 2; fi
project=$(cd -- "$(dirname -- "$0")/.." && pwd)
rom=$(realpath -- "$1")
state=$(realpath -- "$2")
cmake --build "$project/build" --target all pallet_render_smoke --parallel 4
ctest --test-dir "$project/build" --output-on-failure
qa_dir=$(mktemp -d "$project/build/qa/ui-full-dex-XXXXXX")
mkdir -p "$qa_dir/roms" "$qa_dir/logs"
cp -- "$rom" "$qa_dir/roms/pokeyellow.gbc"
cp -- "$state" "$qa_dir/world.state"
cp -- "$project/build/pallet_render_smoke" "$qa_dir/pallet_render_smoke"
cd -- "$qa_dir"
sha256sum roms/pokeyellow.gbc world.state > logs/inputs.sha256
echo "QA output: $qa_dir"
export SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy
for mode in full-dex-short full-dex-short-fp full-dex full-dex-fp; do
    mkdir -p "$mode/logs"
    requested=$mode
    if [[ ${QA_MENU_STYLE:-classic} == integrated ]]; then requested=$mode-styled; fi
    (cd "$mode"; ../pallet_render_smoke ../roms/pokeyellow.gbc ../world.state "$requested" > logs/run.log 2>&1)
done
python3 - "$qa_dir" "${QA_MENU_STYLE:-classic}" <<'CHECK'
import json,re,sys
from pathlib import Path
root=Path(sys.argv[1]);rows=[]
for path in sorted(root.glob('full-dex*/logs/run.log')):
    content=path.read_text()
    assert 'PASS: original full dex' in content and 'FAIL' not in content,path
    assert 'Saved battery RAM' not in content,path
    captures=sorted(p.name for p in path.parent.glob('*.tiles'))
    assert len(captures)>=(8 if "short" in path.parent.parent.name else 9),path
    for name in captures:
        assert path.with_name(name).with_suffix('.state').is_file()
        assert path.with_name(name).with_suffix('.ppm').is_file()
    row=dict(mode=path.parent.parent.name,captures=captures)
    if sys.argv[2]=='integrated':
        match=re.search(r'\[UI-DEX\] frames=(\d+) glyphs=(\d+) bits=(\d+) graphics=(\d+) cursor=(\d+) fallback=(\d+)',content)
        assert match,path
        row.update(zip(('frames','glyphs','bits','graphics','cursors','fallback'),map(int,match.groups())))
        assert all(row[k]>0 for k in ('frames','glyphs','bits','graphics','cursors','fallback')),row
        for control in ('separator','unknown-tile','selection','caught-graphic','font'):
            assert f'[UI-DEX-NEGATIVE] {control} full LCD verified' in content,path
        assert '[DEX-MOTION] PASS focus, Esc and open-menu load' in content,path
        assert not re.search(r'default_glyph_frames=[1-9]',content),path
    rows.append(row)
assert len(rows)==4
(root/'logs/dex.json').write_text(json.dumps(rows,indent=2)+'\n')
CHECK
sha256sum --check logs/inputs.sha256
printf 'PASS: four original full dex journeys, 5/151-entry lists, flags, scroll, DATA/CRY and both cameras; %s\n' "$qa_dir" | tee logs/result.txt
