from enum import IntEnum
from typing import NamedTuple


class CheckLevel(IntEnum):
    """The values of the option ``check_lvl``, as ``BIYOS_CHECKLVL_`` numbers
    them.

    Attributes
    ----------
    NONE
        No invariant check runs.
    CHEAP
        The checks that cost little run, such as finite values and
        dimensions. The default.
    FULL
        Every check runs, the costly ones included.
    """

    NONE = 1
    CHEAP = 2
    FULL = 3


class Masks(NamedTuple):
    """The value of a flags option: the parts forced on and those forced off.

    Each part is a bit of its option's group. A part in neither mask keeps
    its default, and ``Masks(0, 0)`` is the default of every flags option.
    """

    on: int
    off: int


ENUMS: dict[str, type[IntEnum]] = {"check_lvl": CheckLevel}
