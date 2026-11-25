import pytest

import biyos
from biyos import Operator
from biyos._model.postfix import postfix
from refusal import refusal

PWL = Operator.PIECEWISE_LINEAR
VAR = Operator.VARIABLE
UNKNOWN = "invalid argument: expression 0: entry 1: pwl 5 is unknown"
CNT = [3, 2]
X = [0.0, 1.0, 2.0, -1.0, 1.0]
Y = [0.0, 1.0, 3.0, 2.0, 0.0]


@pytest.fixture
def env():
    with biyos.Env() as env:
        yield env


@pytest.fixture
def model(env):
    with biyos.Model(env) as model:
        model.addvar(2)
        model.addpwl(CNT, X, Y)
        yield model


@pytest.fixture
def x(model):
    return model.addvar(2)


@pytest.fixture
def p(model):
    return model.addpwl([2], [0.0, 1.0], [5.0, 6.0])


def entries(model, *trees):
    _, code, arg, val = postfix(model, trees)
    return list(zip(code, arg)), val


def test_variable(model, x, p):
    e = biyos.piecewise(p[0], x[1])
    assert e._op == PWL
    assert e._args == (x[1],)
    assert e._values == ()
    assert entries(model, e) == ([(VAR, 3), (PWL, 2)], [])


def test_index(model, x):
    e = biyos.piecewise(1, x[0])
    assert entries(model, e) == ([(VAR, 2), (PWL, 1)], [])


def test_expression(model, x, p):
    e = biyos.piecewise(p[0], 2 * x[0] + 1) - biyos.piecewise(0, x[1])
    pairs, val = entries(model, e)
    assert pairs == [
        (VAR, 2),
        (Operator.SUM, 1),
        (PWL, 2),
        (VAR, 3),
        (PWL, 0),
        (Operator.SUM, 2),
    ]
    assert val == [1.0, 2.0, 0.0, 1.0, -1.0]


def test_number(model):
    e = biyos.piecewise(0, 2)
    assert entries(model, e) == ([(Operator.CONSTANT, 0), (PWL, 0)], [2.0])


def test_nested(model, x):
    e = biyos.piecewise(0, biyos.piecewise(1, x[0]))
    assert entries(model, e)[0] == [(VAR, 2), (PWL, 1), (PWL, 0)]


def test_shared(model, x):
    s = biyos.piecewise(1, x[0])
    pairs, _ = entries(model, s * s)
    assert pairs == [
        (VAR, 2),
        (PWL, 1),
        (Operator.KEEP, 1),
        (Operator.REFERENCE, 2),
        (Operator.PRODUCT, 2),
    ]


def test_round_trip(model, x, p):
    o = model.addobj(biyos.piecewise(p[0], x[0] * x[1]))
    read = model.getexpr(model.getobj(o)[0])
    code = [VAR, VAR, Operator.PRODUCT, PWL]
    assert read == ([4], code, [2, 3, 2, 2], [1.0])
    with biyos.Model(model._env) as other:
        other.addvar(4)
        other.addpwl(*model.getpwl())
        g = other.addfun(1)
        other.attachexpr(g, other.addexpr(*read))
        assert other.getexpr(g) == read


def test_same_graph(env):
    with biyos.Model(env) as nodes, biyos.Model(env) as trees:
        for model in (nodes, trees):
            model.addpwl([2], [0.0, 1.0], [1.0, 0.0])
        nodes.addvar(1)
        top = nodes.addpwlnode(0, nodes.addvarnode(0))
        x = trees.addvar(1)
        tops = trees.addexpr(*postfix(trees, [biyos.piecewise(0, x[0])]))
        assert tops == [top]


def test_objective(model, x, p):
    o = model.addobj(biyos.piecewise(p[0], x[0]) + x[1])
    f = model.getobj(o)[0]
    assert model.getrlinterm(f) == (f, [3], [1.0])
    assert model.getexpr(f) == ([2], [VAR, PWL], [2, 2], [])


def test_refused(model, x):
    with pytest.raises(TypeError, match="p must be an int, not float"):
        biyos.piecewise(0.0, x[0])
    with pytest.raises(TypeError, match="p must be an int, not bool"):
        biyos.piecewise(True, x[0])
    with pytest.raises(TypeError, match="not str"):
        biyos.piecewise(0, "a")
    with pytest.raises(TypeError, match="expr is a variable, not a pwl"):
        postfix(model, [biyos.piecewise(x[1], x[0])])
    assert refusal(model.addobj, biyos.piecewise(5, x[0])) == UNKNOWN
    assert model.getnfun() == 0


def test_foreign(env, model, x):
    with biyos.Model(env) as other:
        q = other.addpwl([2], [0.0, 1.0], [0.0, 1.0])
        e = biyos.piecewise(q[0], x[0])
        with pytest.raises(ValueError, match="expr is a pwl of another"):
            model.addobj(e)
    assert model.getnfun() == 0


def test_stale(model, x, p):
    e = biyos.piecewise(p[0], 1.0)
    view = p[0]
    model.delvar([0])
    with pytest.raises(ValueError, match="the pwl predates a deletion"):
        biyos.piecewise(view, 1.0)
    with pytest.raises(ValueError, match="the pwl predates a deletion"):
        postfix(model, [e])
