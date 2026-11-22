import array
import ctypes
import math
from functools import partial
from operator import index

import pytest

import biyos
from biyos import EventKind, EventLevel, Sense, _model
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
SAMPLE = (FUNCTIONS, SENSES, PRIORITIES, WEIGHTS)
TOLERANCES = (ABSOLUTE, RELATIVE)
NONE = ([], [], [], [])


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
    o = model.addobj(*SAMPLE)
    model.setobjtol(o, *TOLERANCES)
    return o


def read_lengths(handle, first, count, *arrays):
    return [(array, count) for array in arrays]


def recorded(seen, function, *arguments):
    seen.append(arguments)
    return function(*arguments)


def test_add(model):
    added = add_sample(model)
    assert model.getnobj() == 3
    found = model.getobj()
    assert found == SAMPLE
    assert model.getobjtol(added) == TOLERANCES
    assert all(type(sense) is Sense for sense in found[1])
    assert type(added) is biyos.Objectives
    assert [index(objective) for objective in added] == [0, 1, 2]


def test_block(model):
    model.addobj([0, 0])
    o = model.addobj([1, 1, 1])
    assert len(o) == 3
    assert all(type(objective) is biyos.Objective for objective in o)
    assert type(o[1:]) is biyos.Objectives
    assert [index(objective) for objective in o[1:]] == [3, 4]
    assert len(model.addobj([])) == 0


def test_range(model):
    add_sample(model)
    tail = tuple(values[1:] for values in SAMPLE)
    assert model.getobj(1, 2) == tail
    assert model.getobj(1) == tail
    assert model.getobj(3) == NONE


def test_defaults(model):
    model.addobj([0, 1])
    model.addobj([1], prio=[-7])
    model.addobj([0], sense=[MAX], coef=[2.0])
    model.addobj([1])
    model.addobj([0])
    functions, senses, priorities, weights = model.getobj()
    assert functions == [0, 1, 1, 0, 1, 0]
    assert senses == [MIN, MIN, MIN, MAX, MIN, MIN]
    assert priorities == [0, 1, -7, 3, 4, 5]
    assert weights == [1.0, 1.0, 1.0, 2.0, 1.0, 1.0]
    assert model.getobjtol(range(6)) == ([0.0] * 6, [0.0] * 6)


def test_shared(model):
    model.addcon([1], [0.0], [1.0])
    model.addobj([1, 1], [MIN, MAX])
    model.addobj([1], coef=[2.0])
    functions, senses, priorities, weights = model.getobj()
    assert (functions, senses) == ([1, 1, 1], [MIN, MAX, MIN])
    assert (priorities, weights) == ([0, 1, 2], [1.0, 1.0, 2.0])
    assert model.getcon() == ([1], [0.0], [1.0])
    assert model.getnfun() == 2


def test_functions(model):
    f = model.addfuns(3)
    o = model.addobj(f, coef=[2.0] * 3)
    model.addobj(f[1:])
    model.addobj([f[0], 1])
    assert model.getobj(o[0])[0] == [2, 3, 4, 3, 4, 2, 1]


def test_views(env, model):
    x = model.addvar(2)
    o = add_sample(model)
    other = biyos.Model(env)
    g = other.addfuns(2)
    p = other.addobj(g)
    entry = "an entry of funidx is a function of another model"
    with pytest.raises(ValueError, match=entry):
        model.addobj([0, g[1]])
    with pytest.raises(ValueError, match="funidx is a block of another"):
        model.addobj(g)
    with pytest.raises(ValueError, match="is a variable, not a function"):
        model.addobj([x[0]])
    with pytest.raises(ValueError, match="of variables, not functions"):
        model.addobj(x)
    with pytest.raises(ValueError, match="is an objective of another model"):
        model.setobjsense([p[0]], [MAX])
    with pytest.raises(ValueError, match="is a block of another model"):
        model.setobjsense(p, [MAX, MAX])
    with pytest.raises(ValueError, match="pos is an objective of another"):
        model.getobj(p[0])
    c = model.addcon([0])
    with pytest.raises(ValueError, match="is a constraint, not an objective"):
        model.setobjsense([c[0]], [MAX])
    with pytest.raises(ValueError, match="of constraints, not objectives"):
        model.setobjsense(c, [MAX])
    with pytest.raises(ValueError, match="is an objective, not a function"):
        model.addcon([o[0]])
    model.setobjsense([o[2]], [MAX])
    model.setobjprio([o[2]], [5])
    model.setobjcoef(o[:2], [7.0, 8.0])
    found = model.getobj(o[1])
    assert found[1:4] == ([MIN, MAX], [-1, 5], [8.0, 3.0])
    assert model.getnobj() == 3


