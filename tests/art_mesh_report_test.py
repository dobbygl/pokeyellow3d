import tempfile
from pathlib import Path
import unittest

from art_mesh_report import SUCCESS, compare, parse


def log(ms="1.000000", count=38):
    rows = ["[ART-GPU] Hardware test GPU"]
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
                r.write_text(log())
                c.write_text(log("1.400000"))
                ref.append(r)
                cand.append(c)
            self.assertEqual(compare(ref, cand, "catalog")["status"], "PASS_NUMERIC_ONLY")
            # One slow map must fail even if the other 37 remain fast.
            changed = log("1.400000").replace(
                "map=0 vertices=24 bytes=1344 build=1.400000",
                "map=0 vertices=24 bytes=1344 build=2.000000")
            cand[0].write_text(changed)
            result = compare(ref, cand, "catalog")
            self.assertEqual(result["status"], "FAIL_MESH_BUDGET")
            self.assertAlmostEqual(result["maps"][0]["ratio"], 1.6)
            self.assertEqual(len(result["maps"][0]["candidate_samples_ms"]), 30)
            with self.assertRaises(ValueError):
                compare(ref, [cand[0]] * 3, "catalog")
            cand[0].write_text(log("1.400000").replace("map=37", "map=99"))
            with self.assertRaises(ValueError):
                compare(ref, cand, "catalog")


if __name__ == "__main__":
    unittest.main()
