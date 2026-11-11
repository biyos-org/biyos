import array
import ctypes
from functools import partial

import pytest

import biyos
from biyos import EventKind, Level, _model
from biyos._capi import lib
from number import Growing, Number
from refusal import LOCKED, refusal
from sized import limit

INFINITY = float("inf")

LOWER = [-1.0, -INFINITY, 2.0]
UPPER = [1.0, INFINITY, 2.0]
STARTS = [0, 3, 3, 5]
COLUMNS = [2, 0, 1, 1, 0]
VALUES = [3.0, 1.0, 0.0, 5.0, 4.0]
SORTED = ([0, 3, 3, 5], [0, 1, 2, 0, 1], [1.0, 0.0, 3.0, 4.0, 5.0])

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
    model.add_rows(3, LOWER, UPPER, STARTS, COLUMNS, VALUES)


def recorded(seen, function, *arguments):
    seen.append(arguments)
    return function(*arguments)


def test_add(model):
    add_sample(model)
    assert model.row_count() == 3
    assert model.get_rows() == (LOWER, UPPER)
    assert model.get_entries() == SORTED


def test_range(model):
    add_sample(model)
    assert model.get_rows(1, 2) == (LOWER[1:], UPPER[1:])
    assert model.get_entries(1, 2) == ([0, 0, 2], [0, 1], [4.0, 5.0])
    assert model.get_entries(1) == ([0, 0, 2], [0, 1], [4.0, 5.0])
    assert model.get_entries(3) == ([0], [], [])
    assert model.get_rows(3) == ([], [])


def test_defaults(model):
    model.add_rows(2)
    model.add_rows(1, lower=[0.5])
    model.add_rows(1, upper=[0.5], starts=[0, 0])
    model.add_rows(0)
    low, high = -INFINITY, INFINITY
    assert model.get_rows() == ([low, low, 0.5, low], [high, high, high, 0.5])
    assert model.get_entries() == ([0, 0, 0, 0, 0], [], [])


def test_offsets(model, monkeypatch):
    starts = array.array("q", [0, 2])
    seen = []
    spy = partial(recorded, seen, lib.biyos_add_rows)
    monkeypatch.setattr(lib, "biyos_add_rows", spy)
    model.add_rows(1, None, None, starts, [1, 0], [1.0, 2.0])
    assert len(seen) == 1
    assert ctypes.addressof(seen[0][4]) != starts.buffer_info()[0]
    assert model.get_entries() == ([0, 2], [0, 1], [2.0, 1.0])


def test_values(model, monkeypatch):
    values = array.array("d", [1.0, 2.0])
    seen = []
    spy = partial(recorded, seen, lib.biyos_add_rows)
    monkeypatch.setattr(lib, "biyos_add_rows", spy)
    model.add_rows(1, None, None, [0, 2], [1, 0], values)
    assert len(seen) == 1
    assert ctypes.addressof(seen[0][6]) == values.buffer_info()[0]
    assert model.get_entries() == ([0, 2], [0, 1], [2.0, 1.0])


def test_growing(model):
    grow = partial(add_sample, model)
    assert model.get_rows(0, Growing(3, grow)) == (LOWER, UPPER)
    assert model.get_entries(3, Growing(3, grow)) == SORTED
    assert model.row_count() == 6


def test_stale(model, monkeypatch):
    add_sample(model)
    monkeypatch.setattr(model, "row_count", lambda: 0)
    assert model.get_rows(0, 3) == (LOWER, UPPER)
    assert model.get_entries(0, 3) == SORTED
    assert model.get_entries(1) == ([0], [], [])


def test_copied(model):
    starts = array.array("q", [0, 1])
    model.add_rows(1, None, None, starts, [Shrinking(starts)], [1.0])
    assert starts[1] == 0
    assert model.get_entries() == ([0, 1], [0], [1.0])


@pytest.mark.parametrize("count, starts, message", STARTS_WRONG)
def test_starts(model, count, starts, message):
    with pytest.raises(ValueError, match=message):
        model.add_rows(count, None, None, starts, [0], [1.0])
    assert model.row_count() == 0


def test_outside(model, monkeypatch):
    limit(monkeypatch, _model, 1)
    message = refusal(model.get_entries, 0, 2**31 - 1)
    assert message == "invalid argument: row 0: unknown"
    message = refusal(model.get_rows, 1, 2**31 - 2)
    assert message == "invalid argument: row 1: unknown"
    message = refusal(model.get_entries, -1)
    assert message == "invalid argument: row -1: unknown"


