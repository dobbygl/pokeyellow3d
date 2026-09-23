#!/usr/bin/env python3
"""Private B1 captures: three original exterior states, both cameras and menu styles."""
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def main():
    if len(sys.argv) != 7:
        raise SystemExit('Usage: shadow_qa.py HELPER ROM PALLET_STATE ROUTE_STATE CITY_STATE OUTPUT')
    helper, rom, pallet, route, city, output = (Path(value).resolve() for value in sys.argv[1:])
    output.mkdir(parents=True, exist_ok=True)
    root = Path(tempfile.mkdtemp(prefix='shadow-b1-', dir=output))
    print(f'B1 output: {root}', flush=True)
    inputs = {}
    for name, source in [('helper', helper), ('cartridge.gbc', rom), ('pallet.state', pallet),
                         ('route.state', route), ('city.state', city)]:
        shutil.copy2(source, root / name)
        inputs[name] = sha(root / name)
    report = dict(status='RUNNING', inputs=inputs, cases=[], reviewed=False)
    record = root / 'result.json'
    record.write_text(json.dumps(report, indent=2) + '\n')
    env = {**os.environ, 'SDL_VIDEODRIVER': 'offscreen', 'SDL_AUDIODRIVER': 'dummy',
           'QA_CAPTURE_STATES': '1'}
    if sys.platform == 'win32':
        env.update(SDL_VIDEODRIVER='windows', SDL_OPENGL_ES_DRIVER='1')
    for variable in ('QA_GLYPH_ORACLE', 'QA_FULL_NEGATIVES'):
        env.pop(variable, None)
    try:
        for style in ('classic', 'integrated'):
            for camera in ('ortho', 'fp'):
                for place in ('pallet', 'route', 'city'):
                    name = f'{style}-{camera}-{place}'
                    directory = root / name
                    (directory / 'logs').mkdir(parents=True)
                    (directory / 'captures').mkdir()
                    with (directory / 'run.log').open('w') as log:
                        subprocess.run([str(root / 'helper'), str(root / 'cartridge.gbc'),
                                        str(root / f'{place}.state'),
                                        'shadows-fp' if camera == 'fp' else 'shadows'],
                                       cwd=directory, env={**env, 'QA_MENU_STYLE': style},
                                       stdout=log, stderr=subprocess.STDOUT, check=True)
                    text = (directory / 'run.log').read_text()
                    assert 'PASS: B1 shadows, eight sun states' in text
                    assert 'FAIL' not in text and 'SKIP' not in text
                    images = {p.name: sha(p) for p in (directory / 'logs').glob('*.ppm')}
                    states = {p.name: sha(p) for p in (directory / 'logs').glob('*.machine')}
                    assert len(images) == 17 and len(states) == 17
                    # Every capture came from the same frozen guest, including Esc.
                    assert len(set(states.values())) == 1
                    for hour in (-1, 0, 6, 18, 21):
                        prefix = f'hour-{hour:.6f}'
                        assert images[prefix + '-on.ppm'] == images[prefix + '-off.ppm']
                    report['cases'].append(dict(name=name, images=images, states=states,
                                                log_sha256=sha(directory / 'run.log')))
                    record.write_text(json.dumps(report, indent=2) + '\n')
                    print(f'PASS: {name}', flush=True)
        for name, expected in inputs.items():
            assert sha(root / name) == expected
        report['status'] = 'PASS_PENDING_VISUAL_REVIEW'
    except BaseException as error:
        report['status'] = 'FAILED'
        report['error'] = repr(error)
        raise
    finally:
        record.write_text(json.dumps(report, indent=2) + '\n')
    print(f'PASS: 12 B1 scenes; contacts still require manual review: {root}')


if __name__ == '__main__':
    main()
