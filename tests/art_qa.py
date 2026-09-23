#!/usr/bin/env python3
"""Art QA: real GPU catalogs, exact guest state and paired reference timing.

Run without other GPU workloads. All generated images and fixtures stay private.
The reference executable must be the frozen 7955aaa renderer, not a previous phase.
"""
import argparse
import gzip
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
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('smoke', 'benchmark', 'rom', 'pallet', 'route', 'baseline', 'output'):
        parser.add_argument(name, type=Path)
    parser.add_argument('phase', nargs='?', choices=('A1', 'B1', 'C1'), default='A1')
    parser.add_argument('--captures-only', action='store_true',
                        help='Do not certify timing; write CAPTURES_PASS rather than PASS')
    parser.add_argument('--reference-smoke', type=Path,
                        help='Frozen v0.4.1 smoke executable; required for C1')
    parser.add_argument('--compress-captures', action='store_true',
                        help='Losslessly gzip completed capture groups after hashing')
    args = parser.parse_args()
    phase = args.phase
    if phase == 'C1' and not args.reference_smoke:
        parser.error('C1 requires --reference-smoke for the independent OFF pixel gate')
    candidate, benchmark, rom, pallet, route, baseline, output = (
        getattr(args, name).resolve() for name in
        ('smoke', 'benchmark', 'rom', 'pallet', 'route', 'baseline', 'output'))
    output.mkdir(parents=True, exist_ok=True)
    root = Path(tempfile.mkdtemp(prefix=f'art-{phase.lower()}-', dir=output))
    print(f'QA output: {root}', flush=True)
    for name, source in [('candidate', candidate), ('benchmark', benchmark), ('reference', baseline),
                         ('cartridge.gbc', rom), ('pallet.state', pallet), ('route.state', route)]:
        shutil.copy2(source, root / name)
    provenance = {name: sha(root / name) for name in
                  ('candidate', 'benchmark', 'reference', 'cartridge.gbc', 'pallet.state', 'route.state')}
    if args.reference_smoke:
        shutil.copy2(args.reference_smoke, root / 'reference-smoke')
        provenance['reference-smoke'] = sha(root / 'reference-smoke')
    (root / 'inputs.json').write_text(json.dumps(provenance, indent=2) + '\n')
    env = os.environ.copy()
    env.update(SDL_VIDEODRIVER='offscreen', SDL_AUDIODRIVER='dummy', QA_CAPTURE_STATES='1')
    env.pop('QA_GLYPH_ORACLE', None)
    env.pop('QA_FULL_NEGATIVES', None)
    env.pop('QA_FIXED_HOUR', None)
    results = []
    previous = None

    def compress(directory):
        if not args.compress_captures or directory is None:
            return
        for pattern in ('*.ppm', '*.machine'):
            for path in directory.rglob(pattern):
                compressed = path.with_name(path.name + '.gz')
                with path.open('rb') as source, gzip.open(compressed, 'wb') as target:
                    shutil.copyfileobj(source, target)
                with gzip.open(compressed, 'rb') as source:
                    assert hashlib.file_digest(source, 'sha256').hexdigest() == sha(path)
                path.unlink()  # Verified reversible encoding; raw bytes retained in gzip.

    def run(label, binary, fixture, mode, *arguments, **settings):
        nonlocal previous
        compress(previous)
        directory = root / label
        directory.mkdir()
        previous = directory
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
                mesh_counts = {}
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
                    mesh_counts[enabled] = {m: int(v) for m, v, _ in records}
                    results.append(dict(label=label, images=images, states=states,
                                        meshes=[dict(map=int(m), vertices=int(v), bytes=int(b))
                                                for m, v, b in records]))
                assert captures['off'][1] == captures['on'][1], 'art changed engine state'
                if phase != 'C1' or kind == 'interior-catalog':
                    assert captures['off'][0] == captures['on'][0], (style, camera, kind)
                else:
                    assert captures['off'][0] != captures['on'][0], 'C1 geometry never appeared'
                    assert mesh_counts['off'].keys() == mesh_counts['on'].keys()
                    assert all(mesh_counts['on'][m] <= 2 * n for m, n in mesh_counts['off'].items()), 'C1 vertex budget exceeded'
                # v0.4.1 has no diagnostic FP catalog API. Compare its real
                # FP journey below; do not relabel a later renderer as v0.4.1.
                if args.reference_smoke and camera == 'ortho':
                    mode = kind + ('-fp' if camera == 'fp' else '')
                    directory, _ = run(f'{style}-{camera}-{kind}-reference', 'reference-smoke',
                                       'pallet.state', mode, QA_MENU_STYLE=style, QA_ART_PASS='off',
                                       CATALOG_CAPTURE_DIR='captures')
                    reference_images = {p.name: sha(p) for p in (directory / 'captures').glob('*.ppm')}
                    reference_states = {p.name: sha(p) for p in (directory / 'captures').glob('*.machine')}
                    assert (reference_images, reference_states) == captures['off'], (style, camera, kind, 'OFF differs from frozen reference')
            if phase in ('B1', 'C1'):
                for catalog_hour in ((6.5, 12, 21) if phase == 'C1' else (12,)):
                    noon = {}
                    for enabled in ('off', 'on'):
                        hour_label = 'noon' if catalog_hour == 12 else f'hour-{catalog_hour}'
                        label = f'{style}-{camera}-catalog-{hour_label}-{enabled}'
                        mode = 'catalog' + ('-fp' if camera == 'fp' else '') + ('-art' if enabled == 'on' else '')
                        directory, _ = run(label, 'candidate', 'pallet.state', mode,
                                           QA_MENU_STYLE=style, QA_ART_PASS=enabled,
                                           QA_FIXED_HOUR=str(catalog_hour), CATALOG_CAPTURE_DIR='captures')
                        images = {p.name: sha(p) for p in (directory / 'captures').glob('*.ppm')}
                        states = {p.name: sha(p) for p in (directory / 'captures').glob('*.machine')}
                        assert len(images) == len(states) == 38
                        noon[enabled] = (images, states)
                        results.append(dict(label=label, images=images, states=states))
                    assert noon['off'][1] == noon['on'][1], 'sunlit catalog changed engine state'
                    assert noon['off'][0] != noon['on'][0], 'lit catalog must exercise shadows or C1 geometry'
            journey_off = None
            for enabled in ('off', 'on'):
                mode = 'firstperson' if camera == 'fp' else 'journey'
                if enabled == 'on':
                    mode += '-art'
                _, text = run(f'{style}-{camera}-journey-{enabled}', 'candidate', 'pallet.state', mode,
                              QA_MENU_STYLE=style, QA_ART_PASS=enabled)
                assert 'PASS' in text
                digest = re.findall(r'\[JOURNEY\] final_wram=([0-9a-f]+)', text)
                assert len(digest) == 1, 'journey must complete and emit its engine digest'
                if enabled == 'off':
                    journey_off = digest
                else:
                    assert digest == journey_off, 'ON journey changed final engine RAM'
            if args.reference_smoke:
                mode = 'firstperson' if camera == 'fp' else 'journey'
                _, text = run(f'{style}-{camera}-journey-reference', 'reference-smoke',
                                   'pallet.state', mode, QA_MENU_STYLE=style, QA_ART_PASS='off')
                assert re.findall(r'\[JOURNEY\] final_wram=([0-9a-f]+)', text) == journey_off, (style, camera, 'OFF journey engine differs from v0.4.1')
                if camera == 'fp':
                    views = {}
                    for binary in ('candidate', 'reference-smoke'):
                        directory, _ = run(f'{style}-fp-views-{binary}', binary, 'pallet.state',
                                           'fp-views', QA_MENU_STYLE=style, QA_ART_PASS='off',
                                           FP_CAPTURE_DIR='captures')
                        views[binary] = {p.name: sha(p) for p in (directory / 'captures').iterdir()
                                         if p.suffix in ('.ppm', '.machine')}
                        assert len(views[binary]) == 8, 'four FP orientations plus full snapshots'
                    assert views['candidate'] == views['reference-smoke'], 'OFF FP pixels or machine state differ from v0.4.1'
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
            assert hours['off'][1] == hours['on'][1], (style, camera, 'hour engine states differ')
            for name, digest in hours['off'][0].items():
                if phase == 'C1':
                    assert digest != hours['on'][0][name], (style, camera, name, 'C1 silhouette absent')
                elif phase == 'A1' or name != 'hour-12.ppm':
                    assert digest == hours['on'][0][name], (style, camera, name)
                else:
                    assert digest != hours['on'][0][name], 'noon must exercise visible shadows'

    if args.captures_only:
        compress(previous)
        for name, expected in provenance.items():
            assert sha(root / name) == expected
        report = dict(status='CAPTURES_PASS', phase=phase, inputs=provenance, catalogs=results,
                      performance=[], reviewed=False,
                      note='Timing and visual review remain required. No phase acceptance.')
        (root / 'result.json').write_text(json.dumps(report, indent=2) + '\n')
        print(f'CAPTURES_PASS: {root}; timing not run')
        return

    # Same scene, driver, process setup and synchronization. Alternate order to
    # expose warm-up/drift rather than comparing against an unrelated old number.
    performance = []
    gpu_names = set()
    scenarios = [(mode, hour) for mode in ('catalog', 'interior-catalog', 'ortho', 'fp')
                 for hour in ((-1,) if phase == 'A1' else
                              (12,) if mode == 'interior-catalog' else (6.5, 12, 17.5))]
    for mode, hour in scenarios:
        groups = {'reference': [], 'candidate': []}
        for repeat in range(3):
            order = ('reference', 'candidate') if repeat % 2 == 0 else ('candidate', 'reference')
            for binary in order:
                fixture = 'pallet.state' if 'catalog' in mode else 'route.state'
                animate = phase in ('B1', 'C1') and mode in ('ortho', 'fp')
                _, text = run(f'perf-{mode}-{hour}-{repeat}-{binary}',
                              'benchmark' if binary == 'candidate' else 'reference', fixture, mode,
                              'on' if binary == 'candidate' else 'off', str(hour),
                              *(['animate'] if animate else []))
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
                if phase in ('B1', 'C1') and binary == 'candidate':
                    shadows = re.findall(r'\[ART-SHADOW\] active=(\d+) passes=(\d+) animate=(\d+)', text)
                    assert len(shadows) == len(entries)
                    assert all(int(active) == (mode != 'interior-catalog') and
                               int(moving) == animate and int(passes) == (100 if animate else 0)
                               for active, passes, moving in shadows)
                groups[binary].append(samples)
        means = {key: statistics.mean(n for run in values for n in run) for key, values in groups.items()}
        ratio = means['candidate'] / means['reference']
        assert ratio <= 2.0, (mode, ratio)
        performance.append(dict(mode=mode, hour=hour, animated=phase in ('B1', 'C1') and mode in ('ortho', 'fp'),
                                samples_ms=groups, mean_ms=means, ratio=ratio,
                                statistic='arithmetic mean of synchronized presentation batches; invariant checks excluded'))
    assert len(gpu_names) == 1, gpu_names
    compress(previous)
    for name, expected in provenance.items():
        assert sha(root / name) == expected
    report = dict(status='PASS', phase=phase, inputs=provenance, catalogs=results, performance=performance,
                  gpu=sorted(gpu_names), reviewed=False, note='Contacts must be reviewed separately before phase acceptance.')
    (root / 'result.json').write_text(json.dumps(report, indent=2) + '\n')
    print(f'PASS: {phase} art reference, both cameras/styles and paired performance; {root}')


if __name__ == '__main__':
    main()
