import contextlib
import gzip
import hashlib
import io
import json
import sys
import tempfile
import unittest
from pathlib import Path

from run import Instance, parse_list, run

TOOL = """
import os
import signal
import sys
import time

text = open(sys.argv[1]).read()
if text == "sleep":
    time.sleep(60)
if text == "crash":
    os.kill(os.getpid(), signal.SIGKILL)
if text == "fixed" and sys.argv[3:] != ["fixed"]:
    sys.stderr.write("bad file 'source': line 1\\n")
    sys.exit(2)
if text == "broken":
    sys.stderr.write("bad file 'source': line 3\\n")
    sys.exit(2)
print("read 0.5")
if text == "unwritable":
    print("name 'DEDO3 1R' cannot be written")
    sys.exit(3)
print("write 0.25")
print("reread 0.125")
if text == "differs":
    print("function 0: term count 3 against 5")
    sys.exit(1)
"""
STAMP = {"commit": "0123abcd", "machine": {"host": "test"}}
LIMIT = 30.0
AFIRO = "https://example.org/afiro.mps.gz"
ADLITTLE = "https://example.org/adlittle.mps.gz"


def sha256_of(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


class RunTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name)
        self.cache = self.root / "cache"
        self.cache.mkdir()
        self.record = self.root / "record.jsonl"

    def tearDown(self):
        self.tmp.cleanup()

    def file_of(self, name):
        return self.root / "served" / name

    def served(self, name, text, packed=True, form=""):
        path = self.file_of(f"{name}.mps.gz" if packed else f"{name}.mps")
        path.parent.mkdir(exist_ok=True)
        data = text.encode()
        path.write_bytes(gzip.compress(data) if packed else data)
        url = path.as_uri()
        return Instance("netlib", name, url, sha256_of(path), form)

    def records(self, *instances, limit=LIMIT):
        tool = [sys.executable, "-I", "-c", TOOL]
        with contextlib.redirect_stdout(io.StringIO()):
            run(instances, self.cache, tool, self.record, limit, STAMP)
        lines = self.record.read_text().splitlines()
        return [json.loads(line) for line in lines]

    def outcome(self, text, limit=LIMIT):
        return self.records(self.served("afiro", text), limit=limit)[-1]

    def cached(self):
        return list((self.cache / "netlib").iterdir())

    def test_list(self):
        path = self.root / "netlib.txt"
        path.write_text(
            f"# netlib LP\n\nafiro {AFIRO} 00ff\n"
            f"  adlittle {ADLITTLE} 11ee fixed \n"
        )
        self.assertEqual(
            parse_list(path),
            [
                Instance("netlib", "afiro", AFIRO, "00ff"),
                Instance("netlib", "adlittle", ADLITTLE, "11ee", "fixed"),
            ],
        )

    def test_short_line(self):
        path = self.root / "netlib.txt"
        path.write_text(f"# netlib LP\nafiro {AFIRO}\n")
        with self.assertRaisesRegex(ValueError, "netlib.txt:2:"):
            parse_list(path)

    def test_bad_form(self):
        path = self.root / "netlib.txt"
        path.write_text(f"afiro {AFIRO} 00ff free\n")
        with self.assertRaisesRegex(ValueError, "netlib.txt:1:"):
            parse_list(path)

    def test_fixed(self):
        fixed = self.served("afiro", "fixed", form="fixed")
        self.assertEqual(self.records(fixed)[-1]["outcome"], "same")
        self.assertEqual(self.outcome("fixed")["outcome"], "error")

    def test_same(self):
        entry = self.outcome("same")
        self.assertEqual(entry["library"], "netlib")
        self.assertEqual(entry["instance"], "afiro")
        self.assertEqual(entry["outcome"], "same")
        self.assertEqual(entry["detail"], "")
        self.assertEqual(entry["commit"], STAMP["commit"])
        self.assertEqual(entry["machine"], STAMP["machine"])
        times = entry["times"]
        phases = (times["read"], times["write"], times["reread"])
        self.assertEqual(phases, (0.5, 0.25, 0.125))
        self.assertIn("wall", times)

    def test_differs(self):
        entry = self.outcome("differs")
        self.assertEqual(entry["outcome"], "differs")
        self.assertEqual(entry["detail"], "function 0: term count 3 against 5")

    def test_unwritable(self):
        entry = self.outcome("unwritable")
        self.assertEqual(entry["outcome"], "unwritable")
        self.assertEqual(entry["detail"], "name 'DEDO3 1R' cannot be written")
        self.assertEqual(entry["times"]["read"], 0.5)
        self.assertNotIn("write", entry["times"])

    def test_error(self):
        entry = self.outcome("broken")
        self.assertEqual(entry["outcome"], "error")
        self.assertEqual(entry["detail"], "exit 2: bad file 'source': line 3")

    def test_crash(self):
        entry = self.outcome("crash")
        self.assertEqual(entry["outcome"], "crash")
        self.assertEqual(entry["detail"], "exit -9")
        self.assertEqual(list(entry["times"]), ["wall"])

    def test_timeout(self):
        entry = self.outcome("sleep", limit=0.5)
        self.assertEqual(entry["outcome"], "timeout")

    def test_mismatch(self):
        served = self.served("afiro", "same")
        wrong = Instance("netlib", "afiro", served.url, "0" * 64)
        entry = self.records(wrong)[-1]
        self.assertEqual(entry["outcome"], "refused")
        self.assertIn(served.sha256, entry["detail"])
        self.assertEqual(self.cached(), [])

    def test_missing(self):
        served = self.served("afiro", "same")
        self.file_of("afiro.mps.gz").unlink()
        self.assertEqual(self.records(served)[-1]["outcome"], "refused")
        self.assertEqual(self.cached(), [])

    def test_broken_gzip(self):
        served = self.served("afiro", "same")
        path = self.file_of("afiro.mps.gz")
        path.write_bytes(path.read_bytes()[:-8])
        broken = Instance("netlib", "afiro", served.url, sha256_of(path))
        self.assertEqual(self.records(broken)[-1]["outcome"], "refused")

    def test_plain(self):
        served = self.served("afiro", "same", packed=False)
        self.assertEqual(self.records(served)[-1]["outcome"], "same")

    def test_cached(self):
        served = self.served("afiro", "same")
        self.records(served)
        self.file_of("afiro.mps.gz").unlink()
        entries = self.records(served)
        outcomes = [entry["outcome"] for entry in entries]
        self.assertEqual(outcomes, ["same", "same"])

    def test_stale(self):
        served = self.served("afiro", "same")
        stale = self.cache / "netlib" / "afiro.mps.gz"
        stale.parent.mkdir()
        stale.write_bytes(b"stale")
        self.assertEqual(self.records(served)[-1]["outcome"], "same")
        self.assertEqual(sha256_of(stale), served.sha256)

    def test_crash_continues(self):
        crash = self.served("crash", "crash")
        same = self.served("same", "same")
        entries = self.records(crash, same)
        outcomes = [(entry["instance"], entry["outcome"]) for entry in entries]
        self.assertEqual(outcomes, [("crash", "crash"), ("same", "same")])


if __name__ == "__main__":
    unittest.main()
