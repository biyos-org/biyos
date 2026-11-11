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
    with biyos.Environment() as env:
        yield env


@pytest.fixture
def model(env):
    with biyos.Model(env) as model:
        model.add_functions(2)
        yield model


def add_sample(model):
    return model.add_constraints(FUNCTIONS, LOWER, UPPER)


def read_lengths(handle, first, count, *arrays):
    return [(array, count) for array in arrays]


def recorded(seen, function, *arguments):
    seen.append(arguments)
    return function(*arguments)


def test_add(model):
    added = add_sample(model)
    assert model.constraint_count() == 3
    assert model.get_constraints() == SAMPLE
    assert type(added) is biyos.Constraints
    assert [index(constraint) for constraint in added] == [0, 1, 2]


def test_block(model):
    model.add_constraints([0, 0])
    c = model.add_constraints([1, 1, 1])
    assert len(c) == 3
    assert all(type(constraint) is biyos.Constraint for constraint in c)
    assert type(c[1:]) is biyos.Constraints
    assert [index(constraint) for constraint in c[1:]] == [3, 4]
    assert len(model.add_constraints([])) == 0


def test_range(model):
    add_sample(model)
    assert model.get_constraints(1, 2) == ([0, 1], LOWER[1:], UPPER[1:])
    assert model.get_constraints(1) == ([0, 1], LOWER[1:], UPPER[1:])
    assert model.get_constraints(3) == ([], [], [])


def test_defaults(model):
    model.add_constraints([0, 1])
    model.add_constraints([1], lower=[0.5])
    model.add_constraints([0], upper=[0.5])
    low, high = -INFINITY, INFINITY
    functions, lower, upper = model.get_constraints()
    assert functions == [0, 1, 1, 0]
    assert lower == [low, low, 0.5, low]
    assert upper == [high, high, high, 0.5]


def test_shared(model):
    model.add_constraints([1, 1], [0.0, -INFINITY], [1.0, 3.0])
    found = model.get_constraints()
    assert found == ([1, 1], [0.0, -INFINITY], [1.0, 3.0])
    assert model.function_count() == 2


def test_functions(model):
    f = model.add_functions(3)
    c = model.add_constraints(f, [0.0] * 3)
    model.add_constraints(f[1:])
    model.add_constraints([f[0], 1])
    assert model.get_constraints(c[0])[0] == [2, 3, 4, 3, 4, 2, 1]


def test_views(env, model):
    x = model.add_variables(2)
    c = add_sample(model)
    other = biyos.Model(env)
    g = other.add_functions(2)
    d = other.add_constraints(g)
    entry = "an entry of functions is a function of another model"
    with pytest.raises(ValueError, match=entry):
        model.add_constraints([0, g[1]])
    with pytest.raises(ValueError, match="functions is a block of another"):
        model.add_constraints(g)
    with pytest.raises(ValueError, match="is a variable, not a function"):
        model.add_constraints([x[0]])
    with pytest.raises(ValueError, match="of variables, not functions"):
        model.add_constraints(x)
    with pytest.raises(ValueError, match="is a constraint of another model"):
        model.set_constraint_bounds([d[0]], [0.0])
    with pytest.raises(ValueError, match="is a block of another model"):
        model.set_constraint_bounds(d, [0.0, 0.0])
    with pytest.raises(ValueError, match="first is a constraint of another"):
        model.get_constraints(d[0])
    f = model.add_functions(1)
    with pytest.raises(ValueError, match="is a function, not a constraint"):
        model.set_constraint_bounds([f[0]], [0.0])
    with pytest.raises(ValueError, match="of functions, not constraints"):
        model.set_constraint_bounds(f, [0.0])
    model.set_constraint_bounds([c[2]], [5.0], [6.0])
    model.set_constraint_bounds(c[:2], upper=[7.0, 8.0])
    found = model.get_constraints(c[1])
    assert found == ([0, 1], [-INFINITY, 5.0], [8.0, 6.0])
    assert model.constraint_count() == 3


def test_gone(model):
    c = add_sample(model)
    view = c[0]
    model._version += 1
    with pytest.raises(ValueError, match="the constraint predates a deletion"):
        index(view)
    with pytest.raises(ValueError, match="the constraint predates a deletion"):
        model.set_constraint_bounds([view], [0.0])
    with pytest.raises(ValueError, match="the block predates a deletion"):
        model.set_constraint_bounds(c, [0.0] * 3)
    assert model.get_constraints() == SAMPLE


def test_values(model, monkeypatch):
    lower = array.array("d", [0.0, 1.0])
    upper = array.array("d", [2.0, 3.0])
    seen = []
    spy = partial(recorded, seen, lib.biyos_add_constraints)
    monkeypatch.setattr(lib, "biyos_add_constraints", spy)
    model.add_constraints([0, 1], lower, upper)
    assert len(seen) == 1
    assert ctypes.addressof(seen[0][3]) == lower.buffer_info()[0]
    assert ctypes.addressof(seen[0][4]) == upper.buffer_info()[0]
    assert model.get_constraints() == ([0, 1], [0.0, 1.0], [2.0, 3.0])


