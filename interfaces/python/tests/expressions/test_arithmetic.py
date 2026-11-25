from operator import add, index, mul, pow, sub, truediv

import pytest

import biyos
from biyos import Operator


@pytest.fixture
def env():
    with biyos.Env() as env:
        yield env


@pytest.fixture
def x(env):
    with biyos.Model(env) as model:
        yield model.addvar(3)


def tree(term):
    if isinstance(term, biyos.Variable):
        return index(term)
    children = tuple(tree(arg) for arg in term._args)
    return (term._op, term._values, *children)


def test_sum(x):
    assert tree(x[0] + x[1]) == (Operator.SUM, (0.0, 1.0, 1.0), 0, 1)
    assert tree(x[0] - x[1]) == (Operator.SUM, (0.0, 1.0, -1.0), 0, 1)


def test_shift(x):
    assert tree(x[0] + 2) == (Operator.SUM, (2.0, 1.0), 0)
    assert tree(2 + x[0]) == (Operator.SUM, (2.0, 1.0), 0)
    assert tree(x[0] - 2) == (Operator.SUM, (-2.0, 1.0), 0)
    assert tree(2 - x[0]) == (Operator.SUM, (2.0, -1.0), 0)


def test_scale(x):
    assert tree(3 * x[0]) == (Operator.SUM, (0.0, 3.0), 0)
    assert tree(x[0] * 3) == (Operator.SUM, (0.0, 3.0), 0)
    assert tree(x[0] / 4) == (Operator.SUM, (0.0, 0.25), 0)


def test_product(x):
    assert tree(x[0] * x[1]) == (Operator.PRODUCT, (1.0,), 0, 1)
    assert tree(x[0] / x[1]) == (Operator.DIVISION, (), 0, 1)
    constant = (Operator.CONSTANT, (2.0,))
    assert tree(2 / x[1]) == (Operator.DIVISION, (), constant, 1)


def test_power(x):
    assert tree(x[0] ** 2) == (Operator.CONSTANT_POWER, (2.0,), 0)
    assert tree(x[0] ** x[1]) == (Operator.POWER, (), 0, 1)
    constant = (Operator.CONSTANT, (2.0,))
    assert tree(2 ** x[1]) == (Operator.POWER, (), constant, 1)


def test_unary(x):
    assert tree(-x[0]) == (Operator.NEGATION, (), 0)
    assert tree(abs(x[0])) == (Operator.ABS, (), 0)


def test_nested(x):
    e = (x[0] + 1) * x[1] ** 2
    inner = (Operator.SUM, (1.0, 1.0), 0)
    power = (Operator.CONSTANT_POWER, (2.0,), 1)
    assert tree(e) == (Operator.PRODUCT, (1.0,), inner, power)
    assert type(e) is biyos.Expression


def test_unchanged(x):
    e = x[0] + x[1]
    before = tree(e)
    e += x[2]
    assert tree(e) == (Operator.SUM, (0.0, 1.0, 1.0), before, 2)


def test_integers(x):
    assert tree(x[0] + 2)[1] == (2.0, 1.0)
    assert all(type(value) is float for value in tree(x[0] * 3)[1])
    assert all(type(value) is float for value in tree(x[0] ** 2)[1])


@pytest.mark.parametrize("other", [None, True, 1j])
@pytest.mark.parametrize("apply", [add, sub, mul, truediv, pow])
def test_foreign(x, apply, other):
    with pytest.raises(TypeError):
        apply(x[0], other)
    with pytest.raises(TypeError):
        apply(other, x[0])


@pytest.mark.parametrize("apply", [add, sub, mul, truediv, pow])
def test_text(x, apply):
    with pytest.raises(TypeError):
        apply(x[0], "a")
    with pytest.raises(TypeError):
        apply("a", x[0])


def test_zero(x):
    with pytest.raises(ZeroDivisionError):
        x[0] / 0


UNARY = {
    biyos.exp: Operator.EXP,
    biyos.log: Operator.LOG,
    biyos.sqrt: Operator.SQRT,
    biyos.sin: Operator.SIN,
    biyos.cos: Operator.COS,
    biyos.tan: Operator.TAN,
    biyos.asin: Operator.ASIN,
    biyos.acos: Operator.ACOS,
    biyos.atan: Operator.ATAN,
    biyos.sinh: Operator.SINH,
    biyos.cosh: Operator.COSH,
    biyos.tanh: Operator.TANH,
    biyos.asinh: Operator.ASINH,
    biyos.acosh: Operator.ACOSH,
    biyos.atanh: Operator.ATANH,
    biyos.erf: Operator.ERF,
    biyos.entropy: Operator.ENTROPY,
}


@pytest.mark.parametrize("function", UNARY)
def test_function(x, function):
    assert tree(function(x[0])) == (UNARY[function], (), 0)
    inner = (Operator.SUM, (0.0, 1.0, 1.0), 0, 1)
    assert tree(function(x[0] + x[1])) == (UNARY[function], (), inner)
    constant = (Operator.CONSTANT, (2.0,))
    assert tree(function(2)) == (UNARY[function], (), constant)


@pytest.mark.parametrize("function", UNARY)
def test_refused(x, function):
    with pytest.raises(TypeError, match="not str"):
        function("a")
    with pytest.raises(TypeError, match="not bool"):
        function(True)


def test_signpow(x):
    e = biyos.signpow(x[0], 3)
    assert tree(e) == (Operator.SIGN_POWER, (3.0,), 0)
    with pytest.raises(TypeError, match="p must be a number, not Variable"):
        biyos.signpow(x[0], x[1])
    with pytest.raises(TypeError, match="not NoneType"):
        biyos.signpow(None, 2)


@pytest.mark.parametrize(
    ("function", "op"),
    [(biyos.max, Operator.MAX), (biyos.min, Operator.MIN)],
)
def test_extreme(x, function, op):
    assert tree(function(x[0], x[1])) == (op, (), 0, 1)
    zero = (Operator.CONSTANT, (0.0,))
    assert tree(function(x[0], 0, x[2])) == (op, (), 0, zero, 2)
    with pytest.raises(TypeError):
        function(x[0])
    with pytest.raises(TypeError, match="not str"):
        function(x[0], x[1], "a")


def test_builtins():
    assert "max" not in biyos.__all__
    assert "min" not in biyos.__all__
    assert "exp" in biyos.__all__


def test_operators():
    reached = {
        *UNARY.values(),
        Operator.SIGN_POWER,
        Operator.PIECEWISE_LINEAR,
        Operator.MAX,
        Operator.MIN,
        Operator.SUM,
        Operator.PRODUCT,
        Operator.NEGATION,
        Operator.DIVISION,
        Operator.CONSTANT_POWER,
        Operator.POWER,
        Operator.ABS,
        Operator.CONSTANT,
    }
    postfix = {
        Operator.VARIABLE,
        Operator.NODE,
        Operator.KEEP,
        Operator.REFERENCE,
    }
    assert reached | postfix == set(Operator)
    assert not reached & postfix
