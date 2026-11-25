from collections.abc import Sequence
from enum import Enum
from math import isinf, isnan
from typing import Any, SupportsIndex, TypeGuard

from biyos._capi import inf
from biyos._convert.integers import integer
from biyos._convert.numbers import Index, Real
from biyos._model.operators import Operator
from biyos._model.printer import printed


def _number(value: object) -> TypeGuard[int | Real]:
    return isinstance(value, (int, float)) and not isinstance(value, bool)


def _term(value: object) -> TypeGuard["Term"]:
    return _number(value) or isinstance(value, Operand)


def _no_repeat(value: object) -> None:
    if isinstance(value, Sequence):
        found = type(value).__name__
        raise TypeError(f"an expression multiplies a number, not {found}")


class Operand:
    __slots__ = ()

    def __add__(self, other: "Term") -> "Expression":
        """Return the sum of two operands.

        Parameters
        ----------
        other
            An expression, a variable or a number.

        Returns
        -------
        expr
            The expression ``self + other``.
        """
        if not _term(other):
            return NotImplemented
        return _sum(self, other, 1.0)

    def __radd__(self, other: Real) -> "Expression":
        """Return the sum of a number and the operand.

        Parameters
        ----------
        other
            A number.

        Returns
        -------
        expr
            The expression ``other + self``.
        """
        if not _number(other):
            return NotImplemented
        return _shifted(self, 1.0, other)

    def __sub__(self, other: "Term") -> "Expression":
        """Return the difference of two operands.

        Parameters
        ----------
        other
            An expression, a variable or a number.

        Returns
        -------
        expr
            The expression ``self - other``.
        """
        if not _term(other):
            return NotImplemented
        return _sum(self, other, -1.0)

    def __rsub__(self, other: Real) -> "Expression":
        """Return the difference of a number and the operand.

        Parameters
        ----------
        other
            A number.

        Returns
        -------
        expr
            The expression ``other - self``.
        """
        if not _number(other):
            return NotImplemented
        return _shifted(self, -1.0, other)

    def __mul__(self, other: "Term") -> "Expression":
        """Return the product of two operands.

        A number scales the operand, so its terms stay linear or quadratic.

        Parameters
        ----------
        other
            An expression, a variable or a number.

        Returns
        -------
        expr
            The expression ``self * other``.

        Raises
        ------
        TypeError
            When ``other`` is a sequence, which Python would repeat.
        """
        _no_repeat(other)
        if _number(other):
            return _shifted(self, float(other), 0.0)
        if not isinstance(other, Operand):
            return NotImplemented
        return Expression(Operator.PRODUCT, (self, other), (1.0,))

    def __rmul__(self, other: Real) -> "Expression":
        """Return the product of a number and the operand.

        Parameters
        ----------
        other
            A number.

        Returns
        -------
        expr
            The expression ``other * self``.

        Raises
        ------
        TypeError
            When ``other`` is a sequence, which Python would repeat.
        """
        _no_repeat(other)
        if not _number(other):
            return NotImplemented
        return _shifted(self, float(other), 0.0)

    def __truediv__(self, other: "Term") -> "Expression":
        """Return the quotient of two operands.

        Dividing by a number multiplies by its inverse, so the operand's
        terms stay linear or quadratic.

        Parameters
        ----------
        other
            An expression, a variable or a nonzero number.

        Returns
        -------
        expr
            The expression ``self / other``.

        Raises
        ------
        ZeroDivisionError
            When ``other`` is the number 0.
        """
        if _number(other):
            return _shifted(self, 1.0 / float(other), 0.0)
        if not isinstance(other, Operand):
            return NotImplemented
        return Expression(Operator.DIVISION, (self, other), ())

    def __rtruediv__(self, other: Real) -> "Expression":
        """Return the quotient of a number and the operand.

        Parameters
        ----------
        other
            A number.

        Returns
        -------
        expr
            The expression ``other / self``.
        """
        if not _number(other):
            return NotImplemented
        return Expression(Operator.DIVISION, (_constant(other), self), ())

    def __pow__(self, other: "Term") -> "Expression":
        """Return the operand raised to a power.

        Parameters
        ----------
        other
            An expression, a variable or a number. A number gives a
            constant power.

        Returns
        -------
        expr
            The expression ``self ** other``.
        """
        if _number(other):
            power = (float(other),)
            return Expression(Operator.CONSTANT_POWER, (self,), power)
        if not isinstance(other, Operand):
            return NotImplemented
        return Expression(Operator.POWER, (self, other), ())

    def __rpow__(self, other: Real) -> "Expression":
        """Return a number raised to the operand.

        Parameters
        ----------
        other
            A number.

        Returns
        -------
        expr
            The expression ``other ** self``.
        """
        if not _number(other):
            return NotImplemented
        return Expression(Operator.POWER, (_constant(other), self), ())

    def __neg__(self) -> "Expression":
        """Return the negation of the operand.

        Returns
        -------
        expr
            The expression ``-self``.
        """
        return Expression(Operator.NEGATION, (self,), ())

    def __abs__(self) -> "Expression":
        """Return the absolute value of the operand.

        Returns
        -------
        expr
            The expression ``abs(self)``.
        """
        return Expression(Operator.ABS, (self,), ())

    def __le__(self, other: "Term") -> "Comparison":
        """Return the constraint that the operand is at most another.

        Parameters
        ----------
        other
            An expression, a variable or a number.

        Returns
        -------
        con
            The constraint ``self - other <= 0``, or ``self <= other`` when
            ``other`` is a number.

        Raises
        ------
        TypeError
            When ``other`` is of another type.
        ValueError
            When ``other`` is NaN, or a number that leaves no value, such as
            ``-inf``.
        """
        return _compared(self, other, ComparisonSense.LESS_EQUAL)

    def __ge__(self, other: "Term") -> "Comparison":
        """Return the constraint that the operand is at least another.

        Parameters
        ----------
        other
            An expression, a variable or a number.

        Returns
        -------
        con
            The constraint ``self - other >= 0``, or ``self >= other`` when
            ``other`` is a number.

        Raises
        ------
        TypeError
            When ``other`` is of another type.
        ValueError
            When ``other`` is NaN, or a number that leaves no value, such as
            ``inf``.
        """
        return _compared(self, other, ComparisonSense.GREATER_EQUAL)

    def __eq__(self, other: object) -> Any:
        """Return the constraint that the operand equals another.

        A value of another type is not equal to the operand, so ``==``
        gives False for it, as Python does.

        Parameters
        ----------
        other
            An expression, a variable or a number.

        Returns
        -------
        con
            The constraint ``self - other == 0``, or ``self == other`` when
            ``other`` is a number.

        Raises
        ------
        ValueError
            When ``other`` is NaN or infinite.
        """
        if not _term(other):
            return NotImplemented
        same = _same(self, other)
        return _compared(self, other, ComparisonSense.EQUAL, same)

    def _key(self) -> tuple[object, Index] | None:
        return None


