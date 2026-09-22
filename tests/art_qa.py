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
    if len(sys.argv) != 8:
        raise SystemExit('Expected smoke, benchmark, ROM, Pallet state, five-map state, baseline benchmark, output root')
    candidate, benchmark, rom, pallet, route, baseline, output = map(Path, sys.argv[1:])
    output.mkdir(parents=True, exist_ok=True)
    root = Path(tempfile.mkdtemp(prefix='art-a1-', dir=output))
    print(f'QA output: {root}', flush=True)
    for name, source in [('candidate', candidate), ('benchmark', benchmark), ('reference', baseline),
                         ('cartridge.gbc', rom), ('pallet.state', pallet), ('route.state', route)]:
        shutil.copy2(source, root / name)
    provenance = {name: sha(root / name) for name in
                  ('candidate', 'benchmark', 'reference', 'cartridge.gbc', 'pallet.state', 'route.state')}
    (root / 'inputs.json').write_text(json.dumps(provenance, indent=2) + '\n')
    env = os.environ.copy()
    env.update(SDL_VIDEODRIVER='offscreen', SDL_AUDIODRIVER='dummy', QA_CAPTURE_STATES='1')
    env.pop('QA_GLYPH_ORACLE', None)
    env.pop('QA_FULL_NEGATIVES', None)
    results = []

    def run(label, binary, fixture, mode, *arguments, **settings):
        directory = root / label
        directory.mkdir()
        (directory / "logs").mkdir()
        captures = directory / 'captures'
        captures.mkdir()
        with (directory / 'run.log').open('w') as log:
            subprocess.run([str(root / binary), str(root / 'cartridge.gbc'), str(root / fixture), mode, *arguments],
                           cwd=directory, env={**env, **settings}, stdout=log,
                           stderr=subprocess.STDOUT, check=True)
        text = (directory / 'run.log').read_text()
        if 'FAIL' in text or 'SKIP' in text:
            raise AssertionError(label)
        return directory, text

    for style in ('classic', 'integrated'):
        for enabled in ('off', 'on'):
            config = str(root / f'preferences-{style}-{enabled}.cfg')
            for operation in ('write', 'read'):
                _, text = run(f'preferences-{style}-{enabled}-{operation}', 'candidate',
                              'pallet.state', f'preferences-{operation}', config, style, enabled,
                              QA_ART_PASS='on' if enabled == 'off' else 'off')
                assert 'PASS: presentation preferences survive process restart' in text
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
            hours = {}
            for enabled in ('off', 'on'):
                mode = 'daylight' + ('-fp' if camera == 'fp' else '')
                directory, text = run(f'{style}-{camera}-hours-{enabled}', 'candidate',
                                      'pallet.state', mode, QA_MENU_STYLE=style,
                                      QA_ART_PASS=enabled)
                assert 'PASS: four hours' in text
                # Esc is a runtime settings panel: its checkbox intentionally
                # differs. Every game image and engine snapshot remains exact.
                images = {p.name: sha(p) for p in (directory / 'logs').glob('*.ppm')
                          if p.name != 'settings.ppm'}
                states = {p.name: sha(p) for p in (directory / 'logs').glob('*.machine')}
                assert len(images) == 6 and len(states) == 7
                hours[enabled] = (images, states)
                results.append(dict(label=f'{style}-{camera}-hours-{enabled}',
                                    images=images, states=states))
            assert hours['off'] == hours['on'], (style, camera, 'A1 hour captures differ')

    # Same scene, driver, process setup and synchronization. Alternate order to
    # expose warm-up/drift rather than comparing against an unrelated old number.
    performance = []
    gpu_names = set()
    for mode in ('catalog', 'interior-catalog', 'ortho', 'fp'):
        groups = {'reference': [], 'candidate': []}
        for repeat in range(3):
            order = ('reference', 'candidate') if repeat % 2 == 0 else ('candidate', 'reference')
            for binary in order:
                fixture = 'pallet.state' if 'catalog' in mode else 'route.state'
                _, text = run(f'perf-{mode}-{repeat}-{binary}',
                              'benchmark' if binary == 'candidate' else 'reference', fixture, mode,
                              'on' if binary == 'candidate' else 'off', '-1')
                entries = re.findall(r'\[ART-PERF\] mode=(\S+) map=(-?\d+) sample=(\d+) frames=(\d+) mean_ms=([0-9.]+) vertices=(\d+)', text)
                batches = 3 if 'catalog' in mode else 7
                maps = 179 if mode == 'interior-catalog' else 38 if mode == 'catalog' else 1
                assert len(entries) == maps * batches
                assert len({entry[1] for entry in entries}) == maps
                for map_id in {entry[1] for entry in entries}:
                    assert {int(e[2]) for e in entries if e[1] == map_id} == set(range(batches))
                assert all(e[0] == mode and int(e[3]) == (30 if 'catalog' in mode else 100)
                           and int(e[5]) > 0 for e in entries)
                samples = [float(e[4]) for e in entries]
                gpu = re.findall(r'\[ART-GPU\] (.+)', text)
                assert len(gpu) == 1
                gpu_names.add(gpu[0])
                assert all(n > 0 for n in samples)
                groups[binary].append(samples)
        means = {key: statistics.mean(n for run in values for n in run) for key, values in groups.items()}
        ratio = means['candidate'] / means['reference']
        assert ratio <= 2.0, (mode, ratio)
        performance.append(dict(mode=mode, samples_ms=groups, mean_ms=means, ratio=ratio,
                                statistic='arithmetic mean of synchronized presentation batches; invariant checks excluded'))
    assert len(gpu_names) == 1, gpu_names
    for name, expected in provenance.items():
        assert sha(root / name) == expected
    report = dict(status='PASS', phase='A1', inputs=provenance, catalogs=results, performance=performance,
                  gpu=sorted(gpu_names), reviewed=False, note='Contacts must be reviewed separately before phase acceptance.')
    (root / 'result.json').write_text(json.dumps(report, indent=2) + '\n')
    print(f'PASS: A1 art reference, both cameras/styles and paired performance; {root}')


if __name__ == '__main__':
    main()
