#!/usr/bin/env python3
"""Strict numerical review of isolated mesh-build logs (not environment acceptance)."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import statistics

MARKER = re.compile(r"\[ART-MESH\] mode=(catalog|interior-catalog) map=(\d+) repeat=(\d+)$")
BUILD = re.compile(r"\[3D\] mesh map=(\d+) vertices=(\d+) bytes=(\d+) "
                   r"build=(\d+\.\d{6})ms resident=(\d+)$")
SUCCESS = "PASS: isolated mesh builds, memory intact and zero GL errors per frame"


def require(condition, message):
    if not condition:
        raise ValueError(message)


def parse(text, mode):
    """Only the build following an ART-MESH marker is measured, not the eviction map."""
    count = {"catalog": 38, "interior-catalog": 179}[mode]
    require(text.splitlines().count(SUCCESS) == 1, "missing or repeated success marker")
    require(not re.search(r"\b(?:FAIL|SKIP)\b", text), "failed or skipped execution")
    gpu = re.findall(r"^\[ART-GPU\] (.+)$", text, re.M)
    require(len(gpu) == 1 and "llvmpipe" not in gpu[0].lower(), "missing or software GPU")
    pending = None
    maps = {}
    for line in text.splitlines():
        marker = MARKER.fullmatch(line)
        if marker:
            require(pending is None, "measurement without its build")
            row_mode, mid, repeat = marker.groups()
            require(row_mode == mode, "wrong catalog mode")
            pending = (int(mid), int(repeat))
        elif line.startswith("[ART-MESH]"):
            raise ValueError("malformed measurement marker")
        elif line.startswith("[3D] mesh") and pending is not None:
            build = BUILD.fullmatch(line)
            require(build is not None, "build stamp must have six fractional digits")
            mid, vertices, size, ms, resident = build.groups()
            mid, vertices, size, ms = int(mid), int(vertices), int(size), float(ms)
            require(mid == pending[0] and resident == "1", "wrong or non-isolated map")
            require(vertices > 0 and size == vertices * 56 and ms > 0, "invalid build")
            samples = maps.setdefault(mid, {})
            require(pending[1] not in samples, "duplicate repeat")
            samples[pending[1]] = {"ms": ms, "vertices": vertices}
            pending = None
        elif line == SUCCESS:
            require(pending is None, "unfinished measurement at success")
    require(pending is None and len(maps) == count, "incomplete catalog")
    for samples in maps.values():
        require(set(samples) == set(range(10)), "expected ten distinct repeats per map")
        require(len({s["vertices"] for s in samples.values()}) == 1,
                "vertex count changed between repeats")
    return gpu[0], maps


def compare(reference, candidate, mode):
    require(len(reference) == len(candidate) and len(reference) >= 3,
            "at least three complete pairs are required")
    groups = {}
    files = {}
    gpu_names = set()
    roster = None
    for name, paths in (("reference", reference), ("candidate", candidate)):
        maps = {}
        for path in paths:
            path = Path(path)
            key = str(path.resolve())
            require(key not in files, "a log was supplied more than once")
            raw = path.read_bytes()
            files[key] = hashlib.sha256(raw).hexdigest()
            gpu, rows = parse(raw.decode(), mode)
            gpu_names.add(gpu)
            require(roster is None or set(rows) == roster, "map inventory differs")
            roster = set(rows)
            for mid, repeats in rows.items():
                maps.setdefault(mid, []).extend(repeats[i]["ms"] for i in range(10))
        groups[name] = maps
    require(len(gpu_names) == 1, "GPU differs between executions")
    rows = []
    for mid in sorted(roster):
        ref, cand = groups["reference"][mid], groups["candidate"][mid]
        ref_mean, cand_mean = statistics.mean(ref), statistics.mean(cand)
        rows.append({"map": mid, "reference_ms": ref_mean, "candidate_ms": cand_mean,
                     "ratio": cand_mean / ref_mean,
                     "reference_samples_ms": ref, "candidate_samples_ms": cand})
    return {"status": "PASS_NUMERIC_ONLY" if all(r["ratio"] <= 1.5 for r in rows)
            else "FAIL_MESH_BUDGET", "mode": mode, "maps": rows,
            "gpu": next(iter(gpu_names)), "input_sha256": files,
            "statistic": "Arithmetic mean of every sample; no filtering",
            "scope": "Numerical gate only. Verify paired scheduling, source provenance and "
                     "CPU/GPU environment separately before accepting B2."}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mode", choices=("catalog", "interior-catalog"), required=True)
    parser.add_argument("--reference", nargs="+", type=Path, required=True)
    parser.add_argument("--candidate", nargs="+", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    try:
        report = compare(args.reference, args.candidate, args.mode)
        with args.output.open("x") as stream:
            json.dump(report, stream, indent=2)
            stream.write("\n")
    except (ValueError, OSError) as error:
        parser.exit(1, f"FAIL: {error}\n")
    print(report["status"])
    return 0 if report["status"] == "PASS_NUMERIC_ONLY" else 1


if __name__ == "__main__":
    raise SystemExit(main())
