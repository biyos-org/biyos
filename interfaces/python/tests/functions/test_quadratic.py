import array
import ctypes
from functools import partial
from random import Random

import pytest

import biyos
from biyos import EventKind, EventLevel
from biyos._capi import lib
from listed import indexed
from number import Growing, Number
from refusal import LOCKED, refusal
from sized import Room, limit

LARGE = 1.0e308

SAMPLE = (
    [2, 2, 0, 0, 0],
    [1, 2, 0, 2, 1],
    [0, 2, 0, 1, 1],
    [3.0, 1.0, 2.0, -1.0, 0.0],
)
SORTED = (
    [0, 3, 3, 5],
    [0, 1, 1, 0, 2],
    [0, 1, 2, 1, 2],
    [2.0, 0.0, -1.0, 3.0, 1.0],
)
NONE = ([], [], [], [])


@pytest.fixture
def env():
    with biyos.Env() as env:
        yield env


@pytest.fixture
def model(env):
    with biyos.Model(env) as model:
        model.addvar(3)
        model.addfun(3)
        yield model


def add_sample(model):
    model.addquadterm(*SAMPLE)


def read_lengths(handle, beg, end, size, counts, *rest):
    firsts, seconds, values = rest
    terms = [(firsts, size), (seconds, size), (values, size)]
    return [(counts, end - beg)] + terms


def spread(found, first=0):
    off, *rest = found
    counts = [off[j + 1] - off[j] for j in range(len(off) - 1)]
    records = [first + j for j, n in enumerate(counts) for _ in range(n)]
    return (records, *rest)


def recorded(seen, function, *arguments):
    seen.append(arguments)
    return function(*arguments)


def test_add(model):
    assert model.addquadterm(*SAMPLE) is None
    assert indexed(model.getquadterm()) == spread(SORTED)


def test_sums(model):
    add_sample(model)
    model.addquadterm([0, 2, 2], [2, 1, 1], [2, 0, 2], [5, 1, 4])
    found = indexed(model.getquadterm())
    assert found[0] == [0, 0, 0, 0, 2, 2, 2]
    assert found[1] == [0, 1, 1, 2, 0, 1, 2]
    assert found[2] == [0, 1, 2, 2, 1, 2, 2]
    assert found[3] == [2.0, 0.0, -1.0, 5.0, 4.0, 4.0, 1.0]


def test_coordinates(model):
    fun = [0, 2, 0, 2, 0]
    firsts = [1, 2, 0, 1, 2]
    seconds = [1, 2, 0, 0, 1]
    model.addquadterm(fun, firsts, seconds, [0.0, 1.0, 2.0, 3.0, -1.0])
    assert indexed(model.getquadterm()) == spread(SORTED)
    model.addquadterm([], [], [], [])
    assert indexed(model.getquadterm()) == spread(SORTED)


def test_shuffled(env):
    draw = Random(20261122)
    for _ in range(20):
        n_vars = draw.randint(1, 5)
        n_funs = draw.randint(0, 5)
        with biyos.Model(env) as source, biyos.Model(env) as target:
            for model in (source, target):
                model.addvar(n_vars)
                model.addfun(n_funs)
            add_random(draw, source, n_vars, n_funs)
            found = indexed(source.getquadterm())
            order = list(range(len(found[3])))
            draw.shuffle(order)
            shuffled = ([column[k] for k in order] for column in found)
            target.addquadterm(*shuffled)
            assert indexed(target.getquadterm()) == found


def add_random(draw, model, n_vars, n_funs):
    every = [(i, j) for i in range(n_vars) for j in range(i, n_vars)]
    for f in range(n_funs):
        pairs = draw.sample(every, draw.randint(0, len(every)))
        firsts = [j for _, j in pairs]
        seconds = [i for i, _ in pairs]
        values = [float(draw.randint(-4, 4)) for _ in pairs]
        model.addquadterm([f] * len(pairs), firsts, seconds, values)