Term = Operand | int | Real


class Expression(Operand):
    """An expression in the variables of a model, built by arithmetic.

    An expression is a tree: an operator of ``Operator`` applied to its
    operands, which are expressions, variables or constants, together with
    the values the operator takes, as ``Model.addnode`` takes them. Applying
    ``+``, ``-``, ``*``, ``/``, ``**``, unary ``-`` or ``abs`` to a
    ``Variable`` or an ``Expression``, with ``int`` and ``float`` on either
    side, gives a new expression and leaves its operands as they were. An
    operand of any other type, ``bool`` included, raises ``TypeError``.

    An expression is checked against a model only when the model takes it,
    so a variable of another model or a view made before a deletion raises
    ``ValueError`` then.
    """

    __slots__ = ("_args", "_op", "_values")

    def __init__(
        self,
        op: Operator,
        args: tuple[Operand, ...],
        values: tuple[Real, ...],
    ) -> None:
        self._op = op
        self._args = args
        self._values = values

    def __repr__(self) -> str:
        """Return the expression as it is written.

        Each variable reads ``x[i]``, by its index, and parentheses appear
        only where precedence needs them, as in ``(x[0] + 1)*x[1]``.

        Returns
        -------
        text
            The formula of the expression, such as ``x[0]**2 + 3*x[1]``.
        """
        return printed(self)


class Piecewise(Expression):
    __slots__ = ("_idx", "_pwl")

    def __init__(self, pwl: SupportsIndex, idx: Index, arg: Operand) -> None:
        super().__init__(Operator.PIECEWISE_LINEAR, (arg,), ())
        self._pwl = pwl
        self._idx = idx


