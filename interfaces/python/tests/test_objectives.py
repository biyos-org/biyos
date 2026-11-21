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
    with biyos.Env() as env:
        yield env


@pytest.fixture
def model(env):
    with biyos.Model(env) as model:
        model.addfuns(2)
        yield model


def add_sample(model):
    return model.addobjs(*SAMPLE)


def read_lengths(handle, first, count, *arrays):
    return [(array, count) for array in arrays]


def recorded(seen, function, *arguments):
    seen.append(arguments)
    return function(*arguments)


def test_add(model):
    added = add_sample(model)
    assert model.getnobjs() == 3
    found = model.getobjs()
    assert found == SAMPLE
    assert all(type(sense) is Sense for sense in found[1])
    assert type(added) is biyos.Objectives
    assert [index(objective) for objective in added] == [0, 1, 2]


def test_block(model):
    model.addobjs([0, 0])
    o = model.addobjs([1, 1, 1])
    assert len(o) == 3
    assert all(type(objective) is biyos.Objective for objective in o)
    assert type(o[1:]) is biyos.Objectives
    assert [index(objective) for objective in o[1:]] == [3, 4]
    assert len(model.addobjs([])) == 0


def test_range(model):
    add_sample(model)
    tail = tuple(values[1:] for values in SAMPLE)
    assert model.getobjs(1, 2) == tail
    assert model.getobjs(1) == tail
    assert model.getobjs(3) == NONE


def test_defaults(model):
    model.addobjs([0, 1])
    model.addobjs([1], prios=[-7])
    model.addobjs([0], senses=[MAX], weights=[2.0])
    model.addobjs([1], abstol=[0.5], reltol=[INFINITY])
    model.addobjs([0])
    functions, senses, priorities, *values = model.getobjs()
    assert functions == [0, 1, 1, 0, 1, 0]
    assert senses == [MIN, MIN, MIN, MAX, MIN, MIN]
    assert priorities == [0, 1, -7, 3, 4, 5]
    assert values[0] == [1.0, 1.0, 1.0, 2.0, 1.0, 1.0]
    assert values[1] == [0.0, 0.0, 0.0, 0.0, 0.5, 0.0]
    assert values[2] == [0.0, 0.0, 0.0, 0.0, INFINITY, 0.0]


def test_shared(model):
    model.addcons([1], [0.0], [1.0])
    model.addobjs([1, 1], [MIN, MAX])
    model.addobjs([1], weights=[2.0])
    functions, senses, priorities, weights = model.getobjs()[:4]
    assert (functions, senses) == ([1, 1, 1], [MIN, MAX, MIN])
    assert (priorities, weights) == ([0, 1, 2], [1.0, 1.0, 2.0])
    assert model.getcons() == ([1], [0.0], [1.0])
    assert model.getnfuns() == 2


def test_functions(model):
    f = model.addfuns(3)
    o = model.addobjs(f, weights=[2.0] * 3)
    model.addobjs(f[1:])
    model.addobjs([f[0], 1])
    assert model.getobjs(o[0])[0] == [2, 3, 4, 3, 4, 2, 1]


def test_views(env, model):
    x = model.addvars(2)
    o = add_sample(model)
    other = biyos.Model(env)
    g = other.addfuns(2)
    p = other.addobjs(g)
    entry = "an entry of funs is a function of another model"
    with pytest.raises(ValueError, match=entry):
        model.addobjs([0, g[1]])
    with pytest.raises(ValueError, match="funs is a block of another"):
        model.addobjs(g)
    with pytest.raises(ValueError, match="is a variable, not a function"):
        model.addobjs([x[0]])
    with pytest.raises(ValueError, match="of variables, not functions"):
        model.addobjs(x)
    with pytest.raises(ValueError, match="is an objective of another model"):
        model.setobjs([p[0]], [MAX])
    with pytest.raises(ValueError, match="is a block of another model"):
        model.setobjs(p, [MAX, MAX])
    with pytest.raises(ValueError, match="beg is an objective of another"):
        model.getobjs(p[0])
    c = model.addcons([0])
    with pytest.raises(ValueError, match="is a constraint, not an objective"):
        model.setobjs([c[0]], [MAX])
    with pytest.raises(ValueError, match="of constraints, not objectives"):
        model.setobjs(c, [MAX])
    with pytest.raises(ValueError, match="is an objective, not a function"):
        model.addcons([o[0]])
    model.setobjs([o[2]], [MAX], [5])
    model.setobjs(o[:2], weights=[7.0, 8.0])
    found = model.getobjs(o[1])
    assert found[1:4] == ([MIN, MAX], [-1, 5], [8.0, 3.0])
    assert model.getnobjs() == 3


