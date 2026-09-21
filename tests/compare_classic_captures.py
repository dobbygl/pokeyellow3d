#!/usr/bin/env python3
"""Compare two private QA directories without storing cartridge data in Git."""
import argparse
import hashlib
import json
from pathlib import Path


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('baseline', type=Path)
    parser.add_argument('candidate', type=Path)
    parser.add_argument('--report', required=True, type=Path)
    parser.add_argument('--settings-ui-changed', action='store_true',
                        help='Report the two runtime Esc settings views separately; game captures remain exact.')
    args = parser.parse_args()
    files = lambda directory: {str(p.relative_to(directory)): p for p in directory.rglob('*.ppm')}
    baseline, candidate = files(args.baseline), files(args.candidate)
    missing = sorted(baseline.keys() - candidate.keys())
    extra = sorted(candidate.keys() - baseline.keys())
    differences, settings, compared = [], [], []
    machine_differences = []
    machine_missing = []
    machine_count = 0
    for name in sorted(baseline.keys() & candidate.keys()):
        a, b = baseline[name], candidate[name]
        record = dict(path=name, baseline=digest(a), candidate=digest(b))
        if args.settings_ui_changed and a.name in ('settings.ppm', 'settings-paused.ppm'):
            settings.append(record)
        else:
            compared.append(record)
            if record['baseline'] != record['candidate']:
                differences.append(name)
        ma, mb = Path(str(a) + '.machine'), Path(str(b) + '.machine')
        if ma.exists() != mb.exists():
            machine_missing.append(name)
        if ma.exists() and mb.exists():
            machine_count += 1
            if digest(ma) != digest(mb):
                machine_differences.append(name)
    report = dict(baseline=str(args.baseline), candidate=str(args.candidate),
                  compared=len(compared), differences=differences, missing=missing, extra=extra,
                  runtime_settings=settings, hashes=compared, machine_pairs=machine_count,
                  machine_differences=machine_differences, machine_missing=machine_missing)
    args.report.parent.mkdir(parents=True, exist_ok=True)
    args.report.write_text(json.dumps(report, indent=2) + '\n')
    print(f"Compared {len(compared)} game captures: {len(differences)} differences, "
          f"{len(missing)} missing, {len(extra)} extra; "
          f"{len(settings)} runtime settings views recorded separately; "
          f"{machine_count} engine states, {len(machine_differences)} differences")
    if not compared or missing or extra or differences or machine_differences or machine_missing:
        raise SystemExit(1)


if __name__ == '__main__':
    main()
