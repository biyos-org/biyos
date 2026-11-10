import re
from pathlib import Path

INCLUDE = Path(__file__).resolve().parents[3] / "include" / "biyos"
MACRO = re.compile(r"^#define (BIYOS_\w+) \(?(-?\d+)\)?$", re.M)


def macros(name):
    text = (INCLUDE / name).read_text()
    return {match[1]: int(match[2]) for match in MACRO.finditer(text)}
