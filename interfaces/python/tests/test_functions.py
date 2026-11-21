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

CONSTANTS = [1.5, 0.0, -2.0]
STARTS = [0, 3, 3, 5]
VARIABLES = [2, 0, 1, 1, 0]
VALUES = [3.0, 1.0, 0.0, 5.0, 4.0]
SORTED = (CONSTANTS, [0, 3, 3, 5], [0, 1, 2, 0, 1], [1.0, 0.0, 3.0, 4.0, 5.0])

WRONG = [
    ([0, 1.5], TypeError, "an entry of starts must be an int, not float"),
    ([0, "1"], TypeError, "an entry of starts must be an int, not str"),
    ([0, True], TypeError, "an entry of starts must be an int, not bool"),
    ([0, 2**70], OverflowError, "1180591620717411303424 does not fit"),
]

STARTS_WRONG = [
    (1, [1, 2], "starts begins at 1, not 0"),
    (1, [0, -1], "starts decreases from 0 to -1 at entry 1"),
    (2, [0, 2, 1], "starts decreases from 2 to 1 at entry 2"),
]


class Shrinking:
    def __init__(self, starts):
        self.starts = starts

    def __index__(self):
        self.starts[1] = 0
        return 0


@pytest.fixture
def env():
    with biyos.Env() as env:
        yield env


@pytest.fixture
def model(env):
    with biyos.Model(env) as model:
        model.addvars(3)
        yield model


def add_sample(model):
    return model.addfuns(3, CONSTANTS, STARTS, VARIABLES, VALUES)


def read_lengths(handle, first, count, constants, starts, size, *rest):
    needed, variables, values = rest
    heads = [(constants, count), (starts, count + 1)]
    return heads + [(variables, size), (values, size)]


def recorded(seen, function, *arguments):
    seen.append(arguments)
    return function(*arguments)


def test_add(model):
    added = add_sample(model)
    assert model.getnfuns() == 3
    assert model.getfuns() == SORTED
    assert type(added) is biyos.Functions
    assert [index(function) for function in added] == [0, 1, 2]


def test_block(model):
    model.addfuns(2)
    f = model.addfuns(3)
    assert len(f) == 3
    assert all(type(function) is biyos.Function for function in f)
    assert type(f[1:]) is biyos.Functions
    assert [index(function) for function in f[1:]] == [3, 4]
    assert index(f[-1]) == 4
    assert len(model.addfuns(0)) == 0


def test_range(model):
    add_sample(model)
    tail = ([0.0, -2.0], [0, 0, 2], [0, 1], [4.0, 5.0])
    assert model.getfuns(1, 2) == tail
    assert model.getfuns(1) == tail
    assert model.getfuns(3) == ([], [0], [], [])


def test_defaults(model):
    model.addfuns(2)
    model.addfuns(1, consts=[0.5])
    model.addfuns(1, starts=[0, 0])
    model.addfuns(0)
    constants = [0.0, 0.0, 0.5, 0.0]
    assert model.getfuns() == (constants, [0, 0, 0, 0, 0], [], [])


def test_offsets(model, monkeypatch):
    starts = array.array("q", [0, 2])
    seen = []
    spy = partial(recorded, seen, lib.BIYOS_addfuns)
    monkeypatch.setattr(lib, "BIYOS_addfuns", spy)
    model.addfuns(1, None, starts, [1, 0], [1.0, 2.0])
    assert len(seen) == 1
    assert ctypes.addressof(seen[0][3]) != starts.buffer_info()[0]
    assert model.getfuns() == ([0.0], [0, 2], [0, 1], [2.0, 1.0])


def test_values(model, monkeypatch):
    constants = array.array("d", [4.0])
    values = array.array("d", [1.0, 2.0])
    seen = []
    spy = partial(recorded, seen, lib.BIYOS_addfuns)
    monkeypatch.setattr(lib, "BIYOS_addfuns", spy)
    model.addfuns(1, constants, [0, 2], [1, 0], values)
    assert len(seen) == 1
    assert ctypes.addressof(seen[0][2]) == constants.buffer_info()[0]
    assert ctypes.addressof(seen[0][5]) == values.buffer_info()[0]
    assert model.getfuns() == ([4.0], [0, 2], [0, 1], [2.0, 1.0])


