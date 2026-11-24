import array
import ctypes
from functools import partial
from operator import index

import pytest

import biyos
from biyos import EventKind, EventLevel
from biyos._capi import lib
from number import Number
from refusal import LOCKED, refusal
from sized import Room, limit
from tolerance import CHECKS

INFINITY = float("inf")
NAN = float("nan")

FUNCTIONS = [1, 0, 1]
LOWER = [-1.0, -INFINITY, 2.0]
UPPER = [1.0, INFINITY, 2.0]
SAMPLE = (FUNCTIONS, LOWER, UPPER)


@pytest.fixture
def env():
    with biyos.Env() as env:
        yield env


@pytest.fixture
def model(env):
    with biyos.Model(env) as model:
        model.addfun(2)
        yield model


def add_sample(model):
    return model.addcon(FUNCTIONS, LOWER, UPPER)


def read_lengths(handle, first, count, *arrays):
    return [(array, count) for array in arrays]


def recorded(seen, function, *arguments):
    seen.append(arguments)
    return function(*arguments)


def test_add(model):
    added = add_sample(model)
    assert model.getncon() == 3
    assert model.getcon() == SAMPLE
    assert type(added) is biyos.Constraints
    assert [index(constraint) for constraint in added] == [0, 1, 2]


def test_block(model):
    model.addcon([0, 0])
    c = model.addcon([1, 1, 1])
    assert len(c) == 3
    assert all(type(constraint) is biyos.Constraint for constraint in c)
    assert type(c[1:]) is biyos.Constraints
    assert [index(constraint) for constraint in c[1:]] == [3, 4]
    assert len(model.addcon([])) == 0


def test_range(model):
    add_sample(model)
    assert model.getcon(range(1, 3)) == ([0, 1], LOWER[1:], UPPER[1:])
    assert model.getcon([2, 1]) == ([1, 0], LOWER[:0:-1], UPPER[:0:-1])
    assert model.getcon(range(3, 3)) == ([], [], [])


def test_defaults(model):
    model.addcon([0, 1])
    model.addcon([1], lb=[0.5])
    model.addcon([0], ub=[0.5])
    low, high = -INFINITY, INFINITY
    functions, lower, upper = model.getcon()
    assert functions == [0, 1, 1, 0]
    assert lower == [low, low, 0.5, low]
    assert upper == [high, high, high, 0.5]


def test_shared(model):
    model.addcon([1, 1], [0.0, -INFINITY], [1.0, 3.0])
    found = model.getcon()
    assert found == ([1, 1], [0.0, -INFINITY], [1.0, 3.0])
    assert model.getnfun() == 2


def test_functions(model):
    f = model.addfun(3)
    model.addcon(f, [0.0] * 3)
    model.addcon(f[1:])
    model.addcon([f[0], 1])
    assert model.getcon()[0] == [2, 3, 4, 3, 4, 2, 1]


def test_views(env, model):
    x = model.addvar(2)
    c = add_sample(model)
    other = biyos.Model(env)
    g = other.addfun(2)
    d = other.addcon(g)
    entry = "an entry of funidx is a function of another model"
    with pytest.raises(ValueError, match=entry):
        model.addcon([0, g[1]])
    with pytest.raises(ValueError, match="funidx is a block of another"):
        model.addcon(g)
    with pytest.raises(TypeError, match="is a variable, not a function"):
        model.addcon([x[0]])
    with pytest.raises(TypeError, match="of variables, not functions"):
        model.addcon(x)
    with pytest.raises(ValueError, match="is a constraint of another model"):
        model.setconbnd([d[0]], [0.0])
    with pytest.raises(ValueError, match="is a block of another model"):
        model.setconbnd(d, [0.0, 0.0])
    with pytest.raises(ValueError, match="idx is a block of another model"):
        model.getcon(d)
    f = model.addfun(1)
    with pytest.raises(TypeError, match="is a function, not a constraint"):
        model.setconbnd([f[0]], [0.0])
    with pytest.raises(TypeError, match="of functions, not constraints"):
        model.setconbnd(f, [0.0])
    model.setconbnd([c[2]], [5.0], [6.0])
    model.setconbnd(c[:2], ub=[7.0, 8.0])
    found = model.getcon(c[1:])
    assert found == ([0, 1], [-INFINITY, 5.0], [8.0, 6.0])
    assert model.getncon() == 3


