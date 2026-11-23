from dataclasses import dataclass, field
from pathlib import Path

DIRECTIONS = ("in", "out", "inout")


@dataclass
class Entry:
    kind: str
    name: str
    line: int
    summary: str = ""
    params: list = field(default_factory=list)
    returns: str = ""
    errors: list = field(default_factory=list)
    notes: list = field(default_factory=list)
    values: list = field(default_factory=list)
    see: list = field(default_factory=list)
    takes: dict = field(default_factory=dict)
    since: str = ""
    pair: str = ""

    @property
    def names(self):
        return [self.name, self.pair] if self.pair else [self.name]


@dataclass
class Group:
    key: str
    title: str
    texts: list = field(default_factory=list)
    entries: list = field(default_factory=list)


def _param(entry, rest):
    name, direction, text = (rest.split(" ", 2) + ["", ""])[:3]
    if direction not in DIRECTIONS:
        raise ValueError(f"direction {direction!r} is not in, out or inout")
    entry.params.append((name, direction, text))


def _pair(rest):
    name, _, text = rest.partition(" ")
    return name, text


def _takes(entry, rest):
    if not entry.values:
        raise ValueError("takes comes before any value")
    entry.takes[entry.values[-1][0]] = rest


def _set(name):
    def store(entry, rest):
        setattr(entry, name, rest)

    return store


FIELDS = {
    "summary": _set("summary"),
    "return": _set("returns"),
    "since": _set("since"),
    "param": _param,
    "error": lambda entry, rest: entry.errors.append(_pair(rest)),
    "note": lambda entry, rest: entry.notes.append(rest),
    "see": lambda entry, rest: entry.see.extend(rest.split()),
    "takes": _takes,
}


def _value(entry, rest, number):
    entry.values.append((*_pair(rest), number))


ENTRIES = ("function", "type", "table")


def _add(groups, number, keyword, rest):
    if keyword == "group":
        key, _, title = rest.partition(" ")
        groups.append(Group(key, title))
        return
    if not groups:
        raise ValueError(f"{keyword} comes before any group")
    group = groups[-1]
    if keyword == "text":
        group.texts.append(rest)
    elif keyword == "function":
        name, _, pair = rest.partition(" ")
        group.entries.append(Entry(keyword, name, number, pair=pair))
    elif keyword in ENTRIES:
        group.entries.append(Entry(keyword, rest, number))
    elif keyword == "value" and group.entries:
        _value(group.entries[-1], rest, number)
    elif keyword in FIELDS and group.entries:
        FIELDS[keyword](group.entries[-1], rest)
    else:
        raise ValueError(f"{keyword} is not a keyword here")


def read(path):
    groups = []
    lines = Path(path).read_text().splitlines()
    for number, line in enumerate(lines, 1):
        if not line.strip():
            continue
        keyword, _, rest = line.partition(" ")
        try:
            _add(groups, number, keyword, rest.strip())
        except ValueError as error:
            raise ValueError(f"{path}:{number}: {error}") from None
    return groups


def names(groups):
    found = {}
    for group in groups:
        for entry in group.entries:
            if entry.kind != "table":
                found.update((name, (group, entry)) for name in entry.names)
            for name, *_ in entry.values:
                found[name] = (group, entry)
    return found


def differs(named, arguments, partners):
    if [name for name in named if name in arguments] != arguments:
        return True
    return not set(named) <= set().union(*partners)
