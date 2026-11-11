import array
import ctypes
import math
from functools import partial
from operator import index

import pytest

import biyos
from biyos import EventKind, Level, Sense, _model
from biyos._capi import lib
from number import Growing, Number
from refusal import LOCKED, refusal
from sized import Room, limit

INFINITY = float("inf")
NAN = float("nan")
SIGNED_NAN = math.copysign(NAN, -1.0)
MAX = Sense.MAXIMIZE
MIN = Sense.MINIMIZE

FUNCTIONS = [1, 0, 1]
SENSES = [MAX, MIN, MIN]
PRIORITIES = [2, -1, 2]
WEIGHTS = [0.5, 1.0, 3.0]
ABSOLUTE = [0.0, 1.5, INFINITY]
RELATIVE = [0.25, 0.0, 0.0]
SAMPLE = (FUNCTIONS, SENSES, PRIORITIES, WEIGHTS, ABSOLUTE, RELATIVE)
NONE = ([], [], [], [], [], [])


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
    return model.add_objectives(*SAMPLE)


def read_lengths(handle, first, count, *arrays):
    return [(array, count) for array in arrays]


def recorded(seen, function, *arguments):
    seen.append(arguments)
    return function(*arguments)


def test_add(model):
    added = add_sample(model)
    assert model.objective_count() == 3
    found = model.get_objectives()
    assert found == SAMPLE
    assert all(type(sense) is Sense for sense in found[1])
    assert type(added) is biyos.Objectives
    assert [index(objective) for objective in added] == [0, 1, 2]


def test_block(model):
    model.add_objectives([0, 0])
    o = model.add_objectives([1, 1, 1])
    assert len(o) == 3
    assert all(type(objective) is biyos.Objective for objective in o)
    assert type(o[1:]) is biyos.Objectives
    assert [index(objective) for objective in o[1:]] == [3, 4]
    assert len(model.add_objectives([])) == 0


def test_range(model):
    add_sample(model)
    tail = tuple(values[1:] for values in SAMPLE)
    assert model.get_objectives(1, 2) == tail
    assert model.get_objectives(1) == tail
    assert model.get_objectives(3) == NONE


def test_defaults(model):
    model.add_objectives([0, 1])
    model.add_objectives([1], priorities=[-7])
    model.add_objectives([0], senses=[MAX], weights=[2.0])
    model.add_objectives([1], absolute=[0.5], relative=[INFINITY])
    model.add_objectives([0])
    functions, senses, priorities, *values = model.get_objectives()
    assert functions == [0, 1, 1, 0, 1, 0]
    assert senses == [MIN, MIN, MIN, MAX, MIN, MIN]
    assert priorities == [0, 1, -7, 3, 4, 5]
    assert values[0] == [1.0, 1.0, 1.0, 2.0, 1.0, 1.0]
    assert values[1] == [0.0, 0.0, 0.0, 0.0, 0.5, 0.0]
    assert values[2] == [0.0, 0.0, 0.0, 0.0, INFINITY, 0.0]


def test_shared(model):
    model.add_constraints([1], [0.0], [1.0])
    model.add_objectives([1, 1], [MIN, MAX])
    model.add_objectives([1], weights=[2.0])
    functions, senses, priorities, weights = model.get_objectives()[:4]
    assert (functions, senses) == ([1, 1, 1], [MIN, MAX, MIN])
    assert (priorities, weights) == ([0, 1, 2], [1.0, 1.0, 2.0])
    assert model.get_constraints() == ([1], [0.0], [1.0])
    assert model.function_count() == 2


def test_functions(model):
    f = model.add_functions(3)
    o = model.add_objectives(f, weights=[2.0] * 3)
    model.add_objectives(f[1:])
    model.add_objectives([f[0], 1])
    assert model.get_objectives(o[0])[0] == [2, 3, 4, 3, 4, 2, 1]


