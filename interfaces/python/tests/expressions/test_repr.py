import sys
from math import nan
from operator import index

import pytest

import biyos
from biyos import Expression, Operator


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


def direct(x):
    a, b, _ = x
    return [
        (Expression(Operator.CONSTANT, (), (2.0,)), "2"),
        (Expression(Operator.VARIABLE, (), (1.0,)), "var(1)"),
        (Expression(Operator.PRODUCT, (a, b), (2.0,)), "2*x[0]*x[1]"),
        (Expression(Operator.NODE, (), (3.0,)), "node(3)"),
        (Expression(Operator.KEEP, (a + b,), ()), "x[0] + x[1]"),
        (Expression(Operator.REFERENCE, (), (4.0,)), "ref(4)"),
        (Expression(Operator.LESS_EQUAL, (a, b), ()), "le(x[0], x[1])"),
        (Expression(Operator.GREATER_EQUAL, (a, b), ()), "ge(x[0], x[1])"),
        (Expression(Operator.EQUAL, (a, b), ()), "eq(x[0], x[1])"),
        (Expression(Operator.LESS, (a, b), ()), "lt(x[0], x[1])"),
        (Expression(Operator.GREATER, (a, b), ()), "gt(x[0], x[1])"),
        (Expression(Operator.NOT_EQUAL, (a, b), ()), "ne(x[0], x[1])"),
        (Expression(Operator.AND, (a, b), ()), "and(x[0], x[1])"),
        (Expression(Operator.OR, (a, b), ()), "or(x[0], x[1])"),
        (Expression(Operator.NOT, (a,), ()), "not(x[0])"),
        (Expression(Operator.IF, (a, b, a), ()), "ifelse(x[0], x[1], x[0])"),
    ]


def built(x):
    a, b, c = x
    return [
        (a + 2 * b, "x[0] + 2*x[1]"),
        (a * b, "x[0]*x[1]"),
        (-a, "-x[0]"),
        (a / b, "x[0]/x[1]"),
        (a**2, "x[0]**2"),
        (a**b, "x[0]**x[1]"),
        (biyos.exp(a), "exp(x[0])"),
        (biyos.log(a), "log(x[0])"),
        (biyos.sqrt(a), "sqrt(x[0])"),
        (biyos.sin(a), "sin(x[0])"),
        (biyos.cos(a), "cos(x[0])"),
        (biyos.tan(a), "tan(x[0])"),
        (abs(a), "abs(x[0])"),
        (biyos.max(a, b), "max(x[0], x[1])"),
        (biyos.min(a, b, 2), "min(x[0], x[1], 2)"),
        (biyos.asin(a), "asin(x[0])"),
        (biyos.acos(a), "acos(x[0])"),
        (biyos.atan(a), "atan(x[0])"),
        (biyos.atan2(b, a), "atan2(x[1], x[0])"),
        (biyos.sinh(a), "sinh(x[0])"),
        (biyos.cosh(a), "cosh(x[0])"),
        (biyos.tanh(a), "tanh(x[0])"),
        (biyos.asinh(a), "asinh(x[0])"),
        (biyos.acosh(a), "acosh(x[0])"),
        (biyos.atanh(a), "atanh(x[0])"),
        (biyos.entropy(a), "entropy(x[0])"),
        (biyos.signpow(a, 1.5), "signpow(x[0], 1.5)"),
        (biyos.erf(a), "erf(x[0])"),
        (biyos.piecewise(1, a + c), "piecewise(1, x[0] + x[2])"),
        (biyos.ifelse(a >= b, a, 2), "ifelse(x[0] >= x[1], x[0], 2)"),
        ((a + b) * (a + b), "(x[0] + x[1])*(x[0] + x[1])"),
    ]


def test_operator(x):
    cases = direct(x) + built(x)
    for expr, text in cases:
        assert repr(expr) == text
    assert {expr._op for expr, _ in cases} == set(Operator)


def test_variable(x):
    assert repr(x[2]) == "x[2]"
    assert repr(x[0] ** 2 + 3 * x[1]) == "x[0]**2 + 3*x[1]"


def test_closed(env):
    with biyos.Model(env) as model:
        x = model.addvar(2)
    assert repr(x[1]) == "x[1]"
    assert repr(x[0] - x[1]) == "x[0] - x[1]"


