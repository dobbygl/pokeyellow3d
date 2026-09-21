#!/usr/bin/env python3
"""Linux private-ROM audit: exact canonical engine frames with cosmetics on/off."""
import argparse
import csv
import gzip
import hashlib
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile
import threading


def verify_closing(logs):
    """These input journeys do not load states or disable the renderer."""
    panels = {}
    with (logs / 'motion-panels.csv').open() as stream:
        for row in csv.DictReader(stream):
            key = tuple(int(row[k]) for k in ('domain', 'x', 'y', 'w', 'h'))
            panels.setdefault(int(row['frame']), {})[key] = row
    previous = None
    checked = 0
    with (logs / 'motion-frames.csv').open() as stream:
        for frame in csv.DictReader(stream):
            current = panels.get(int(frame['frame']), {})
            if previous:
                old_frame, old_panels = previous
                elapsed = (int(frame['cycles']) - int(old_frame['cycles'])) & 0xffffffff
                step = elapsed / 629146
                if elapsed < 629146 and frame['paused'] == old_frame['paused'] == '0':
                    for key, panel in old_panels.items():
                        if panel['visible'] == '1' and float(panel['amount']) > step + 0.00001:
                            assert key in current, (logs, frame['frame'], key, 'panel vanished before completing its fade')
                            if panel['target'] == '0':
                                expected = max(0, float(panel['amount']) - step)
                                assert abs(float(current[key]['amount']) - expected) < 0.00001, (logs, frame['frame'], 'closing phase jumped')
                                checked += 1
            previous = frame, current
    assert checked > 0, (logs, 'no closing phase observed')
    return checked


def compare(directory):
    left, right = (directory / mode / 'logs' for mode in ('on', 'off'))
    frames = total = 0
    with gzip.open(left / 'engine.bin.gz', 'rb') as a, gzip.open(right / 'engine.bin.gz', 'rb') as b:
        while True:
            header = a.read(4)
            assert header == b.read(4), (frames, 'frame length/EOF differs')
            if not header:
                break
            assert len(header) == 4
            size, = struct.unpack('=I', header)
            assert 0 < size < 16 * 1024 * 1024
            data = a.read(size)
            assert len(data) == size and data == b.read(size), (frames, 'engine differs')
            frames += 1
            total += size
    assert frames > 0
    names = lambda folder: {p.name for p in folder.glob('*.ppm')
                            if not (p.stem.startswith('motion-') and p.stem[7:].isdigit())}
    captures = names(left)
    assert captures and captures == names(right), 'fixed capture set differs'
    for name in sorted(captures):
        for suffix in ('', '.machine'):
            assert (left / (name + suffix)).read_bytes() == (right / (name + suffix)).read_bytes(), name + suffix
    return dict(frames=frames, engine_bytes_compared=total, fixed_captures=sorted(captures),
                closing_steps_checked=verify_closing(left))


def run(helper, rom, fixture, mode, directory, animated):
    logs = directory / 'logs'
    logs.mkdir(parents=True)
    fifo = directory / 'engine.pipe'
    os.mkfifo(fifo)
    errors = []

    def compress():
        try:
            with fifo.open('rb') as source, gzip.open(logs / 'engine.bin.gz', 'wb', compresslevel=1) as target:
                shutil.copyfileobj(source, target, 1024 * 1024)
        except BaseException as error:
            errors.append(repr(error))

    reader = threading.Thread(target=compress, daemon=True)
    reader.start()
    env = os.environ.copy()
    env.update(SDL_VIDEODRIVER='offscreen', SDL_AUDIODRIVER='dummy', QA_CAPTURE_STATES='1',
               QA_GLYPH_ORACLE='1', QA_MENU_MOTION=animated, QA_MENU_ENGINE_TRACE=str(fifo),
               QA_MENU_MOTION_AUDIT='1')
    env.pop('QA_MENU_MOTION_CAPTURE', None)
    if animated == 'on':
        env['QA_MENU_MOTION_CAPTURE'] = '1'
    command = [str(helper), str(rom), str(fixture), mode + '-styled']
    (directory / 'command.json').write_text(json.dumps(dict(command=command, cwd=str(directory), motion=animated), indent=2) + '\n')
    with (logs / 'run.log').open('w') as output:
        result = subprocess.run(command, cwd=directory, env=env, stdout=output, stderr=subprocess.STDOUT)
    (directory / 'run.exit').write_text(str(result.returncode) + '\n')
    # Unblock a reader if the helper failed before opening its output stream.
    if result.returncode and reader.is_alive():
        descriptor = os.open(fifo, os.O_RDWR | os.O_NONBLOCK)
        os.close(descriptor)
    reader.join(30)
    assert not reader.is_alive() and not errors, errors
    assert result.returncode == 0, (mode, animated, result.returncode, logs / 'run.log')
    print('PASS journey', mode, animated, flush=True)


def main():
    project = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('rom', type=Path)
    parser.add_argument('pallet', type=Path, help='prepared world fixture with two party members')
    parser.add_argument('--center', type=Path, help='prepared center fixture from ui_menus_qa.sh')
    parser.add_argument('--shop', type=Path, help='prepared shop fixture from ui_menus_qa.sh')
    parser.add_argument('--helper', type=Path, default=project / 'build/pallet_render_smoke')
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    directory = args.output.resolve() if args.output else Path(tempfile.mkdtemp(prefix='menu-motion-pair-', dir=project / 'build/qa'))
    directory.mkdir(parents=True, exist_ok=True)
    helper = directory / 'pallet_render_smoke'
    shutil.copy2(args.helper, helper)
    report = dict(helper_sha256=hashlib.sha256(helper.read_bytes()).hexdigest(), scenarios={})
    scenarios = [('rapid', args.pallet, 'menu-motion-rapid'), ('menus', args.pallet, 'menus')]
    if args.center:
        scenarios.append(('center', args.center, 'menus-center'))
    if args.shop:
        scenarios.append(('shop', args.shop, 'menus-shop'))
    print('QA output:', directory, flush=True)
    for name, fixture, mode in scenarios:
        for camera in ('', '-fp'):
            destination = directory / (name + camera)
            for animated in ('on', 'off'):
                run(helper, args.rom.resolve(), fixture.resolve(), mode + camera,
                    destination / animated, animated)
            report['scenarios'][name + camera] = compare(destination)
            (directory / 'report.json').write_text(json.dumps(report, indent=2) + '\n')
            print('PASS exact per-frame engine and fixed-phase captures', name + camera, flush=True)
    print('PASS all paired motion journeys', flush=True)


if __name__ == '__main__':
    main()
