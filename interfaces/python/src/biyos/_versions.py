from typing import NamedTuple


class Versions(NamedTuple):
    """The versions that added, deprecated and retired an option."""

    since: str | None
    deprecated: str | None
    retired: str | None


def version_text(number: int) -> str | None:
    if number == 0:
        return None
    major, rest = divmod(number, 10000)
    minor, patch = divmod(rest, 100)
    return f"{major}.{minor}.{patch}"