def test_misplaced(model):
    add_sample(model)
    before = indexed(model.getquadterm())
    terms = ([0, 1, 0], [0, 1, 1], [1, 1, 0], [1, 2, 3])
    message = refusal(model.addquadterm, *terms)
    assert message == "invalid argument: function 0: pair (0, 1) is repeated"
    message = refusal(model.addquadterm, [0, 3], [1, 1], [1, 1], [1, 2])
    assert message == "invalid argument: function 3: unknown"
    terms = ([1, 2], [0, 2], [0, 2], [1, LARGE * 2])
    message = refusal(model.addquadterm, *terms)
    assert message == (
        "invalid argument: function 2: coefficient of pair (2, 2) is infinity"
    )
    with pytest.raises(ValueError, match="var2 has length 1, not 2"):
        model.addquadterm([0, 1], [0, 1], [0], [1.0, 2.0])
    with pytest.raises(ValueError, match="coef has length 1, not 2"):
        model.addquadterm([0, 1], [0, 1], [0, 1], [1.0])
    assert indexed(model.getquadterm()) == before


def test_range(model):
    add_sample(model)
    tail = ([2, 2], [0, 2], [1, 2], [3.0, 1.0])
    assert indexed(model.getquadterm(range(1, 3))) == tail
    assert indexed(model.getquadterm([1, 2])) == tail
    assert indexed(model.getquadterm(range(3, 3))) == ([], [], [], [])


def test_listed(model):
    add_sample(model)
    assert indexed(model.getquadterm([2, 0, 2])) == (
        [2, 2, 0, 0, 0, 2, 2],
        [0, 2, 0, 1, 1, 0, 2],
        [1, 2, 0, 1, 2, 1, 2],
        [3.0, 1.0, 2.0, 0.0, -1.0, 3.0, 1.0],
    )
    assert indexed(model.getquadterm([1, 1])) == ([], [], [], [])


def test_counts(model):
    add_sample(model)
    assert model.getnquadterm() == 5
    assert model.getnquadterm([2, 0, 2]) == 7
    assert model.getnquadterm(range(1, 3)) == 2
    assert model.getnquadterm([]) == 0


def test_forms(env):
    draw = Random(20261122)
    for _ in range(8):
        with biyos.Model(env) as model:
            n_vars = draw.randint(1, 4)
            model.addvar(n_vars)
            count = draw.randint(1, 4)
            model.addfun(count)
            every = [(i, j) for i in range(n_vars) for j in range(i, n_vars)]
            for f in range(count):
                size = draw.randint(0, min(len(every), 3))
                pairs = draw.sample(every, size)
                firsts = [j for _, j in pairs]
                seconds = [i for i, _ in pairs]
                values = [float(draw.randint(-4, 4)) for _ in pairs]
                fun = [f] * len(pairs)
                model.addquadterm(fun, firsts, seconds, values)
            expect_lists(draw, model, count)


def expect_lists(draw, model, count):
    listed = [draw.randrange(count) for _ in range(draw.randint(0, 8))]
    wanted = ([], [], [], [])
    for index in listed:
        for column, more in zip(wanted, indexed(model.getquadterm([index]))):
            column += more
    assert indexed(model.getquadterm(listed)) == wanted
    assert model.getnquadterm(listed) == len(wanted[3])
    pos = draw.randint(0, count)
    n = draw.randint(0, count - pos)
    span = range(pos, pos + n)
    assert indexed(model.getquadterm(span)) == indexed(
        model.getquadterm(list(span))
    )


def test_values(model, monkeypatch):
    values = array.array("d", [1.0, 2.0])
    seen = []
    spy = partial(recorded, seen, lib.BIYOS_addquadterm)
    monkeypatch.setattr(lib, "BIYOS_addquadterm", spy)
    model.addquadterm([0, 0], [1, 0], [0, 0], values)
    assert len(seen) == 1
    assert ctypes.addressof(seen[0][5]) == values.buffer_info()[0]
    expected = ([0, 0], [0, 0], [0, 1], [2.0, 1.0])
    assert indexed(model.getquadterm()) == expected


def test_growing(model):
    add_sample(model)
    grow = partial(model.addfun, 3)
    expected = indexed(model.getquadterm())
    assert indexed(model.getquadterm([0, Growing(1, grow), 2])) == expected
    assert model.getnfun() == 9


