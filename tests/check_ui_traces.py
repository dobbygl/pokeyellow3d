#!/usr/bin/env python3
"""Check private per-frame traces; no ROM or state data is embedded here."""
import csv
import itertools
import sys
from pathlib import Path


def rows(path):
    with Path(path).open() as stream:
        return list(csv.DictReader(stream))


def compare(original, rendered):
    before, after = rows(original), rows(rendered)
    fields = ("frame", "cycles", "map", "x", "y", "view", "bgp", "lcdc", "font", "sprites", "battle")
    assert len(before) == len(after), "engine frame count changed"
    for a, b in zip(before, after):
        assert all(a[k] == b[k] for k in fields), f"engine trace changed at frame {a['frame']}"
    print(f"PASS: {len(after)} engine frames exactly match the original presentation trace")


def check(path):
    trace = rows(path)
    segments = []
    for composing, group in itertools.groupby(trace, lambda r: r["warp"] == "1"):
        segment = list(group)
        if not composing:
            continue
        assert all(r["covered"] == r["active"] == "1" for r in segment), "2D gap in composed warp"
        stages = [(int(key), len(list(g))) for key, g in itertools.groupby(segment, lambda r: r["bgp"])]
        if any(p == 0xf9 for p, n in stages):
            assert stages == [(0xe4, 8), (0xf9, 9), (0xfe, 9), (0xff, 21)], stages
        elif any(p == 0x40 for p, n in stages):
            # GBFadeInFromWhite also waits nine frames at normal E4 before
            # returning. They belong to the live routine, even though view()
            # already considers the map visible during that last step.
            assert [p for p, n in stages] == [0, 0x40, 0x90, 0xe4], stages
            assert stages[1:] == [(0x40, 9), (0x90, 9), (0xe4, 9)], stages
        else:
            raise AssertionError(f"unmeasured transition sequence: {stages}")
        segments.append(stages)
    assert segments, "no composed transitions"
    print(f"PASS: {path}: {len(segments)} BGP sequences, original frame durations and full coverage")


if __name__ == "__main__":
    if len(sys.argv) == 4 and sys.argv[1] == "--compare":
        compare(sys.argv[2], sys.argv[3])
    else:
        assert len(sys.argv) > 1, "Usage: check_ui_traces.py TRACE... | --compare ORIGINAL RENDERED"
        for path in sys.argv[1:]:
            check(path)