def test_gone(model):
    o = add_sample(model)
    view = o[0]
    model._gen += 1
    with pytest.raises(ValueError, match="the objective predates a deletion"):
        index(view)
    with pytest.raises(ValueError, match="the objective predates a deletion"):
        model.setobjsense([view], [MIN])
    with pytest.raises(ValueError, match="the block predates a deletion"):
        model.setobjsense(o, [MIN] * 3)
    assert model.getobj() == SAMPLE


def test_values(model, monkeypatch):
    weights = array.array("d", [1.0, 2.0])
    absolute = array.array("d", [3.0, 4.0])
    relative = array.array("d", [5.0, 6.0])
    seen = []
    for name in ("BIYOS_addobj", "BIYOS_setobjtolrg"):
        spy = partial(recorded, seen, getattr(lib, name))
        monkeypatch.setattr(lib, name, spy)
    o = model.addobj([0, 1], None, None, weights)
    model.setobjtol(o, absolute, relative)
    assert len(seen) == 2
    assert ctypes.addressof(seen[0][5]) == weights.buffer_info()[0]
    assert ctypes.addressof(seen[1][3]) == absolute.buffer_info()[0]
    assert ctypes.addressof(seen[1][4]) == relative.buffer_info()[0]
    assert model.getobj()[3] == [1.0, 2.0]
    assert model.getobjtol(o) == ([3.0, 4.0], [5.0, 6.0])


def test_growing(model):
    grow = partial(add_sample, model)
    assert model.getobj(0, Growing(3, grow)) == SAMPLE
    assert model.getobj(3, Growing(3, grow)) == SAMPLE
    assert model.getnobj() == 6


def test_stale(model, monkeypatch):
    add_sample(model)
    monkeypatch.setattr(model, "getnobj", lambda: 0)
    spy = Room(lib.BIYOS_getobjrg, read_lengths)
    monkeypatch.setattr(lib, "BIYOS_getobjrg", spy)
    assert model.getobj(0, 3) == SAMPLE
    assert model.getobj(1) == NONE


def test_outside(model, monkeypatch):
    limit(monkeypatch, _model, 1)
    message = refusal(model.getobj, 0, 2**31 - 1)
    assert message == "invalid argument: objective 0: unknown"
    message = refusal(model.getobj, 1, 2**31 - 2)
    assert message == "invalid argument: objective 1: unknown"


def test_sequences(model):
    functions = array.array("i", [1, 0])
    senses = array.array("i", [2, 1])
    model.addobj(functions, senses, range(2), [2, 3])
    read_only = memoryview(array.array("d", [4.0]).tobytes()).cast("d")
    model.addobj((Number(1),), (Number(2),), (Number(-3),), read_only)
    found = model.getobj()
    assert found == ([1, 0, 1], [MAX, MIN, MAX], [0, 1, -3], [2, 3, 4])


def test_set(model):
    add_sample(model)
    model.setobjsense([2, 0], [MAX, MIN])
    model.setobjprio([2], [-5])
    model.setobjcoef([0, 2], [4.0, 8.0])
    model.setobjtol([1], atol=[INFINITY], rtol=[0.5])
    model.setobjtol([1, 2], None, None)
    found = model.getobj()
    assert found[0] == FUNCTIONS
    assert found[1:3] == ([MIN, MIN, MAX], [2, -1, -5])
    assert found[3] == [4.0, 1.0, 8.0]
    tolerances = ([0.0, INFINITY, INFINITY], [0.25, 0.5, 0.0])
    assert model.getobjtol(range(3)) == tolerances
    model.setobjsense([0, 0], [MAX, MIN])
    model.setobjprio([0, 0], [5, 6])
    assert model.getobj(0, 1)[1:3] == ([MIN], [6])


def test_listed(model):
    o = add_sample(model)
    idx = [2, 0, 2, 1]
    sense = model.getobjsense(idx)
    assert sense == [MIN, MAX, MIN, MIN]
    assert all(type(value) is Sense for value in sense)
    assert model.getobjprio(idx) == [2, 2, 2, -1]
    assert model.getobjcoef(idx) == [3.0, 0.5, 3.0, 1.0]
    tolerances = ([INFINITY, 0.0, INFINITY, 1.5], [0.0, 0.25, 0.0, 0.0])
    assert model.getobjtol(idx) == tolerances
    assert model.getobjsense(o[1:]) == SENSES[1:]
    assert model.getobjprio([o[1], Number(0)]) == [-1, 2]
    assert model.getobjcoef([]) == []
    assert model.getobjtol([]) == ([], [])