def test_stale(model, monkeypatch):
    add_sample(model)
    monkeypatch.setattr(model, "getnfun", lambda: 0)
    spy = Room(lib.BIYOS_getquadtermrg, read_lengths)
    monkeypatch.setattr(lib, "BIYOS_getquadtermrg", spy)
    assert indexed(model.getquadterm(range(3))) == spread(SORTED)
    assert indexed(model.getquadterm()) == NONE


def test_outside(model, monkeypatch):
    limit(monkeypatch, biyos._model.model, 1)
    message = refusal(model.getquadterm, range(2**31 - 1))
    assert message == "invalid argument: function 3: unknown"
    message = refusal(model.getquadterm, range(-1, 3))
    assert message == "invalid argument: function -1: unknown"
    message = refusal(model.getquadterm, [0, 3])
    assert message == "invalid argument: function 3: unknown"
    message = refusal(model.getnquadterm, range(4, 4))
    assert message == "invalid argument: function 4: unknown"


def test_sequences(model):
    read_only = memoryview(array.array("i", [2]).tobytes()).cast("i")
    firsts = array.array("i", [2, 0])
    four = array.array("d", [4])
    model.addquadterm(range(2, 3), read_only, (1,), four)
    model.addquadterm((Number(0), Number(0)), firsts, range(1, 3), [1, 2])
    functions, firsts, seconds, values = indexed(model.getquadterm())
    assert functions == [0, 0, 2]
    assert firsts == [0, 1, 1]
    assert seconds == [2, 2, 2]
    assert values == [2.0, 1.0, 4.0]


def test_views(env, model):
    x = model.addvar(2)
    f = model.addfun(2)
    model.addquadterm(f, [x[1], 0], x, [1.0, 2.0])
    model.addquadterm([f[0]], [x[0]], [x[1]], [3.0])
    found = indexed(model.getquadterm(f))
    assert found == ([3, 4], [3, 0], [4, 4], [4.0, 2.0])
    other = biyos.Model(env)
    y = other.addvar(1)
    g = other.addfun(1)
    entry = "an entry of var1 is a variable of another model"
    with pytest.raises(ValueError, match=entry):
        model.addquadterm([0], [y[0]], [0], [1.0])
    with pytest.raises(ValueError, match="var2 is a block of another"):
        model.addquadterm([0], [0], y, [1.0])
    with pytest.raises(ValueError, match="fun is a block of another"):
        model.addquadterm(g, [0], [0], [1.0])
    with pytest.raises(ValueError, match="idx is a block of another"):
        model.getquadterm(g)
    with pytest.raises(TypeError, match="is a function, not a variable"):
        model.addquadterm([0], [f[0]], [0], [1.0])
    with pytest.raises(TypeError, match="fun is a block of variables"):
        model.addquadterm(x, [0, 0], [0, 0], [1.0, 1.0])
    with pytest.raises(TypeError, match="idx is a variable, not a"):
        model.getquadterm([x[0]])
    assert indexed(model.getquadterm(f)) == found


def test_gone(model, monkeypatch):
    x = model.addvar(2)
    f = model.addfun(2)
    model.addquadterm(f, [0, 1], x, [1.0, 2.0])
    before = indexed(model.getquadterm())
    function, other, variable = f[0], f[1], x[1]
    model._gen += 1
    seen = []
    names = ["BIYOS_addquadterm"]
    for form in ("sp", "rg"):
        names += [f"BIYOS_getnquadterm{form}", f"BIYOS_getquadterm{form}"]
    for name in names:
        spy = partial(recorded, seen, getattr(lib, name))
        monkeypatch.setattr(lib, name, spy)
    block = "the block predates a deletion"
    view = "the function predates a deletion"
    stale = "the variable predates a deletion"
    add = model.addquadterm
    cases = [
        (block, partial(add, f, [0, 0], [0, 0], [1.0, 1.0])),
        (view, partial(add, [function], [0], [0], [1.0])),
        (block, partial(add, [0, 0], x, [0, 0], [1.0, 1.0])),
        (stale, partial(add, [0], [0], [variable], [1.0])),
        (view, partial(model.getquadterm, [other])),
        (view, partial(model.getnquadterm, [other])),
    ]
    for message, call in cases:
        with pytest.raises(ValueError, match=message):
            call()
    assert seen == []
    assert indexed(model.getquadterm()) == before


