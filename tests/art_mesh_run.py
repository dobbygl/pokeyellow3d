#!/usr/bin/env python3
"""Run isolated mesh-build pairs, alternating reference and candidate.

Writes one log per execution and a manifest recording order, commands,
binary and log hashes, wall-clock bounds and system load around each run.
art_mesh_report.py only accepts logs described by such a manifest.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import time


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def system():
    values = {}
    for name in ("/proc/loadavg", "/proc/pressure/cpu", "/proc/pressure/io"):
        try:
            values[name] = Path(name).read_text()
        except OSError as error:
            values[name] = f"unavailable: {error}"
    return values


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--reference", type=Path, required=True)
    parser.add_argument("--candidate", type=Path, required=True)
    parser.add_argument("--rom", type=Path, required=True)
    parser.add_argument("--state", type=Path, required=True)
    parser.add_argument("--mode", choices=("catalog", "interior-catalog"), required=True)
    parser.add_argument("--pairs", type=int, default=3)
    parser.add_argument("--hour", default="12")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if args.pairs < 3:
        parser.error("at least three pairs are required")
    args.output.mkdir(parents=True)  # Never reuse a directory.
    binaries = {"reference": args.reference.resolve(), "candidate": args.candidate.resolve()}
    hashes = {role: digest(path) for role, path in binaries.items()}
    if hashes["reference"] == hashes["candidate"]:
        parser.error("reference and candidate are the same binary")
    env = {**os.environ, "SDL_VIDEODRIVER": "offscreen", "SDL_AUDIODRIVER": "dummy"}
    for name in ("LIBGL_ALWAYS_SOFTWARE", "QA_ART_PASS", "QA_FIXED_HOUR"):
        env.pop(name, None)
    runs = []
    for pair in range(args.pairs):
        for role in ("reference", "candidate") if pair % 2 == 0 else ("candidate", "reference"):
            log = args.output / f"pair-{pair}-{role}.log"
            command = [str(binaries[role]), str(args.rom), str(args.state), f"mesh-{args.mode}",
                       "off" if role == "reference" else "on", args.hour]
            before = system()
            started = time.time()
            with log.open("w") as stream:
                subprocess.run(command, env=env, stdout=stream, stderr=subprocess.STDOUT,
                               check=True)
            ended = time.time()
            runs.append({"pair": pair, "role": role, "command": command,
                         "binary_sha256": hashes[role], "log": log.name,
                         "log_sha256": digest(log), "started_unix": started,
                         "ended_unix": ended, "system_before": before, "system_after": system()})
            print("DONE", log.name, flush=True)
    manifest = {"mode": args.mode, "hour": args.hour, "pairs": args.pairs,
                "rom_sha256": digest(args.rom), "state_sha256": digest(args.state),
                "runs": runs}
    (args.output / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    print(args.output / "manifest.json")


if __name__ == "__main__":
    main()
