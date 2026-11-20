import array
import ctypes
from functools import partial
from operator import index

import pytest

import biyos
from biyos import EventKind, Level, _model
from biyos._capi import lib
from number import Growing, Number
from refusal import LOCKED, refusal
from sized import Room, limit

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
        model.addfuns(2)
        yield model


def add_sample(model):
    return model.addcons(FUNCTIONS, LOWER, UPPER)


def read_lengths(handle, first, count, *arrays):
    return [(array, count) for array in arrays]


def recorded(seen, function, *arguments):
    seen.append(arguments)
    return function(*arguments)


def test_add(model):
    added = add_sample(model)
    assert model.getncons() == 3
    assert model.getcons() == SAMPLE
    assert type(added) is biyos.Constraints
    assert [index(constraint) for constraint in added] == [0, 1, 2]


def test_block(model):
    model.addcons([0, 0])
    c = model.addcons([1, 1, 1])
    assert len(c) == 3
    assert all(type(constraint) is biyos.Constraint for constraint in c)
    assert type(c[1:]) is biyos.Constraints
    assert [index(constraint) for constraint in c[1:]] == [3, 4]
    assert len(model.addcons([])) == 0


def test_range(model):
    add_sample(model)
    assert model.getcons(1, 2) == ([0, 1], LOWER[1:], UPPER[1:])
    assert model.getcons(1) == ([0, 1], LOWER[1:], UPPER[1:])
    assert model.getcons(3) == ([], [], [])


def test_defaults(model):
    model.addcons([0, 1])
    model.addcons([1], lower=[0.5])
    model.addcons([0], upper=[0.5])
    low, high = -INFINITY, INFINITY
    functions, lower, upper = model.getcons()
    assert functions == [0, 1, 1, 0]
    assert lower == [low, low, 0.5, low]
    assert upper == [high, high, high, 0.5]


def test_shared(model):
    model.addcons([1, 1], [0.0, -INFINITY], [1.0, 3.0])
    found = model.getcons()
    assert found == ([1, 1], [0.0, -INFINITY], [1.0, 3.0])
    assert model.getnfuns() == 2


def test_functions(model):
    f = model.addfuns(3)
    c = model.addcons(f, [0.0] * 3)
    model.addcons(f[1:])
    model.addcons([f[0], 1])
    assert model.getcons(c[0])[0] == [2, 3, 4, 3, 4, 2, 1]


def test_views(env, model):
    x = model.addvars(2)
    c = add_sample(model)
    other = biyos.Model(env)
    g = other.addfuns(2)
    d = other.addcons(g)
    entry = "an entry of functions is a function of another model"
    with pytest.raises(ValueError, match=entry):
        model.addcons([0, g[1]])
    with pytest.raises(ValueError, match="functions is a block of another"):
        model.addcons(g)
    with pytest.raises(ValueError, match="is a variable, not a function"):
        model.addcons([x[0]])
    with pytest.raises(ValueError, match="of variables, not functions"):
        model.addcons(x)
    with pytest.raises(ValueError, match="is a constraint of another model"):
        model.setconbounds([d[0]], [0.0])
    with pytest.raises(ValueError, match="is a block of another model"):
        model.setconbounds(d, [0.0, 0.0])
    with pytest.raises(ValueError, match="first is a constraint of another"):
        model.getcons(d[0])
    f = model.addfuns(1)
    with pytest.raises(ValueError, match="is a function, not a constraint"):
        model.setconbounds([f[0]], [0.0])
    with pytest.raises(ValueError, match="of functions, not constraints"):
        model.setconbounds(f, [0.0])
    model.setconbounds([c[2]], [5.0], [6.0])
    model.setconbounds(c[:2], upper=[7.0, 8.0])
    found = model.getcons(c[1])
    assert found == ([0, 1], [-INFINITY, 5.0], [8.0, 6.0])
    assert model.getncons() == 3


def test_gone(model):
    c = add_sample(model)
    view = c[0]
    model._version += 1
    with pytest.raises(ValueError, match="the constraint predates a deletion"):
        index(view)
    with pytest.raises(ValueError, match="the constraint predates a deletion"):
        model.setconbounds([view], [0.0])
    with pytest.raises(ValueError, match="the block predates a deletion"):
        model.setconbounds(c, [0.0] * 3)
    assert model.getcons() == SAMPLE


def test_values(model, monkeypatch):
    lower = array.array("d", [0.0, 1.0])
    upper = array.array("d", [2.0, 3.0])
    seen = []
    spy = partial(recorded, seen, lib.BIYOS_addcons)
    monkeypatch.setattr(lib, "BIYOS_addcons", spy)
    model.addcons([0, 1], lower, upper)
    assert len(seen) == 1
    assert ctypes.addressof(seen[0][3]) == lower.buffer_info()[0]
    assert ctypes.addressof(seen[0][4]) == upper.buffer_info()[0]
    assert model.getcons() == ([0, 1], [0.0, 1.0], [2.0, 3.0])


