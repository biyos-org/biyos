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
    with biyos.Environment() as env:
        yield env


@pytest.fixture
def model(env):
    with biyos.Model(env) as model:
        model.add_variables(3)
        yield model


def add_sample(model):
    return model.add_functions(3, CONSTANTS, STARTS, VARIABLES, VALUES)


def read_lengths(handle, first, count, constants, starts, size, *rest):
    needed, variables, values = rest
    heads = [(constants, count), (starts, count + 1)]
    return heads + [(variables, size), (values, size)]


def recorded(seen, function, *arguments):
    seen.append(arguments)
    return function(*arguments)


def test_add(model):
    added = add_sample(model)
    assert model.function_count() == 3
    assert model.get_functions() == SORTED
    assert type(added) is biyos.Functions
    assert [index(function) for function in added] == [0, 1, 2]


def test_block(model):
    model.add_functions(2)
    f = model.add_functions(3)
    assert len(f) == 3
    assert all(type(function) is biyos.Function for function in f)
    assert type(f[1:]) is biyos.Functions
    assert [index(function) for function in f[1:]] == [3, 4]
    assert index(f[-1]) == 4
    assert len(model.add_functions(0)) == 0


def test_range(model):
    add_sample(model)
    tail = ([0.0, -2.0], [0, 0, 2], [0, 1], [4.0, 5.0])
    assert model.get_functions(1, 2) == tail
    assert model.get_functions(1) == tail
    assert model.get_functions(3) == ([], [0], [], [])


def test_defaults(model):
    model.add_functions(2)
    model.add_functions(1, constants=[0.5])
    model.add_functions(1, starts=[0, 0])
    model.add_functions(0)
    constants = [0.0, 0.0, 0.5, 0.0]
    assert model.get_functions() == (constants, [0, 0, 0, 0, 0], [], [])


def test_offsets(model, monkeypatch):
    starts = array.array("q", [0, 2])
    seen = []
    spy = partial(recorded, seen, lib.biyos_add_functions)
    monkeypatch.setattr(lib, "biyos_add_functions", spy)
    model.add_functions(1, None, starts, [1, 0], [1.0, 2.0])
    assert len(seen) == 1
    assert ctypes.addressof(seen[0][3]) != starts.buffer_info()[0]
    assert model.get_functions() == ([0.0], [0, 2], [0, 1], [2.0, 1.0])


def test_values(model, monkeypatch):
    constants = array.array("d", [4.0])
    values = array.array("d", [1.0, 2.0])
    seen = []
    spy = partial(recorded, seen, lib.biyos_add_functions)
    monkeypatch.setattr(lib, "biyos_add_functions", spy)
    model.add_functions(1, constants, [0, 2], [1, 0], values)
    assert len(seen) == 1
    assert ctypes.addressof(seen[0][2]) == constants.buffer_info()[0]
    assert ctypes.addressof(seen[0][5]) == values.buffer_info()[0]
    assert model.get_functions() == ([4.0], [0, 2], [0, 1], [2.0, 1.0])


def test_growing(model):
    grow = partial(add_sample, model)
    assert model.get_functions(0, Growing(3, grow)) == SORTED
    assert model.get_functions(3, Growing(3, grow)) == SORTED
    assert model.function_count() == 6


def test_stale(model, monkeypatch):
    add_sample(model)
    monkeypatch.setattr(model, "function_count", lambda: 0)
    spy = Room(lib.biyos_get_functions, read_lengths)
    monkeypatch.setattr(lib, "biyos_get_functions", spy)
    assert model.get_functions(0, 3) == SORTED
    assert model.get_functions(1) == ([], [0], [], [])


def test_copied(model):
    starts = array.array("q", [0, 1])
    model.add_functions(1, None, starts, [Shrinking(starts)], [1.0])
    assert starts[1] == 0
    assert model.get_functions() == ([0.0], [0, 1], [0], [1.0])


@pytest.mark.parametrize("count, starts, message", STARTS_WRONG)
def test_starts(model, count, starts, message):
    with pytest.raises(ValueError, match=message):
        model.add_functions(count, None, starts, [0], [1.0])
    assert model.function_count() == 0


def test_outside(model, monkeypatch):
    limit(monkeypatch, _model, 1)
    message = refusal(model.get_functions, 0, 2**31 - 1)
    assert message == "invalid argument: function 0: unknown"
    message = refusal(model.get_functions, -1)
    assert message == "invalid argument: function -1: unknown"


def test_sequences(model):
    read_only = memoryview(array.array("q", [0, 1]).tobytes()).cast("q")
    variables = array.array("i", [2, 1, 0, 1])
    model.add_functions(1, (2,), read_only, (2,), array.array("d", [4.0]))
    model.add_functions(2, None, range(0, 6, 2), variables, [1, 2, 3, 4])
    model.add_functions(1, None, (Number(0), Number(1)), [Number(0)], [5])
    constants, starts, variables, values = model.get_functions()
    assert constants == [2.0, 0.0, 0.0, 0.0]
    assert starts == [0, 1, 3, 5, 6]
    assert variables == [2, 1, 2, 0, 1, 0]
    assert values == [4.0, 2.0, 1.0, 3.0, 4.0, 5.0]


