from functools import partial

import pytest

import biyos
from biyos import EventKind, EventLevel, Operator
from biyos._capi import lib
from listed import indexed
from number import Number
from refusal import LOCKED, refusal


CNT = [8]
CODE = [
    Operator.CONSTANT,
    Operator.VARIABLE,
    Operator.VARIABLE,
    Operator.VARIABLE,
    Operator.PRODUCT,
    Operator.VARIABLE,
    Operator.EXP,
    Operator.SUM,
]
ARG = [0, 0, 0, 1, 2, 1, 1, 4]
VAL = [3.0, 1.0, 0.0, 1.0, 2.0, 1.0, 1.0]


@pytest.fixture
def env():
    with biyos.Env() as env:
        yield env


@pytest.fixture
def model(env):
    with biyos.Model(env) as model:
        model.addvar(3)
        model.addfun(2)
        model.setcst([0], [0.5])
        yield model


def next_id(model):
    return model.addnode(Operator.CONSTANT, [], [1.0])


def recorded(seen, function, *arguments):
    seen.append(arguments)
    return function(*arguments)


def traced(seen, form, function, *arguments):
    seen.append(form)
    return function(*arguments)


def trace(monkeypatch, seen, call):
    for form in ("sp", "rg"):
        name = f"BIYOS_{call}{form}"
        spy = partial(traced, seen, form, getattr(lib, name))
        monkeypatch.setattr(lib, name, spy)


def test_nodes(model):
    x = model.addvarnode(0)
    assert (x, model.addvarnode(0)) == (0, 0)
    exponential = model.addnode(Operator.EXP, [x])
    assert exponential == 1
    ratio = model.addnode(Operator.DIVISION, [exponential, exponential], [])
    total = model.addnode(Operator.SUM, [ratio, x], [0.5, 1.0, 2.0])
    product = model.addnode(Operator.PRODUCT, (total, Number(x)), [3])
    power = model.addnode(Operator.CONSTANT_POWER, [product], [2.5])
    assert model.addnode(Operator.SIGN_POWER, [power], [0.5]) == 6
    assert model.addvarnode(2) == 7
    assert isinstance(x, int)


def test_views(env, model):
    x = model.addvar(1)
    f = model.addfun(1)
    assert model.addvarnode(x[0]) == 0
    other = biyos.Model(env)
    y = other.addvar(1)
    with pytest.raises(ValueError, match="var is a variable of another"):
        model.addvarnode(y[0])
    with pytest.raises(TypeError, match="var is a function, not a"):
        model.addvarnode(f[0])
    model.attachexpr(f, [0])
    model.attachexpr([f[0]], [0])
    assert indexed(model.getrlinterm(f)) == ([2], [3], [2.0])
    model.clrfun(f)
    assert indexed(model.getrlinterm(f)) == ([], [], [])


def test_postfix(model):
    assert model.addexpr(CNT, CODE, ARG, VAL) == [5]
    assert next_id(model) == 6
    assert model.addexpr([], [], []) == []
    shared = model.addexpr(
        [2, 2],
        [Operator.NODE, Operator.SIN, Operator.NODE, Operator.COS],
        [3, 1, 3, 1],
    )
    assert shared == [7, 8]
    assert next_id(model) == 9


KEPT_CODE = [
    Operator.VARIABLE,
    Operator.VARIABLE,
    Operator.SUM,
    Operator.KEEP,
    Operator.REFERENCE,
    Operator.SUM,
    Operator.REFERENCE,
    Operator.SQRT,
]
KEPT_ARG = [0, 1, 2, 1, 3, 2, 3, 1]
KEPT_VAL = [0.0, 1.0, 1.0, 0.5, 2.0, 3.0]


def test_kept(model):
    tops = model.addexpr([6, 2], KEPT_CODE, KEPT_ARG, KEPT_VAL)
    assert tops == [3, 4]
    assert next_id(model) == 5


def test_same_graph(env):
    with biyos.Model(env) as nodes, biyos.Model(env) as postfix:
        for model in (nodes, postfix):
            model.addvar(2)
            model.addfun(1)
        x = nodes.addvarnode(0)
        exponential = nodes.addnode(Operator.EXP, [x])
        y = nodes.addvarnode(1)
        sine = nodes.addnode(Operator.SIN, [y])
        top = nodes.addnode(Operator.PRODUCT, [exponential, sine], [2.0])
        code = [
            Operator.VARIABLE,
            Operator.EXP,
            Operator.VARIABLE,
            Operator.SIN,
            Operator.PRODUCT,
        ]
        tops = postfix.addexpr([5], code, [0, 1, 1, 1, 2], [2.0])
        assert tops == [top]
        assert next_id(nodes) == next_id(postfix)