class ComparisonSense(Enum):
    """The senses of comparisons, each valued by its operator.

    Attributes
    ----------
    LESS_EQUAL
        The left side is at most the right side.
    GREATER_EQUAL
        The left side is at least the right side.
    EQUAL
        The two sides are equal.
    """

    LESS_EQUAL = "<="
    GREATER_EQUAL = ">="
    EQUAL = "=="


class Comparison:
    """A constraint written as a comparison of two operands.

    ``<=``, ``>=`` and ``==`` between expressions, variables and numbers
    give a comparison, which ``Model.addcon`` takes. With an operand on each
    side it bounds their difference by 0, and with a number on one side it
    bounds the other side by the number, so ``x <= 3`` and ``3 >= x`` are
    the same constraint. The comparison keeps its sides and its sense in
    ``lhs``, ``rhs`` and ``sense``, and ``repr`` writes it as a formula. A
    number always lands in ``rhs``, since Python hands ``3 >= x`` to ``x``
    as ``x <= 3``. ``==`` between two variables is true when both name one
    variable of one model, so variables serve as keys of dictionaries and
    sets. Any other comparison has no truth value: ``if x + 1 == y`` raises
    ``TypeError``, and so does a chained ``0 <= x <= 1``.
    """

    __slots__ = ("_body", "_lb", "_lhs", "_rhs", "_same", "_sense", "_ub")

    def __init__(
        self,
        lhs: Operand,
        sense: ComparisonSense,
        rhs: Operand | Real,
        body: Operand,
        lb: Real,
        ub: Real,
        same: bool | None = None,
    ) -> None:
        self._lhs = lhs
        self._sense = sense
        self._rhs = rhs
        self._body = body
        self._lb = lb
        self._ub = ub
        self._same = same

    @property
    def lhs(self) -> Operand:
        """The operand on the left."""
        return self._lhs

    @property
    def rhs(self) -> Operand | Real:
        """The operand on the right, or the number as a ``float``."""
        return self._rhs

    @property
    def sense(self) -> ComparisonSense:
        """The operator between the two sides."""
        return self._sense

    def __repr__(self) -> str:
        """Return the comparison as a formula.

        Each side prints as an ``Expression`` prints, a number included, so
        ``x[0] <= 2`` reads back as itself.

        Returns
        -------
        text
            The two sides around the operator.
        """
        lhs = printed(self._lhs)
        rhs = printed(_operand(self._rhs))
        return f"{lhs} {self._sense.value} {rhs}"

    def __bool__(self) -> bool:
        """Return whether ``==`` names one variable twice.

        Returns
        -------
        same
            For ``==`` between two variables, whether both name one variable
            of one model.

        Raises
        ------
        TypeError
            For any other comparison, since it is a constraint, not a test.
        """
        if self._same is None:
            raise TypeError(
                "a comparison is a constraint and has no truth value"
            )
        return self._same


_SIDES = {
    ComparisonSense.LESS_EQUAL: (False, True),
    ComparisonSense.GREATER_EQUAL: (True, False),
    ComparisonSense.EQUAL: (True, True),
}


def _constant(value: Real) -> Expression:
    return Expression(Operator.CONSTANT, (), (float(value),))


def _shifted(term: Operand, weight: Real, shift: Real) -> Expression:
    return Expression(Operator.SUM, (term,), (float(shift), weight))


def _sum(left: Operand, right: "Term", sign: Real) -> Expression:
    if isinstance(right, Operand):
        return Expression(Operator.SUM, (left, right), (0.0, 1.0, sign))
    return _shifted(left, 1.0, sign * float(right))


def _same(left: Operand, right: "Term") -> bool | None:
    if not isinstance(right, Operand):
        return None
    mine = left._key()
    theirs = right._key()
    if mine is None or theirs is None:
        return None
    return mine == theirs


def _bound(value: Real, lower: bool, upper: bool) -> Real:
    closed = (lower and value > 0) or (upper and value < 0)
    if isnan(value) or (isinf(value) and closed):
        raise ValueError(f"a comparison refuses the bound {value}")
    return value


