from enum import IntEnum


class Operator(IntEnum):
    """The codes of an expression's entries: its operators and ``NODE``.

    Each member is the C constant of the same number, ``CONSTANT`` for
    ``BIYOS_OP_CONST`` through ``ERF`` for ``BIYOS_OP_ERF``. ``NODE`` is no
    operator: in postfix it names a node the model holds, by its id.

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

