#!/usr/bin/env python3
"""A1: real GPU catalogs in both cameras/styles, exact toggles and paired timing.

Run without other GPU workloads. All generated images and fixtures stay private.
The reference executable must be the frozen 7955aaa renderer, not a previous phase.
"""
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import statistics
import subprocess
import sys
import tempfile


def sha(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def main():
    if len(sys.argv) != 7:
        raise SystemExit('Expected candidate, ROM, Pallet state, five-map state, baseline, output root')
    candidate, rom, pallet, route, baseline, output = map(Path, sys.argv[1:])
    output.mkdir(parents=True, exist_ok=True)
    root = Path(tempfile.mkdtemp(prefix='art-a1-', dir=output))
    print(f'QA output: {root}', flush=True)
    for name, source in [('candidate', candidate), ('reference', baseline),
                         ('cartridge.gbc', rom), ('pallet.state', pallet), ('route.state', route)]:
        shutil.copy2(source, root / name)
    provenance = {name: sha(root / name) for name in
                  ('candidate', 'reference', 'cartridge.gbc', 'pallet.state', 'route.state')}
    (root / 'inputs.json').write_text(json.dumps(provenance, indent=2) + '\n')
    env = os.environ.copy()
    env.update(SDL_VIDEODRIVER='offscreen', SDL_AUDIODRIVER='dummy', QA_CAPTURE_STATES='1')
    env.pop('QA_GLYPH_ORACLE', None)
    env.pop('QA_FULL_NEGATIVES', None)
    results = []

    def run(label, binary, fixture, mode, **settings):
        directory = root / label
        directory.mkdir()
        (directory / "logs").mkdir()
        captures = directory / 'captures'
        captures.mkdir()
        with (directory / 'run.log').open('w') as log:
            subprocess.run([str(root / binary), str(root / 'cartridge.gbc'), str(root / fixture), mode],
                           cwd=directory, env={**env, **settings}, stdout=log,
                           stderr=subprocess.STDOUT, check=True)
        text = (directory / 'run.log').read_text()
        if 'FAIL' in text or 'SKIP' in text:
            raise AssertionError(label)
        return directory, text

    for style in ('classic', 'integrated'):
        for camera in ('ortho', 'fp'):
            for kind, count in (('catalog', 38), ('interior-catalog', 179)):
                captures = {}
                for enabled in ('off', 'on'):
                    label = f'{style}-{camera}-{kind}-{enabled}'
                    mode = kind + ('-fp' if camera == 'fp' else '') + ('-art' if enabled == 'on' else '')
                    directory, text = run(label, 'candidate', 'pallet.state', mode,
                                          QA_MENU_STYLE=style, QA_ART_PASS=enabled,
                                          CATALOG_CAPTURE_DIR='captures')
                    images = {p.name: sha(p) for p in sorted((directory / 'captures').glob('*.ppm'))}
                    states = {p.name: sha(p) for p in sorted((directory / 'captures').glob('*.machine'))}
                    assert len(images) == len(states) == count, (label, len(images), len(states))
                    records = re.findall(r'\[CATALOG\] map=(\d+) vertices=(\d+) bytes=(\d+)', text)
                    assert len(records) == count and all(int(n) > 0 for _, n, _ in records)
                    captures[enabled] = (images, states)
                    results.append(dict(label=label, images=images, states=states,
                                        meshes=[dict(map=int(m), vertices=int(v), bytes=int(b))
                                                for m, v, b in records]))
                assert captures['off'] == captures['on'], (style, camera, kind, 'A1 changes pixels or memory')
            for enabled in ('off', 'on'):
                mode = 'firstperson' if camera == 'fp' else 'journey'
                if enabled == 'on':
                    mode += '-art'
                _, text = run(f'{style}-{camera}-journey-{enabled}', 'candidate', 'pallet.state', mode,
                              QA_MENU_STYLE=style, QA_ART_PASS=enabled)
                assert 'PASS' in text

    # Same scene, driver, process setup and synchronization. Alternate order to
    # expose warm-up/drift rather than comparing against an unrelated old number.
    performance = []
    for mode in ('catalog', 'interior-catalog', 'ortho-benchmark', 'fp-benchmark'):
        groups = {'reference': [], 'candidate': []}
        for repeat in range(3):
            order = ('reference', 'candidate') if repeat % 2 == 0 else ('candidate', 'reference')
            for binary in order:
                fixture = 'pallet.state' if 'catalog' in mode else 'route.state'
                _, text = run(f'perf-{mode}-{repeat}-{binary}', binary, fixture, mode,
                              QA_MENU_STYLE='integrated', QA_ART_PASS='on' if binary == 'candidate' else 'off',
                              CATALOG_BENCH_FRAMES='60')
                if 'catalog' in mode:
                    samples = [float(n) for n in re.findall(r'\[PERF\].*mean_ms=([0-9.]+)', text)]
                    assert len(samples) == (179 if mode == 'interior-catalog' else 38)
                else:
                    samples = [float(n) for n in re.findall(r'\[FPBENCH\].*median=([0-9.]+)', text)]
                    assert len(samples) == 1
                assert all(n > 0 for n in samples)
                groups[binary].append(samples)
        means = {key: statistics.mean(n for run in values for n in run) for key, values in groups.items()}
        ratio = means['candidate'] / means['reference']
        assert ratio <= 2.0, (mode, ratio)
        performance.append(dict(mode=mode, samples_ms=groups, mean_ms=means, ratio=ratio,
                                statistic='mean of map presentation means' if 'catalog' in mode else
                                          'mean of three process medians (seven 100-frame batches each)'))
    for name, expected in provenance.items():
        assert sha(root / name) == expected
    report = dict(status='PASS', phase='A1', inputs=provenance, catalogs=results, performance=performance,
                  reviewed=False, note='Contacts must be reviewed separately before phase acceptance.')
    (root / 'result.json').write_text(json.dumps(report, indent=2) + '\n')
    print(f'PASS: A1 art reference, both cameras/styles and paired performance; {root}')


if __name__ == '__main__':
    main()