def test_setters(model):
    add_sample(model)
    idx = [2, 0, 2]
    model.setobjsense(idx, [MIN, MAX, MAX])
    model.setobjprio(idx, [5, -3, 7])
    model.setobjcoef(idx, [2.0, 4.0, 8.0])
    model.setobjtol(idx, [0.5, INFINITY, 1.0], [0.125, 0.25, 0.0])
    assert model.getobjsense(idx) == [MAX, MAX, MAX]
    assert model.getobjprio(idx) == [7, -3, 7]
    assert model.getobjcoef(idx) == [8.0, 4.0, 8.0]
    assert model.getobjtol(idx) == ([1.0, INFINITY, 1.0], [0.0, 0.25, 0.0])
    model.setobjsense(idx, None)
    model.setobjprio(idx, None)
    model.setobjcoef(idx, None)
    model.setobjtol(idx)
    model.setobjtol([2], atol=[0.5])
    model.setobjtol([1], rtol=[INFINITY])
    found = model.getobj()
    assert found == (FUNCTIONS, [MAX, MIN, MAX], [-3, -1, 7], [4, 1, 8])
    tolerances = ([INFINITY, 1.5, 0.5], [0.25, INFINITY, 0.0])
    assert model.getobjtol(range(3)) == tolerances


def test_listed_refusals(env, model):
    o = add_sample(model)
    message = refusal(model.getobjsense, [0, 3])
    assert message == "invalid argument: objective 3: unknown"
    message = refusal(model.getobjprio, [-1])
    assert message == "invalid argument: objective -1: unknown"
    message = refusal(model.getobjcoef, [3])
    assert message == "invalid argument: objective 3: unknown"
    message = refusal(model.getobjtol, [1, 4])
    assert message == "invalid argument: objective 4: unknown"
    message = refusal(model.setobjsense, [1, 0], [MAX, 0])
    assert message == "invalid argument: objective 0: sense 0 is unknown"
    message = refusal(model.setobjprio, [3], [0])
    assert message == "invalid argument: objective 3: unknown"
    message = refusal(model.setobjcoef, [1, 0], [2.0, -1.0])
    assert message == (
        "invalid argument: objective 0: weight -1 is not positive"
    )
    message = refusal(model.setobjtol, [0], [-1.0])
    assert message == (
        "invalid argument: objective 0: absolute tolerance -1 is negative"
    )
    message = refusal(model.setobjtol, [0], None, [NAN])
    assert message == (
        "invalid argument: objective 0: relative tolerance is NaN"
    )
    assert model.getobj() == SAMPLE
    c = model.addcon([0])
    with pytest.raises(ValueError, match="is a constraint, not an objective"):
        model.getobjsense([c[0]])
    with pytest.raises(ValueError, match="of constraints, not objectives"):
        model.setobjtol(c, [0.0])
    other = biyos.Model(env)
    other.addfuns(1)
    p = other.addobj([0])
    with pytest.raises(ValueError, match="is an objective of another model"):
        model.setobjprio([p[0]], [1])
    with pytest.raises(ValueError, match="is a block of another model"):
        model.getobjtol(p)
    with pytest.raises(ValueError, match="sense has length 1, not 2"):
        model.setobjsense([0, 1], [MIN])
    with pytest.raises(ValueError, match="rtol has length 3, not 1"):
        model.setobjtol([0], None, [0.0] * 3)
    with pytest.raises(TypeError, match="an entry of prio must be an int"):
        model.setobjprio([0], [1.5])
    with pytest.raises(TypeError, match="an entry of coef must be a number"):
        model.setobjcoef([0], ["1"])
    model._gen += 1
    with pytest.raises(ValueError, match="the block predates a deletion"):
        model.getobjcoef(o)
    other.close()