PRECEDENCE = {
    "-(x[0] + x[1])": lambda a, b, c: -(a + b),
    "(x[0] + x[1])*x[2]": lambda a, b, c: (a + b) * c,
    "x[0]**-1": lambda a, b, c: a**-1,
    "-x[0]**2": lambda a, b, c: -(a**2),
    "(-x[0])**2": lambda a, b, c: (-a) ** 2,
    "-(-x[0])": lambda a, b, c: -(-a),
    "-(2*x[0])": lambda a, b, c: -(2 * a),
    "x[0]/(x[1]*x[2])": lambda a, b, c: a / (b * c),
    "x[0]*x[1]/x[2]": lambda a, b, c: a * b / c,
    "x[0]*(x[1]/x[2])": lambda a, b, c: a * (b / c),
    "x[0]**x[1]**x[2]": lambda a, b, c: a ** (b**c),
    "(x[0]**x[1])**x[2]": lambda a, b, c: (a**b) ** c,
    "x[0]**(x[1] + 1)": lambda a, b, c: a ** (b + 1),
    "x[0]**-x[1]": lambda a, b, c: a ** (-b),
    "x[0]**(2*x[1])": lambda a, b, c: a ** (2 * b),
    "(-2)**x[0]": lambda a, b, c: (-2) ** a,
    "2/x[0]": lambda a, b, c: 2 / a,
    "x[0] - (x[1] - x[2])": lambda a, b, c: a - (b - c),
    "x[0] + (x[1] + x[2])": lambda a, b, c: a + (b + c),
    "x[0] + x[1] + x[2]": lambda a, b, c: a + b + c,
    "x[0] - x[1]*x[2]": lambda a, b, c: a - b * c,
    "3*(x[0] + x[1])": lambda a, b, c: 3 * (a + b),
    "3*(x[0]*x[1])": lambda a, b, c: 3 * (a * b),
    "3*-x[0]": lambda a, b, c: 3 * -a,
    "exp(x[0] + 1)*x[1]": lambda a, b, c: biyos.exp(a + 1) * b,
}


@pytest.mark.parametrize("text", PRECEDENCE)
def test_precedence(x, text):
    assert repr(PRECEDENCE[text](*x)) == text


WEIGHTS = {
    "x[0] + x[1]": lambda a, b: a + b,
    "x[0] - x[1]": lambda a, b: a - b,
    "-1*x[0]": lambda a, b: -1 * a,
    "-1*x[0] + x[1]": lambda a, b: -1 * a + b,
    "2.5*x[0]": lambda a, b: 2.5 * a,
    "-2*x[0]": lambda a, b: -2 * a,
    "0.25*x[0]": lambda a, b: a / 4,
    "x[0] - 2*x[1]": lambda a, b: a - 2 * b,
    "x[0] + -1*x[1]": lambda a, b: a + -1 * b,
    "x[0] + -x[1]": lambda a, b: a + -b,
    "x[0] + nan*x[1]": lambda a, b: a + nan * b,
}


@pytest.mark.parametrize("text", WEIGHTS)
def test_weight(x, text):
    assert repr(WEIGHTS[text](x[0], x[1])) == text


CONSTANTS = {
    "x[0] + 2": lambda a: a + 2,
    "x[0] - 2": lambda a: a - 2,
    "2 - x[0]": lambda a: 2 - a,
    "-2 - x[0]": lambda a: -2 - a,
    "2 - 3*x[0]": lambda a: 2 - 3 * a,
    "x[0] + 0.5": lambda a: 0.5 + a,
    "x[0] - inf": lambda a: a - biyos.inf,
    "2*x[0] + 1e+20": lambda a: 2 * a + 1e20,
}


@pytest.mark.parametrize("text", CONSTANTS)
def test_constant(x, text):
    assert repr(CONSTANTS[text](x[0])) == text


def test_zero(x):
    assert repr(x[0] + 0) == "x[0]"


NAMES = (
    "exp",
    "log",
    "sqrt",
    "sin",
    "cos",
    "tan",
    "max",
    "min",
    "asin",
    "acos",
    "atan",
    "atan2",
    "sinh",
    "cosh",
    "tanh",
    "asinh",
    "acosh",
    "atanh",
    "entropy",
    "signpow",
    "erf",
    "piecewise",
    "ifelse",
    "inf",
)


def tree(term):
    if isinstance(term, biyos.Variable):
        return index(term)
    values = tuple(value.hex() for value in term._values)
    children = tuple(tree(arg) for arg in term._args)
    return (term._op, values, *children)


def read(text, x):
    names = {name: getattr(biyos, name) for name in NAMES}
    return eval(text, {**names, "nan": nan, "x": x})


def written(x):
    a, b, c = x
    cases = built(x)
    cases += [(PRECEDENCE[text](a, b, c), text) for text in PRECEDENCE]
    cases += [(WEIGHTS[text](a, b), text) for text in WEIGHTS]
    cases += [(CONSTANTS[text](a), text) for text in CONSTANTS]
    return cases


def test_trip(x):
    for expr, _ in written(x):
        assert tree(read(repr(expr), x)) == tree(expr)


def test_view(model, x):
    p = model.addpwl([2], [0.0, 1.0], [5.0, 6.0])
    e = biyos.piecewise(p[0], x[1])
    model.delvar([x[2]])
    assert repr(e) == "piecewise(0, x[1])"


def test_deep(x):
    e = x[0]
    depth = 10 * sys.getrecursionlimit()
    for _ in range(depth):
        e = e + x[1]
    assert repr(e) == "x[0]" + depth * " + x[1]"