def test_gone(model):
    o = add_sample(model)
    view = o[0]
    model._version += 1
    with pytest.raises(ValueError, match="the objective predates a deletion"):
        index(view)
    with pytest.raises(ValueError, match="the objective predates a deletion"):
        model.setobjs([view], [MIN])
    with pytest.raises(ValueError, match="the block predates a deletion"):
        model.setobjs(o, [MIN] * 3)
    assert model.getobjs() == SAMPLE


def test_values(model, monkeypatch):
    weights = array.array("d", [1.0, 2.0])
    absolute = array.array("d", [3.0, 4.0])
    relative = array.array("d", [5.0, 6.0])
    seen = []
    spy = partial(recorded, seen, lib.BIYOS_addobjs)
    monkeypatch.setattr(lib, "BIYOS_addobjs", spy)
    model.addobjs([0, 1], None, None, weights, absolute, relative)
    assert len(seen) == 1
    assert ctypes.addressof(seen[0][5]) == weights.buffer_info()[0]
    assert ctypes.addressof(seen[0][6]) == absolute.buffer_info()[0]
    assert ctypes.addressof(seen[0][7]) == relative.buffer_info()[0]
    assert model.getobjs()[3:] == ([1.0, 2.0], [3.0, 4.0], [5.0, 6.0])


def test_growing(model):
    grow = partial(add_sample, model)
    assert model.getobjs(0, Growing(3, grow)) == SAMPLE
    assert model.getobjs(3, Growing(3, grow)) == SAMPLE
    assert model.getnobjs() == 6


def test_stale(model, monkeypatch):
    add_sample(model)
    monkeypatch.setattr(model, "getnobjs", lambda: 0)
    spy = Room(lib.BIYOS_getobjs, read_lengths)
    monkeypatch.setattr(lib, "BIYOS_getobjs", spy)
    assert model.getobjs(0, 3) == SAMPLE
    assert model.getobjs(1) == NONE


def test_outside(model, monkeypatch):
    limit(monkeypatch, _model, 1)
    message = refusal(model.getobjs, 0, 2**31 - 1)
    assert message == "invalid argument: objective 0: unknown"
    message = refusal(model.getobjs, 1, 2**31 - 2)
    assert message == "invalid argument: objective 1: unknown"


def test_sequences(model):
    functions = array.array("i", [1, 0])
    senses = array.array("i", [2, 1])
    model.addobjs(functions, senses, range(2), [2, 3])
    read_only = memoryview(array.array("d", [4.0]).tobytes()).cast("d")
    model.addobjs((Number(1),), (Number(2),), (Number(-3),), read_only)
    found = model.getobjs()
    assert found[:4] == ([1, 0, 1], [MAX, MIN, MAX], [0, 1, -3], [2, 3, 4])


def test_set(model):
    add_sample(model)
    model.setobjs([2, 0], senses=[MAX, MIN])
    model.setobjs([2], prios=[-5])
    model.setobjs([0, 2], weights=[4.0, 8.0])
    model.setobjs([1], abstol=[INFINITY], reltol=[0.5])
    model.setobjs([1, 2], None, None, None, None, None)
    found = model.getobjs()
    assert found[0] == FUNCTIONS
    assert found[1:3] == ([MIN, MIN, MAX], [2, -1, -5])
    assert found[3] == [4.0, 1.0, 8.0]
    assert found[4:] == ([0.0, INFINITY, INFINITY], [0.25, 0.5, 0.0])
    model.setobjs([0, 0], [MAX, MIN], [5, 6])
    assert model.getobjs(0, 1)[1:3] == ([MIN], [6])


