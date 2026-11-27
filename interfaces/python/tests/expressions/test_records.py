import pytest

import biyos
from biyos import ObjectiveSense, Operator

STALE = "the variable predates a deletion from its model"
VAR = Operator.VARIABLE
CONST = Operator.CONSTANT


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
    return model.addvar(4)


def records(model, funs):
    return (
        model.getcst(funs),
        model.getrlinterm(funs),
        model.getquadterm(funs),
        model.getexpr(funs),
    )


def by_hand(env, code, arg, val):
    with biyos.Model(env) as model:
        model.addvar(4)
        nodes = model.addexpr([len(code)], code, arg, val)
        funs = model.addfun(1)
        model.attachexpr(funs, nodes)
        return records(model, funs)


def objective(model, objs):
    return model.getobj(objs)[0].indices()


def test_acceptance(model, x):
    objs = model.addobj(x[0] + x[1] + x[2] ** 2 + biyos.log(x[3]))
    funs = objective(model, objs)
    assert model.getrlinterm(funs) == ([0, 0], [0, 1], [1.0, 1.0])
    assert model.getquadterm(funs) == ([0], [2], [2], [1.0])
    cnt, code, arg, val = model.getexpr(funs)
    assert (cnt, code, arg, val) == ([2], [VAR, Operator.LOG], [3, 1], [])


UNARY = [
    (biyos.exp, Operator.EXP),
    (biyos.log, Operator.LOG),
    (biyos.sqrt, Operator.SQRT),
    (biyos.sin, Operator.SIN),
    (biyos.cos, Operator.COS),
    (biyos.tan, Operator.TAN),
    (biyos.asin, Operator.ASIN),
    (biyos.acos, Operator.ACOS),
    (biyos.atan, Operator.ATAN),
    (biyos.sinh, Operator.SINH),
    (biyos.cosh, Operator.COSH),
    (biyos.tanh, Operator.TANH),
    (biyos.asinh, Operator.ASINH),
    (biyos.acosh, Operator.ACOSH),
    (biyos.atanh, Operator.ATANH),
    (biyos.erf, Operator.ERF),
    (biyos.entropy, Operator.ENTROPY),
    (abs, Operator.ABS),
]


@pytest.mark.parametrize(("function", "op"), UNARY)
def test_unary(env, model, x, function, op):
    objs = model.addobj(function(x[0]))
    expected = by_hand(env, [VAR, op], [0, 1], [])
    assert records(model, objective(model, objs)) == expected


def sum_of(x):
    return x[0] + 2 * x[1] - 3


def product(x):
    return x[0] * x[1] * x[2]


def division(x):
    return x[0] / x[1]


def scaled(x):
    return x[0] / 4


def inverse(x):
    return 2 / x[0]


def cube(x):
    return x[0] ** 3


def power(x):
    return x[0] ** x[1]


def exponential(x):
    return 2 ** x[0]


def negation(x):
    return -x[0]


def signpow(x):
    return biyos.signpow(x[0], 2.5)


def largest(x):
    return biyos.max(x[0], x[1], x[2])


def smallest(x):
    return biyos.min(x[0], 1)


def nested(x):
    return biyos.exp(x[0] * biyos.sin(x[1] + 1)) - x[2] / x[3]


SUM = Operator.SUM
PRODUCT = Operator.PRODUCT
DIVISION = Operator.DIVISION
EXP = Operator.EXP
OTHERS = [
    (sum_of, [VAR, VAR, SUM], [0, 1, 2], [-3.0, 1.0, 2.0]),
    (product, [VAR, VAR, VAR, PRODUCT], [0, 1, 2, 3], [1.0]),
    (division, [VAR, VAR, DIVISION], [0, 1, 2], []),
    (scaled, [VAR, SUM], [0, 1], [0.0, 0.25]),
    (inverse, [CONST, VAR, DIVISION], [0, 0, 2], [2.0]),
    (cube, [VAR, Operator.CONSTANT_POWER], [0, 1], [3.0]),
    (power, [VAR, VAR, Operator.POWER], [0, 1, 2], []),
    (exponential, [CONST, VAR, Operator.POWER], [0, 0, 2], [2.0]),
    (negation, [VAR, Operator.NEGATION], [0, 1], []),
    (signpow, [VAR, Operator.SIGN_POWER], [0, 1], [2.5]),
    (largest, [VAR, VAR, VAR, Operator.MAX], [0, 1, 2, 3], []),
    (smallest, [VAR, CONST, Operator.MIN], [0, 0, 2], [1.0]),
    (
        nested,
        [VAR, VAR, SUM, Operator.SIN, PRODUCT, EXP, VAR, VAR, DIVISION, SUM],
        [0, 1, 1, 1, 2, 1, 2, 3, 2, 2],
        [1.0, 1.0, 1.0, 0.0, 1.0, -1.0],
    ),
]