def test_attach(model):
    top = model.addexpr(CNT, CODE, ARG, VAL)
    model.addlinterm([1], [2], [1.0])
    assert model.attachexpr([1], top) is None
    assert model.getcst(range(2)) == [0.5, 3.0]
    assert indexed(model.getrlinterm()) == ([1, 1], [0, 2], [2.0, 1.0])
    assert indexed(model.getquadterm()) == ([1], [0], [1], [1.0])
    model.attachexpr([1], top)
    assert model.getcst(range(2)) == [0.5, 6.0]
    assert indexed(model.getrlinterm()) == ([1, 1], [0, 2], [4.0, 1.0])
    assert indexed(model.getquadterm()) == ([1], [0], [1], [2.0])


@pytest.mark.parametrize(
    "kind, form", [("block", "rg"), ("range", "rg"), ("list", "sp")]
)
def test_forms(model, monkeypatch, kind, form):
    f = model.addfun(2)
    idx = {"block": f, "range": range(2, 4), "list": [2, 3]}[kind]
    x = model.addvarnode(0)
    seen = []
    trace(monkeypatch, seen, "attachexpr")
    trace(monkeypatch, seen, "clrfun")
    model.attachexpr(idx, [x, x])
    assert indexed(model.getrlinterm()) == ([2, 3], [0, 0], [1.0, 1.0])
    model.clrfun(idx)
    assert indexed(model.getrlinterm()) == ([], [], [])
    assert seen == [form] * 2


def test_shared(model):
    x = model.addvarnode(0)
    sine = model.addnode(Operator.SIN, [x])
    tops = model.addexpr(
        [3, 2],
        [Operator.NODE, Operator.VARIABLE, Operator.SUM]
        + [Operator.NODE, Operator.SQRT],
        [sine, 1, 2, sine, 1],
        [0.0, 1.0, 1.0],
    )
    model.attachexpr([0, 1], tops)
    assert indexed(model.getrlinterm()) == ([0], [1], [1.0])
    assert next_id(model) == 5


def test_clear(model):
    top = model.addexpr(CNT, CODE, ARG, VAL)
    model.attachexpr([0, 1], top * 2)
    model.clrfun([1, 1])
    assert model.getcst(range(2)) == [3.5, 0.0]
    assert indexed(model.getrlinterm()) == ([0], [0], [2.0])
    assert indexed(model.getquadterm()) == ([0], [0], [1], [1.0])
    model.attachexpr([1], top)
    assert model.getcst(range(2)) == [3.5, 3.0]
    model.clrfun([])
    model.clrfun(range(2, 2))
    message = refusal(model.clrfun, [2])
    assert message == "invalid argument: function 2: unknown"
    message = refusal(model.clrfun, range(1, 3))
    assert message == "invalid argument: function 2: unknown"


def rebuilt(env, model):
    copy = biyos.Model(env)
    copy.addvar(model.getnvar())
    f = copy.addfun(model.getnfun())
    copy.setcst(f, model.getcst(range(model.getnfun())))
    copy.addlinterm(*indexed(model.getrlinterm()))
    copy.addquadterm(*indexed(model.getquadterm()))
    copy.attachexpr(f, copy.addexpr(*model.getexpr()))
    return copy


def test_read(model):
    top = model.addexpr(CNT, CODE, ARG, VAL)
    model.attachexpr([1], top)
    assert model.getnexpr() == (3, 1)
    cnt, code, arg, val = model.getexpr()
    assert (cnt, code, arg, val) == (
        [1, 2],
        [Operator.CONSTANT, Operator.VARIABLE, Operator.EXP],
        [0, 1, 1],
        [0.0],
    )
    assert all(type(entry) is Operator for entry in code)
    twice = [Operator.VARIABLE, Operator.EXP, Operator.KEEP]
    twice += [Operator.REFERENCE]
    assert model.getexpr([1, 1]) == ([3, 1], twice, [1, 1, 1, 2], [])
    assert model.getnexpr([1, 1]) == (4, 0)
    assert model.getexpr([]) == ([], [], [], [])
    assert model.getnexpr(range(2, 2)) == (0, 0)