def test_refused(model):
    add_sample(model)
    message = refusal(model.addobjs, [0, 9])
    assert message == "invalid argument: objective 4: function 9 is unknown"
    message = refusal(model.addobjs, [0], [3])
    assert message == "invalid argument: objective 3: sense 3 is unknown"
    message = refusal(model.addobjs, [0], None, None, [0.0])
    assert message == "invalid argument: objective 3: weight 0 is not positive"
    message = refusal(model.addobjs, [0], None, None, [NAN])
    assert message == "invalid argument: objective 3: weight is NaN"
    message = refusal(model.addobjs, [0], None, None, None, [-1.0])
    assert message == (
        "invalid argument: objective 3: absolute tolerance -1 is negative"
    )
    tolerances = partial(model.addobjs, [0], None, None, None)
    message = refusal(tolerances, [SIGNED_NAN])
    assert message == (
        "invalid argument: objective 3: absolute tolerance is NaN"
    )
    message = refusal(tolerances, None, [-INFINITY])
    assert message == (
        "invalid argument: objective 3: relative tolerance is minus infinity"
    )
    message = refusal(model.setobjs, [0], None, None, None, None, [NAN])
    assert message == (
        "invalid argument: objective 0: relative tolerance is NaN"
    )
    message = refusal(model.setobjs, [3], [MIN])
    assert message == "invalid argument: objective 3: unknown"
    message = refusal(model.getobjs, 4)
    assert message == "invalid argument: objective 4: unknown"
    message = refusal(model.getobjs, 0, -1)
    assert message == "invalid argument: argument 'n': -1 is negative"
    assert model.getobjs() == SAMPLE


def test_lengths(model):
    with pytest.raises(ValueError, match="senses has length 1, not 2"):
        model.addobjs([0, 1], [MIN])
    with pytest.raises(ValueError, match="prios has length 3, not 2"):
        model.addobjs([0, 1], None, [0, 1, 2])
    with pytest.raises(ValueError, match="reltol has length 1, not 2"):
        model.setobjs([0, 1], reltol=[1.0])
    assert model.getnobjs() == 0


def test_numbers(model):
    with pytest.raises(TypeError, match="an entry of funs must be an"):
        model.addobjs([0.0])
    with pytest.raises(TypeError, match="an entry of senses must be an int"):
        model.addobjs([0], [1.0])
    with pytest.raises(OverflowError, match="prios: 2147483648 does not"):
        model.addobjs([0], None, [2**31])
    with pytest.raises(TypeError, match="an entry of weights must be a"):
        model.addobjs([0], None, None, ["1"])
    with pytest.raises(TypeError, match="an entry of abstol must be a"):
        model.addobjs([0], None, None, None, [True])
    with pytest.raises(OverflowError, match="beg: 4294967296 does not fit"):
        model.getobjs(2**32)
    assert model.getnobjs() == 0


def test_locked(env, model):
    seen = []

    def change(event):
        for call in (
            partial(model.addobjs, [0]),
            partial(model.setobjs, [0], [MAX]),
        ):
            try:
                call()
            except biyos.Error as error:
                seen.append(str(error))
        seen.append(model.getobjs())

    add_sample(model)
    env.addhook(change, EventKind.MESSAGE, Level.ERROR)
    with pytest.raises(biyos.Error):
        env.setopt("seed", -1)
    assert seen == [LOCKED, LOCKED, SAMPLE]
    assert model.getnobjs() == 3


def test_closed(env):
    model = biyos.Model(env)
    model.close()
    calls = (
        partial(model.addobjs, [0]),
        partial(model.setobjs, []),
        model.getnobjs,
        model.getobjs,
    )
    for call in calls:
        with pytest.raises(ValueError, match="the model is closed"):
            call()


def test_on(env, model):
    o = add_sample(model)
    copy = model.copy()
    p = o[1:].on(copy)
    assert type(p) is biyos.Objectives
    copy.setobjs(p, [MAX, MAX])
    assert copy.getobjs()[1] == [MAX, MAX, MAX]
    assert model.getobjs() == SAMPLE
    smaller = biyos.Model(env)
    smaller.addfuns(1)
    smaller.addobjs([0, 0])
    few = "model holds too few objectives: 2, the block needs 3"
    with pytest.raises(ValueError, match=few):
        o.on(smaller)
    assert len(o[:2].on(smaller)) == 2
    copy.close()
