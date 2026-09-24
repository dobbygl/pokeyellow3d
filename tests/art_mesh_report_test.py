import hashlib
import json
import tempfile
from pathlib import Path
import unittest

from art_mesh_report import SUCCESS, compare, parse, schedule


def log(ms="1.000000", count=38, run=0):
    # Real executions never produce identical bytes; a copied log must not pass.
    rows = ["[ART-GPU] Hardware test GPU", f"[3D] ROM catalog: run {run}"]
    for mid in range(count):
        for repeat in range(10):
            rows.extend((f"[ART-MESH] mode=catalog map={mid} repeat={repeat}",
                         f"[3D] mesh map={mid} vertices=24 bytes=1344 "
                         f"build={ms}ms resident=1"))
    return "\n".join(rows + [SUCCESS])


class MeshReportTest(unittest.TestCase):
    def test_complete_inventory(self):
        _, maps = parse(log(), "catalog")
        self.assertEqual(len(maps), 38)
        self.assertTrue(all(len(samples) == 10 for samples in maps.values()))

    def test_incomplete_and_corrupt_logs_rejected(self):
        good = log()
        cases = [log(count=37), good.replace("repeat=9", "repeat=8"),
                 good.replace("repeat=9", "repeat=10"), log(ms="1.00"), log(ms="0.000000"),
                 good.replace(SUCCESS, ""), good + "\nSKIP: unavailable context",
                 good.replace("resident=1", "resident=2", 1),
                 good.replace("bytes=1344", "bytes=1343", 1),
                 good.replace("Hardware test GPU", "llvmpipe"),
                 good.replace("mode=catalog", "mode=interior-catalog", 1),
                 good.replace("[3D] mesh map=0", "[3D] mesh map=1", 1)]
        for case in cases:
            with self.subTest(case=case[:80]), self.assertRaises(ValueError):
                parse(case, "catalog")

    def test_per_map_budget_and_unfiltered_mean(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            ref, cand = [], []
            for i in range(3):
                r, c = root / f"ref{i}.log", root / f"cand{i}.log"
                r.write_text(log(run=2 * i))
                c.write_text(log("1.400000", run=2 * i + 1))
                ref.append(r)
                cand.append(c)
            self.assertEqual(compare(ref, cand, "catalog")["status"], "PASS_NUMERIC_ONLY")
            # One slow map must fail even if the other 37 remain fast.
            changed = log("1.400000", run=1).replace(
                "map=0 vertices=24 bytes=1344 build=1.400000",
                "map=0 vertices=24 bytes=1344 build=2.000000")
            cand[0].write_text(changed)
            result = compare(ref, cand, "catalog")
            self.assertEqual(result["status"], "FAIL_MESH_BUDGET")
            self.assertAlmostEqual(result["maps"][0]["ratio"], 1.6)
            self.assertEqual(len(result["maps"][0]["candidate_samples_ms"]), 30)
            with self.assertRaises(ValueError):
                compare(ref, [cand[0]] * 3, "catalog")
            cand[0].write_text(log("1.400000", run=1).replace("map=37", "map=99"))
            with self.assertRaises(ValueError):
                compare(ref, cand, "catalog")
            # Byte-identical copies under other names are one run, not three.
            for i in range(3):
                cand[i].write_text(log("1.400000", run=1))
            with self.assertRaises(ValueError):
                compare(ref, cand, "catalog")

    def test_manifest_schedule(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)

            def write(runs):
                manifest = root / "manifest.json"
                manifest.write_text(json.dumps({"mode": "catalog", "runs": runs}))
                return manifest

            runs = []
            clock = 0.0
            for pair in range(3):
                order = ("reference", "candidate") if pair % 2 == 0 else ("candidate", "reference")
                for role in order:
                    name = f"pair-{pair}-{role}.log"
                    text = log("1.000000" if role == "reference" else "1.200000", run=len(runs))
                    (root / name).write_text(text)
                    runs.append({"pair": pair, "role": role, "log": name,
                                 "log_sha256": hashlib.sha256(text.encode()).hexdigest(),
                                 "binary_sha256": role, "started_unix": clock,
                                 "ended_unix": clock + 1})
                    clock += 2
            _, ref, cand = schedule(write(runs))
            self.assertEqual(compare(ref, cand, "catalog")["status"], "PASS_NUMERIC_ONLY")
            broken = [
                lambda r: r[:4],  # two pairs only
                lambda r: [r[1], r[0]] + r[2:],  # first pair starts with the candidate
                lambda r: r[:2] + [r[3], r[2]] + r[4:],  # second pair not alternated
                lambda r: [dict(r[0], ended_unix=2.5)] + r[1:],  # overlapping runs
                lambda r: [dict(x, binary_sha256="same") for x in r],  # one binary
                lambda r: r[:2] + [dict(r[2], binary_sha256="other")] + r[3:],
                lambda r: [dict(r[0], log_sha256="0" * 64)] + r[1:],  # tampered log
                lambda r: r[:2] + [dict(r[2], log=r[0]["log"], log_sha256=r[0]["log_sha256"])]
                + r[3:],  # a copied log reused as a later run
            ]
            for index, change in enumerate(broken):
                with self.subTest(case=index), self.assertRaises(ValueError):
                    schedule(write(change([dict(x) for x in runs])))


if __name__ == "__main__":
    unittest.main()