def test_views(env, model):
    x = model.add_variables(2)
    o = add_sample(model)
    other = biyos.Model(env)
    g = other.add_functions(2)
    p = other.add_objectives(g)
    entry = "an entry of functions is a function of another model"
    with pytest.raises(ValueError, match=entry):
        model.add_objectives([0, g[1]])
    with pytest.raises(ValueError, match="functions is a block of another"):
        model.add_objectives(g)
    with pytest.raises(ValueError, match="is a variable, not a function"):
        model.add_objectives([x[0]])
    with pytest.raises(ValueError, match="of variables, not functions"):
        model.add_objectives(x)
    with pytest.raises(ValueError, match="is an objective of another model"):
        model.set_objectives([p[0]], [MAX])
    with pytest.raises(ValueError, match="is a block of another model"):
        model.set_objectives(p, [MAX, MAX])
    with pytest.raises(ValueError, match="first is an objective of another"):
        model.get_objectives(p[0])
    c = model.add_constraints([0])
    with pytest.raises(ValueError, match="is a constraint, not an objective"):
        model.set_objectives([c[0]], [MAX])
    with pytest.raises(ValueError, match="of constraints, not objectives"):
        model.set_objectives(c, [MAX])
    with pytest.raises(ValueError, match="is an objective, not a function"):
        model.add_constraints([o[0]])
    model.set_objectives([o[2]], [MAX], [5])
    model.set_objectives(o[:2], weights=[7.0, 8.0])
    found = model.get_objectives(o[1])
    assert found[1:4] == ([MIN, MAX], [-1, 5], [8.0, 3.0])
    assert model.objective_count() == 3


def test_gone(model):
    o = add_sample(model)
    view = o[0]
    model._version += 1
    with pytest.raises(ValueError, match="the objective predates a deletion"):
        index(view)
    with pytest.raises(ValueError, match="the objective predates a deletion"):
        model.set_objectives([view], [MIN])
    with pytest.raises(ValueError, match="the block predates a deletion"):
        model.set_objectives(o, [MIN] * 3)
    assert model.get_objectives() == SAMPLE


def test_values(model, monkeypatch):
    weights = array.array("d", [1.0, 2.0])
    absolute = array.array("d", [3.0, 4.0])
    relative = array.array("d", [5.0, 6.0])
    seen = []
    spy = partial(recorded, seen, lib.biyos_add_objectives)
    monkeypatch.setattr(lib, "biyos_add_objectives", spy)
    model.add_objectives([0, 1], None, None, weights, absolute, relative)
    assert len(seen) == 1
    assert ctypes.addressof(seen[0][5]) == weights.buffer_info()[0]
    assert ctypes.addressof(seen[0][6]) == absolute.buffer_info()[0]
    assert ctypes.addressof(seen[0][7]) == relative.buffer_info()[0]
    assert model.get_objectives()[3:] == ([1.0, 2.0], [3.0, 4.0], [5.0, 6.0])


def test_growing(model):
    grow = partial(add_sample, model)
    assert model.get_objectives(0, Growing(3, grow)) == SAMPLE
    assert model.get_objectives(3, Growing(3, grow)) == SAMPLE
    assert model.objective_count() == 6


def test_stale(model, monkeypatch):
    add_sample(model)
    monkeypatch.setattr(model, "objective_count", lambda: 0)
    spy = Room(lib.biyos_get_objectives, read_lengths)
    monkeypatch.setattr(lib, "biyos_get_objectives", spy)
    assert model.get_objectives(0, 3) == SAMPLE
    assert model.get_objectives(1) == NONE


def test_outside(model, monkeypatch):
    limit(monkeypatch, _model, 1)
    message = refusal(model.get_objectives, 0, 2**31 - 1)
    assert message == "invalid argument: objective 0: unknown"
    message = refusal(model.get_objectives, 1, 2**31 - 2)
    assert message == "invalid argument: objective 1: unknown"


def test_sequences(model):
    functions = array.array("i", [1, 0])
    senses = array.array("i", [2, 1])
    model.add_objectives(functions, senses, range(2), [2, 3])
    read_only = memoryview(array.array("d", [4.0]).tobytes()).cast("d")
    model.add_objectives((Number(1),), (Number(2),), (Number(-3),), read_only)
    found = model.get_objectives()
    assert found[:4] == ([1, 0, 1], [MAX, MIN, MAX], [0, 1, -3], [2, 3, 4])


def test_set(model):
    add_sample(model)
    model.set_objectives([2, 0], senses=[MAX, MIN])
    model.set_objectives([2], priorities=[-5])
    model.set_objectives([0, 2], weights=[4.0, 8.0])
    model.set_objectives([1], absolute=[INFINITY], relative=[0.5])
    model.set_objectives([1, 2], None, None, None, None, None)
    found = model.get_objectives()
    assert found[0] == FUNCTIONS
    assert found[1:3] == ([MIN, MIN, MAX], [2, -1, -5])
    assert found[3] == [4.0, 1.0, 8.0]
    assert found[4:] == ([0.0, INFINITY, INFINITY], [0.25, 0.5, 0.0])
    model.set_objectives([0, 0], [MAX, MIN], [5, 6])
    assert model.get_objectives(0, 1)[1:3] == ([MIN], [6])


