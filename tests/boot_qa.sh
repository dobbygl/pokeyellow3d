#!/usr/bin/env bash
# B2: unskipped original intro, title fade and cold boot through to the world.
set -euo pipefail
if (( $# != 2 )); then
    echo "Usage: $0 ROM BATTERY_SAVE" >&2
    exit 2
fi
project=$(cd -- "$(dirname -- "$0")/.." && pwd)
rom=$(realpath -- "$1")
battery=$(realpath -- "$2")
command -v ffmpeg >/dev/null
command -v ffprobe >/dev/null
cmake --build "$project/build" --target all pallet_render_smoke --parallel 4
ctest --test-dir "$project/build" --output-on-failure
qa_dir=$(mktemp -d "$project/build/qa/boot-XXXXXX")
mkdir -p "$qa_dir/roms" "$qa_dir/logs"
cp -- "$rom" "$qa_dir/roms/pokeyellow.gbc"
cp -- "$battery" "$qa_dir/battery.sav"
cp -- "$project/build/pallet_render_smoke" "$qa_dir/pallet_render_smoke"
cd -- "$qa_dir"
sha256sum roms/pokeyellow.gbc battery.sav > logs/inputs.sha256
export SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy
echo "QA output: $qa_dir"
mkdir -p menu/logs new/logs continue/logs
# Exercise the public default, without a savestate or any optional argument.
(cd menu; env -u BOOT_VIDEO ../pallet_render_smoke ../roms/pokeyellow.gbc boot > logs/run.log 2>&1)
# Record every frame, including all of the original intro, with a 60 Hz cap.
# The presentation fade still uses its real clock, including rendering stalls.
(cd new; BOOT_VIDEO=1 ../pallet_render_smoke ../roms/pokeyellow.gbc boot new 12000 > logs/run.log 2>&1)
(cd continue; BOOT_VIDEO=1 ../pallet_render_smoke ../roms/pokeyellow.gbc boot continue 4000 ../battery.sav > logs/run.log 2>&1)
python3 - <<'CHECK'
import csv
import json
import subprocess
from pathlib import Path

summary = {}
for mode in ('menu', 'new', 'continue'):
    logs = Path(mode) / 'logs'
    assert f'[BOOT] PASS mode={mode}' in (logs / 'run.log').read_text()
    rows = list(csv.DictReader((logs / 'boot.csv').open()))
    title = next(int(row['frame']) for row in rows if row['phase'] == '1')
    menu = next(int(row['frame']) for row in rows if row['phase'] == '2')
    assert title > 1500 and menu > title
    assert all(row['phase'] == row['active'] == row['blend'] == row['native_errors'] == '0'
               and not row['input'] for row in rows[:title])
    fade = [row for row in rows[title:menu] if row['blend'] == '1']
    assert len(fade) >= 3 and float(fade[0]['progress']) == 0
    assert all(row['target'] == row['active'] == '1' for row in fade)
    progress = [float(row['progress']) for row in fade]
    assert progress == sorted(progress) and progress[-1] < 1
    assert any(row['blend'] == '0' for row in rows[title:menu])
    if mode != 'menu':
        expected = '4' if mode == 'new' else '3'
        assert any(row['phase'] == expected for row in rows)
        assert all(row['phase'] == '0' and row['active'] == '1' for row in rows[-60:])
        for name, dimensions in (('composed', (800, 720)), ('original', (160, 144))):
            stream = json.loads(subprocess.check_output([
                'ffprobe', '-v', 'error', '-select_streams', 'v:0', '-count_frames',
                '-show_entries', 'stream=width,height,nb_read_frames,r_frame_rate',
                '-of', 'json', str(logs / f'boot-{name}.mp4')]))['streams'][0]
            assert (stream['width'], stream['height']) == dimensions
            assert int(stream['nb_read_frames']) == len(rows)
            assert stream['r_frame_rate'] == '60/1'
    summary[mode] = dict(frames=len(rows), native_frames=title, first_menu=menu,
                         title_fade_frames=len(fade), native_pixel_errors=0,
                         video=mode != 'menu')
Path('logs/summary.json').write_text(json.dumps(summary, indent=2) + '\n')
print(json.dumps(summary, indent=2))
CHECK
sha256sum --check logs/inputs.sha256
echo "PASS: full native intro, title fade, menu/new/continue, complete videos and per-frame read-only guards; $qa_dir" | tee logs/result.txt
