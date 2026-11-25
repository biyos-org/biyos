import array
import ctypes
import struct
from functools import partial
from operator import index

import pytest

import biyos
from biyos import EventKind, EventLevel, Operator
from biyos._capi import lib
from number import Number
from refusal import LOCKED, refusal
from sized import Room, limit

NAN = float("nan")
BIG = 1.7976931348623157e308

CNT = [3, 2, 2]
X = [-1.0, 0.5, 2.0, -0.0, 0.1, 1.0, 4.0]
Y = [1.0, -0.0, 3.0, 0.3, 7.0, 2.0, 2.0]
SAMPLE = (CNT, X, Y)
TAIL = ([2, 2], [-0.0, 0.1, 1.0, 4.0], [0.3, 7.0, 2.0, 2.0])
NONE = ([], [], [])

FORMS = {
    "block": "rg",
    "range": "rg",
    "list": "sp",
    "stepped": "sp",
}


@pytest.fixture
def env():
    with biyos.Env() as env:
        yield env


@pytest.fixture
def model(env):
    with biyos.Model(env) as model:
        model.addvar(4)
        yield model


def bits(values):
    return [struct.pack("<d", value) for value in values]


def same(found, wanted):
    assert found == wanted
    assert bits(found[1]) == bits(wanted[1])
    assert bits(found[2]) == bits(wanted[2])


def read_lengths(handle, beg, end, cap, counts, x, y):
    return [(counts, end - beg), (x, cap), (y, cap)]


def recorded(seen, function, *arguments):
    seen.append(arguments)
    return function(*arguments)


def traced(seen, form, function, *arguments):
    seen.append(form)
    return function(*arguments)


def trace(monkeypatch, seen, call):
    for form in ("sp", "rg"):
        name = f"{call}{form}"
        spy = partial(traced, seen, form, getattr(lib, name))
        monkeypatch.setattr(lib, name, spy)


def selection(block, kind):
    if kind == "block":
        return block[1:3]
    if kind == "range":
        return range(1, 3)
    if kind == "list":
        return [1, 2]
    return range(0, 3, 2)


def records(kind):
    return [0, 2] if kind == "stepped" else [1, 2]


def test_add(model):
    added = model.addpwl(*SAMPLE)
    assert model.getnpwl() == 3
    same(model.getpwl(), SAMPLE)
    assert type(added) is biyos.PiecewiseLinears
    assert [index(item) for item in added] == [0, 1, 2]
    assert model.getnpwlpt() == 7


def test_block(model):
    model.addpwl([2], [0.0, 1.0], [0.0, 1.0])
    p = model.addpwl([2, 2, 2], [0.0, 1.0] * 3, [1.0, 0.0] * 3)
    assert len(p) == 3
    assert all(type(item) is biyos.PiecewiseLinear for item in p)
    assert type(p[1:]) is biyos.PiecewiseLinears
    assert [index(item) for item in p[1:]] == [2, 3]
    assert len(model.addpwl([], [], [])) == 0
    assert model.getnpwl() == 4


def test_empty(model):
    assert model.getpwl() == NONE
    assert model.getnpwlpt() == 0
    model.addpwl(*SAMPLE)
    assert model.getpwl(range(3, 3)) == NONE
    assert model.getpwl([]) == NONE
    assert model.getnpwlpt([]) == 0


def test_listed(model):
    model.addpwl(*SAMPLE)
    idx = [2, 0, 2, 1]
    found = model.getpwl(idx)
    wanted = (
        [2, 3, 2, 2],
        [1.0, 4.0, -1.0, 0.5, 2.0, 1.0, 4.0, -0.0, 0.1],
        [2.0, 2.0, 1.0, -0.0, 3.0, 2.0, 2.0, 0.3, 7.0],
    )
    same(found, wanted)
    assert model.getnpwlpt(idx) == 9
    assert model.getpwl([Number(1)]) == ([2], [-0.0, 0.1], [0.3, 7.0])


def test_range(model):
    p = model.addpwl(*SAMPLE)
    same(model.getpwl(range(1, 3)), TAIL)
    same(model.getpwl(p[1:]), TAIL)
    assert model.getnpwlpt(range(1, 3)) == 4
    assert model.getnpwlpt(p[:1]) == 3


@pytest.mark.parametrize("kind", FORMS)
def test_forms(model, monkeypatch, kind):
    p = model.addpwl(*SAMPLE)
    wanted = model.getpwl(records(kind))
    seen = []
    trace(monkeypatch, seen, "BIYOS_getnpwlpt")
    trace(monkeypatch, seen, "BIYOS_getpwl")
    assert model.getpwl(selection(p, kind)) == wanted
    assert seen == [FORMS[kind]] * 2


