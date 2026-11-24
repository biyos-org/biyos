from operator import contains, eq, ge, gt, index, le, lt, ne

import pytest

import biyos
from biyos import Operator

INFINITY = float("inf")
STALE = "the block predates a deletion from its model"


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


def tree(term):
    if isinstance(term, biyos.Variable):
        return index(term)
    children = tuple(tree(arg) for arg in term._args)
    return (term._op, term._values, *children)


def parts(con):
    assert type(con) is biyos.Comparison
    return tree(con._body), con._lb, con._ub


def chained(term):
    return 0 <= term <= 1


DIFFERENCE = (Operator.SUM, (0.0, 1.0, -1.0), 0, 1)


def test_less(x):
    assert parts(x[0] <= x[1]) == (DIFFERENCE, -INFINITY, 0.0)


def test_greater(x):
    assert parts(x[0] >= x[1]) == (DIFFERENCE, 0.0, INFINITY)


def test_equal(x):
    assert parts(x[0] == x[1]) == (DIFFERENCE, 0.0, 0.0)


def test_number(x):
    assert parts(x[0] <= 3) == (0, -INFINITY, 3.0)
    assert parts(x[0] >= 3) == (0, 3.0, INFINITY)
    assert parts(x[0] == 3) == (0, 3.0, 3.0)


def test_reversed(x):
    assert parts(3 <= x[0]) == parts(x[0] >= 3)
    assert parts(3 >= x[0]) == parts(x[0] <= 3)
    assert parts(3 == x[0]) == parts(x[0] == 3)


def test_expressions(x):
    e = x[0] * x[1]
    product = (Operator.PRODUCT, (1.0,), 0, 1)
    body = (Operator.SUM, (0.0, 1.0, -1.0), product, 2)
    assert parts(e <= x[2]) == (body, -INFINITY, 0.0)
    assert parts(e + 1 <= 2) == (
        (Operator.SUM, (1.0, 1.0), product),
        -INFINITY,
        2.0,
    )


def test_truth(x):
    with pytest.raises(TypeError, match="has no truth value"):
        bool(x[0] + 0 == x[1])
    with pytest.raises(TypeError, match="has no truth value"):
        ne(x[0] + 0, x[1])
    with pytest.raises(TypeError, match="has no truth value"):
        bool(x[0] == 1)
    with pytest.raises(TypeError, match="has no truth value"):
        bool(x[0] <= x[0])
    with pytest.raises(TypeError, match="has no truth value"):
        chained(x[0])


def test_same(env, x):
    assert x[0] == x[0]
    assert not x[0] == x[1]
    assert x[0] != x[1]
    assert {x[0]: 1}[x[0]] == 1
    assert x[0] in {x[1], x[0]}
    assert x[0] in [x[1], x[0]]
    assert [x[1], x[0]].index(x[0]) == 1
    with biyos.Model(env) as other:
        assert other.addvar(1)[0] != x[0]


def test_refused(x):
    nan = float("nan")
    for bound in (nan, INFINITY, -INFINITY):
        with pytest.raises(ValueError, match="refuses the bound"):
            eq(x[0], bound)
    for bound in (nan, INFINITY):
        with pytest.raises(ValueError, match="refuses the bound"):
            ge(x[0], bound)
        with pytest.raises(ValueError, match="refuses the bound"):
            le(bound, x[0])
    for bound in (nan, -INFINITY):
        with pytest.raises(ValueError, match="refuses the bound"):
            le(x[0], bound)
        with pytest.raises(ValueError, match="refuses the bound"):
            ge(bound, x[0])


def test_open(x):
    assert parts(x[0] <= INFINITY) == (0, -INFINITY, INFINITY)
    assert parts(x[0] >= -INFINITY) == (0, -INFINITY, INFINITY)


@pytest.mark.parametrize("other", ["a", None, True, 1j])
def test_foreign(x, other):
    assert not eq(x[0], other)
    assert not eq(other, x[0])
    assert x[0] in [other, x[0]]
    with pytest.raises(TypeError, match="compares with a number, not"):
        le(x[0], other)
    with pytest.raises(TypeError, match="compares with a number, not"):
        le(other, x[0])


def test_strict(x):
    with pytest.raises(TypeError):
        lt(x[0], x[1])
    with pytest.raises(TypeError):
        gt(x[0], 1)


def test_hash(model, x):
    assert hash(x[0]) == hash(x[0])
    assert len({x[0], x[1], x[2]}) == 3
    keys = dict.fromkeys(x)
    first = next(iter(keys))
    assert first in keys
    assert index(first) == 0


def test_unhashable(x):
    with pytest.raises(TypeError):
        hash(x[0] + x[1])


def test_contains(model, x):
    y = model.addvar(2)
    assert x[1] in x
    assert x[1] in x[1:]
    assert x[1] not in x[2:]
    assert y[0] not in x
    assert 1 not in x
    assert "a" not in x
    f = model.addfun(1)
    assert biyos.Function(model, 1, model._gen) not in f
    assert f[0] in f


def test_other_model(env, x):
    with biyos.Model(env) as other:
        y = other.addvar(3)
        assert y[0] not in x


def test_stale(model, x):
    view = x[0]
    model.delvar([2])
    with pytest.raises(ValueError, match=STALE):
        contains(x, view)
    fresh = model.addvar(1)
    with pytest.raises(ValueError, match="variable predates"):
        contains(fresh, view)
