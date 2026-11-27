import array
import ctypes
from functools import partial
from operator import index

import pytest

import biyos
from biyos import EventKind, EventLevel, SpecialOrderedSetKind
from biyos._capi import lib
from listed import indexed
from number import Number
from refusal import LOCKED, refusal
from sized import Room, limit

NAN = float("nan")
ONE = SpecialOrderedSetKind.ONE
TWO = SpecialOrderedSetKind.TWO

KINDS = [TWO, ONE, ONE]
CNT = [3, 0, 2]
VARIDX = [2, 0, 3, 1, 0]
COEF = [3.0, -1.0, 2.5, 0.5, 1.5]
SAMPLE = (KINDS, CNT, VARIDX, COEF)
TAIL = ([ONE, ONE], [0, 2], [1, 0], [0.5, 1.5])
NONE = ([], [], [], [])

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


def read_lengths(handle, beg, end, cap, kinds, counts, members, weights):
    n = end - beg
    return [(kinds, n), (counts, n), (members, cap), (weights, cap)]


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
    added = model.addsos(*SAMPLE)
    assert model.getnsos() == 3
    found = indexed(model.getsos())
    assert found == SAMPLE
    assert all(type(kind) is SpecialOrderedSetKind for kind in found[0])
    assert type(added) is biyos.SpecialOrderedSets
    assert [index(item) for item in added] == [0, 1, 2]
    assert model.getnsosvar() == 5


def test_block(model):
    model.addsos([ONE, ONE], [0, 0], [])
    s = model.addsos([TWO] * 3, [1, 1, 1], [0, 1, 2])
    assert len(s) == 3
    assert all(type(item) is biyos.SpecialOrderedSet for item in s)
    assert type(s[1:]) is biyos.SpecialOrderedSets
    assert [index(item) for item in s[1:]] == [3, 4]
    assert len(model.addsos([], [], [])) == 0
    assert model.getnsos() == 5


def test_defaults(model):
    model.addsos([ONE, TWO], [2, 3], [3, 1, 0, 2, 1])
    model.addsos([TWO], [1], [0], [7.0])
    assert model.getsos()[3] == [1.0, 2.0, 1.0, 2.0, 3.0, 7.0]


def test_empty(model):
    model.addsos([ONE, TWO], [0, 0], [])
    assert indexed(model.getsos()) == ([ONE, TWO], [0, 0], [], [])
    assert model.getnsosvar() == 0
    assert indexed(model.getsos(range(2, 2))) == NONE
    assert indexed(model.getsos([])) == NONE


def test_shared(model):
    weights = [1.0, 2.0, 1.0, 2.0]
    model.addsos([ONE, TWO], [2, 2], [0, 1, 1, 0], weights)
    model.addsos([ONE], [1], [0])
    assert model.getsos()[2].indices() == [0, 1, 1, 0, 0]


def test_listed(model):
    model.addsos(*SAMPLE)
    idx = [2, 0, 2, 1]
    assert indexed(model.getsos(idx)) == (
        [ONE, TWO, ONE, ONE],
        [2, 3, 2, 0],
        [1, 0, 2, 0, 3, 1, 0],
        [0.5, 1.5, 3.0, -1.0, 2.5, 0.5, 1.5],
    )
    assert model.getnsosvar(idx) == 7
    assert indexed(model.getsos([Number(1)])) == ([ONE], [0], [], [])


def test_range(model):
    s = model.addsos(*SAMPLE)
    assert indexed(model.getsos(range(1, 3))) == TAIL
    assert indexed(model.getsos(s[1:])) == TAIL
    assert model.getnsosvar(range(1, 3)) == 2
    assert indexed(model.getsos(range(3, 3))) == NONE


@pytest.mark.parametrize("kind", FORMS)
def test_forms(model, monkeypatch, kind):
    s = model.addsos(*SAMPLE)
    wanted = indexed(model.getsos(records(kind)))
    seen = []
    trace(monkeypatch, seen, "BIYOS_getnsosvar")
    trace(monkeypatch, seen, "BIYOS_getsos")
    assert indexed(model.getsos(selection(s, kind))) == wanted
    assert seen == [FORMS[kind]] * 2