def test_refused(model):
    add_sample(model)
    message = refusal(model.addquadterm, [3], [0], [0], [1.0])
    assert message == "invalid argument: function 3: unknown"
    message = refusal(model.addquadterm, [1], [3], [0], [1])
    assert message == "invalid argument: function 1: variable 3 is unknown"
    message = refusal(model.addquadterm, [1, 1], [0, 1], [1, 0], [1, 2])
    assert message == "invalid argument: function 1: pair (0, 1) is repeated"
    message = refusal(model.addquadterm, [1], [2], [0], [biyos.inf])
    assert message == (
        "invalid argument: function 1: coefficient of pair (2, 0) is infinity"
    )
    large = ([1], [1], [1], [LARGE])
    model.addquadterm(*large)
    message = refusal(model.addquadterm, *large)
    assert message == (
        "invalid argument: function 1: sum of coefficients of pair (1, 1)"
        " is infinity"
    )
    message = refusal(model.getquadterm, [4])
    assert message == "invalid argument: function 4: unknown"
    found = indexed(model.getquadterm())
    assert found[0] == [0, 0, 0, 1, 2, 2]
    assert found[3] == [2.0, 0.0, -1.0, LARGE, 3.0, 1.0]


def test_lengths(model):
    with pytest.raises(ValueError, match="var1 has length 1, not 2"):
        model.addquadterm([0, 0], [0], [0, 1], [1.0, 2.0])
    with pytest.raises(ValueError, match="var2 has length 1, not 2"):
        model.addquadterm([0, 0], [0, 1], [0], [1.0, 2.0])
    with pytest.raises(ValueError, match="coef has length 1, not 0"):
        model.addquadterm([], [], [], [1.0])
    assert indexed(model.getquadterm()) == NONE


def test_numbers(model):
    with pytest.raises(TypeError, match="an entry of coef must be a number"):
        model.addquadterm([0], [0], [0], ["1"])
    with pytest.raises(TypeError, match="an entry of var1 must be an"):
        model.addquadterm([0], [0.0], [0], [1.0])
    with pytest.raises(TypeError, match="an entry of var2 must be an"):
        model.addquadterm([0], [0], [0.0], [1.0])
    with pytest.raises(TypeError, match="an entry of fun must be an"):
        model.addquadterm([0.0], [0], [0], [1.0])
    with pytest.raises(OverflowError, match="idx: 4294967296 does not fit"):
        model.getquadterm(range(2**32, 2**32 + 1))
    assert indexed(model.getquadterm()) == NONE


def test_locked(env, model):
    seen = []

    def change(event):
        try:
            model.addquadterm([0], [0], [0], [1.0])
        except biyos.Error as error:
            seen.append(str(error))
        seen.append(indexed(model.getquadterm()))

    add_sample(model)
    env.addhook(change, EventKind.MESSAGE, EventLevel.ERROR)
    with pytest.raises(biyos.Error):
        env.setopt("seed", -1)
    assert seen == [LOCKED, spread(SORTED)]
    assert indexed(model.getquadterm()) == spread(SORTED)


def test_closed(env):
    model = biyos.Model(env)
    model.close()
    calls = (
        partial(model.addquadterm, [], [], [], []),
        model.getquadterm,
        model.getnquadterm,
    )
    for call in calls:
        with pytest.raises(ValueError, match="the model is closed"):
            call()


def test_copy(env, model):
    add_sample(model)
    copy = model.copy()
    assert indexed(copy.getquadterm()) == spread(SORTED)
    copy.addquadterm([1], [0], [0], [1.0])
    assert indexed(model.getquadterm()) == spread(SORTED)
    assert indexed(copy.getquadterm())[0] == [0, 0, 0, 1, 2, 2]
    copy.close()
