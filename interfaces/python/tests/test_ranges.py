from functools import partial

import pytest

import biyos
from biyos import Sense, VariableKind
from biyos._capi import lib
from refusal import refusal

PAIRS = [
    ("var", "varbnd", ([-1.0, -2.0], [1.0, 2.0])),
    ("var", "varknd", ([VariableKind.INTEGER, VariableKind.SEMIINTEGER],)),
    ("con", "conbnd", ([-3.0, -4.0], [3.0, 4.0])),
    ("obj", "objsense", ([Sense.MAXIMIZE, Sense.MINIMIZE],)),
    ("obj", "objprio", ([3, 4],)),
    ("obj", "objcoef", ([2.0, 3.0],)),
    ("obj", "objtol", ([0.1, 0.2], [0.3, 0.4])),
    ("var", "varname", (["a", "b"],)),
    ("fun", "funname", (["c", "d"],)),
    ("con", "conname", (["e", "f"],)),
    ("obj", "objname", (["g", "h"],)),
]

SPREAD = {
    "var": ("setvarbnd", [-1.0, -2.0, -3.0, -4.0], [1.0, 2.0, 3.0, 4.0]),
    "con": ("setconbnd", [-5.0, -6.0, -7.0, -8.0], [5.0, 6.0, 7.0, 8.0]),
    "obj": ("setobjprio", [5, 6, 7, 8]),
}

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
def blocks(env):
    with biyos.Model(env) as model:
        found = {
            "var": model.addvar(4),
            "fun": model.addfuns(4),
            "con": model.addcon([0] * 4),
            "obj": model.addobj([0] * 4),
        }
        yield model, found


def traced(seen, name, function, *arguments):
    seen.append(name)
    return function(*arguments)


def trace(monkeypatch, seen, call):
    for form in ("sp", "rg"):
        name = f"BIYOS_{call}{form}"
        spy = partial(traced, seen, form, getattr(lib, name))
        monkeypatch.setattr(lib, name, spy)


def selection(block, kind):
    if kind == "block":
        return block[1:3]
    if kind == "range":
        return range(1, 3)
    if kind == "list":
        return [1, 2]
    return range(1, 4, 2)


def read(model, call, idx):
    found = getattr(model, f"get{call}")(idx)
    return found if isinstance(found, tuple) else (found,)


def records(kind):
    return [1, 3] if kind == "stepped" else [1, 2]


@pytest.mark.parametrize("family, call, values", PAIRS)
@pytest.mark.parametrize("kind", FORMS)
def test_forms(blocks, monkeypatch, family, call, values, kind):
    model, found = blocks
    idx = selection(found[family], kind)
    seen = []
    trace(monkeypatch, seen, f"set{call}")
    trace(monkeypatch, seen, f"get{call}")
    getattr(model, f"set{call}")(idx, *values)
    assert read(model, call, idx) == values
    assert seen == [FORMS[kind]] * 2
    assert read(model, call, records(kind)) == values


@pytest.mark.parametrize("family", ["var", "fun", "con", "obj"])
@pytest.mark.parametrize("kind", FORMS)
def test_delete(blocks, monkeypatch, family, kind):
    model, found = blocks
    getattr(model, f"set{family}name")(range(4), ["w", "x", "y", "z"])
    seen = []
    trace(monkeypatch, seen, f"del{family}name")
    getattr(model, f"del{family}name")(selection(found[family], kind))
    left = getattr(model, f"get{family}name")(range(4))
    assert left.count(None) == 2
    assert [left[k] for k in records(kind)] == [None, None]
    assert seen == [FORMS[kind]]


@pytest.mark.parametrize("family", SPREAD)
@pytest.mark.parametrize("kind", FORMS)
def test_readers(blocks, monkeypatch, family, kind):
    model, found = blocks
    setter, *values = SPREAD[family]
    getattr(model, setter)(range(4), *values)
    every = getattr(model, f"get{family}")()
    seen = []
    trace(monkeypatch, seen, f"get{family}")
    got = getattr(model, f"get{family}")(selection(found[family], kind))
    picked = tuple([column[k] for k in records(kind)] for column in every)
    assert got == picked
    assert seen == [FORMS[kind]]


def test_refused(blocks):
    model, _ = blocks
    before = model.getvarbnd(range(4))
    message = refusal(model.setvarbnd, range(3, 5), [0.0] * 2, [1.0] * 2)
    assert message == "invalid argument: variable 4: unknown"
    message = refusal(model.getvarbnd, range(5, 5))
    assert message == "invalid argument: variable 5: unknown"
    message = refusal(model.setobjprio, range(-1, 1), [0, 0])
    assert message == "invalid argument: objective -1: unknown"
    assert model.getvarbnd(range(4)) == before
    assert model.getvarbnd(range(4, 4)) == ([], [])


def test_start(blocks):
    model, _ = blocks
    with pytest.raises(OverflowError, match="the start of idx: 1099511627776"):
        model.getvarknd(range(2**40, 2**40 + 1))