def test_round_trip(env, model):
    tops = model.addexpr([6, 2], KEPT_CODE, KEPT_ARG, KEPT_VAL)
    x = model.addvarnode(0)
    sine = model.addnode(Operator.SIN, [x])
    ratio = model.addnode(Operator.DIVISION, [sine, sine])
    f = model.addfun(2)
    model.attachexpr([0, 1, f[0]], [*tops, ratio])
    model.addlinterm([f[1]], [2], [4.0])
    model.addquadterm([f[1]], [1], [2], [0.5])
    with rebuilt(env, model) as copy:
        assert copy.getexpr() == model.getexpr()
        assert copy.getcst(range(4)) == model.getcst(range(4))
        assert indexed(copy.getrlinterm()) == indexed(model.getrlinterm())
        assert indexed(copy.getquadterm()) == indexed(model.getquadterm())
    code = [Operator.VARIABLE, Operator.SIN, Operator.KEEP]
    code += [Operator.REFERENCE, Operator.DIVISION, Operator.CONSTANT]
    assert model.getexpr(f) == ([5, 1], code, [0, 1, 1, 2, 2, 0], [0.0])


@pytest.mark.parametrize(
    "kind, form", [("block", "rg"), ("range", "rg"), ("list", "sp")]
)
def test_read_forms(model, monkeypatch, kind, form):
    f = model.addfun(2)
    idx = {"block": f, "range": range(2, 4), "list": [2, 3]}[kind]
    model.attachexpr([3], model.addexpr(CNT, CODE, ARG, VAL))
    seen = []
    trace(monkeypatch, seen, "getnexpr")
    trace(monkeypatch, seen, "getexpr")
    code = [Operator.CONSTANT, Operator.VARIABLE, Operator.EXP]
    assert model.getexpr(idx) == ([1, 2], code, [0, 1, 1], [0.0])
    assert model.getnexpr(idx) == (3, 1)
    assert seen == [form] * 3


def test_refused(model):
    x = model.addvarnode(0)
    expression = "invalid argument: expression 0: "
    cases = [
        (([2], [Operator.VARIABLE] * 2, [0, 1]), "2 operands left, not 1"),
        (([1], [35], [0]), "entry 0: operator 35 is unknown"),
        (([1], [Operator.VARIABLE], [3]), "entry 0: variable 3 is unknown"),
        (([1], [Operator.NODE], [1]), "entry 0: node 1 is unknown"),
        (
            ([2], [Operator.VARIABLE, Operator.REFERENCE], [0, 0]),
            "entry 1: entry 0 is no earlier keep",
        ),
        (
            ([2], [Operator.VARIABLE, Operator.DIVISION], [0, 2]),
            "entry 1: 2 operands taken, 1 given",
        ),
        (
            ([2], [Operator.VARIABLE, Operator.LOG], [0, 2]),
            "entry 1: operator 10 takes 1 child, not 2",
        ),
        (([0], [], []), "0 operands left, not 1"),
    ]
    for arguments, message in cases:
        assert refusal(model.addexpr, *arguments) == expression + message
    message = refusal(
        model.addexpr, [1], [Operator.CONSTANT], [0], [biyos.inf]
    )
    assert message == expression + "entry 0: constant is infinity"
    message = refusal(model.addnode, Operator.EXP, [x, x])
    assert message == (
        "invalid argument: expression: operator 9 takes 1 child, not 2"
    )
    message = refusal(model.addnode, Operator.NODE, [x])
    assert message == "invalid argument: expression: operator 30 is unknown"
    message = refusal(model.addnode, Operator.KEEP, [x])
    assert message == "invalid argument: expression: operator 32 is unknown"
    message = refusal(model.addnode, Operator.PRODUCT, [x])
    assert message == "invalid argument: argument 'val': is null"
    message = refusal(model.addvarnode, 3)
    assert message == "invalid argument: variable 3: unknown"
    message = refusal(model.attachexpr, [0, 0], [x, x])
    assert message == (
        "invalid argument: argument 'idx': function 0 is repeated"
    )
    message = refusal(model.attachexpr, [1], [1])
    assert message == "invalid argument: function 1: node 1 is unknown"
    message = refusal(model.attachexpr, range(1, 3), [x, x])
    assert message == "invalid argument: function 2: unknown"
    message = refusal(model.attachexpr, range(1), [1])
    assert message == "invalid argument: function 0: node 1 is unknown"
    message = refusal(model.getexpr, [2])
    assert message == "invalid argument: function 2: unknown"
    message = refusal(model.getexpr, range(1, 3))
    assert message == "invalid argument: function 2: unknown"
    message = refusal(model.getnexpr, [0, 2])
    assert message == "invalid argument: function 2: unknown"
    assert next_id(model) == 1