def _compared(
    left: Operand,
    right: object,
    sense: ComparisonSense,
    same: bool | None = None,
) -> Comparison:
    if not _term(right):
        found = type(right).__name__
        raise TypeError(f"an expression compares with a number, not {found}")
    lower, upper = _SIDES[sense]
    body: Operand = left
    bound = 0.0
    rhs: Operand | Real = right
    if isinstance(right, Operand):
        body = _sum(left, right, -1.0)
    else:
        bound = _bound(float(right), lower, upper)
        rhs = bound
    lb = bound if lower else -inf
    ub = bound if upper else inf
    return Comparison(left, sense, rhs, body, lb, ub, same)


def _operand(value: object) -> Operand:
    if isinstance(value, Operand):
        return value
    if _number(value):
        return _constant(value)
    found = type(value).__name__
    raise TypeError(
        f"an operand must be an expression or a number, not {found}"
    )


def _apply(op: Operator, *args: Term) -> Expression:
    return Expression(op, tuple(map(_operand, args)), ())


def exp(x: Term) -> Expression:
    """Return the exponential of an operand.

    Parameters
    ----------
    x
        An expression, a variable or a number.

    Returns
    -------
    expr
        The expression ``exp(x)``.

    Raises
    ------
    TypeError
        When ``x`` is of another type.
    """
    return _apply(Operator.EXP, x)


def log(x: Term) -> Expression:
    """Return the natural logarithm of an operand.

    Parameters
    ----------
    x
        An expression, a variable or a number.

    Returns
    -------
    expr
        The expression ``log(x)``.

    Raises
    ------
    TypeError
        When ``x`` is of another type.
    """
    return _apply(Operator.LOG, x)


def sqrt(x: Term) -> Expression:
    """Return the square root of an operand.

    Parameters
    ----------
    x
        An expression, a variable or a number.

    Returns
    -------
    expr
        The expression ``sqrt(x)``.

    Raises
    ------
    TypeError
        When ``x`` is of another type.
    """
    return _apply(Operator.SQRT, x)


def sin(x: Term) -> Expression:
    """Return the sine of an operand.

    Parameters
    ----------
    x
        An expression, a variable or a number.

    Returns
    -------
    expr
        The expression ``sin(x)``.

    Raises
    ------
    TypeError
        When ``x`` is of another type.
    """
    return _apply(Operator.SIN, x)


def cos(x: Term) -> Expression:
    """Return the cosine of an operand.

    Parameters
    ----------
    x
        An expression, a variable or a number.

    Returns
    -------
    expr
        The expression ``cos(x)``.

    Raises
    ------
    TypeError
        When ``x`` is of another type.
    """
    return _apply(Operator.COS, x)


def tan(x: Term) -> Expression:
    """Return the tangent of an operand.

    Parameters
    ----------
    x
        An expression, a variable or a number.

    Returns
    -------
    expr
        The expression ``tan(x)``.

    Raises
    ------
    TypeError
        When ``x`` is of another type.
    """
    return _apply(Operator.TAN, x)


def asin(x: Term) -> Expression:
    """Return the arc sine of an operand.

    Parameters
    ----------
    x
        An expression, a variable or a number.

    Returns
    -------
    expr
        The expression ``asin(x)``.

    Raises
    ------
    TypeError
        When ``x`` is of another type.
    """
    return _apply(Operator.ASIN, x)


def acos(x: Term) -> Expression:
    """Return the arc cosine of an operand.

    Parameters
    ----------
    x
        An expression, a variable or a number.

    Returns
    -------
    expr
        The expression ``acos(x)``.

    Raises
    ------
    TypeError
        When ``x`` is of another type.
    """
    return _apply(Operator.ACOS, x)


def atan(x: Term) -> Expression:
    """Return the arc tangent of an operand.

    Parameters
    ----------
    x
        An expression, a variable or a number.

    Returns
    -------
    expr
        The expression ``atan(x)``.

    Raises
    ------
    TypeError
        When ``x`` is of another type.
    """
    return _apply(Operator.ATAN, x)


def sinh(x: Term) -> Expression:
    """Return the hyperbolic sine of an operand.

    Parameters
    ----------
    x
        An expression, a variable or a number.

    Returns
    -------
    expr
        The expression ``sinh(x)``.

    Raises
    ------
    TypeError
        When ``x`` is of another type.
    """
    return _apply(Operator.SINH, x)


