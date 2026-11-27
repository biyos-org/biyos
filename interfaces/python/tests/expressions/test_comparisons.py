import sys
from operator import contains, eq, ge, gt, index, le, lt, ne

import pytest

import biyos
from biyos import ComparisonSense, Operator

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


@pytest.fixture
def terms(x):
    return {
        "e": biyos.exp(x[0]),
        "f": biyos.log(x[1]),
        "p": biyos.piecewise(0, x[1]),
        "q": biyos.piecewise(1, x[2]),
        "v": x[2],
    }


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
VAR = Operator.VARIABLE
SINGLE = ([2], [VAR, Operator.EXP], [0, 1], [])
DOUBLE = (
    [5],
    [VAR, Operator.EXP, VAR, Operator.LOG, Operator.SUM],
    [0, 1, 1, 1, 2],
    [0.0, 1.0, -1.0],
)
STORED = [
    (le, "e", "f", ([-biyos.inf], [0.0]), DOUBLE),
    (ge, "e", "f", ([0.0], [biyos.inf]), DOUBLE),
    (eq, "e", "f", ([0.0], [0.0]), DOUBLE),
    (le, "e", 2, ([-biyos.inf], [2.0]), SINGLE),
    (ge, "e", 2, ([2.0], [biyos.inf]), SINGLE),
    (eq, "e", 2, ([2.0], [2.0]), SINGLE),
    (le, 2, "e", ([2.0], [biyos.inf]), SINGLE),
    (ge, 2, "e", ([-biyos.inf], [2.0]), SINGLE),
    (eq, 2, "e", ([2.0], [2.0]), SINGLE),
]
LESS_EQUAL = ComparisonSense.LESS_EQUAL
GREATER_EQUAL = ComparisonSense.GREATER_EQUAL
EQUAL = ComparisonSense.EQUAL
LESS = ComparisonSense.LESS
GREATER = ComparisonSense.GREATER
NOT_EQUAL = ComparisonSense.NOT_EQUAL
PAIR = "piecewise(0, x[1]) <= piecewise(1, x[2])"
OPERANDS = [
    (le, "e", "f", ("e", LESS_EQUAL, "f"), "exp(x[0]) <= log(x[1])"),
    (ge, "e", "f", ("e", GREATER_EQUAL, "f"), "exp(x[0]) >= log(x[1])"),
    (eq, "e", "f", ("e", EQUAL, "f"), "exp(x[0]) == log(x[1])"),
    (le, "e", "p", ("e", LESS_EQUAL, "p"), "exp(x[0]) <= piecewise(0, x[1])"),
    (
        ge,
        "e",
        "p",
        ("e", GREATER_EQUAL, "p"),
        "exp(x[0]) >= piecewise(0, x[1])",
    ),
    (eq, "e", "p", ("e", EQUAL, "p"), "exp(x[0]) == piecewise(0, x[1])"),
    (lt, "e", "f", ("e", LESS, "f"), "exp(x[0]) < log(x[1])"),
    (gt, "e", "f", ("e", GREATER, "f"), "exp(x[0]) > log(x[1])"),
    (ne, "e", "f", ("e", NOT_EQUAL, "f"), "exp(x[0]) != log(x[1])"),
    (ne, "v", "p", ("v", NOT_EQUAL, "p"), "x[2] != piecewise(0, x[1])"),
    (le, "p", "e", ("p", LESS_EQUAL, "e"), "piecewise(0, x[1]) <= exp(x[0])"),
    (le, "v", "p", ("v", LESS_EQUAL, "p"), "x[2] <= piecewise(0, x[1])"),
    (le, "p", "v", ("p", LESS_EQUAL, "v"), "piecewise(0, x[1]) <= x[2]"),
    (le, "p", "q", ("p", LESS_EQUAL, "q"), PAIR),
]
NUMBERS = [
    (le, "e", 2, LESS_EQUAL, "exp(x[0]) <= 2"),
    (ge, "e", 2, GREATER_EQUAL, "exp(x[0]) >= 2"),
    (eq, "e", 2, EQUAL, "exp(x[0]) == 2"),
    (le, 2, "e", GREATER_EQUAL, "exp(x[0]) >= 2"),
    (ge, 2, "e", LESS_EQUAL, "exp(x[0]) <= 2"),
    (eq, 2, "e", EQUAL, "exp(x[0]) == 2"),
    (lt, "e", 2, LESS, "exp(x[0]) < 2"),
    (gt, "e", 2, GREATER, "exp(x[0]) > 2"),
    (ne, "e", 2, NOT_EQUAL, "exp(x[0]) != 2"),
    (lt, 2, "e", GREATER, "exp(x[0]) > 2"),
    (gt, 2, "e", LESS, "exp(x[0]) < 2"),
    (ne, 2, "e", NOT_EQUAL, "exp(x[0]) != 2"),
]


