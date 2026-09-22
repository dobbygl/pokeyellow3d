#!/usr/bin/env bash
# Original options from the world and initial menu, in both cameras.
set -euo pipefail
if (( $# < 2 || $# > 3 )); then
    echo "Usage: $0 ROM WORLD_WITH_POKEDEX [INITIAL_MENU_STATE]" >&2
    exit 2
fi
project=$(cd -- "$(dirname -- "$0")/.." && pwd)
rom=$(realpath -- "$1")
world=$(realpath -- "$2")
title=
if (( $# == 3 )); then title=$(realpath -- "$3"); fi
cmake --build "$project/build" --target all pallet_render_smoke --parallel 4
ctest --test-dir "$project/build" --output-on-failure
qa_dir=$(mktemp -d "$project/build/qa/ui-full-options-XXXXXX")
mkdir -p "$qa_dir/roms" "$qa_dir/logs"
cp -- "$rom" "$qa_dir/roms/pokeyellow.gbc"
cp -- "$world" "$qa_dir/world.state"
cp -- "$project/build/pallet_render_smoke" "$qa_dir/pallet_render_smoke"
cd -- "$qa_dir"
export SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy QA_FULL_NEGATIVES=1
echo "QA output: $qa_dir"
if [[ -n $title ]]; then
    cp -- "$title" title.state
else
    mkdir -p prepare-title/logs
    (cd prepare-title; ../pallet_render_smoke ../roms/pokeyellow.gbc boot menu 3000 > logs/run.log 2>&1)
    cp -- prepare-title/logs/boot-final.state title.state
fi
sha256sum roms/pokeyellow.gbc world.state title.state > logs/inputs.sha256
for area in world title; do
    for camera in ortho fp; do
        mode=options-$area
        if [[ $camera == fp ]]; then mode=$mode-fp; fi
        mkdir -p "$mode/logs"
        requested=$mode
        if [[ ${QA_MENU_STYLE:-classic} == integrated ]]; then requested=$mode-styled; fi
        (cd "$mode"; ../pallet_render_smoke ../roms/pokeyellow.gbc "../$area.state" "$requested" > logs/run.log 2>&1)
    done
done
python3 - "$qa_dir" "${QA_MENU_STYLE:-classic}" <<'CHECK'
import json,re,sys
from pathlib import Path
root=Path(sys.argv[1]); rows=[]
for path in sorted(root.glob('options-*/logs/run.log')):
    content=path.read_text()
    assert 'PASS: original options,' in content and 'FAIL' not in content,path
    assert 'Saved battery RAM' not in content,path
    captures=sorted(p.name for p in path.parent.glob('*.tiles'))
    assert len(captures)==34,path
    for name in captures:
        for suffix in ('.state','.ppm'):
            assert path.with_name(name).with_suffix(suffix).is_file(),(path,name,suffix)
    row=dict(mode=path.parent.parent.name,captures=captures)
    if sys.argv[2]=='integrated':
        match=re.search(r'\[UI-FULL\] frames=(\d+) glyphs=(\d+) bits=(\d+) graphics=(\d+) fallback=(\d+) cursor=(\d+) pending_cursor=(\d+) kinds=([\d,]+)',content)
        assert match,path
        row.update(zip(('frames','glyphs','bits','graphics','fallback','cursors','pending_cursor'),map(int,match.groups()[:7])))
        row['kinds']=list(map(int,match[8].split(',')))
        assert all(row[key]>0 for key in ('frames','glyphs','bits','fallback','cursors')),row
        assert row['kinds'][10]>0,row
        assert '[OPTIONS-MOTION] PASS focus, Esc and open-menu load' in content,path
        for control in ('selection','border','unknown-tile','font'):
            assert f'[UI-FULL-NEGATIVE] {control} full LCD verified' in content,path
        assert not re.search(r'default_glyph_frames=[1-9]',content),path
    rows.append(row)
assert len(rows)==4
(root/'logs/options.json').write_text(json.dumps(rows,indent=2)+'\n')
CHECK
sha256sum --check logs/inputs.sha256
printf 'PASS: four original options journeys, every setting, cursor wrap, exits and both cameras; evidence in %s\n' "$qa_dir" | tee logs/result.txt