def test_sequences(model):
    read_only = memoryview(array.array("q", [0, 1]).tobytes()).cast("q")
    columns = array.array("i", [2, 1, 0, 1])
    model.add_rows(1, None, None, read_only, (2,), array.array("d", [4.0]))
    model.add_rows(2, None, None, range(0, 6, 2), columns, [1, 2, 3, 4])
    model.add_rows(1, None, None, (Number(0), Number(1)), [Number(0)], [5])
    starts, columns, values = model.get_entries()
    assert starts == [0, 1, 3, 5, 6]
    assert columns == [2, 1, 2, 0, 1, 0]
    assert values == [4.0, 2.0, 1.0, 3.0, 4.0, 5.0]


def test_bounds(model):
    model.add_rows(2)
    model.set_row_bounds([1, 0], lower=[1.0, 2.0])
    model.set_row_bounds([1], upper=[3.0])
    assert model.get_rows() == ([2.0, 1.0], [INFINITY, 3.0])
    model.set_row_bounds([0, 0], [5.0, 6.0], [7.0, 8.0])
    assert model.get_rows(0, 1) == ([6.0], [8.0])


def test_refused(model):
    add_sample(model)
    message = refusal(model.add_rows, 1, None, None, [0, 2], [1, 1], [1, 2])
    assert message == "invalid argument: row 3: column 1 is repeated"
    message = refusal(model.add_rows, 1, None, None, [0, 1], [3], [1.0])
    assert message == "invalid argument: row 3: column 3 is unknown"
    message = refusal(model.add_rows, 1, None, None, [0, 1], [0], [INFINITY])
    assert message == (
        "invalid argument: row 3: coefficient of column 0 is infinity"
    )
    message = refusal(model.add_rows, 1, [INFINITY])
    assert message == "invalid argument: row 3: lower bound is infinity"
    message = refusal(model.set_row_bounds, [3], [0.0], [1.0])
    assert message == "invalid argument: row 3: unknown"
    message = refusal(model.get_entries, 4)
    assert message == "invalid argument: row 4: unknown"
    message = refusal(model.get_rows, 0, -1)
    assert message == "invalid argument: argument 'count': -1 is negative"
    message = refusal(model.add_rows, -1)
    assert message == "invalid argument: argument 'count': -1 is negative"
    assert model.get_entries() == SORTED


def test_lengths(model):
    with pytest.raises(ValueError, match="starts has length 1, not 2"):
        model.add_rows(1, starts=[0])
    with pytest.raises(ValueError, match="columns has length 1, not 2"):
        model.add_rows(1, starts=[0, 2], columns=[0], values=[1.0, 2.0])
    with pytest.raises(ValueError, match="values has length 1, not 0"):
        model.add_rows(1, values=[1.0])
    with pytest.raises(ValueError, match="upper has length 1, not 2"):
        model.set_row_bounds([0, 1], upper=[1.0])
    assert model.row_count() == 0


@pytest.mark.parametrize("starts, error, message", WRONG)
def test_wrong(model, starts, error, message):
    with pytest.raises(error, match=message):
        model.add_rows(1, starts=starts)
    assert model.row_count() == 0


def test_numbers(model):
    with pytest.raises(TypeError, match="an entry of values must be a number"):
        model.add_rows(1, None, None, [0, 1], [0], ["1"])
    with pytest.raises(TypeError, match="an entry of columns must be an int"):
        model.add_rows(1, None, None, [0, 1], [0.0], [1.0])
    with pytest.raises(OverflowError, match="first: 4294967296 does not fit"):
        model.get_entries(2**32)
    assert model.row_count() == 0


def test_locked(env, model):
    seen = []

    def change(event):
        try:
            model.add_rows(1)
        except biyos.Error as error:
            seen.append(str(error))
        seen.append(model.get_entries())

    add_sample(model)
    env.subscribe(change, EventKind.MESSAGE, Level.ERROR)
    with pytest.raises(biyos.Error):
        env.set_option("seed", -1)
    assert seen == [LOCKED, SORTED]
    assert model.row_count() == 3


def test_closed(env):
    model = biyos.Model(env)
    model.close()
    calls = (
        partial(model.add_rows, 1),
        partial(model.set_row_bounds, []),
        model.row_count,
        model.get_rows,
        model.get_entries,
    )
    for call in calls:
        with pytest.raises(ValueError, match="the model is closed"):
            call()