def test_gone(model):
    c = add_sample(model)
    view = c[0]
    model._gen += 1
    with pytest.raises(ValueError, match="the constraint predates a deletion"):
        index(view)
    with pytest.raises(ValueError, match="the constraint predates a deletion"):
        model.setconbnd([view], [0.0])
    with pytest.raises(ValueError, match="the block predates a deletion"):
        model.setconbnd(c, [0.0] * 3)
    assert model.getcon() == SAMPLE


def test_values(model, monkeypatch):
    lower = array.array("d", [0.0, 1.0])
    upper = array.array("d", [2.0, 3.0])
    seen = []
    spy = partial(recorded, seen, lib.BIYOS_addcon)
    monkeypatch.setattr(lib, "BIYOS_addcon", spy)
    model.addcon([0, 1], lower, upper)
    assert len(seen) == 1
    assert ctypes.addressof(seen[0][3]) == lower.buffer_info()[0]
    assert ctypes.addressof(seen[0][4]) == upper.buffer_info()[0]
    assert model.getcon() == ([0, 1], [0.0, 1.0], [2.0, 3.0])


def test_stale(model, monkeypatch):
    add_sample(model)
    monkeypatch.setattr(model, "getncon", lambda: 0)
    spy = Room(lib.BIYOS_getconrg, read_lengths)
    monkeypatch.setattr(lib, "BIYOS_getconrg", spy)
    assert model.getcon(range(3)) == SAMPLE
    assert model.getcon(range(1, 1)) == ([], [], [])


def test_outside(model, monkeypatch):
    limit(monkeypatch, biyos._model.model, 1)
    message = refusal(model.getcon, range(2**31 - 1))
    assert message == "invalid argument: constraint 0: unknown"
    message = refusal(model.getcon, range(1, 2**31 - 1))
    assert message == "invalid argument: constraint 1: unknown"


def test_sequences(model):
    functions = array.array("i", [1, 0])
    model.addcon(functions, range(2), array.array("d", [2.0, 3.0]))
    read_only = memoryview(array.array("d", [4.0]).tobytes()).cast("d")
    model.addcon((Number(1),), read_only, [5])
    found = model.getcon()
    assert found == ([1, 0, 1], [0.0, 1.0, 4.0], [2.0, 3.0, 5.0])


def test_bounds(model):
    model.addcon([0, 1])
    model.setconbnd([1, 0], lb=[1.0, 2.0])
    model.setconbnd([1], ub=[3.0])
    assert model.getcon() == ([0, 1], [2.0, 1.0], [INFINITY, 3.0])
    model.setconbnd([0, 0], [5.0, 6.0], [7.0, 8.0])
    assert model.getcon([0]) == ([0], [6.0], [8.0])


def test_listed(env, model):
    c = add_sample(model)
    found = model.getconbnd([2, 0, 2, 1])
    assert found == ([2.0, -1.0, 2.0, -INFINITY], [2.0, 1.0, 2.0, INFINITY])
    assert model.getconbnd(c[1:]) == (LOWER[1:], UPPER[1:])
    assert model.getconbnd([]) == ([], [])
    idx = [1, 0, 1]
    model.setconbnd(idx, [-2.0, 0.0, -3.0], [2.0, 0.5, 3.0])
    assert model.getconbnd(idx) == ([-3.0, 0.0, -3.0], [3.0, 0.5, 3.0])
    assert model.getconbnd([c[2]]) == ([2.0], [2.0])
    message = refusal(model.getconbnd, [0, 3])
    assert message == "invalid argument: constraint 3: unknown"
    message = refusal(model.getconbnd, [-1])
    assert message == "invalid argument: constraint -1: unknown"
    other = biyos.Model(env)
    other.addfun(1)
    d = other.addcon([0])
    with pytest.raises(ValueError, match="is a constraint of another model"):
        model.getconbnd([d[0]])
    with pytest.raises(TypeError, match="of functions, not constraints"):
        model.getconbnd(model.addfun(1))
    other.close()