def compared(terms, compare, left, right):
    return compare(terms.get(left, left), terms.get(right, right))


def test_less(x):
    assert parts(x[0] <= x[1]) == (DIFFERENCE, -biyos.inf, 0.0)


def test_greater(x):
    assert parts(x[0] >= x[1]) == (DIFFERENCE, 0.0, biyos.inf)


def test_equal(x):
    assert parts(x[0] == x[1]) == (DIFFERENCE, 0.0, 0.0)


def test_number(x):
    assert parts(x[0] <= 3) == (0, -biyos.inf, 3.0)
    assert parts(x[0] >= 3) == (0, 3.0, biyos.inf)
    assert parts(x[0] == 3) == (0, 3.0, 3.0)


def test_reversed(x):
    assert parts(3 <= x[0]) == parts(x[0] >= 3)
    assert parts(3 >= x[0]) == parts(x[0] <= 3)
    assert parts(3 == x[0]) == parts(x[0] == 3)


def test_expressions(x):
    e = x[0] * x[1]
    product = (Operator.PRODUCT, (1.0,), 0, 1)
    body = (Operator.SUM, (0.0, 1.0, -1.0), product, 2)
    assert parts(e <= x[2]) == (body, -biyos.inf, 0.0)
    assert parts(e + 1 <= 2) == (
        (Operator.SUM, (1.0, 1.0), product),
        -biyos.inf,
        2.0,
    )


@pytest.mark.parametrize(
    ("compare", "left", "right", "bounds", "expr"), STORED
)
def test_stored(model, terms, compare, left, right, bounds, expr):
    cons = model.addcon(compared(terms, compare, left, right))
    funs = model.getcon(cons)[0]
    assert model.getconbnd(cons) == bounds
    assert model.getexpr(funs) == expr


@pytest.mark.parametrize(("compare", "left", "right", "kept", "text"), OPERANDS)
def test_operands(terms, compare, left, right, kept, text):
    con = compared(terms, compare, left, right)
    lhs, sense, rhs = kept
    assert con.lhs is terms[lhs]
    assert con.rhs is terms[rhs]
    assert con.sense is sense
    assert repr(con) == text


@pytest.mark.parametrize(("compare", "left", "right", "sense", "text"), NUMBERS)
def test_numbers(terms, compare, left, right, sense, text):
    con = compared(terms, compare, left, right)
    assert con.lhs is terms["e"]
    assert type(con.rhs) is float
    assert con.rhs == 2.0
    assert con.sense is sense
    assert repr(con) == text


def test_printed(x):
    assert repr(x[0] ** 2 <= x[1] + 1) == "x[0]**2 <= x[1] + 1"
    assert repr(1 <= x[0]) == "x[0] >= 1"
    assert repr(x[0] == x[0]) == "x[0] == x[0]"
    assert repr(x[0] - x[1] >= -0.5) == "x[0] - x[1] >= -0.5"
    assert repr(x[0] <= biyos.inf) == "x[0] <= inf"


@pytest.mark.parametrize("name", ["lhs", "rhs", "sense"])
def test_frozen(x, name):
    con = x[0] <= x[1]
    with pytest.raises(AttributeError):
        setattr(con, name, x[2])


def test_truth(x):
    with pytest.raises(TypeError, match="has no truth value"):
        bool(x[0] + 0 == x[1])
    with pytest.raises(TypeError, match="has no truth value"):
        bool(x[0] + 0 != x[1])
    with pytest.raises(TypeError, match="has no truth value"):
        bool(x[0] < x[1])
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
    assert not ne(x[0], x[0])
    assert not ne(x[0:2][1], x[1])
    assert {x[0]: 1}[x[0]] == 1
    assert x[0] in {x[1], x[0]}
    assert x[0] in [x[1], x[0]]
    assert [x[1], x[0]].index(x[0]) == 1
    with biyos.Model(env) as other:
        assert other.addvar(1)[0] != x[0]
        assert not eq(other.addvar(1)[0], x[0])


def test_refused(x):
    nan = float("nan")
    for bound in (nan, biyos.inf, -biyos.inf):
        for compare in (eq, ne, lt, gt):
            with pytest.raises(ValueError, match="refuses the bound"):
                compare(x[0], bound)
            with pytest.raises(ValueError, match="refuses the bound"):
                compare(bound, x[0])
    for bound in (nan, biyos.inf):
        with pytest.raises(ValueError, match="refuses the bound"):
            ge(x[0], bound)
        with pytest.raises(ValueError, match="refuses the bound"):
            le(bound, x[0])
    for bound in (nan, -biyos.inf):
        with pytest.raises(ValueError, match="refuses the bound"):
            le(x[0], bound)
        with pytest.raises(ValueError, match="refuses the bound"):
            ge(bound, x[0])


