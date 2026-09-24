#!/usr/bin/env python3
"""Private A2 interior journeys: OFF pixels vs parent, ON/OFF guest equality.

Starts with the original Pallet (9,7) party fixture. Every subsequent fixture is
made by original input or the smoke helper's explicit engine warp setup. No
renderer writes engine state. Run this serially, without another GPU test.
"""
import argparse
import gzip
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def require(value, why):
    if not value:
        raise RuntimeError(why)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('smoke', 'parent', 'rom', 'state', 'output'):
        parser.add_argument('--' + name, required=True, type=Path)
    parser.add_argument('--styles', nargs='+', choices=('classic', 'integrated'),
                        default=['classic', 'integrated'])
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    root = Path(tempfile.mkdtemp(prefix='a2-interiors-', dir=args.output.resolve()))
    print(root, flush=True)
    inputs = {}
    for name in ('smoke', 'parent', 'rom', 'state'):
        source = getattr(args, name).resolve()
        shutil.copy2(source, root / name)
        inputs[name] = {'source': str(source), 'sha256': sha(root / name)}
    (root / 'inputs.json').write_text(json.dumps(inputs, indent=2) + '\n')
    env = {k: v for k, v in os.environ.items()
           if not k.startswith(('QA_', 'SMOKE_', 'JOURNEY_', 'INTERIOR_', 'CATALOG_'))}
    env.update(SDL_VIDEODRIVER='offscreen', SDL_AUDIODRIVER='dummy',
               QA_ART_PASS='off', QA_MENU_STYLE='classic', QA_CAPTURE_STATES='1')
    results = []
    status = 'INCOMPLETE'

    def run(label, binary, fixture, mode, *extra, **settings):
        directory = root / label
        (directory / 'logs').mkdir(parents=True)
        with (directory / 'run.log').open('w') as log:
            subprocess.run([str(root / binary), str(root / 'rom'), str(root / fixture), mode,
                            *map(str, extra)], cwd=directory, env={**env, **settings},
                           stdout=log, stderr=subprocess.STDOUT, check=True)
        text = (directory / 'run.log').read_text()
        require(not re.search(r'\b(FAIL|SKIP)\b', text), label + ': failed/skipped')
        return directory, text

    def captures(directory, expected):
        images = {p.name: sha(p) for p in (directory / 'logs').glob('*.ppm')}
        states = {p.name: sha(p) for p in (directory / 'logs').glob('*.machine')}
        require(len(images) == len(states) == expected, str(directory) + ': missing captures')
        for path in (directory / 'logs').iterdir():
            if path.suffix not in ('.ppm', '.machine', '.state'):
                continue
            digest = sha(path)
            with path.open('rb') as source, gzip.open(str(path) + '.gz', 'wb', compresslevel=1) as dest:
                shutil.copyfileobj(source, dest)
            with gzip.open(str(path) + '.gz', 'rb') as source:
                require(hashlib.file_digest(source, 'sha256').hexdigest() == digest,
                        'lossless private capture compression')
            path.unlink()
        return images, states

    try:
        _, text = run('prepare-house', 'parent', 'state', 'play', '20:D:8', '80', root / 'house.state')
        require('[PLAY] map=0 xy=9,8 ' in text, 'house fixture must be Pallet (9,8)')
        for name, map_id in (('mart', 122), ('cave', 46)):
            _, text = run('prepare-' + name, 'parent', 'state', 'warp', map_id, 0, root / (name + '.state'))
            require(f'[WARP] map={map_id} ' in text, 'engine did not reach fixture map')
        # The original journey reaches this position using real movement/combat.
        run('prepare-city', 'parent', 'state', 'viridian',
            JOURNEY_CITY_STATE=str(root / 'city.state'))
        require((root / 'city.state').is_file(), 'journey did not save the Viridian fixture')
        scenarios = [('house', 'house.state', 'interior', 7),
                     ('house-fp', 'house.state', 'interior-fp', 7),
                     ('stairs', 'mart.state', 'interior-transitions', 8),
                     ('cave', 'cave.state', 'interior-transitions', 4),
                     ('town', 'city.state', 'town', None)]
        for style in args.styles:
            for name, fixture, mode, expected in scenarios:
                groups = {}
                for binary, art in (('parent', 'off'), ('smoke', 'off'), ('smoke', 'on')):
                    label = f'{style}-{name}-{binary}-{art}'
                    directory, text = run(label, binary, fixture, mode, QA_MENU_STYLE=style,
                                          QA_ART_PASS=art, QA_GLYPH_ORACLE='1',
                                          INTERIOR_CAPTURE_DIR='logs')
                    require('PASS:' in text, label + ': journey did not finish')
                    if style == 'integrated' and name in ('house', 'house-fp', 'town'):
                        oracle = re.search(r'\[UI-STYLE\] frames=(\d+) glyphs=(\d+) bits=(\d+)', text)
                        require(oracle and all(int(v) > 0 for v in oracle.groups()),
                                label + ': text oracle did not inspect glyphs')
                        require(not re.search(r'default_glyph_frames=[1-9]', text),
                                label + ': unexpected default-font text')
                    if expected is None:
                        # Town includes a parcel branch controlled by the shared
                        # input fixture. The original parent fixes the inventory.
                        expected = len(list((directory / 'logs').glob('*.ppm')))
                        require(expected >= 10, 'town did not exercise healing/shop/dialogues')
                    groups[binary, art] = captures(directory, expected)
                    results.append({'label': label, 'images': groups[binary, art][0],
                                    'states': groups[binary, art][1],
                                    'log_sha256': sha(directory / 'run.log')})
                    print(label + ': passed', flush=True)
                require(groups['smoke', 'off'] == groups['parent', 'off'],
                        f'OFF pixels/guest changed: {style}/{name}')
                require(groups['smoke', 'on'][1] == groups['smoke', 'off'][1],
                        f'ON guest changed: {style}/{name}')
        for name, record in inputs.items():
            require(sha(root / name) == record['sha256'], 'input changed: ' + name)
        status = 'PASS_INTERIOR_JOURNEYS'
    except Exception as error:
        status = 'FAIL: ' + str(error)
        raise
    finally:
        (root / 'result.json').write_text(json.dumps({'status': status, 'runs': results,
            'styles': args.styles,
            'scope': 'House in both cameras; town, stairs/elevator and cave including FP. '
                     'Requested menu styles. OFF matches parent; ON/OFF serialized state matches. '
                     'This does not certify every prop/player view or historical whole-game gates.'},
            indent=2) + '\n')
    print(status, flush=True)


if __name__ == '__main__':
    main()
