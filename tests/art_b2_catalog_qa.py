#!/usr/bin/env python3
"""Private art catalog captures: OFF vs parent and v0.4.1, ON vs parent.

This checks catalogs only, not the full interactive regression or timing gates.
The reference helper must be built from 7955aaa; the parent from pre-phase main.
--phase a2 also requires every exterior ON capture to match the parent.
Never commit the output (ROM, states and ROM-derived captures).
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


def require(value, message):
    if not value:
        raise RuntimeError(message)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for key in ('smoke', 'parent', 'reference', 'rom', 'state', 'output'):
        parser.add_argument('--' + key, type=Path, required=True)
    parser.add_argument('--phase', choices=('b2', 'a2'), default='b2')
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)
    root = Path(tempfile.mkdtemp(prefix=args.phase + '-catalog-', dir=args.output.resolve()))
    print(root, flush=True)
    inputs = {}
    for key in ('smoke', 'parent', 'reference', 'rom', 'state'):
        source = getattr(args, key).resolve()
        dest = root / key
        shutil.copy2(source, dest)
        inputs[key] = {'source': str(source), 'sha256': sha(dest)}
    (root / 'inputs.json').write_text(json.dumps(inputs, indent=2) + '\n')
    env = {k: v for k, v in os.environ.items()
           if not k.startswith(('QA_', 'CATALOG_'))}
    env.update(SDL_VIDEODRIVER='offscreen', SDL_AUDIODRIVER='dummy', QA_CAPTURE_STATES='1')
    results = []

    def run(binary, style, camera, kind, enabled):
        label = f'{binary}-{style}-{camera}-{kind}-{enabled}'
        directory = root / label
        captures = directory / 'captures'
        captures.mkdir(parents=True)
        (directory / 'logs').mkdir()
        mode = kind + ('-fp' if camera == 'fp' else '')
        if enabled == 'on':
            mode += '-art'
        with (directory / 'run.log').open('w') as log:
            subprocess.run([str(root / binary), str(root / 'rom'), str(root / 'state'), mode],
                           cwd=directory, env={**env, 'QA_ART_PASS': enabled,
                           'QA_MENU_STYLE': style, 'CATALOG_CAPTURE_DIR': 'captures'},
                           stdout=log, stderr=subprocess.STDOUT, check=True)
        text = (directory / 'run.log').read_text()
        count = 38 if kind == 'catalog' else 179
        require(f'PASS: all {count} ' in text and not re.search(r'\b(FAIL|SKIP)\b', text), label)
        images = {p.name: sha(p) for p in captures.glob('*.ppm')}
        states = {p.name: sha(p) for p in captures.glob('*.machine')}
        require(len(images) == len(states) == count, label + ': incomplete captures')
        records = re.findall(r'\[CATALOG\] map=(\d+) vertices=(\d+) bytes=(\d+)', text)
        require(len(records) == count and len({r[0] for r in records}) == count,
                label + ': incomplete mesh inventory')
        require(all(int(v) > 0 and int(b) == 56 * int(v) for _, v, b in records),
                label + ': invalid mesh')
        results.append({'label': label, 'images': images, 'states': states,
                        'log_sha256': sha(directory / 'run.log')})
        # Retain exact bytes with reversible compression; verify before removal.
        for path in captures.iterdir():
            digest = sha(path)
            dest = path.with_name(path.name + '.gz')
            with path.open('rb') as source, gzip.open(dest, 'wb', compresslevel=1) as target:
                shutil.copyfileobj(source, target)
            with gzip.open(dest, 'rb') as source:
                require(hashlib.file_digest(source, 'sha256').hexdigest() == digest,
                        label + ': compression mismatch')
            path.unlink()
        print(label + ': captured', flush=True)
        return images, states

    status = 'INCOMPLETE'
    try:
        for style in ('classic', 'integrated'):
            for camera in ('ortho', 'fp'):
                for kind in ('catalog', 'interior-catalog'):
                    off = run('smoke', style, camera, kind, 'off')
                    parent_off = run('parent', style, camera, kind, 'off')
                    require(off == parent_off, f'OFF changed: {style}/{camera}/{kind}')
                    if camera == 'ortho':
                        historical = run('reference', style, camera, kind, 'off')
                        require(off == historical, f'OFF differs from 7955aaa: {style}/{kind}')
                    on = run('smoke', style, camera, kind, 'on')
                    parent_on = run('parent', style, camera, kind, 'on')
                    require(on[1] == parent_on[1] == off[1], 'Art changed the guest state')
                    require(on[0].keys() == parent_on[0].keys(), 'ON inventory mismatch')
                    if args.phase == 'a2' and kind == 'catalog':
                        require(on[0] == parent_on[0], f'A2 changed exteriors: {style}/{camera}')
                    else:
                        require(on[0] != parent_on[0], f'Art never appeared: {camera}/{kind}')
        status = 'PASS_CATALOGS_ONLY'
    except Exception as error:
        status = 'FAIL: ' + str(error)
        raise
    finally:
        (root / 'result.json').write_text(json.dumps({'status': status, 'phase': args.phase, 'runs': results,
            'scope': 'Catalogs only. FP compares pre-phase parent; 7955aaa lacks FP preview API. '
                     'Historical FP journeys, full suites, visual review and performance remain separate.'},
            indent=2) + '\n')
    print(status, flush=True)


if __name__ == '__main__':
    main()
