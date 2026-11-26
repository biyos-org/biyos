import argparse
import gzip
import hashlib
import json
import os
import platform
import shutil
import subprocess
import sys
import tempfile
import time
import urllib.request
import zlib
from dataclasses import dataclass
from http.client import HTTPException
from pathlib import Path

CHUNK = 1 << 20
FETCH_SECONDS = 300
LIMIT_SECONDS = 600.0
FAILURES = (OSError, ValueError, EOFError, zlib.error, HTTPException)
OUTCOMES = {0: "same", 1: "differs", 2: "error", 3: "unwritable"}
PHASES = ("read", "write", "reread")


@dataclass(frozen=True)
class Instance:
    library: str
    name: str
    url: str
    sha256: str
    fixed: bool = False


def parse_list(path):
    library = path.stem
    instances = []
    for number, line in enumerate(path.read_text().splitlines(), 1):
        fields = line.split()
        if not fields or fields[0].startswith("#"):
            continue
        if len(fields) < 3 or fields[3:] not in ([], ["fixed"]):
            raise ValueError(
                f"{path}:{number}: expected name, URL, SHA-256, maybe fixed"
            )
        instances.append(Instance(library, *fields[:3], len(fields) == 4))
    return instances


def sha256_of(path):
    digest = hashlib.sha256()
    with path.open("rb") as file:
        while chunk := file.read(CHUNK):
            digest.update(chunk)
    return digest.hexdigest()


def download(url, target):
    part = target.with_name(target.name + ".part")
    try:
        with urllib.request.urlopen(url, timeout=FETCH_SECONDS) as response:
            with part.open("wb") as file:
                shutil.copyfileobj(response, file, CHUNK)
    except BaseException:
        part.unlink(missing_ok=True)
        raise
    return part


def fetch(instance, cache):
    target = cache / instance.library / instance.url.rsplit("/", 1)[-1]
    if target.is_file() and sha256_of(target) == instance.sha256:
        return target
    target.parent.mkdir(parents=True, exist_ok=True)
    part = download(instance.url, target)
    found = sha256_of(part)
    if found != instance.sha256:
        part.unlink()
        raise ValueError(f"SHA-256 {found}, the list has {instance.sha256}")
    return part.replace(target)


def expanded(fetched, folder):
    if fetched.suffix != ".gz":
        return fetched
    source = folder / "source"
    with gzip.open(fetched, "rb") as packed, source.open("wb") as file:
        shutil.copyfileobj(packed, file, CHUNK)
    return source


def last_line(text):
    lines = text.strip().splitlines()
    return lines[-1] if lines else ""


def timings(output):
    times = {}
    for line in output.splitlines():
        fields = line.split()
        if len(fields) == 2 and fields[0] in PHASES:
            times[fields[0]] = float(fields[1])
    return times


def failure_of(done):
    code = f"exit {done.returncode}"
    reason = last_line(done.stderr)
    return f"{code}: {reason}" if reason else code


def judged(done):
    outcome = OUTCOMES.get(done.returncode, "crash")
    timed = outcome in ("same", "differs", "unwritable")
    times = timings(done.stdout) if timed else {}
    if outcome in ("differs", "unwritable"):
        detail = last_line(done.stdout)
    elif outcome == "same":
        detail = ""
    else:
        detail = failure_of(done)
    return outcome, times, detail


def round_trip(tool, source, scratch, limit, fixed):
    command = [*tool, str(source), str(scratch), *(["fixed"] if fixed else [])]
    start = time.monotonic()
    try:
        done = subprocess.run(
            command,
            capture_output=True,
            text=True,
            errors="replace",
            timeout=limit,
        )
    except subprocess.TimeoutExpired:
        return "timeout", {"wall": limit}, f"over {limit} s"
    outcome, times, detail = judged(done)
    times["wall"] = time.monotonic() - start
    return outcome, times, detail


def attempt(instance, cache, tool, limit):
    with tempfile.TemporaryDirectory(dir=cache) as name:
        folder = Path(name)
        try:
            source = expanded(fetch(instance, cache), folder)
        except FAILURES as error:
            return "refused", {}, str(error)
        scratch = folder / "written"
        return round_trip(tool, source, scratch, limit, instance.fixed)


def record_of(instance, outcome, times, detail, stamp):
    return {
        "library": instance.library,
        "instance": instance.name,
        "outcome": outcome,
        "times": times,
        "detail": detail,
        **stamp,
    }


def run(instances, cache, tool, record, limit, stamp):
    with record.open("a") as file:
        for instance in instances:
            outcome, times, detail = attempt(instance, cache, tool, limit)
            entry = record_of(instance, outcome, times, detail, stamp)
            file.write(json.dumps(entry) + "\n")
            file.flush()
            print(instance.library, instance.name, outcome, detail, flush=True)


def commit_of(folder):
    done = subprocess.run(
        ["git", "-C", str(folder), "rev-parse", "HEAD"],
        capture_output=True,
        text=True,
    )
    return done.stdout.strip() if done.returncode == 0 else None


def machine_of():
    return {
        "host": platform.node(),
        "system": platform.platform(),
        "arch": platform.machine(),
        "cpus": os.cpu_count(),
        "python": platform.python_version(),
    }


def chosen(instances, names):
    if not names:
        return instances
    return [instance for instance in instances if instance.name in names]


def arguments():
    parser = argparse.ArgumentParser()
    parser.add_argument("--lists", type=Path, nargs="+", required=True)
    parser.add_argument("--cache", type=Path, required=True)
    parser.add_argument("--tool", nargs="+", required=True)
    parser.add_argument("--record", type=Path, required=True)
    parser.add_argument("--repository", type=Path, required=True)
    parser.add_argument("--limit", type=float, default=LIMIT_SECONDS)
    parser.add_argument("--only", nargs="+", default=[])
    return parser.parse_args()


def main():
    given = arguments()
    instances = [
        instance for path in given.lists for instance in parse_list(path)
    ]
    given.cache.mkdir(parents=True, exist_ok=True)
    run(
        chosen(instances, given.only),
        given.cache,
        given.tool,
        given.record,
        given.limit,
        {"commit": commit_of(given.repository), "machine": machine_of()},
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