def test_growing(model):
    grow = partial(add_sample, model)
    assert model.getfuns(0, Growing(3, grow)) == SORTED
    assert model.getfuns(3, Growing(3, grow)) == SORTED
    assert model.getnfuns() == 6


def test_stale(model, monkeypatch):
    add_sample(model)
    monkeypatch.setattr(model, "getnfuns", lambda: 0)
    spy = Room(lib.BIYOS_getfuns, read_lengths)
    monkeypatch.setattr(lib, "BIYOS_getfuns", spy)
    assert model.getfuns(0, 3) == SORTED
    assert model.getfuns(1) == ([], [0], [], [])


def test_copied(model):
    starts = array.array("q", [0, 1])
    model.addfuns(1, None, starts, [Shrinking(starts)], [1.0])
    assert starts[1] == 0
    assert model.getfuns() == ([0.0], [0, 1], [0], [1.0])


@pytest.mark.parametrize("count, starts, message", STARTS_WRONG)
def test_starts(model, count, starts, message):
    with pytest.raises(ValueError, match=message):
        model.addfuns(count, None, starts, [0], [1.0])
    assert model.getnfuns() == 0


def test_outside(model, monkeypatch):
    limit(monkeypatch, _model, 1)
    message = refusal(model.getfuns, 0, 2**31 - 1)
    assert message == "invalid argument: function 0: unknown"
    message = refusal(model.getfuns, -1)
    assert message == "invalid argument: function -1: unknown"


def test_sequences(model):
    read_only = memoryview(array.array("q", [0, 1]).tobytes()).cast("q")
    variables = array.array("i", [2, 1, 0, 1])
    model.addfuns(1, (2,), read_only, (2,), array.array("d", [4.0]))
    model.addfuns(2, None, range(0, 6, 2), variables, [1, 2, 3, 4])
    model.addfuns(1, None, (Number(0), Number(1)), [Number(0)], [5])
    constants, starts, variables, values = model.getfuns()
    assert constants == [2.0, 0.0, 0.0, 0.0]
    assert starts == [0, 1, 3, 5, 6]
    assert variables == [2, 1, 2, 0, 1, 0]
    assert values == [4.0, 2.0, 1.0, 3.0, 4.0, 5.0]


def test_views(env, model):
    x = model.addvars(2)
    f = model.addfuns(1, None, [0, 2], [x[1], 0], [1.0, 2.0])
    model.addfuns(1, None, [0, 2], x, [3.0, 4.0])
    found = model.getfuns(f[0], 1)
    assert found == ([0.0], [0, 2], [0, 4], [2.0, 1.0])
    assert model.getfuns(1)[2] == [3, 4]
    other = biyos.Model(env)
    y = other.addvars(1)
    g = other.addfuns(1)
    entry = "an entry of vars is a variable of another model"
    with pytest.raises(ValueError, match=entry):
        model.addfuns(1, None, [0, 1], [y[0]], [1.0])
    with pytest.raises(ValueError, match="vars is a block of another"):
        model.addfuns(1, None, [0, 1], y, [1.0])
    with pytest.raises(ValueError, match="beg is a function of another"):
        model.getfuns(g[0])
    with pytest.raises(ValueError, match="is a function, not a variable"):
        model.addfuns(1, None, [0, 1], [f[0]], [1.0])
    with pytest.raises(ValueError, match="beg is a variable, not a"):
        model.getfuns(x[0])
    with pytest.raises(ValueError, match="vars has length 2, not 1"):
        model.addfuns(1, None, [0, 1], x, [1.0])
    assert model.getnfuns() == 2


