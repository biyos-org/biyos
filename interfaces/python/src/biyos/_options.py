from enum import IntEnum


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
