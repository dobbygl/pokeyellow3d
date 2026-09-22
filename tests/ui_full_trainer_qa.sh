#!/usr/bin/env bash
# Original trainer card: absent, mixed and complete badges in both cameras.
set -euo pipefail
if (( $# != 2 )); then echo "Usage: $0 ROM WORLD_WITH_POKEDEX" >&2; exit 2; fi
project=$(cd -- "$(dirname -- "$0")/.." && pwd)
rom=$(realpath -- "$1")
state=$(realpath -- "$2")
cmake --build "$project/build" --target all pallet_render_smoke --parallel 4
ctest --test-dir "$project/build" --output-on-failure
qa_dir=$(mktemp -d "$project/build/qa/ui-full-trainer-XXXXXX")
mkdir -p "$qa_dir/roms" "$qa_dir/logs"
cp -- "$rom" "$qa_dir/roms/pokeyellow.gbc"
cp -- "$state" "$qa_dir/world.state"
cp -- "$project/build/pallet_render_smoke" "$qa_dir/pallet_render_smoke"
cd -- "$qa_dir"
sha256sum roms/pokeyellow.gbc world.state > logs/inputs.sha256
echo "QA output: $qa_dir"
export SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy QA_FULL_NEGATIVES=1
for variant in none mixed all; do
    for camera in ortho fp; do
        mode=trainer-card-$variant
        if [[ $camera == fp ]]; then mode=$mode-fp; fi
        mkdir -p "$mode/logs"
        requested=$mode
        if [[ ${QA_MENU_STYLE:-classic} == integrated ]]; then requested=$mode-styled; fi
        (cd "$mode"; ../pallet_render_smoke ../roms/pokeyellow.gbc ../world.state "$requested" > logs/run.log 2>&1)
    done
done
python3 - "$qa_dir" "${QA_MENU_STYLE:-classic}" <<'CHECK'
import json,re,sys
from pathlib import Path
root=Path(sys.argv[1]);rows=[]
for path in sorted(root.glob('trainer-card-*/logs/run.log')):
    content=path.read_text()
    assert 'PASS: original trainer card,' in content and 'FAIL' not in content,path
    assert 'Saved battery RAM' not in content,path
    captures=sorted(p.name for p in path.parent.glob('*.tiles'))
    assert len(captures)==2,path
    for name in captures:
        for suffix in ('.state','.ppm'):
            assert path.with_name(name).with_suffix(suffix).is_file(),(path,name,suffix)
    row=dict(mode=path.parent.parent.name,captures=captures)
    if sys.argv[2]=='integrated':
        match=re.search(r'\[UI-FULL\] frames=(\d+) glyphs=(\d+) bits=(\d+) graphics=(\d+) fallback=(\d+) cursor=(\d+) pending_cursor=(\d+) kinds=([\d,]+)',content)
        assert match,path
        row.update(zip(('frames','glyphs','bits','graphics','fallback','cursors','pending_cursor'),map(int,match.groups()[:7])))
        row['kinds']=list(map(int,match[8].split(',')))
        assert all(row[key]>0 for key in ('frames','glyphs','bits','graphics','fallback')),row
        assert row['kinds'][11]>0,row
        assert '[TRAINER-CARD-MOTION] PASS focus, Esc and open-menu load' in content,path
        for control in ('border','unknown-tile','font'):
            assert f'[UI-FULL-NEGATIVE] {control} full LCD verified' in content,path
        for control in ('portrait','face-or-badge','badge-number','time-colon','background','circle','border'):
            assert f'[TRAINER-CARD-NEGATIVE] {control} full LCD verified' in content,path
        assert not re.search(r'default_glyph_frames=[1-9]',content),path
    rows.append(row)
assert len(rows)==6
(root/'logs/trainer.json').write_text(json.dumps(rows,indent=2)+'\n')
CHECK
sha256sum --check logs/inputs.sha256
printf 'PASS: six original trainer-card journeys, absent/mixed/all badges, original text/graphics, A/B and both cameras; evidence in %s\n' "$qa_dir" | tee logs/result.txt
