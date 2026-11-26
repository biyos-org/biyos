import argparse
import re
import sys
import tomllib
from pathlib import Path

LIBRARY = re.compile(r"project\(biyos VERSION ([0-9.]+)")
EXAMPLES = re.compile(r"find_package\(biyos ([0-9.]+) EXACT")


def differing(licence, copies):
    text = licence.read_bytes()
    problems = []
    for copy in copies:
        if not copy.is_file():
            problems.append(f"{copy}: missing")
        elif copy.read_bytes() != text:
            problems.append(f"{copy}: differs from {licence}")
    return problems


def cmake_version(pattern, path):
    match = pattern.search(path.read_text())
    return match.group(1) if match else None


def package_version(path):
    with path.open("rb") as file:
        return tomllib.load(file).get("project", {}).get("version")


def disagreeing(library, expected, others):
    if expected is None:
        return [f"{library}: names no version"]
    has = f"{library} has {expected}"
    problems = []
    for path, version in others.items():
        if version is None:
            problems.append(f"{path}: names no version")
        elif version != expected:
            problems.append(f"{path}: version {version}, {has}")
    return problems


def check(licence, copies, library, examples, python):
    expected = cmake_version(LIBRARY, library)
    others = {
        examples: cmake_version(EXAMPLES, examples),
        python: package_version(python),
    }
    return differing(licence, copies) + disagreeing(library, expected, others)


def arguments():
    parser = argparse.ArgumentParser()
    parser.add_argument("--licence", type=Path, required=True)
    parser.add_argument("--copies", type=Path, nargs="+", required=True)
    parser.add_argument("--library", type=Path, required=True)
    parser.add_argument("--examples", type=Path, required=True)
    parser.add_argument("--python", type=Path, required=True)
    return parser.parse_args()


def main():
    problems = check(**vars(arguments()))
    for problem in problems:
        print(problem)
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