@pytest.mark.parametrize(("build", "code", "arg", "val"), OTHERS)
def test_operator(env, model, x, build, code, arg, val):
    objs = model.addobj(build(x))
    expected = by_hand(env, code, arg, val)
    assert records(model, objective(model, objs)) == expected


def test_shared(model, x):
    s = x[0] * x[1]
    objs = model.addobj(biyos.exp(s) * biyos.sin(s))
    code = model.getexpr(objective(model, objs))[1]
    assert code.count(Operator.KEEP) == 1
    assert code.count(Operator.REFERENCE) == 1


def test_variable(model, x):
    objs = model.addobj(x[2])
    funs = objective(model, objs)
    assert model.getrlinterm(funs) == ([0], [2], [1.0])
    assert model.getexpr(funs) == ([1], [CONST], [0], [0.0])


def test_objectives(model, x):
    sense = [ObjectiveSense.MAXIMIZE, ObjectiveSense.MINIMIZE]
    objs = model.addobj([x[0], 2 * x[1]], sense, [1, 0], [0.5, 2.0])
    assert type(objs) is biyos.Objectives
    funs, read, prio, coef = model.getobj(objs)
    found = (funs.indices(), read, prio, coef)
    assert found == ([0, 1], sense, [1, 0], [0.5, 2.0])
    assert model.getnfun() == 2


def test_lengths(model, x):
    sense = [ObjectiveSense.MAXIMIZE] * 2
    with pytest.raises(ValueError, match="sense has length 2, not 1"):
        model.addobj([x[0] + x[1]], sense)
    assert model.getnfun() == 0


def test_functions(model, x):
    funs = model.addfun(1)
    objs = model.addobj(funs)
    assert objective(model, objs) == [0]
    assert model.getnfun() == 1


def bounds(model, cons):
    funs, lb, ub = model.getcon(cons)
    return model.getrlinterm(funs)[1:], lb, ub


def test_less(model, x):
    cons = model.addcon(x[0] <= x[1])
    assert type(cons) is biyos.Constraints
    assert bounds(model, cons) == (([0, 1], [1.0, -1.0]), [-biyos.inf], [0.0])


def test_greater(model, x):
    cons = model.addcon(x[0] >= x[1])
    assert bounds(model, cons) == (([0, 1], [1.0, -1.0]), [0.0], [biyos.inf])


def test_reversed(model, x):
    cons = model.addcon([1 <= x[0] + x[1], x[0] + x[1] >= 1])
    terms = ([0, 1, 0, 1], [1.0, 1.0, 1.0, 1.0])
    assert bounds(model, cons) == (terms, [1.0, 1.0], [biyos.inf] * 2)


def test_equal(model, x):
    cons = model.addcon(x[0] * x[1] == 2)
    funs, lb, ub = model.getcon(cons)
    assert (lb, ub) == ([2.0], [2.0])
    assert model.getquadterm(funs) == ([0], [0], [1], [1.0])


def test_constant(model, x):
    cons = model.addcon(x[0] + 3 <= 5)
    funs, _, ub = model.getcon(cons)
    assert ub == [5.0]
    assert model.getcst(funs) == [3.0]


def test_bounds(model, x):
    with pytest.raises(TypeError, match="lb and ub come from"):
        model.addcon(x[0] <= 1, ub=[2.0])
    with pytest.raises(TypeError, match="lb and ub come from"):
        model.addcon([x[0] <= 1], lb=[0.0])
    assert model.getnfun() == 0


def test_mixed(model, x):
    with pytest.raises(TypeError, match="fun.1. is int, not a comparison"):
        model.addcon([x[0] <= 1, 0])
    with pytest.raises(TypeError, match="fun.1. is str, not an expression"):
        model.addobj([x[0], "a"])
    assert model.getnfun() == 0


def test_stale(model, x):
    e = x[0] + x[1]
    model.delvar([3])
    with pytest.raises(ValueError, match=STALE):
        model.addobj(e)
    with pytest.raises(ValueError, match=STALE):
        model.addcon(e <= 1)
    assert model.getnfun() == 0


def test_foreign(env, model, x):
    with biyos.Model(env) as other:
        y = other.addvar(1)
        with pytest.raises(ValueError, match="variable of another model"):
            model.addobj(x[0] + y[0])
        with pytest.raises(ValueError, match="variable of another model"):
            model.addcon(y[0] <= 1)
    assert model.getnfun() == 0
