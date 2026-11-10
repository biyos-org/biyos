import re
from pathlib import Path

INCLUDE = Path(__file__).resolve().parents[3] / "include" / "biyos"
MACRO = re.compile(r"^#define (BIYOS_\w+) \(?(-?\d+)\)?$", re.M)
FUNCTION = re.compile(r"\bBIYOS_API [^(;]*?\b(biyos_\w+)\(")


def macros(name):
    text = (INCLUDE / name).read_text()
    return {match[1]: int(match[2]) for match in MACRO.finditer(text)}


def functions(name):
    text = (INCLUDE / name).read_text()
    return [match[1] for match in FUNCTION.finditer(text)]


def lines(name, start):
    text = (INCLUDE / name).read_text()
    return sum(line.startswith(start) for line in text.splitlines())