def test_values(model, monkeypatch):
    x = array.array("d", [0.0, 1.0])
    y = array.array("d", [2.0, 3.0])
    seen = []
    spy = partial(recorded, seen, lib.BIYOS_addpwl)
    monkeypatch.setattr(lib, "BIYOS_addpwl", spy)
    model.addpwl([2], x, y)
    assert len(seen) == 1
    assert ctypes.addressof(seen[0][3]) == x.buffer_info()[0]
    assert ctypes.addressof(seen[0][4]) == y.buffer_info()[0]
    assert model.getpwl() == ([2], [0.0, 1.0], [2.0, 3.0])


def test_sequences(model):
    sizes = array.array("q", [2, 3])
    model.addpwl(sizes, (0, 1, 0, 1, 2), range(5))
    model.addpwl((Number(2),), [1, 2], [3, 4])
    found = model.getpwl()
    assert found[0] == [2, 3, 2]
    assert found[1] == [0.0, 1.0, 0.0, 1.0, 2.0, 1.0, 2.0]
    assert found[2] == [0.0, 1.0, 2.0, 3.0, 4.0, 3.0, 4.0]
    assert all(type(value) is float for value in found[1] + found[2])


def test_views(env, model):
    x = model.addvar(1)
    p = model.addpwl(*SAMPLE)
    assert model.getpwl([p[2], 0])[0] == [2, 3]
    other = biyos.Model(env)
    q = other.addpwl([2], [0.0, 1.0], [0.0, 1.0])
    with pytest.raises(ValueError, match="is a pwl of another model"):
        model.getpwl([q[0]])
    with pytest.raises(ValueError, match="idx is a block of another model"):
        model.getnpwlpt(q)
    with pytest.raises(ValueError, match="pwl is a pwl of another model"):
        model.addpwlnode(q[0], 0)
    with pytest.raises(TypeError, match="is a variable, not a pwl"):
        model.getpwl([x[0]])
    with pytest.raises(TypeError, match="of variables, not pwls"):
        model.getpwl(x)
    with pytest.raises(TypeError, match="var is a pwl, not a variable"):
        model.addvarnode(p[0])
    with pytest.raises(TypeError, match="is a pwl, not a set"):
        model.getsos([p[0]])
    assert model.getnpwl() == 3
    other.close()


def test_gone(model):
    p = model.addpwl(*SAMPLE)
    view = p[0]
    model._gen += 1
    with pytest.raises(ValueError, match="the pwl predates a deletion"):
        index(view)
    with pytest.raises(ValueError, match="the pwl predates a deletion"):
        model.getpwl([view])
    with pytest.raises(ValueError, match="the block predates a deletion"):
        model.getpwl(p)
    same(model.getpwl(), SAMPLE)


def test_stale(model, monkeypatch):
    model.addpwl(*SAMPLE)
    monkeypatch.setattr(model, "getnpwl", lambda: 0)
    spy = Room(lib.BIYOS_getpwlrg, read_lengths)
    monkeypatch.setattr(lib, "BIYOS_getpwlrg", spy)
    same(model.getpwl(range(3)), SAMPLE)
    assert model.getpwl() == NONE


def test_outside(model, monkeypatch):
    model.addpwl(*SAMPLE)
    limit(monkeypatch, biyos._model.model, 1)
    message = refusal(model.getpwl, range(2**31 - 1))
    assert message == "invalid argument: pwl 3: unknown"
    message = refusal(model.getpwl, range(-1, 2))
    assert message == "invalid argument: pwl -1: unknown"
    message = refusal(model.getpwl, [0, 3])
    assert message == "invalid argument: pwl 3: unknown"
    message = refusal(model.getnpwlpt, range(4, 4))
    assert message == "invalid argument: pwl 4: unknown"


REFUSALS = [
    (
        ([2, 1], [0.0, 1.0, 2.0], [0.0, 1.0, 2.0]),
        "pwl 4: a pwl takes 2 points or more, not 1",
    ),
    (([2], [0.0, 0.0], [1.0, 2.0]), "pwl 3: x 1 is not above x 0"),
    (([2], [1.0, 0.0], [1.0, 2.0]), "pwl 3: x 1 is not above x 0"),
    (([2], [0.0, NAN], [1.0, 2.0]), "pwl 3: x 1 is NaN"),
    (([2], [0.0, 1.0], [1.0, -biyos.inf]), "pwl 3: y 1 is minus infinity"),
    (
        ([2], [-BIG, BIG], [0.0, 1.0]),
        "pwl 3: the pwl's width from point 0 to point 1 is infinity",
    ),
    (([2], None, [0.0, 1.0]), "argument 'x': is null"),
    (([2], [0.0, 1.0], None), "argument 'y': is null"),
]