def test_values(model, monkeypatch):
    weights = array.array("d", [2.0, 1.0])
    seen = []
    spy = partial(recorded, seen, lib.BIYOS_addsos)
    monkeypatch.setattr(lib, "BIYOS_addsos", spy)
    model.addsos([TWO], [2], [0, 1], weights)
    assert len(seen) == 1
    assert ctypes.addressof(seen[0][5]) == weights.buffer_info()[0]
    assert model.getsos()[3] == [2.0, 1.0]


def test_sequences(model):
    kinds = array.array("i", [2, 1])
    sizes = array.array("q", [2, 1])
    weights = array.array("d", [0.5, 1.5, 4.0])
    model.addsos(kinds, sizes, range(3), weights)
    model.addsos((Number(1),), (Number(1),), (Number(3),))
    found = indexed(model.getsos())
    assert found[:2] == ([TWO, ONE, ONE], [2, 1, 1])
    assert found[2:] == ([0, 1, 2, 3], [0.5, 1.5, 4.0, 1.0])


def test_views(env, model):
    x = model.addvar(2)
    f = model.addfun(1)
    s = model.addsos([ONE, TWO], [2, 2], [x[0], 1, x[1], 0])
    assert model.getsos(s)[2].indices() == [4, 1, 5, 0]
    assert model.getsos([s[1]])[0] == [TWO]
    other = biyos.Model(env)
    y = other.addvar(1)
    t = other.addsos([ONE], [1], y)
    entry = "an entry of var is a variable of another model"
    with pytest.raises(ValueError, match=entry):
        model.addsos([ONE], [1], [y[0]])
    with pytest.raises(ValueError, match="var is a block of another"):
        model.addsos([ONE], [1], y)
    with pytest.raises(ValueError, match="is a set of another model"):
        model.getsos([t[0]])
    with pytest.raises(ValueError, match="sos is a block of another model"):
        model.getnsosvar(t)
    with pytest.raises(TypeError, match="is a set, not a variable"):
        model.addsos([ONE], [1], [s[0]])
    with pytest.raises(TypeError, match="of functions, not variables"):
        model.addsos([ONE], [1], f)
    with pytest.raises(TypeError, match="is a variable, not a set"):
        model.getsos([x[0]])
    with pytest.raises(TypeError, match="of variables, not sets"):
        model.getsos(x)
    with pytest.raises(TypeError, match="is a set, not a constraint"):
        model.getconname([s[0]])
    assert model.getnsos() == 2
    other.close()


def test_gone(model):
    s = model.addsos(*SAMPLE)
    view = s[0]
    model._gen += 1
    with pytest.raises(ValueError, match="the set predates a deletion"):
        index(view)
    with pytest.raises(ValueError, match="the set predates a deletion"):
        model.getsos([view])
    with pytest.raises(ValueError, match="the block predates a deletion"):
        model.getsos(s)
    assert indexed(model.getsos()) == SAMPLE


def test_stale(model, monkeypatch):
    model.addsos(*SAMPLE)
    monkeypatch.setattr(model, "getnsos", lambda: 0)
    spy = Room(lib.BIYOS_getsosrg, read_lengths)
    monkeypatch.setattr(lib, "BIYOS_getsosrg", spy)
    assert indexed(model.getsos(range(3))) == SAMPLE
    assert indexed(model.getsos()) == NONE


def test_outside(model, monkeypatch):
    model.addsos(*SAMPLE)
    limit(monkeypatch, biyos._model.model, 1)
    message = refusal(model.getsos, range(2**31 - 1))
    assert message == "invalid argument: set 3: unknown"
    message = refusal(model.getsos, range(-1, 2))
    assert message == "invalid argument: set -1: unknown"
    message = refusal(model.getsos, [0, 3])
    assert message == "invalid argument: set 3: unknown"
    message = refusal(model.getnsosvar, range(4, 4))
    assert message == "invalid argument: set 4: unknown"