def test_gone(model):
    f = model.addfuns(2)
    view = f[0]
    model._version += 1
    with pytest.raises(ValueError, match="the function predates a deletion"):
        index(view)
    with pytest.raises(ValueError, match="the function predates a deletion"):
        model.getfuns(view)
    with pytest.raises(ValueError, match="the block predates a deletion"):
        len(f)


def test_refused(model):
    add_sample(model)
    message = refusal(model.addfuns, 1, None, [0, 2], [1, 1], [1, 2])
    assert message == "invalid argument: function 3: variable 1 is repeated"
    message = refusal(model.addfuns, 1, None, [0, 1], [3], [1.0])
    assert message == "invalid argument: function 3: variable 3 is unknown"
    message = refusal(model.addfuns, 1, None, [0, 1], [0], [INFINITY])
    assert message == (
        "invalid argument: function 3: coefficient of variable 0 is infinity"
    )
    message = refusal(model.addfuns, 1, [INFINITY])
    assert message == "invalid argument: function 3: constant is infinity"
    message = refusal(model.getfuns, 4)
    assert message == "invalid argument: function 4: unknown"
    message = refusal(model.getfuns, 0, -1)
    assert message == "invalid argument: argument 'n': -1 is negative"
    message = refusal(model.addfuns, -1)
    assert message == "invalid argument: argument 'n': -1 is negative"
    assert model.getfuns() == SORTED


def test_lengths(model):
    with pytest.raises(ValueError, match="starts has length 1, not 2"):
        model.addfuns(1, starts=[0])
    with pytest.raises(ValueError, match="vars has length 1, not 2"):
        model.addfuns(1, None, [0, 2], [0], [1.0, 2.0])
    with pytest.raises(ValueError, match="vals has length 1, not 0"):
        model.addfuns(1, vals=[1.0])
    with pytest.raises(ValueError, match="consts has length 2, not 1"):
        model.addfuns(1, [1.0, 2.0])
    assert model.getnfuns() == 0


@pytest.mark.parametrize("starts, error, message", WRONG)
def test_wrong(model, starts, error, message):
    with pytest.raises(error, match=message):
        model.addfuns(1, starts=starts)
    assert model.getnfuns() == 0


def test_numbers(model):
    with pytest.raises(TypeError, match="an entry of vals must be a number"):
        model.addfuns(1, None, [0, 1], [0], ["1"])
    with pytest.raises(TypeError, match="an entry of vars must be an"):
        model.addfuns(1, None, [0, 1], [0.0], [1.0])
    with pytest.raises(OverflowError, match="beg: 4294967296 does not fit"):
        model.getfuns(2**32)
    assert model.getnfuns() == 0


def test_locked(env, model):
    seen = []

    def change(event):
        try:
            model.addfuns(1)
        except biyos.Error as error:
            seen.append(str(error))
        seen.append(model.getfuns())

    add_sample(model)
    env.addhook(change, EventKind.MESSAGE, Level.ERROR)
    with pytest.raises(biyos.Error):
        env.setopt("seed", -1)
    assert seen == [LOCKED, SORTED]
    assert model.getnfuns() == 3


def test_closed(env):
    model = biyos.Model(env)
    model.close()
    calls = (
        partial(model.addfuns, 1),
        model.getnfuns,
        model.getfuns,
    )
    for call in calls:
        with pytest.raises(ValueError, match="the model is closed"):
            call()


def test_on(env, model):
    f = add_sample(model)
    copy = model.copy()
    g = f.on(copy)
    assert type(g) is biyos.Functions
    copy.addcons(g)
    assert copy.getcons()[0] == [0, 1, 2]
    with pytest.raises(ValueError, match="funs is a block of another"):
        model.addcons(g)
    smaller = biyos.Model(env)
    smaller.addfuns(2)
    few = "model holds too few functions: 2, the block needs 3"
    with pytest.raises(ValueError, match=few):
        f.on(smaller)
    assert [index(function) for function in f[:2].on(smaller)] == [0, 1]
    copy.close()
