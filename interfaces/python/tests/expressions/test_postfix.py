import pytest

import biyos
from biyos import Operator
from biyos._model.postfix import postfix

STALE = "the variable predates a deletion from its model"
VAR = Operator.VARIABLE


@pytest.fixture
def env():
    with biyos.Env() as env:
        yield env


@pytest.fixture
def model(env):
    with biyos.Model(env) as model:
        yield model


@pytest.fixture
def x(model):
    return model.addvar(3)


def entries(model, *trees):
    _, code, arg, val = postfix(model, trees, {})
    return list(zip(code, arg)), val


def test_variable(model, x):
    assert entries(model, x[2]) == ([(VAR, 2)], [])


def test_sum(model, x):
    pairs, val = entries(model, x[0] + 2 * x[1] - 3)
    assert pairs == [(VAR, 0), (VAR, 1), (Operator.SUM, 2)]
    assert val == [-3.0, 1.0, 2.0]


def test_scaled(model, x):
    pairs, val = entries(model, 2 * (x[0] + x[1] + 1))
    assert pairs == [(VAR, 0), (VAR, 1), (Operator.SUM, 2)]
    assert val == [2.0, 2.0, 2.0]


def test_overflow(model, x):
    pairs, val = entries(model, 1e300 * (1e300 * x[0]))
    assert pairs == [(VAR, 0), (Operator.SUM, 1), (Operator.SUM, 1)]
    assert val == [0.0, 1e300, 0.0, 1e300]


def test_large(model, x):
    e = (x[0] + 1e308) + (x[1] + 1e308)
    pairs, val = entries(model, e)
    assert pairs == [(VAR, 0), (VAR, 1), (Operator.SUM, 1), (Operator.SUM, 2)]
    assert val == [1e308, 1.0, 1e308, 1.0, 1.0]
    assert len(model.addexpr(*postfix(model, [e], {}))) == 1


def test_operators(model, x):
    e = biyos.exp(x[0] * x[1]) / x[2] ** 2
    pairs, val = entries(model, e)
    assert pairs == [
        (VAR, 0),
        (VAR, 1),
        (Operator.PRODUCT, 2),
        (Operator.EXP, 1),
        (VAR, 2),
        (Operator.CONSTANT_POWER, 1),
        (Operator.DIVISION, 2),
    ]
    assert val == [1.0, 2.0]


def test_constant(model, x):
    pairs, val = entries(model, biyos.max(x[0], 0))
    assert pairs == [(VAR, 0), (Operator.CONSTANT, 0), (Operator.MAX, 2)]
    assert val == [0.0]


def test_shared(model, x):
    s = x[0] + x[1]
    pairs, val = entries(model, biyos.exp(s) * s)
    assert pairs == [
        (VAR, 0),
        (VAR, 1),
        (Operator.SUM, 2),
        (Operator.KEEP, 1),
        (Operator.EXP, 1),
        (Operator.REFERENCE, 3),
        (Operator.PRODUCT, 2),
    ]
    assert val == [0.0, 1.0, 1.0, 1.0]


def test_unmerged(model, x):
    s = x[0] + x[1]
    pairs, _ = entries(model, s + s)
    assert pairs[2:] == [
        (Operator.SUM, 2),
        (Operator.KEEP, 1),
        (Operator.REFERENCE, 3),
        (Operator.SUM, 2),
    ]


def test_several(model, x):
    s = x[0] * x[1]
    pairs, _ = entries(model, s, x[2], biyos.log(s))
    assert postfix(model, [s, x[2], biyos.log(s)], {})[0] == [4, 1, 2]
    assert pairs[3] == (Operator.KEEP, 1)
    assert pairs[5:] == [(Operator.REFERENCE, 3), (Operator.LOG, 1)]


def test_long(model, x):
    e = 0
    for k in range(10**5):
        e += x[k % 3]
    cnt, code, arg, val = postfix(model, [e], {})
    assert cnt == [10**5 + 1]
    assert code[-1] == Operator.SUM
    assert arg[-1] == 10**5
    assert len(model.addexpr(cnt, code, arg, val)) == 1


def test_deep(model, x):
    e = x[0]
    for _ in range(10**5):
        e = -e
    cnt, code = postfix(model, [e], {})[:2]
    assert cnt == [10**5 + 1]
    assert code.count(Operator.NEGATION) == 10**5


def test_stale(model, x):
    e = x[0] + x[1]
    model.delvar([2])
    with pytest.raises(ValueError, match=STALE):
        postfix(model, [e], {})


def test_foreign(env, model, x):
    with biyos.Model(env) as other:
        y = other.addvar(1)
        with pytest.raises(ValueError, match="expr is a variable of another"):
            postfix(model, [x[0] + y[0]], {})


def test_refused(model):
    with pytest.raises(TypeError, match="expr must be an expression, not"):
        postfix(model, [1.0], {})
