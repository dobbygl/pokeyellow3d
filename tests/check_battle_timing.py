#!/usr/bin/env python3
"""Check C2's canonical private Route 1 and Route 22 recordings."""
import csv
import sys
from pathlib import Path


def read(path):
    with path.open() as source:
        return list(csv.DictReader(source))


def verify(root):
    engine_fields = ("frame", "battle_frame", "cycles", "battle", "bgp", "lcdc",
                     "scx", "scy", "wy", "wx", "first", "enemy_party_pos",
                     "black_tiles", "enemy_rect", "player_rect", "ready",
                     "sp", "bank", "stack", "displayed_tiles")
    for scenario, wipe_frames, flashes in (("wild", 30, 108), ("trainer", 153, 0)):
        recordings = []
        for camera in ("ortho", "fp"):
            name = f"{scenario}-{camera}"
            rows = read(root / name / "logs/battle-timing.csv")
            assert rows and all(r["covered"] == "1" for r in rows), f"{name}: 2D gap"
            hud = next(int(r["battle_frame"]) for r in rows if r["hud"] == "1")
            arena = next(int(r["battle_frame"]) for r in rows if r["arena"] == "1")
            assert arena == hud, f"{name}: arena {arena} != original HUD {hud}"
            wipe = [r for r in rows if r["phase"] == "3"]
            assert len(wipe) == wipe_frames, f"{name}: wrong wipe duration"
            assert sum(r["phase"] == "2" for r in rows) == flashes, f"{name}: wrong flash duration"
            progress = [float(r["wipe"]) for r in wipe]
            assert progress == sorted(progress) and 0 < progress[0] <= progress[-1] <= 1
            assert any(r["phase"] == "7" and r["battle"] == "0" for r in rows), f"{name}: exit untested"
            if scenario == "trainer":
                assert all(r["battle"] == "0" for r in wipe), f"{name}: pre-flag wipe untested"
            recordings.append([tuple(r[k] for k in engine_fields) for r in rows])
            print(f"PASS {name}: {len(rows)} covered frames; HUD/arena {hud}; wipe {len(wipe)}; flashes {flashes}")
        assert recordings[0] == recordings[1], f"{scenario}: cameras changed the original engine trace"
        print(f"PASS {scenario}: identical original engine, stack and displayed tiles in both cameras")


if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit("Usage: check_battle_timing.py QA_DIRECTORY")
    verify(Path(sys.argv[1]))