def test_open(x):
    assert parts(x[0] <= biyos.inf) == (0, -biyos.inf, biyos.inf)
    assert parts(x[0] >= -biyos.inf) == (0, -biyos.inf, biyos.inf)


@pytest.mark.parametrize("other", ["a", None, True, 1j])
def test_foreign(x, other):
    assert not eq(x[0], other)
    assert not eq(other, x[0])
    assert ne(x[0], other)
    assert ne(other, x[0])
    assert x[0] in [other, x[0]]
    with pytest.raises(TypeError, match="compares with a number, not"):
        le(x[0], other)
    with pytest.raises(TypeError, match="compares with a number, not"):
        le(other, x[0])
    with pytest.raises(TypeError, match="compares with a number, not"):
        lt(x[0], other)


def test_strict(x):
    assert parts(x[0] < x[1]) == (DIFFERENCE, -biyos.inf, 0.0)
    assert parts(x[0] > 3) == (0, 3.0, biyos.inf)
    assert parts(x[0] != 3) == (0, 3.0, 3.0)
    assert parts(3 > x[0]) == parts(x[0] < 3)
    assert repr(x[0] + 1 != x[1]) == "x[0] + 1 != x[1]"


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


def logic(cond):
    if type(cond) is biyos.Comparison:
        return (repr(cond.lhs), cond.sense, repr(cond.rhs))
    assert type(cond) is biyos.Condition
    return (cond.operator, *map(logic, cond.operands))


def test_condition(x):
    a = x[0] <= 1
    b = x[1] >= 2
    either = a | b
    assert either.operator is Operator.OR
    assert either.operands[0] is a
    assert either.operands[1] is b
    assert (a & b).operator is Operator.AND
    assert (~a).operator is Operator.NOT
    assert (~a).operands == (a,)


def test_nested(x):
    a = x[0] <= 1
    b = x[1] >= 2
    c = x[2] == 0
    assert logic(~(a & b) | c) == (
        Operator.OR,
        (Operator.NOT, (Operator.AND, logic(a), logic(b))),
        logic(c),
    )


CONDITIONS = [
    "(x[0] <= 1) | (x[1] >= 2)",
    "(x[0] <= 1) & (x[1] >= 2)",
    "~(x[0] >= 1)",
    "(x[0] < 1) | (x[1] != x[2])",
    "~(x[0] > 1)",
    "~~(x[0] >= 1)",
    "~((x[0] >= 1) & (x[1] <= 0))",
    "(x[0] <= 1) & (x[1] <= 1) & (x[2] <= 1)",
    "(x[0] <= 1) & ((x[1] <= 1) & (x[2] <= 1))",
    "(x[0] <= 1) | (x[1] <= 1) | (x[2] <= 1)",
    "(x[0] <= 1) | ((x[1] <= 1) | (x[2] <= 1))",
    "(x[0] <= 1) & (x[1] <= 1) | (x[2] <= 1)",
    "(x[0] <= 1) | (x[1] <= 1) & (x[2] <= 1)",
    "((x[0] <= 1) | (x[1] <= 1)) & (x[2] <= 1)",
    "~(x[0] <= 1) & ~(x[1] <= 1)",
    "~((x[0] <= 1) | (x[1] <= 1))",
    "(exp(x[0]) == x[1] + 1) | (x[2] >= x[0]*x[1])",
]


@pytest.mark.parametrize("text", CONDITIONS)
def test_written(x, text):
    ns = {"exp": biyos.exp, "x": x}
    assert repr(eval(text, ns)) == text


def test_deep_condition(x):
    cond = x[0] <= 0
    depth = 10 * sys.getrecursionlimit()
    for _ in range(depth):
        cond = cond & (x[1] <= 0)
    assert repr(cond) == "(x[0] <= 0)" + depth * " & (x[1] <= 0)"


def unparenthesized(a, b):
    return a <= 1 & b


def test_precedence(x):
    with pytest.raises(TypeError):
        unparenthesized(x[0], x[1])
    with pytest.raises(TypeError):
        (x[0] <= 1) & x[1]
    with pytest.raises(TypeError):
        x[1] | (x[0] <= 1)
    with pytest.raises(TypeError):
        ~x[0]
    with pytest.raises(TypeError):
        (x[0] <= 1) | True


def test_condition_truth(x):
    with pytest.raises(TypeError, match="has no truth value"):
        bool((x[0] <= 1) | (x[1] >= 2))
    with pytest.raises(TypeError, match="has no truth value"):
        bool(~(x[0] == x[1]))