def cosh(x: Term) -> Expression:
    """Return the hyperbolic cosine of an operand.

    Parameters
    ----------
    x
        An expression, a variable or a number.

    Returns
    -------
    expr
        The expression ``cosh(x)``.

    Raises
    ------
    TypeError
        When ``x`` is of another type.
    """
    return _apply(Operator.COSH, x)


def tanh(x: Term) -> Expression:
    """Return the hyperbolic tangent of an operand.

    Parameters
    ----------
    x
        An expression, a variable or a number.

    Returns
    -------
    expr
        The expression ``tanh(x)``.

    Raises
    ------
    TypeError
        When ``x`` is of another type.
    """
    return _apply(Operator.TANH, x)


def asinh(x: Term) -> Expression:
    """Return the inverse hyperbolic sine of an operand.

    Parameters
    ----------
    x
        An expression, a variable or a number.

    Returns
    -------
    expr
        The expression ``asinh(x)``.

    Raises
    ------
    TypeError
        When ``x`` is of another type.
    """
    return _apply(Operator.ASINH, x)


def acosh(x: Term) -> Expression:
    """Return the inverse hyperbolic cosine of an operand.

    Parameters
    ----------
    x
        An expression, a variable or a number.

    Returns
    -------
    expr
        The expression ``acosh(x)``.

    Raises
    ------
    TypeError
        When ``x`` is of another type.
    """
    return _apply(Operator.ACOSH, x)


def atanh(x: Term) -> Expression:
    """Return the inverse hyperbolic tangent of an operand.

    Parameters
    ----------
    x
        An expression, a variable or a number.

    Returns
    -------
    expr
        The expression ``atanh(x)``.

    Raises
    ------
    TypeError
        When ``x`` is of another type.
    """
    return _apply(Operator.ATANH, x)


def erf(x: Term) -> Expression:
    """Return the error function of an operand.

    Parameters
    ----------
    x
        An expression, a variable or a number.

    Returns
    -------
    expr
        The expression ``erf(x)``.

    Raises
    ------
    TypeError
        When ``x`` is of another type.
    """
    return _apply(Operator.ERF, x)


def entropy(x: Term) -> Expression:
    """Return the entropy of an operand.

    Parameters
    ----------
    x
        An expression, a variable or a number.

    Returns
    -------
    expr
        The expression ``x * log(x)``, 0 at 0.

    Raises
    ------
    TypeError
        When ``x`` is of another type.
    """
    return _apply(Operator.ENTROPY, x)


def signpow(x: Term, p: Real) -> Expression:
    """Return the sign power of an operand.

    Parameters
    ----------
    x
        An expression, a variable or a number.
    p
        The exponent, a positive number.

    Returns
    -------
    expr
        The expression ``sign(x) * abs(x) ** p``.

    Raises
    ------
    TypeError
        When ``x`` or ``p`` is of another type.
    """
    if not _number(p):
        raise TypeError(f"p must be a number, not {type(p).__name__}")
    power = (float(p),)
    return Expression(Operator.SIGN_POWER, (_operand(x),), power)


def piecewise(p: SupportsIndex, x: Term) -> Expression:
    """Return a piecewise linear function applied to an operand.

    Parameters
    ----------
    p
        The pwl, a ``PiecewiseLinear`` or its index. The model that takes
        the expression checks that it names one of its pwls.
    x
        An expression, a variable or a number.

    Returns
    -------
    expr
        The expression ``p(x)``.

    Raises
    ------
    TypeError
        When ``p`` has no integer index or ``x`` is of another type.

    See Also
    --------
    Model.addpwl
    """
    idx = integer("p", p)
    return Piecewise(p, idx, _operand(x))


def max(x: Term, y: Term, *rest: Term) -> Expression:
    """Return the largest of two or more operands.

    Parameters
    ----------
    x, y, *rest
        Expressions, variables or numbers.

    Returns
    -------
    expr
        The expression ``max(x, y, *rest)``.

    Raises
    ------
    TypeError
        When an operand is of another type.
    """
    return _apply(Operator.MAX, x, y, *rest)


def min(x: Term, y: Term, *rest: Term) -> Expression:
    """Return the smallest of two or more operands.

    Parameters
    ----------
    x, y, *rest
        Expressions, variables or numbers.

    Returns
    -------
    expr
        The expression ``min(x, y, *rest)``.

    Raises
    ------
    TypeError
        When an operand is of another type.
    """
    return _apply(Operator.MIN, x, y, *rest)