def test_views(env, model):
    x = model.add_variables(2)
    f = model.add_functions(1, None, [0, 2], [x[1], 0], [1.0, 2.0])
    model.add_functions(1, None, [0, 2], x, [3.0, 4.0])
    found = model.get_functions(f[0], 1)
    assert found == ([0.0], [0, 2], [0, 4], [2.0, 1.0])
    assert model.get_functions(1)[2] == [3, 4]
    other = biyos.Model(env)
    y = other.add_variables(1)
    g = other.add_functions(1)
    entry = "an entry of variables is a variable of another model"
    with pytest.raises(ValueError, match=entry):
        model.add_functions(1, None, [0, 1], [y[0]], [1.0])
    with pytest.raises(ValueError, match="variables is a block of another"):
        model.add_functions(1, None, [0, 1], y, [1.0])
    with pytest.raises(ValueError, match="first is a function of another"):
        model.get_functions(g[0])
    with pytest.raises(ValueError, match="is a function, not a variable"):
        model.add_functions(1, None, [0, 1], [f[0]], [1.0])
    with pytest.raises(ValueError, match="first is a variable, not a"):
        model.get_functions(x[0])
    with pytest.raises(ValueError, match="variables has length 2, not 1"):
        model.add_functions(1, None, [0, 1], x, [1.0])
    assert model.function_count() == 2


def test_gone(model):
    f = model.add_functions(2)
    view = f[0]
    model._version += 1
    with pytest.raises(ValueError, match="the function predates a deletion"):
        index(view)
    with pytest.raises(ValueError, match="the function predates a deletion"):
        model.get_functions(view)
    with pytest.raises(ValueError, match="the block predates a deletion"):
        len(f)


def test_refused(model):
    add_sample(model)
    message = refusal(model.add_functions, 1, None, [0, 2], [1, 1], [1, 2])
    assert message == "invalid argument: function 3: variable 1 is repeated"
    message = refusal(model.add_functions, 1, None, [0, 1], [3], [1.0])
    assert message == "invalid argument: function 3: variable 3 is unknown"
    message = refusal(model.add_functions, 1, None, [0, 1], [0], [INFINITY])
    assert message == (
        "invalid argument: function 3: coefficient of variable 0 is infinity"
    )
    message = refusal(model.add_functions, 1, [INFINITY])
    assert message == "invalid argument: function 3: constant is infinity"
    message = refusal(model.get_functions, 4)
    assert message == "invalid argument: function 4: unknown"
    message = refusal(model.get_functions, 0, -1)
    assert message == "invalid argument: argument 'count': -1 is negative"
    message = refusal(model.add_functions, -1)
    assert message == "invalid argument: argument 'count': -1 is negative"
    assert model.get_functions() == SORTED


def test_lengths(model):
    with pytest.raises(ValueError, match="starts has length 1, not 2"):
        model.add_functions(1, starts=[0])
    with pytest.raises(ValueError, match="variables has length 1, not 2"):
        model.add_functions(1, None, [0, 2], [0], [1.0, 2.0])
    with pytest.raises(ValueError, match="values has length 1, not 0"):
        model.add_functions(1, values=[1.0])
    with pytest.raises(ValueError, match="constants has length 2, not 1"):
        model.add_functions(1, [1.0, 2.0])
    assert model.function_count() == 0


@pytest.mark.parametrize("starts, error, message", WRONG)
def test_wrong(model, starts, error, message):
    with pytest.raises(error, match=message):
        model.add_functions(1, starts=starts)
    assert model.function_count() == 0


def test_numbers(model):
    with pytest.raises(TypeError, match="an entry of values must be a number"):
        model.add_functions(1, None, [0, 1], [0], ["1"])
    with pytest.raises(TypeError, match="an entry of variables must be an"):
        model.add_functions(1, None, [0, 1], [0.0], [1.0])
    with pytest.raises(OverflowError, match="first: 4294967296 does not fit"):
        model.get_functions(2**32)
    assert model.function_count() == 0


def test_locked(env, model):
    seen = []

    def change(event):
        try:
            model.add_functions(1)
        except biyos.Error as error:
            seen.append(str(error))
        seen.append(model.get_functions())

    add_sample(model)
    env.subscribe(change, EventKind.MESSAGE, Level.ERROR)
    with pytest.raises(biyos.Error):
        env.set_option("seed", -1)
    assert seen == [LOCKED, SORTED]
    assert model.function_count() == 3


def test_closed(env):
    model = biyos.Model(env)
    model.close()
    calls = (
        partial(model.add_functions, 1),
        model.function_count,
        model.get_functions,
    )
    for call in calls:
        with pytest.raises(ValueError, match="the model is closed"):
            call()