def test_refused(model):
    add_sample(model)
    message = refusal(model.addcon, [0, 9])
    assert message == "invalid argument: constraint 4: function 9 is unknown"
    message = refusal(model.addcon, [0], [NAN])
    assert message == "invalid argument: constraint 3: lower bound is NaN"
    message = refusal(model.addcon, [0], None, [-INFINITY])
    assert message == (
        "invalid argument: constraint 3: upper bound is minus infinity"
    )
    message = refusal(model.setconbnd, [3], [0.0], [1.0])
    assert message == "invalid argument: constraint 3: unknown"
    message = refusal(model.setconbnd, [0], [INFINITY])
    assert message == "invalid argument: constraint 0: lower bound is infinity"
    message = refusal(model.getcon, range(4, 5))
    assert message == "invalid argument: constraint 4: unknown"
    message = refusal(model.getcon, [-1])
    assert message == "invalid argument: constraint -1: unknown"
    assert model.getcon() == SAMPLE


def test_lengths(model):
    with pytest.raises(ValueError, match="lb has length 1, not 2"):
        model.addcon([0, 1], [0.0])
    with pytest.raises(ValueError, match="ub has length 1, not 2"):
        model.setconbnd([0, 1], ub=[1.0])
    assert model.getncon() == 0


def test_numbers(model):
    with pytest.raises(TypeError, match="an entry of funidx must be an"):
        model.addcon([0.0])
    with pytest.raises(TypeError, match="an entry of lb must be a number"):
        model.addcon([0], ["1"])
    with pytest.raises(OverflowError, match="idx: 4294967296 does not fit"):
        model.getcon(range(2**32, 2**32 + 1))
    assert model.getncon() == 0


def test_locked(env, model):
    seen = []

    def change(event):
        for call in (
            partial(model.addcon, [0]),
            partial(model.setconbnd, [0], [0.0]),
        ):
            try:
                call()
            except biyos.Error as error:
                seen.append(str(error))
        seen.append(model.getcon())
        seen.append(model.getconbnd([1]))

    add_sample(model)
    env.addhook(change, EventKind.MESSAGE, EventLevel.ERROR)
    with pytest.raises(biyos.Error):
        env.setopt("seed", -1)
    assert seen == [LOCKED, LOCKED, SAMPLE, ([-INFINITY], [INFINITY])]
    assert model.getncon() == 3


def test_closed(env):
    model = biyos.Model(env)
    model.close()
    calls = (
        partial(model.addcon, [0]),
        partial(model.setconbnd, []),
        partial(model.getconbnd, []),
        model.getncon,
        model.getcon,
    )
    for call in calls:
        with pytest.raises(ValueError, match="the model is closed"):
            call()


def test_on(env, model):
    c = add_sample(model)
    copy = model.copy()
    d = c[1:].on(copy)
    assert type(d) is biyos.Constraints
    copy.setconbnd(d, [0.0, 0.0])
    assert copy.getcon()[1] == [-1.0, 0.0, 0.0]
    assert model.getcon() == SAMPLE
    smaller = biyos.Model(env)
    smaller.addfun(1)
    smaller.addcon([0, 0])
    few = "mdl holds too few constraints: 2, the block needs 3"
    with pytest.raises(ValueError, match=few):
        c.on(smaller)
    assert len(c[:2].on(smaller)) == 2
    copy.close()


@pytest.mark.parametrize("check", CHECKS)
def test_tolerances(env, model, check):
    add_sample(model)
    check(env, model, "con", "constraint")