def test_lengths(model):
    with pytest.raises(ValueError, match="code has length 7, not 8"):
        model.addexpr(CNT, CODE[1:], ARG, VAL)
    with pytest.raises(ValueError, match="arg has length 7, not 8"):
        model.addexpr(CNT, CODE, ARG[1:], VAL)
    with pytest.raises(ValueError, match="val has length 6, not 7"):
        model.addexpr(CNT, CODE, ARG, VAL[1:])
    with pytest.raises(ValueError, match="val has length 1, not 0"):
        model.addnode(Operator.EXP, [0], [1.0])
    with pytest.raises(ValueError, match="val has length 1, not 3"):
        model.addnode(Operator.SUM, [0, 0], [1.0])
    with pytest.raises(ValueError, match="node has length 0, not 1"):
        model.attachexpr([0], [])
    with pytest.raises(ValueError, match="cnt: -1 is negative at entry 1"):
        model.addexpr([2, -1], [Operator.VARIABLE], [0])
    assert next_id(model) == 0


def test_types(model):
    with pytest.raises(TypeError, match="op must be an int, not float"):
        model.addnode(1.0, [], [1.0])
    with pytest.raises(TypeError, match="an entry of arg must be an int"):
        model.addnode(Operator.EXP, [0.0])
    with pytest.raises(TypeError, match="an entry of code must be an int"):
        model.addexpr([1], ["1"], [0])
    with pytest.raises(TypeError, match="an entry of val must be a number"):
        model.addexpr([1], [Operator.CONSTANT], [0], ["1"])
    with pytest.raises(TypeError, match="an entry of node must be an int"):
        model.attachexpr([0], [True])
    with pytest.raises(OverflowError, match="2147483648 does not fit"):
        model.addnode(2**31, [], [])
    assert next_id(model) == 0


def test_gone(model, monkeypatch):
    variable = model.addvar(1)[0]
    f = model.addfun(1)
    model._gen += 1
    seen = []
    names = ["BIYOS_addvarnode"]
    for call in ("attachexpr", "clrfun", "getnexpr", "getexpr"):
        names += [f"BIYOS_{call}sp", f"BIYOS_{call}rg"]
    for name in names:
        spy = partial(recorded, seen, getattr(lib, name))
        monkeypatch.setattr(lib, name, spy)
    stale = "the block predates a deletion"
    cases = [
        ("the variable predates", partial(model.addvarnode, variable)),
        (stale, partial(model.attachexpr, f, [0])),
        (stale, partial(model.clrfun, f)),
        (stale, partial(model.getexpr, f)),
    ]
    for message, call in cases:
        with pytest.raises(ValueError, match=message):
            call()
    assert seen == []


def test_locked(env, model):
    seen = []

    def change(event):
        calls = (
            partial(model.addvarnode, 0),
            partial(model.addnode, Operator.CONSTANT, [], [1.0]),
            partial(model.addexpr, [], [], []),
            partial(model.attachexpr, [], []),
            partial(model.clrfun, []),
        )
        for call in calls:
            try:
                call()
            except biyos.Error as error:
                seen.append(str(error))
        seen.append(model.getexpr())

    env.addhook(change, EventKind.MESSAGE, EventLevel.ERROR)
    with pytest.raises(biyos.Error):
        env.setopt("seed", -1)
    empty = ([1, 1], [Operator.CONSTANT] * 2, [0, 0], [0.0, 0.0])
    assert seen == [LOCKED] * 5 + [empty]
    assert next_id(model) == 0


def test_closed(env):
    model = biyos.Model(env)
    model.close()
    calls = (
        partial(model.addvarnode, 0),
        partial(model.addnode, Operator.CONSTANT, [], [1.0]),
        partial(model.addexpr, [], [], []),
        partial(model.attachexpr, [], []),
        partial(model.clrfun, []),
        partial(model.getnexpr, []),
        partial(model.getexpr, []),
    )
    for call in calls:
        with pytest.raises(ValueError, match="the model is closed"):
            call()


def test_copy(model):
    top = model.addexpr(CNT, CODE, ARG, VAL)
    model.attachexpr([1], top)
    copy = model.copy()
    assert copy.getcst(range(2)) == model.getcst(range(2))
    assert indexed(copy.getrlinterm()) == indexed(model.getrlinterm())
    assert indexed(copy.getquadterm()) == indexed(model.getquadterm())
    assert next_id(copy) == 6
    copy.clrfun([1])
    assert model.getcst(range(2)) == [0.5, 3.0]
