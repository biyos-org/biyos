from collections import namedtuple

Versions = namedtuple("Versions", ["since", "deprecated", "retired"])
"""The versions that added, deprecated and retired an option."""


def version_text(number: int) -> str | None:
    if number == 0:
        return None
    major, rest = divmod(number, 10000)
    minor, patch = divmod(rest, 100)
    return f"{major}.{minor}.{patch}"