def test_refused(model):
    model.addsos(*SAMPLE)
    equal = ([TWO], [3], [0, 1, 2], [2.0, 1.0, 2.0])
    message = refusal(model.addsos, *equal)
    assert message == "invalid argument: set 3: weight 2 is repeated"
    message = refusal(model.addsos, [ONE, ONE], [1, 2], [1, 3, 3])
    assert message == "invalid argument: set 4: variable 3 is repeated"
    message = refusal(model.addsos, [3], [0], [])
    assert message == "invalid argument: set 3: kind 3 is unknown"
    message = refusal(model.addsos, [ONE], [1], [4])
    assert message == "invalid argument: set 3: variable 4 is unknown"
    message = refusal(model.addsos, [ONE], [1], [0], [NAN])
    assert message == "invalid argument: set 3: weight of variable 0 is NaN"
    message = refusal(model.addsos, [TWO], [1], [1], [-biyos.inf])
    assert message == (
        "invalid argument: set 3: weight of variable 1 is minus infinity"
    )
    message = refusal(model.addsos, [ONE], [1], None)
    assert message == "invalid argument: argument 'var': is null"
    assert indexed(model.getsos()) == SAMPLE


def test_lengths(model):
    with pytest.raises(ValueError, match="cnt has length 1, not 2"):
        model.addsos([ONE, ONE], [1], [0])
    with pytest.raises(ValueError, match="var has length 1, not 2"):
        model.addsos([ONE], [2], [0])
    with pytest.raises(ValueError, match="coef has length 3, not 2"):
        model.addsos([ONE], [2], [0, 1], [1.0, 2.0, 3.0])
    with pytest.raises(ValueError, match="cnt: -1 is negative at entry 1"):
        model.addsos([ONE, ONE], [2, -1], [0])
    assert model.getnsos() == 0


def test_numbers(model):
    with pytest.raises(TypeError, match="an entry of knd must be an int"):
        model.addsos([1.0], [0], [])
    with pytest.raises(TypeError, match="cnt must be a sequence of int"):
        model.addsos([ONE], None, [0])
    with pytest.raises(TypeError, match="an entry of cnt must be an int"):
        model.addsos([ONE], [1.0], [0])
    with pytest.raises(TypeError, match="an entry of var must be an"):
        model.addsos([ONE], [1], [0.0])
    with pytest.raises(TypeError, match="an entry of coef must be a number"):
        model.addsos([ONE], [1], [0], ["1"])
    with pytest.raises(OverflowError, match="knd: 2147483648 does not"):
        model.addsos([2**31], [0], [])
    with pytest.raises(OverflowError, match="sos: 4294967296 does not fit"):
        model.getsos(range(2**32, 2**32 + 1))
    assert model.getnsos() == 0


def test_copy(model):
    s = model.addsos(*SAMPLE)
    copy = model.copy()
    assert indexed(copy.getsos()) == SAMPLE
    copy.addsos([ONE], [1], [3])
    assert model.getnsos() == 3
    assert indexed(copy.getsos(s.on(copy))) == SAMPLE
    assert copy.getnsos() == 4
    copy.close()


def test_locked(env, model):
    seen = []

    def change(event):
        try:
            model.addsos([ONE], [0], [])
        except biyos.Error as error:
            seen.append(str(error))
        seen.append(model.getnsos())
        seen.append(indexed(model.getsos([0])))

    model.addsos(*SAMPLE)
    env.addhook(change, EventKind.MESSAGE, EventLevel.ERROR)
    with pytest.raises(biyos.Error):
        env.setopt("seed", -1)
    first = ([TWO], [3], [2, 0, 3], [3.0, -1.0, 2.5])
    assert seen == [LOCKED, 3, first]
    assert model.getnsos() == 3


def test_closed(env):
    model = biyos.Model(env)
    model.close()
    calls = (
        partial(model.addsos, [], [], []),
        model.getnsos,
        model.getnsosvar,
        model.getsos,
    )
    for call in calls:
        with pytest.raises(ValueError, match="the model is closed"):
            call()