def test_refused(model):
    add_sample(model)
    message = refusal(model.add_objectives, [0, 9])
    assert message == "invalid argument: objective 4: function 9 is unknown"
    message = refusal(model.add_objectives, [0], [3])
    assert message == "invalid argument: objective 3: sense 3 is unknown"
    message = refusal(model.add_objectives, [0], None, None, [0.0])
    assert message == "invalid argument: objective 3: weight 0 is not positive"
    message = refusal(model.add_objectives, [0], None, None, [NAN])
    assert message == "invalid argument: objective 3: weight is NaN"
    message = refusal(model.add_objectives, [0], None, None, None, [-1.0])
    assert message == (
        "invalid argument: objective 3: absolute tolerance -1 is negative"
    )
    tolerances = partial(model.add_objectives, [0], None, None, None)
    message = refusal(tolerances, [SIGNED_NAN])
    assert message == (
        "invalid argument: objective 3: absolute tolerance is NaN"
    )
    message = refusal(tolerances, None, [-INFINITY])
    assert message == (
        "invalid argument: objective 3: relative tolerance is minus infinity"
    )
    message = refusal(model.set_objectives, [0], None, None, None, None, [NAN])
    assert message == (
        "invalid argument: objective 0: relative tolerance is NaN"
    )
    message = refusal(model.set_objectives, [3], [MIN])
    assert message == "invalid argument: objective 3: unknown"
    message = refusal(model.get_objectives, 4)
    assert message == "invalid argument: objective 4: unknown"
    message = refusal(model.get_objectives, 0, -1)
    assert message == "invalid argument: argument 'count': -1 is negative"
    assert model.get_objectives() == SAMPLE


def test_lengths(model):
    with pytest.raises(ValueError, match="senses has length 1, not 2"):
        model.add_objectives([0, 1], [MIN])
    with pytest.raises(ValueError, match="priorities has length 3, not 2"):
        model.add_objectives([0, 1], None, [0, 1, 2])
    with pytest.raises(ValueError, match="relative has length 1, not 2"):
        model.set_objectives([0, 1], relative=[1.0])
    assert model.objective_count() == 0


def test_numbers(model):
    with pytest.raises(TypeError, match="an entry of functions must be an"):
        model.add_objectives([0.0])
    with pytest.raises(TypeError, match="an entry of senses must be an int"):
        model.add_objectives([0], [1.0])
    with pytest.raises(OverflowError, match="priorities: 2147483648 does not"):
        model.add_objectives([0], None, [2**31])
    with pytest.raises(TypeError, match="an entry of weights must be a"):
        model.add_objectives([0], None, None, ["1"])
    with pytest.raises(TypeError, match="an entry of absolute must be a"):
        model.add_objectives([0], None, None, None, [True])
    with pytest.raises(OverflowError, match="first: 4294967296 does not fit"):
        model.get_objectives(2**32)
    assert model.objective_count() == 0


def test_locked(env, model):
    seen = []

    def change(event):
        for call in (
            partial(model.add_objectives, [0]),
            partial(model.set_objectives, [0], [MAX]),
        ):
            try:
                call()
            except biyos.Error as error:
                seen.append(str(error))
        seen.append(model.get_objectives())

    add_sample(model)
    env.subscribe(change, EventKind.MESSAGE, Level.ERROR)
    with pytest.raises(biyos.Error):
        env.set_option("seed", -1)
    assert seen == [LOCKED, LOCKED, SAMPLE]
    assert model.objective_count() == 3


def test_closed(env):
    model = biyos.Model(env)
    model.close()
    calls = (
        partial(model.add_objectives, [0]),
        partial(model.set_objectives, []),
        model.objective_count,
        model.get_objectives,
    )
    for call in calls:
        with pytest.raises(ValueError, match="the model is closed"):
            call()


def test_on(env, model):
    o = add_sample(model)
    copy = model.copy()
    p = o[1:].on(copy)
    assert type(p) is biyos.Objectives
    copy.set_objectives(p, [MAX, MAX])
    assert copy.get_objectives()[1] == [MAX, MAX, MAX]
    assert model.get_objectives() == SAMPLE
    smaller = biyos.Model(env)
    smaller.add_functions(1)
    smaller.add_objectives([0, 0])
    few = "model holds too few objectives: 2, the block needs 3"
    with pytest.raises(ValueError, match=few):
        o.on(smaller)
    assert len(o[:2].on(smaller)) == 2
    copy.close()