def test_growing(model):
    grow = partial(add_sample, model)
    assert model.getcons(0, Growing(3, grow)) == SAMPLE
    assert model.getcons(3, Growing(3, grow)) == SAMPLE
    assert model.getncons() == 6


def test_stale(model, monkeypatch):
    add_sample(model)
    monkeypatch.setattr(model, "getncons", lambda: 0)
    spy = Room(lib.BIYOS_getcons, read_lengths)
    monkeypatch.setattr(lib, "BIYOS_getcons", spy)
    assert model.getcons(0, 3) == SAMPLE
    assert model.getcons(1) == ([], [], [])


def test_outside(model, monkeypatch):
    limit(monkeypatch, _model, 1)
    message = refusal(model.getcons, 0, 2**31 - 1)
    assert message == "invalid argument: constraint 0: unknown"
    message = refusal(model.getcons, 1, 2**31 - 2)
    assert message == "invalid argument: constraint 1: unknown"


def test_sequences(model):
    functions = array.array("i", [1, 0])
    model.addcons(functions, range(2), array.array("d", [2.0, 3.0]))
    read_only = memoryview(array.array("d", [4.0]).tobytes()).cast("d")
    model.addcons((Number(1),), read_only, [5])
    found = model.getcons()
    assert found == ([1, 0, 1], [0.0, 1.0, 4.0], [2.0, 3.0, 5.0])


def test_bounds(model):
    model.addcons([0, 1])
    model.setconbounds([1, 0], lower=[1.0, 2.0])
    model.setconbounds([1], upper=[3.0])
    assert model.getcons() == ([0, 1], [2.0, 1.0], [INFINITY, 3.0])
    model.setconbounds([0, 0], [5.0, 6.0], [7.0, 8.0])
    assert model.getcons(0, 1) == ([0], [6.0], [8.0])


def test_refused(model):
    add_sample(model)
    message = refusal(model.addcons, [0, 9])
    assert message == "invalid argument: constraint 4: function 9 is unknown"
    message = refusal(model.addcons, [0], [NAN])
    assert message == "invalid argument: constraint 3: lower bound is NaN"
    message = refusal(model.addcons, [0], None, [-INFINITY])
    assert message == (
        "invalid argument: constraint 3: upper bound is minus infinity"
    )
    message = refusal(model.setconbounds, [3], [0.0], [1.0])
    assert message == "invalid argument: constraint 3: unknown"
    message = refusal(model.setconbounds, [0], [INFINITY])
    assert message == "invalid argument: constraint 0: lower bound is infinity"
    message = refusal(model.getcons, 4)
    assert message == "invalid argument: constraint 4: unknown"
    message = refusal(model.getcons, 0, -1)
    assert message == "invalid argument: argument 'n': -1 is negative"
    assert model.getcons() == SAMPLE


def test_lengths(model):
    with pytest.raises(ValueError, match="lower has length 1, not 2"):
        model.addcons([0, 1], [0.0])
    with pytest.raises(ValueError, match="upper has length 1, not 2"):
        model.setconbounds([0, 1], upper=[1.0])
    assert model.getncons() == 0


def test_numbers(model):
    with pytest.raises(TypeError, match="an entry of functions must be an"):
        model.addcons([0.0])
    with pytest.raises(TypeError, match="an entry of lower must be a number"):
        model.addcons([0], ["1"])
    with pytest.raises(OverflowError, match="first: 4294967296 does not fit"):
        model.getcons(2**32)
    assert model.getncons() == 0


def test_locked(env, model):
    seen = []

    def change(event):
        for call in (
            partial(model.addcons, [0]),
            partial(model.setconbounds, [0], [0.0]),
        ):
            try:
                call()
            except biyos.Error as error:
                seen.append(str(error))
        seen.append(model.getcons())

    add_sample(model)
    env.addhook(change, EventKind.MESSAGE, Level.ERROR)
    with pytest.raises(biyos.Error):
        env.setopt("seed", -1)
    assert seen == [LOCKED, LOCKED, SAMPLE]
    assert model.getncons() == 3


def test_closed(env):
    model = biyos.Model(env)
    model.close()
    calls = (
        partial(model.addcons, [0]),
        partial(model.setconbounds, []),
        model.getncons,
        model.getcons,
    )
    for call in calls:
        with pytest.raises(ValueError, match="the model is closed"):
            call()


def test_on(env, model):
    c = add_sample(model)
    copy = model.copy()
    d = c[1:].on(copy)
    assert type(d) is biyos.Constraints
    copy.setconbounds(d, [0.0, 0.0])
    assert copy.getcons()[1] == [-1.0, 0.0, 0.0]
    assert model.getcons() == SAMPLE
    smaller = biyos.Model(env)
    smaller.addfuns(1)
    smaller.addcons([0, 0])
    few = "model holds too few constraints: 2, the block needs 3"
    with pytest.raises(ValueError, match=few):
        c.on(smaller)
    assert len(c[:2].on(smaller)) == 2
    copy.close()
