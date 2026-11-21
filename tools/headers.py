import re
from pathlib import Path

DEFINE = re.compile(r"^#define (BIYOS_\w+) \(?(-?\d+)\)?$", re.M)
STATEMENT = re.compile(r"^(?:BIYOS_API|typedef) [^;]*;", re.M)
NAME = re.compile(r"\(?\*?(BIYOS_\w+)\)?\s*(?:\(|;)")
PREFIX = "BIYOS_API "
WIDTH = 76


def _dedent(line):
    indent = " " * len(PREFIX)
    return line[len(PREFIX) :] if line.startswith(indent) else line


def _wrapped(line):
    if "\n" in line or len(line) <= WIDTH or "(" not in line:
        return line
    head, _, tail = line.partition("(")
    joint = ",\n" + " " * (len(head) + 1)
    return head + "(" + joint.join(tail.split(", "))


def _shown(statement):
    if not statement.startswith(PREFIX):
        return statement
    first, *rest = statement[len(PREFIX) :].split("\n")
    return _wrapped("\n".join([first, *map(_dedent, rest)]))


def _named(statement):
    found = NAME.findall(statement)
    return found[0] if found else None


def read(include):
    values = {}
    declarations = {}
    for path in sorted(Path(include).glob("*.h")):
        text = path.read_text()
        values.update(
            (name, int(value)) for name, value in DEFINE.findall(text)
        )
        for match in STATEMENT.finditer(text):
            name = _named(match.group())
            if name:
                declarations[name] = _shown(match.group())
    return values, declarations
