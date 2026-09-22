#!/usr/bin/env bash
# Original player/rival, Name Rater and captured-Pokemon keyboard journeys.
set -euo pipefail
if (( $# < 3 || $# > 4 )); then
    echo "Usage: $0 ROM TWO_POKEMON_WORLD SUCCESSFUL_CAPTURE_THROW [INITIAL_MENU_STATE]" >&2
    exit 2
fi
project=$(cd -- "$(dirname -- "$0")/.." && pwd)
rom=$(realpath -- "$1")
world=$(realpath -- "$2")
capture=$(realpath -- "$3")
title=
if (( $# == 4 )); then title=$(realpath -- "$4"); fi
cmake --build "$project/build" --target all pallet_render_smoke --parallel 4
ctest --test-dir "$project/build" --output-on-failure
qa_dir=$(mktemp -d "$project/build/qa/ui-full-naming-XXXXXX")
mkdir -p "$qa_dir/roms" "$qa_dir/logs"
cp -- "$rom" "$qa_dir/roms/pokeyellow.gbc"
cp -- "$world" "$qa_dir/world.state"
cp -- "$capture" "$qa_dir/capture-throw.state"
cp -- "$project/build/pallet_render_smoke" "$qa_dir/pallet_render_smoke"
cd -- "$qa_dir"
export SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy QA_FULL_NEGATIVES=1
echo "QA output: $qa_dir"
if [[ -n $title ]]; then
    cp -- "$title" title.state
else
    mkdir -p prepare-title/logs
    (cd prepare-title; QA_MENU_STYLE=classic ../pallet_render_smoke ../roms/pokeyellow.gbc boot menu 3000 > logs/run.log 2>&1)
    cp -- prepare-title/logs/boot-final.state title.state
fi
sha256sum roms/pokeyellow.gbc world.state capture-throw.state title.state > logs/inputs.sha256
mkdir -p prepare-names/logs prepare-mon/logs prepare-capture/logs
# Fixture preparation uses original input handlers in classic presentation.
# No name, alphabet, cursor, species or battle outcome is injected.
(cd prepare-names; QA_MENU_STYLE=classic ../pallet_render_smoke ../roms/pokeyellow.gbc ../title.state prepare-names > logs/run.log 2>&1)
QA_MENU_STYLE=classic ./pallet_render_smoke roms/pokeyellow.gbc world.state warp 229 0 mon-house.state > logs/prepare-mon.log 2>&1
(cd prepare-mon; QA_MENU_STYLE=classic ../pallet_render_smoke ../roms/pokeyellow.gbc ../mon-house.state menus-name > logs/run.log 2>&1)
(cd prepare-capture; QA_MENU_STYLE=classic ../pallet_render_smoke ../roms/pokeyellow.gbc ../capture-throw.state prepare-captured-name > logs/run.log 2>&1)
cp -- prepare-names/logs/naming-player-empty.state player.state
cp -- prepare-names/logs/naming-rival-empty.state rival.state
cp -- prepare-mon/logs/naming-empty.state mon.state
cp -- prepare-capture/logs/naming-capture-empty.state capture.state
sha256sum player.state rival.state mon.state capture.state > logs/fixtures.sha256
for subject in player rival mon capture; do
    for submit in start end; do
        for camera in ortho fp; do
            mode=naming-$subject-$submit
            if [[ $camera == fp ]]; then mode=$mode-fp; fi
            mkdir -p "$mode/logs"
            requested=$mode
            if [[ ${QA_MENU_STYLE:-classic} == integrated ]]; then requested=$mode-styled; fi
            (cd "$mode"; ../pallet_render_smoke ../roms/pokeyellow.gbc "../$subject.state" "$requested" > logs/run.log 2>&1)
        done
    done
done
python3 - "$qa_dir" "${QA_MENU_STYLE:-classic}" <<'CHECK'
import json,re,sys
from pathlib import Path
root=Path(sys.argv[1]); rows=[]
for path in sorted(root.glob('naming-*/logs/run.log')):
    content=path.read_text()
    assert 'PASS: original naming,' in content and 'FAIL' not in content,path
    assert 'Saved battery RAM' not in content,path
    captures=sorted(p.name for p in path.parent.glob('*.tiles'))
    assert len(captures)==7,path
    for name in captures:
        for suffix in ('.state','.ppm'):
            assert path.with_name(name).with_suffix(suffix).is_file(),(path,name,suffix)
    row=dict(mode=path.parent.parent.name,captures=captures)
    pokemon=any(s in row['mode'] for s in ('-mon-', '-capture-'))
    if pokemon:
        assert '[NAMING-ICON] 120 frames,' in content,path
    if sys.argv[2]=='integrated':
        match=re.search(r'\[UI-FULL\] frames=(\d+) glyphs=(\d+) bits=(\d+) graphics=(\d+) fallback=(\d+) cursor=(\d+) pending_cursor=(\d+) kinds=([\d,]+)',content)
        assert match,path
        row.update(zip(('frames','glyphs','bits','graphics','fallback','cursors','pending_cursor'),map(int,match.groups()[:7])))
        row['kinds']=list(map(int,match[8].split(',')))
        assert all(row[key]>0 for key in ('frames','glyphs','bits','graphics','fallback','cursors')),row
        assert row['kinds'][12]>0,row
        assert '[NAMING-MOTION] PASS focus, Esc and open-menu load' in content,path
        for control in ('selection','border','unknown-tile','font'):
            assert f'[UI-FULL-NEGATIVE] {control} full LCD verified' in content,path
        for control in ('end','underscore','raised-underscore') + (('icon','icon-geometry') if pokemon else ()):
            assert f'[NAMING-NEGATIVE] {control} full LCD verified' in content,path
        assert not re.search(r'default_glyph_frames=[1-9]',content),path
    rows.append(row)
assert len(rows)==16
(root/'logs/naming.json').write_text(json.dumps(rows,indent=2)+'\n')
CHECK
sha256sum --check logs/inputs.sha256
sha256sum --check logs/fixtures.sha256
printf 'PASS: sixteen original naming journeys, cases, keyboard, editing, limits, Start/ED and both cameras; evidence in %s\n' "$qa_dir" | tee logs/result.txt
