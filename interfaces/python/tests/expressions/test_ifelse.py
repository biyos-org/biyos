import sys

import pytest

import biyos
from biyos import Operator, ifelse
from biyos._model.postfix import postfix

VAR = Operator.VARIABLE
CONST = Operator.CONSTANT
SQUARED = "ifelse(x[0] >= x[1], x[0]**2, x[1] + x[0])"


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


def squared(x):
    return ifelse(x[0] >= x[1], x[0] ** 2, x[1] + x[0])


def test_operation(x):
    a = x[0]
    cond = a >= x[1]
    e = ifelse(cond, a, 2)
    assert type(e) is not biyos.Expression
    assert isinstance(e, biyos.Expression)
    assert e._op is Operator.IF
    assert e._cond is cond
    assert e._args[0] is a
    assert (e._args[1]._op, e._args[1]._values) == (CONST, (2.0,))
    assert e._values == ()


def test_taken(model, x):
    model.addobj(squared(x))
    assert model.evalf([3.0, 1.0, 0.0]) == [9.0]
    assert model.evalf([1.0, 3.0, 0.0]) == [4.0]
    assert model.getgpat()[1].indices() == [0, 1]
    assert model.evalg([3.0, 1.0, 0.0]) == [6.0, 0.0]
    assert model.evalg([1.0, 3.0, 0.0]) == [1.0, 1.0]


def test_domain(model, x):
    model.addobj(ifelse(x[0] >= 0, biyos.sqrt(x[0]), 0))
    assert model.evalf([-1.0, 0.0, 0.0]) == [0.0]
    assert model.evalf([4.0, 0.0, 0.0]) == [2.0]


def test_stored(model, x):
    e = squared(x)
    model.addobj(e)
    f = model.getobj()[0]
    assert model.getexpr(f) == (
        [9],
        [VAR, VAR, Operator.GREATER_EQUAL, VAR]
        + [Operator.CONSTANT_POWER, VAR, VAR, Operator.SUM, Operator.IF],
        [0, 1, 2, 0, 1, 1, 0, 2, 3],
        [2.0, 0.0, 1.0, 1.0],
    )
    assert list(model.getexpr(f)) == list(postfix(model, [e], {}))


def test_round_trip(env, model, x):
    model.addobj(squared(x))
    read = model.getexpr(model.getobj()[0])
    with biyos.Model(env) as other:
        other.addvar(3)
        other.attachexpr(other.addfun(1), other.addexpr(*read))
        assert other.getexpr([0]) == read


def test_condition(model, x):
    cond = (x[0] <= 1) | (x[1] >= 2)
    model.addobj(ifelse(cond, x[2], -x[2]))
    code = model.getexpr(model.getobj()[0])[1]
    assert code[:7] == [
        VAR,
        CONST,
        Operator.LESS_EQUAL,
        VAR,
        CONST,
        Operator.GREATER_EQUAL,
        Operator.OR,
    ]
    assert code[-1] is Operator.IF
    assert model.evalf([0.0, 0.0, 5.0]) == [5.0]
    assert model.evalf([3.0, 0.0, 5.0]) == [-5.0]


def test_shared(model, x):
    cond = x[0] >= x[1]
    picked = [ifelse(cond, x[0], 0) <= 1, ifelse(cond, 0, x[1]) >= 0]
    model.addcon([*picked, ~cond])
    code = model.getexpr(model.getcon()[0])[1]
    assert code.count(Operator.GREATER_EQUAL) == 1
    assert code.count(Operator.KEEP) == 1


PRINTED = [
    SQUARED,
    "ifelse(x[0] < 1, 0, 1)",
    "ifelse(x[0] != x[1], x[2], -x[2])",
    "ifelse((x[0] <= 1) | (x[1] >= 2), x[2], 3)",
    "ifelse(~(x[0] == 0), 1/x[0], 0)",
    "ifelse(x[0] >= 0, ifelse(x[1] >= 0, 1, 2), 3)",
    "ifelse(ifelse(x[0] >= 0, x[1], x[2]) <= 1, x[0], 0)",
    "2*ifelse(x[0] >= 0, x[0], 0) + 1",
    "ifelse(x[0] >= 0, x[0], 0)**2",
]


@pytest.mark.parametrize("text", PRINTED)
def test_printed(x, text):
    assert repr(eval(text, {"ifelse": ifelse, "x": x})) == text


def test_direct(x):
    e = biyos.Expression(Operator.IF, (x[0], x[1], x[2]), ())
    assert repr(e) == "ifelse(x[0], x[1], x[2])"


def in_branch(x, e):
    return ifelse(x[1] >= 0, e, x[2])


def in_comparison(x, e):
    return ifelse(e >= 0, x[1], x[2])


def in_condition(x, e):
    return ifelse(~(e >= 0), x[1], x[2])


@pytest.mark.parametrize(
    ("step", "opened", "closed"),
    [
        (in_branch, "ifelse(x[1] >= 0, ", ", x[2])"),
        (in_comparison, "ifelse(", " >= 0, x[1], x[2])"),
        (in_condition, "ifelse(~(", " >= 0), x[1], x[2])"),
    ],
)
def test_deep(model, x, step, opened, closed):
    e = x[0]
    depth = 10 * sys.getrecursionlimit()
    for _ in range(depth):
        e = step(x, e)
    assert repr(e) == depth * opened + "x[0]" + depth * closed
    model.addobj(e)
    code = model.getexpr(model.getobj()[0])[1]
    assert code.count(Operator.IF) == depth


@pytest.mark.parametrize(
    ("cond", "found"),
    [
        (lambda x: 1, "int"),
        (lambda x: 1.5, "float"),
        (lambda x: True, "bool"),
        (lambda x: None, "NoneType"),
        (lambda x: "x[0] >= 0", "str"),
        (lambda x: x[0], "Variable"),
        (lambda x: x[0] + 1, "Operation"),
    ],
)
def test_refused(x, cond, found):
    message = f"cond must be a comparison or a condition, not {found}"
    with pytest.raises(TypeError, match=message):
        ifelse(cond(x), x[0], 0)


def test_branches(x):
    with pytest.raises(TypeError, match="not Comparison"):
        ifelse(x[0] >= 0, x[1] <= 0, 0)
    with pytest.raises(TypeError, match="not Condition"):
        ifelse(x[0] >= 0, 1, ~(x[1] <= 0))
    with pytest.raises(TypeError, match="not str"):
        ifelse(x[0] >= 0, "a", 0)
    with pytest.raises(TypeError, match="not bool"):
        ifelse(x[0] >= 0, 0, True)


@pytest.mark.parametrize(
    "cond",
    [
        lambda x: x[0] <= biyos.inf,
        lambda x: x[0] >= -biyos.inf,
        lambda x: (x[1] >= 0) & (biyos.inf >= x[0]),
    ],
)
def test_infinite(model, x, cond):
    e = ifelse(cond(x), x[1], 0)
    with pytest.raises(ValueError, match="a condition refuses the bound"):
        model.addobj(e)
    with pytest.raises(ValueError, match="a condition refuses the bound"):
        model.addcon([x[0] <= 1, e <= 1])
    assert model.getnobj() == 0
    assert model.getncon() == 0
    assert model.getnfun() == 0


def test_stale(model, x):
    e = ifelse(x[2] >= 0, x[0], x[1])
    model.delvar([2])
    with pytest.raises(ValueError, match="predates a deletion"):
        model.addobj(e)
    assert model.getnfun() == 0
