from collections.abc import Sequence
from typing import TypeGuard

from biyos._model.operators import Operator


def _number(value: object) -> TypeGuard[int | float]:
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

    def __radd__(self, other: float) -> "Expression":
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

    def __rsub__(self, other: float) -> "Expression":
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

    def __rmul__(self, other: float) -> "Expression":
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

    def __rtruediv__(self, other: float) -> "Expression":
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

    def __rpow__(self, other: float) -> "Expression":
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


Term = Operand | int | float


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
        values: tuple[float, ...],
    ) -> None:
        self._op = op
        self._args = args
        self._values = values


def _constant(value: float) -> Expression:
    return Expression(Operator.CONSTANT, (), (float(value),))


def _shifted(term: Operand, weight: float, shift: float) -> Expression:
    return Expression(Operator.SUM, (term,), (float(shift), weight))


def _sum(left: Operand, right: "Term", sign: float) -> Expression:
    if isinstance(right, Operand):
        return Expression(Operator.SUM, (left, right), (0.0, 1.0, sign))
    return _shifted(left, 1.0, sign * float(right))
