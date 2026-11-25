from ctypes import Array
from enum import IntEnum

from biyos._convert.numbers import c_integer, c_offset


class Operator(IntEnum):
    """The codes of an expression's entries: its operators and three more.

    Each member is the C constant of the same number, ``CONSTANT`` for
    ``BIYOS_OP_CONST`` through ``ERF`` for ``BIYOS_OP_ERF``, and
    ``PIECEWISE_LINEAR`` for ``BIYOS_OP_PWL``. ``NODE``, ``KEEP`` and
    ``REFERENCE`` are no operators and appear only in postfix. ``NODE``
    names a node the model holds, by its id. ``KEEP`` makes the operand
    before it a node of its own, and ``REFERENCE`` names that node again by
    the position of the ``KEEP`` entry.

    See Also
    --------
    BIYOS_addnode, BIYOS_addexpr
    """

    CONSTANT = 1
    VARIABLE = 2
    SUM = 3
    PRODUCT = 4
    NEGATION = 5
    DIVISION = 6
    CONSTANT_POWER = 7
    POWER = 8
    EXP = 9
    LOG = 10
    SQRT = 11
    SIN = 12
    COS = 13
    TAN = 14
    ABS = 15
    MAX = 16
    MIN = 17
    ASIN = 18
    ACOS = 19
    ATAN = 20
    SINH = 21
    COSH = 22
    TANH = 23
    ASINH = 24
    ACOSH = 25
    ATANH = 26
    ENTROPY = 27
    SIGN_POWER = 28
    ERF = 29
    NODE = 30
    PIECEWISE_LINEAR = 31
    KEEP = 32
    REFERENCE = 33


_ONE_VALUE = frozenset(
    (
        Operator.CONSTANT,
        Operator.PRODUCT,
        Operator.CONSTANT_POWER,
        Operator.SIGN_POWER,
    )
)


def node_values(code: int, arity: int) -> int:
    if code == Operator.SUM:
        return 1 + max(arity, 0)
    return 1 if code in _ONE_VALUE else 0


def postfix_values(
    code: Array[c_integer] | None, arg: Array[c_offset] | None
) -> int:
    if code is None or arg is None:
        return 0
    entries = len(code)
    most = 2 * entries
    total = 0
    for k in range(entries):
        if total >= most:
            break
        total += node_values(code[k], min(max(arg[k], 0), entries))
    return min(total, most)
