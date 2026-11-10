from collections import namedtuple

Versions = namedtuple("Versions", ["since", "deprecated", "retired"])


def version_text(number):
    if number == 0:
        return None
    major, rest = divmod(number, 10000)
    minor, patch = divmod(rest, 100)
    return f"{major}.{minor}.{patch}"
