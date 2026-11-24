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
