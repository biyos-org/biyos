from enum import IntEnum
from typing import NamedTuple


class Checking(IntEnum):
    """The values of the option ``checking``, as ``BIYOS_CHK_`` numbers
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


class OptionSource(IntEnum):
    """Where the value a model reads of an option comes from, as
    ``BIYOS_OPTSRC_`` numbers them.

    Attributes
    ----------
    MODEL
        The model holds its own value of the option.
    ENVIRONMENT
        The model shares the value set on its environment.
    DEFAULT
        Neither the model nor its environment set the option, so it holds
        its default.
    """

    MODEL = 1
    ENVIRONMENT = 2
    DEFAULT = 3


class Masks(NamedTuple):
    """The value of a flags option: the parts forced on and those forced off.

    Each part is a bit of its option's group. A part in neither mask keeps
    its default, and ``Masks(0, 0)`` is the default of every flags option.
    """

    on: int
    off: int


ENUMS: dict[str, type[IntEnum]] = {"checking": Checking}