def test_refused(model):
    add_sample(model)
    message = refusal(model.addobj, [0, 9])
    assert message == "invalid argument: objective 4: function 9 is unknown"
    message = refusal(model.addobj, [0], [3])
    assert message == "invalid argument: objective 3: sense 3 is unknown"
    message = refusal(model.addobj, [0], None, None, [0.0])
    assert message == "invalid argument: objective 3: weight 0 is not positive"
    message = refusal(model.addobj, [0], None, None, [NAN])
    assert message == "invalid argument: objective 3: weight is NaN"
    tolerances = partial(model.setobjtol, [2])
    message = refusal(tolerances, [-1.0])
    assert message == (
        "invalid argument: objective 2: absolute tolerance -1 is negative"
    )
    message = refusal(tolerances, [SIGNED_NAN])
    assert message == (
        "invalid argument: objective 2: absolute tolerance is NaN"
    )
    message = refusal(tolerances, None, [-INFINITY])
    assert message == (
        "invalid argument: objective 2: relative tolerance is minus infinity"
    )
    message = refusal(model.setobjtol, [0], None, [NAN])
    assert message == (
        "invalid argument: objective 0: relative tolerance is NaN"
    )
    message = refusal(model.setobjsense, [3], [MIN])
    assert message == "invalid argument: objective 3: unknown"
    message = refusal(model.getobj, 4)
    assert message == "invalid argument: objective 4: unknown"
    message = refusal(model.getobj, 0, -1)
    assert message == "invalid argument: argument 'n': -1 is negative"
    assert model.getobj() == SAMPLE
    assert model.getobjtol(range(3)) == TOLERANCES


def test_lengths(model):
    with pytest.raises(ValueError, match="sense has length 1, not 2"):
        model.addobj([0, 1], [MIN])
    with pytest.raises(ValueError, match="prio has length 3, not 2"):
        model.addobj([0, 1], None, [0, 1, 2])
    with pytest.raises(ValueError, match="rtol has length 1, not 2"):
        model.setobjtol([0, 1], rtol=[1.0])
    assert model.getnobj() == 0


def test_numbers(model):
    with pytest.raises(TypeError, match="an entry of funidx must be an"):
        model.addobj([0.0])
    with pytest.raises(TypeError, match="an entry of sense must be an int"):
        model.addobj([0], [1.0])
    with pytest.raises(OverflowError, match="prio: 2147483648 does not"):
        model.addobj([0], None, [2**31])
    with pytest.raises(TypeError, match="an entry of coef must be a"):
        model.addobj([0], None, None, ["1"])
    with pytest.raises(TypeError, match="an entry of atol must be a"):
        model.setobjtol([0], [True])
    with pytest.raises(OverflowError, match="pos: 4294967296 does not fit"):
        model.getobj(2**32)
    assert model.getnobj() == 0


def test_locked(env, model):
    seen = []

    def change(event):
        for call in (
            partial(model.addobj, [0]),
            partial(model.setobjsense, [0], [MAX]),
            partial(model.setobjprio, [0], [0]),
            partial(model.setobjcoef, [0], [1.0]),
            partial(model.setobjtol, [0], [1.0]),
        ):
            try:
                call()
            except biyos.Error as error:
                seen.append(str(error))
        seen.append(model.getobj())
        seen.append(model.getobjsense([0]))
        seen.append(model.getobjprio([0]))
        seen.append(model.getobjcoef([0]))
        seen.append(model.getobjtol([0]))

    add_sample(model)
    env.addhook(change, EventKind.MESSAGE, EventLevel.ERROR)
    with pytest.raises(biyos.Error):
        env.setopt("seed", -1)
    read = [[MAX], [2], [0.5], ([0.0], [0.25])]
    assert seen == [LOCKED] * 5 + [SAMPLE] + read
    assert model.getnobj() == 3


def test_closed(env):
    model = biyos.Model(env)
    model.close()
    calls = (
        partial(model.addobj, [0]),
        partial(model.setobjsense, [], []),
        partial(model.getobjsense, []),
        partial(model.setobjprio, [], []),
        partial(model.getobjprio, []),
        partial(model.setobjcoef, [], []),
        partial(model.getobjcoef, []),
        partial(model.setobjtol, []),
        partial(model.getobjtol, []),
        model.getnobj,
        model.getobj,
    )
    for call in calls:
        with pytest.raises(ValueError, match="the model is closed"):
            call()


def test_on(env, model):
    o = add_sample(model)
    copy = model.copy()
    p = o[1:].on(copy)
    assert type(p) is biyos.Objectives
    copy.setobjsense(p, [MAX, MAX])
    assert copy.getobj()[1] == [MAX, MAX, MAX]
    assert model.getobj() == SAMPLE
    smaller = biyos.Model(env)
    smaller.addfuns(1)
    smaller.addobj([0, 0])
    few = "mdl holds too few objectives: 2, the block needs 3"
    with pytest.raises(ValueError, match=few):
        o.on(smaller)
    assert len(o[:2].on(smaller)) == 2
    copy.close()
