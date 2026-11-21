import array
import ctypes
from functools import partial

import pytest

import biyos
from biyos import EventKind, EventLevel, _model
from biyos._capi import lib
from number import Growing, Number
from refusal import LOCKED, refusal
from sized import Room, limit

INFINITY = float("inf")
LARGE = 1.0e308

FUNCTIONS = [2, 0]
STARTS = [0, 2, 5]
FIRSTS = [1, 2, 0, 2, 1]
SECONDS = [0, 2, 0, 1, 1]
VALUES = [3.0, 1.0, 2.0, -1.0, 0.0]
SORTED = (
    [0, 3, 3, 5],
    [0, 1, 1, 0, 2],
    [0, 1, 2, 1, 2],
    [2.0, 0.0, -1.0, 3.0, 1.0],
)
NONE = ([0, 0, 0, 0], [], [], [])

WRONG = [
    ([0, 1.5], TypeError, "an entry of off must be an int, not float"),
    ([0, "1"], TypeError, "an entry of off must be an int, not str"),
    ([0, True], TypeError, "an entry of off must be an int, not bool"),
    ([0, 2**70], OverflowError, "1180591620717411303424 does not fit"),
]

STARTS_WRONG = [
    ([0], [1, 2], "off begins at 1, not 0"),
    ([0], [0, -1], "off decreases from 0 to -1 at entry 1"),
    ([0, 1], [0, 2, 1], "off decreases from 2 to 1 at entry 2"),
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
        model.addfuns(3)
        yield model


def add_sample(model):
    terms = (STARTS, FIRSTS, SECONDS, VALUES)
    model.addquadterms(FUNCTIONS, *terms)


def read_lengths(handle, first, count, size, starts, *rest):
    needed, firsts, seconds, values = rest
    terms = [(firsts, size), (seconds, size), (values, size)]
    return [(starts, count + 1)] + terms


def recorded(seen, function, *arguments):
    seen.append(arguments)
    return function(*arguments)


def test_add(model):
    terms = (STARTS, FIRSTS, SECONDS, VALUES)
    assert model.addquadterms(FUNCTIONS, *terms) is None
    assert model.getquadterms() == SORTED


def test_sums(model):
    add_sample(model)
    terms = ([0, 1, 3], [2, 1, 1], [2, 0, 2], [5, 1, 4])
    model.addquadterms([0, 2], *terms)
    found = model.getquadterms()
    assert found[0] == [0, 4, 4, 7]
    assert found[1] == [0, 1, 1, 2, 0, 1, 2]
    assert found[2] == [0, 1, 2, 2, 1, 2, 2]
    assert found[3] == [2.0, 0.0, -1.0, 5.0, 4.0, 4.0, 1.0]


def test_range(model):
    add_sample(model)
    tail = ([0, 0, 2], [0, 2], [1, 2], [3.0, 1.0])
    assert model.getquadterms(1, 2) == tail
    assert model.getquadterms(1) == tail
    assert model.getquadterms(3) == ([0], [], [], [])


def test_defaults(model):
    model.addquadterms([0, 1], None, None, None, None)
    model.addquadterms([2], [0, 0], [], [], [])
    model.addquadterms([], [0], [], [], [])
    assert model.getquadterms() == ([0, 0, 0, 0], [], [], [])


def test_offsets(model, monkeypatch):
    starts = array.array("q", [0, 2])
    seen = []
    spy = partial(recorded, seen, lib.BIYOS_addquadterms)
    monkeypatch.setattr(lib, "BIYOS_addquadterms", spy)
    model.addquadterms([1], starts, [1, 0], [0, 0], [1.0, 2.0])
    assert len(seen) == 1
    assert ctypes.addressof(seen[0][3]) != starts.buffer_info()[0]
    expected = ([0, 0, 2, 2], [0, 0], [0, 1], [2.0, 1.0])
    assert model.getquadterms() == expected


def test_values(model, monkeypatch):
    values = array.array("d", [1.0, 2.0])
    seen = []
    spy = partial(recorded, seen, lib.BIYOS_addquadterms)
    monkeypatch.setattr(lib, "BIYOS_addquadterms", spy)
    model.addquadterms([0], [0, 2], [1, 0], [0, 0], values)
    assert len(seen) == 1
    assert ctypes.addressof(seen[0][6]) == values.buffer_info()[0]
    expected = ([0, 2, 2, 2], [0, 0], [0, 1], [2.0, 1.0])
    assert model.getquadterms() == expected


def test_growing(model):
    add_sample(model)
    grow = partial(model.addfuns, 3)
    assert model.getquadterms(0, Growing(3, grow)) == SORTED
    assert model.getquadterms(3, Growing(3, grow)) == NONE
    assert model.getnfuns() == 9


def test_stale(model, monkeypatch):
    add_sample(model)
    monkeypatch.setattr(model, "getnfuns", lambda: 0)
    spy = Room(lib.BIYOS_getquadterms, read_lengths)
    monkeypatch.setattr(lib, "BIYOS_getquadterms", spy)
    assert model.getquadterms(0, 3) == SORTED
    assert model.getquadterms(1) == ([0], [], [], [])


def test_copied(model):
    starts = array.array("q", [0, 1])
    model.addquadterms([0], starts, [Shrinking(starts)], [1], [1.0])
    assert starts[1] == 0
    assert model.getquadterms() == ([0, 1, 1, 1], [0], [1], [1.0])


@pytest.mark.parametrize("functions, starts, message", STARTS_WRONG)
def test_starts(model, functions, starts, message):
    with pytest.raises(ValueError, match=message):
        model.addquadterms(functions, starts, [0], [0], [1.0])
    assert model.getquadterms() == NONE


def test_outside(model, monkeypatch):
    limit(monkeypatch, _model, 1)
    message = refusal(model.getquadterms, 0, 2**31 - 1)
    assert message == "invalid argument: function 3: unknown"
    message = refusal(model.getquadterms, -1)
    assert message == "invalid argument: function -1: unknown"


def test_sequences(model):
    read_only = memoryview(array.array("q", [0, 1]).tobytes()).cast("q")
    firsts = array.array("i", [2, 0])
    four = array.array("d", [4])
    model.addquadterms((2,), read_only, (2,), (1,), four)
    model.addquadterms(range(1), range(0, 4, 2), firsts, [1, 2], [1, 2])
    model.addquadterms([Number(1)], (Number(0), Number(0)), [], [], [])
    starts, firsts, seconds, values = model.getquadterms()
    assert starts == [0, 2, 2, 3]
    assert firsts == [0, 1, 1]
    assert seconds == [2, 2, 2]
    assert values == [2.0, 1.0, 4.0]


def test_views(env, model):
    x = model.addvars(2)
    f = model.addfuns(2)
    model.addquadterms(f, [0, 1, 2], [x[1], 0], x, [1.0, 2.0])
    model.addquadterms([f[0]], [0, 1], [x[0]], [x[1]], [3.0])
    found = model.getquadterms(f[0])
    assert found == ([0, 1, 2], [3, 0], [4, 4], [4.0, 2.0])
    other = biyos.Model(env)
    y = other.addvars(1)
    g = other.addfuns(1)
    entry = "an entry of varidx1 is a variable of another model"
    with pytest.raises(ValueError, match=entry):
        model.addquadterms([0], [0, 1], [y[0]], [0], [1.0])
    with pytest.raises(ValueError, match="varidx2 is a block of another"):
        model.addquadterms([0], [0, 1], [0], y, [1.0])
    with pytest.raises(ValueError, match="funidx is a block of another"):
        model.addquadterms(g, [0, 1], [0], [0], [1.0])
    with pytest.raises(ValueError, match="beg is a function of another"):
        model.getquadterms(g[0])
    with pytest.raises(ValueError, match="is a function, not a variable"):
        model.addquadterms([0], [0, 1], [f[0]], [0], [1.0])
    with pytest.raises(ValueError, match="funidx is a block of variables"):
        model.addquadterms(x, [0, 1, 1], [0], [0], [1.0])
    with pytest.raises(ValueError, match="beg is a variable, not a"):
        model.getquadterms(x[0])
    assert model.getquadterms(f[0]) == found


def test_gone(model, monkeypatch):
    x = model.addvars(2)
    f = model.addfuns(2)
    model.addquadterms(f, [0, 1, 2], [0, 1], x, [1.0, 2.0])
    before = model.getquadterms()
    function, other, variable = f[0], f[1], x[1]
    model._version += 1
    seen = []
    for name in ("BIYOS_addquadterms", "BIYOS_getquadterms"):
        spy = partial(recorded, seen, getattr(lib, name))
        monkeypatch.setattr(lib, name, spy)
    block = "the block predates a deletion"
    view = "the function predates a deletion"
    stale = "the variable predates a deletion"
    add = model.addquadterms
    cases = [
        (block, partial(add, f, [0, 0, 0], [], [], [])),
        (view, partial(add, [function], [0, 0], [], [], [])),
        (block, partial(add, [0], [0, 2], x, [0, 0], [1.0, 1.0])),
        (stale, partial(add, [0], [0, 1], [0], [variable], [1.0])),
        (view, partial(model.getquadterms, other)),
    ]
    for message, call in cases:
        with pytest.raises(ValueError, match=message):
            call()
    assert seen == []
    assert model.getquadterms() == before


def test_refused(model):
    add_sample(model)
    message = refusal(model.addquadterms, [1, 1], [0, 0, 0], [], [], [])
    assert message == (
        "invalid argument: argument 'funidx': function 1 is repeated"
    )
    message = refusal(model.addquadterms, [3], [0, 0], [], [], [])
    assert message == "invalid argument: function 3: unknown"
    message = refusal(model.addquadterms, [1], [0, 1], [3], [0], [1])
    assert message == "invalid argument: function 1: variable 3 is unknown"
    message = refusal(
        model.addquadterms, [1], [0, 2], [0, 1], [1, 0], [1, 2]
    )
    assert message == "invalid argument: function 1: pair (0, 1) is repeated"
    message = refusal(
        model.addquadterms, [1], [0, 1], [2], [0], [INFINITY]
    )
    assert message == (
        "invalid argument: function 1: coefficient of pair (2, 0) is infinity"
    )
    large = ([1], [0, 1], [1], [1], [LARGE])
    model.addquadterms(*large)
    message = refusal(model.addquadterms, *large)
    assert message == (
        "invalid argument: function 1: sum of coefficients of pair (1, 1)"
        " is infinity"
    )
    message = refusal(model.getquadterms, 4)
    assert message == "invalid argument: function 4: unknown"
    message = refusal(model.getquadterms, 0, -1)
    assert message == "invalid argument: argument 'n': -1 is negative"
    found = model.getquadterms()
    assert found[0] == [0, 3, 4, 6]
    assert found[3] == [2.0, 0.0, -1.0, LARGE, 3.0, 1.0]


def test_lengths(model):
    with pytest.raises(ValueError, match="off has length 1, not 2"):
        model.addquadterms([0], [0], [], [], [])
    with pytest.raises(ValueError, match="varidx1 has length 1, not 2"):
        model.addquadterms([0], [0, 2], [0], [0, 1], [1.0, 2.0])
    with pytest.raises(ValueError, match="varidx2 has length 1, not 2"):
        model.addquadterms([0], [0, 2], [0, 1], [0], [1.0, 2.0])
    with pytest.raises(ValueError, match="coef has length 1, not 0"):
        model.addquadterms([0], None, None, None, [1.0])
    assert model.getquadterms() == NONE


@pytest.mark.parametrize("starts, error, message", WRONG)
def test_wrong(model, starts, error, message):
    with pytest.raises(error, match=message):
        model.addquadterms([0], starts, [0], [0], [1.0])
    assert model.getquadterms() == NONE


def test_numbers(model):
    with pytest.raises(TypeError, match="an entry of coef must be a number"):
        model.addquadterms([0], [0, 1], [0], [0], ["1"])
    with pytest.raises(TypeError, match="an entry of varidx1 must be an"):
        model.addquadterms([0], [0, 1], [0.0], [0], [1.0])
    with pytest.raises(TypeError, match="an entry of varidx2 must be an"):
        model.addquadterms([0], [0, 1], [0], [0.0], [1.0])
    with pytest.raises(TypeError, match="an entry of funidx must be an"):
        model.addquadterms([0.0], [0, 0], [], [], [])
    with pytest.raises(OverflowError, match="beg: 4294967296 does not fit"):
        model.getquadterms(2**32)
    assert model.getquadterms() == NONE


def test_locked(env, model):
    seen = []

    def change(event):
        try:
            model.addquadterms([0], [0, 1], [0], [0], [1.0])
        except biyos.Error as error:
            seen.append(str(error))
        seen.append(model.getquadterms())

    add_sample(model)
    env.addhook(change, EventKind.MESSAGE, EventLevel.ERROR)
    with pytest.raises(biyos.Error):
        env.setopt("seed", -1)
    assert seen == [LOCKED, SORTED]
    assert model.getquadterms() == SORTED


def test_closed(env):
    model = biyos.Model(env)
    model.close()
    calls = (
        partial(model.addquadterms, [], [0], [], [], []),
        model.getquadterms,
    )
    for call in calls:
        with pytest.raises(ValueError, match="the model is closed"):
            call()


def test_copy(env, model):
    add_sample(model)
    copy = model.copy()
    assert copy.getquadterms() == SORTED
    copy.addquadterms([1], [0, 1], [0], [0], [1.0])
    assert model.getquadterms() == SORTED
    assert copy.getquadterms()[0] == [0, 3, 4, 6]
    copy.close()