def test_growing(model):
    grow = partial(add_sample, model)
    assert model.get_constraints(0, Growing(3, grow)) == SAMPLE
    assert model.get_constraints(3, Growing(3, grow)) == SAMPLE
    assert model.constraint_count() == 6


def test_stale(model, monkeypatch):
    add_sample(model)
    monkeypatch.setattr(model, "constraint_count", lambda: 0)
    spy = Room(lib.biyos_get_constraints, read_lengths)
    monkeypatch.setattr(lib, "biyos_get_constraints", spy)
    assert model.get_constraints(0, 3) == SAMPLE
    assert model.get_constraints(1) == ([], [], [])


def test_outside(model, monkeypatch):
    limit(monkeypatch, _model, 1)
    message = refusal(model.get_constraints, 0, 2**31 - 1)
    assert message == "invalid argument: constraint 0: unknown"
    message = refusal(model.get_constraints, 1, 2**31 - 2)
    assert message == "invalid argument: constraint 1: unknown"


def test_sequences(model):
    functions = array.array("i", [1, 0])
    model.add_constraints(functions, range(2), array.array("d", [2.0, 3.0]))
    read_only = memoryview(array.array("d", [4.0]).tobytes()).cast("d")
    model.add_constraints((Number(1),), read_only, [5])
    found = model.get_constraints()
    assert found == ([1, 0, 1], [0.0, 1.0, 4.0], [2.0, 3.0, 5.0])


def test_bounds(model):
    model.add_constraints([0, 1])
    model.set_constraint_bounds([1, 0], lower=[1.0, 2.0])
    model.set_constraint_bounds([1], upper=[3.0])
    assert model.get_constraints() == ([0, 1], [2.0, 1.0], [INFINITY, 3.0])
    model.set_constraint_bounds([0, 0], [5.0, 6.0], [7.0, 8.0])
    assert model.get_constraints(0, 1) == ([0], [6.0], [8.0])


def test_refused(model):
    add_sample(model)
    message = refusal(model.add_constraints, [0, 9])
    assert message == "invalid argument: constraint 4: function 9 is unknown"
    message = refusal(model.add_constraints, [0], [NAN])
    assert message == "invalid argument: constraint 3: lower bound is NaN"
    message = refusal(model.add_constraints, [0], None, [-INFINITY])
    assert message == (
        "invalid argument: constraint 3: upper bound is minus infinity"
    )
    message = refusal(model.set_constraint_bounds, [3], [0.0], [1.0])
    assert message == "invalid argument: constraint 3: unknown"
    message = refusal(model.set_constraint_bounds, [0], [INFINITY])
    assert message == "invalid argument: constraint 0: lower bound is infinity"
    message = refusal(model.get_constraints, 4)
    assert message == "invalid argument: constraint 4: unknown"
    message = refusal(model.get_constraints, 0, -1)
    assert message == "invalid argument: argument 'count': -1 is negative"
    assert model.get_constraints() == SAMPLE


def test_lengths(model):
    with pytest.raises(ValueError, match="lower has length 1, not 2"):
        model.add_constraints([0, 1], [0.0])
    with pytest.raises(ValueError, match="upper has length 1, not 2"):
        model.set_constraint_bounds([0, 1], upper=[1.0])
    assert model.constraint_count() == 0


def test_numbers(model):
    with pytest.raises(TypeError, match="an entry of functions must be an"):
        model.add_constraints([0.0])
    with pytest.raises(TypeError, match="an entry of lower must be a number"):
        model.add_constraints([0], ["1"])
    with pytest.raises(OverflowError, match="first: 4294967296 does not fit"):
        model.get_constraints(2**32)
    assert model.constraint_count() == 0


def test_locked(env, model):
    seen = []

    def change(event):
        for call in (
            partial(model.add_constraints, [0]),
            partial(model.set_constraint_bounds, [0], [0.0]),
        ):
            try:
                call()
            except biyos.Error as error:
                seen.append(str(error))
        seen.append(model.get_constraints())

    add_sample(model)
    env.subscribe(change, EventKind.MESSAGE, Level.ERROR)
    with pytest.raises(biyos.Error):
        env.set_option("seed", -1)
    assert seen == [LOCKED, LOCKED, SAMPLE]
    assert model.constraint_count() == 3


def test_closed(env):
    model = biyos.Model(env)
    model.close()
    calls = (
        partial(model.add_constraints, [0]),
        partial(model.set_constraint_bounds, []),
        model.constraint_count,
        model.get_constraints,
    )
    for call in calls:
        with pytest.raises(ValueError, match="the model is closed"):
            call()