@pytest.mark.parametrize(("given", "text"), REFUSALS)
def test_refused(model, given, text):
    model.addpwl(*SAMPLE)
    assert refusal(model.addpwl, *given) == f"invalid argument: {text}"
    same(model.getpwl(), SAMPLE)


def test_lengths(model):
    with pytest.raises(ValueError, match="x has length 1, not 2"):
        model.addpwl([2], [0.0], [0.0, 1.0])
    with pytest.raises(ValueError, match="y has length 3, not 2"):
        model.addpwl([2], [0.0, 1.0], [0.0, 1.0, 2.0])
    with pytest.raises(ValueError, match="cnt: -1 is negative at entry 1"):
        model.addpwl([2, -1], [0.0, 1.0], [0.0, 1.0])
    assert model.getnpwl() == 0


def test_numbers(model):
    with pytest.raises(TypeError, match="cnt must be a sequence of int"):
        model.addpwl(None, [], [])
    with pytest.raises(TypeError, match="an entry of cnt must be an int"):
        model.addpwl([2.0], [0.0, 1.0], [0.0, 1.0])
    with pytest.raises(TypeError, match="an entry of x must be a number"):
        model.addpwl([2], ["0", 1.0], [0.0, 1.0])
    with pytest.raises(TypeError, match="an entry of y must be a number"):
        model.addpwl([2], [0.0, 1.0], [0.0, True])
    with pytest.raises(OverflowError, match="cnt: 9223372036854775808 does"):
        model.addpwl([2**63], [], [])
    with pytest.raises(OverflowError, match="idx: 4294967296 does not fit"):
        model.getpwl(range(2**32, 2**32 + 1))
    assert model.getnpwl() == 0


def test_node(model):
    p = model.addpwl(*SAMPLE)
    f = model.addfun(2)
    x = model.addvarnode(1)
    first = model.addpwlnode(p[2], x)
    second = model.addpwlnode(1, first)
    model.attachexpr(f, [first, second])
    pwl = Operator.PIECEWISE_LINEAR
    alone = ([3], [Operator.VARIABLE, pwl, pwl], [1, 2, 1], [])
    assert model.getexpr([1]) == alone
    code = [Operator.VARIABLE, pwl, Operator.KEEP, Operator.REFERENCE, pwl]
    assert model.getexpr() == ([3, 2], code, [1, 2, 1, 2, 1], [])


def test_node_refused(env, model):
    model.addpwl(*SAMPLE)
    x = model.addvarnode(0)
    message = refusal(model.addpwlnode, 3, x)
    assert message == "invalid argument: expression: pwl 3 is unknown"
    message = refusal(model.addpwlnode, -1, x)
    assert message == "invalid argument: expression: pwl -1 is unknown"
    message = refusal(model.addpwlnode, 0, x + 1)
    assert message == "invalid argument: expression: node 1 is unknown"
    with pytest.raises(TypeError, match="pwl is a variable, not a pwl"):
        model.addpwlnode(model.addvar(1)[0], x)
    with pytest.raises(TypeError, match="arg must be an int, not float"):
        model.addpwlnode(0, 0.0)
    with pytest.raises(OverflowError, match="arg: 9223372036854775808"):
        model.addpwlnode(0, 2**63)
    assert model.addvarnode(1) == 1


def test_copy(model):
    p = model.addpwl(*SAMPLE)
    copy = model.copy()
    same(copy.getpwl(), SAMPLE)
    copy.addpwl([2], [0.0, 1.0], [0.0, 1.0])
    assert model.getnpwl() == 3
    same(copy.getpwl(p.on(copy)), SAMPLE)
    assert copy.getnpwl() == 4
    copy.close()


def test_locked(env, model):
    seen = []

    def change(event):
        try:
            model.addpwl([2], [0.0, 1.0], [0.0, 1.0])
        except biyos.Error as error:
            seen.append(str(error))
        seen.append(model.getnpwl())
        seen.append(model.getpwl([0]))

    model.addpwl(*SAMPLE)
    env.addhook(change, EventKind.MESSAGE, EventLevel.ERROR)
    with pytest.raises(biyos.Error):
        env.setopt("seed", -1)
    first = ([3], [-1.0, 0.5, 2.0], [1.0, -0.0, 3.0])
    assert seen == [LOCKED, 3, first]
    assert model.getnpwl() == 3


def test_closed(env):
    model = biyos.Model(env)
    model.close()
    calls = (
        partial(model.addpwl, [], [], []),
        model.getnpwl,
        model.getnpwlpt,
        model.getpwl,
        partial(model.addpwlnode, 0, 0),
    )
    for call in calls:
        with pytest.raises(ValueError, match="the model is closed"):
            call()
