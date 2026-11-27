import array
import ctypes
import random
from bisect import bisect_left
from functools import partial

import pytest

import biyos
from biyos import EventKind, EventLevel
from biyos._capi import lib
from listed import indexed
from number import Growing, Number
from refusal import LOCKED, refusal
from sized import Room, limit

LARGE = 1.0e308

HELD = ([0, 3, 3, 5], [2, 0, 1, 1, 0], [3.0, 1.0, 0.0, 5.0, 4.0])
SORTED = ([0, 3, 3, 5], [0, 1, 2, 0, 1], [1.0, 0.0, 3.0, 4.0, 5.0])
ADDED = ([2, 2, 1, 1], [2, 0, 2, 0], [0.5, -1.0, 2.0, 0.0])
MERGED = (
    [0, 3, 5, 8],
    [0, 1, 2, 0, 2, 0, 1, 2],
    [1.0, 0.0, 3.0, 0.0, 2.0, 3.0, 5.0, 0.5],
)
HELD_VARS = ([0, 2, 4, 5], [0, 2, 0, 2, 0], [1.0, 4.0, 0.0, 5.0, 3.0])
MERGED_VARS = (
    [0, 3, 5, 8],
    [0, 1, 2, 0, 2, 0, 1, 2],
    [1.0, 0.0, 3.0, 0.0, 5.0, 3.0, 2.0, 0.5],
)


@pytest.fixture
def env():
    with biyos.Env() as env:
        yield env


@pytest.fixture
def model(env):
    with biyos.Model(env) as model:
        model.addvar(3)
        model.addfun(3)
        model.addlinterm(*spread(HELD, 0, 0))
        yield model


def terms(model):
    fun, var, coef = indexed(model.getrlinterm())
    ends = range(model.getnfun() + 1)
    return [bisect_left(fun, j) for j in ends], var, coef


def read_lengths(handle, beg, end, size, counts, funs, values):
    return [(counts, end - beg), (funs, size), (values, size)]


def spread(found, first=0, place=1):
    off, other, coef = found
    counts = [off[j + 1] - off[j] for j in range(len(off) - 1)]
    records = [first + j for j, n in enumerate(counts) for _ in range(n)]
    pair = [records, other]
    return pair[place], pair[1 - place], coef


def recorded(seen, function, *arguments):
    seen.append(arguments)
    return function(*arguments)


def transposed(by_function, pos, n):
    starts, variables, values = by_function
    found = [[] for _ in range(n)]
    for fun in range(len(starts) - 1):
        for k in range(starts[fun], starts[fun + 1]):
            if pos <= variables[k] < pos + n:
                found[variables[k] - pos].append((fun, values[k]))
    off = [0]
    for held in found:
        off.append(off[-1] + len(held))
    pairs = [pair for held in found for pair in held]
    return off, [fun for fun, _ in pairs], [value for _, value in pairs]


def random_terms(draw, n_vars, n_funs):
    off, var, coef = [0], [], []
    for _ in range(n_funs):
        chosen = draw.sample(range(n_vars), draw.randint(0, n_vars))
        var += chosen
        coef += [float(draw.randint(-4, 4)) for _ in chosen]
        off.append(len(var))
    return off, var, coef


def test_coordinates(model):
    fun = [1, 2, 1, 2]
    model.addlinterm(fun, [0, 2, 2, 0], [0.0, 0.5, 2.0, -1.0])
    assert terms(model) == MERGED
    model.addlinterm([], [], [])
    assert terms(model) == MERGED


def test_copied(env):
    draw = random.Random(20261122)
    for _ in range(20):
        n_vars = draw.randint(1, 7)
        n_funs = draw.randint(0, 6)
        with biyos.Model(env) as source, biyos.Model(env) as target:
            for model in (source, target):
                model.addvar(n_vars)
                model.addfun(n_funs)
            drawn = random_terms(draw, n_vars, n_funs)
            source.addlinterm(*spread(drawn, 0, 0))
            fun, var, coef = indexed(source.getrlinterm())
            order = list(range(len(coef)))
            draw.shuffle(order)
            target.addlinterm(
                [fun[k] for k in order],
                [var[k] for k in order],
                [coef[k] for k in order],
            )
            assert terms(target) == terms(source)


def test_misplaced(model):
    before = terms(model)
    message = refusal(model.addlinterm, [0, 1, 0], [1, 1, 1], [1, 2, 3])
    assert message == "invalid argument: function 0: variable 1 is repeated"
    message = refusal(model.addlinterm, [0, 3], [1, 1], [1, 2])
    assert message == "invalid argument: function 3: unknown"
    message = refusal(model.addlinterm, [0, 2], [1, 2], [1, LARGE * 2])
    assert message == (
        "invalid argument: function 2: coefficient of variable 2 is infinity"
    )
    with pytest.raises(ValueError, match="var has length 1, not 2"):
        model.addlinterm([0, 1], [0], [1.0, 2.0])
    with pytest.raises(ValueError, match="coef has length 1, not 2"):
        model.addlinterm([0, 1], [0, 1], [1.0])
    assert terms(model) == before


def test_sums(model):
    model.addlinterm(*ADDED)
    model.addlinterm([2, 2], [1, 2], [-5.0, 0.25])
    found = terms(model)
    assert found[:2] == MERGED[:2]
    assert found[2] == [1.0, 0.0, 3.0, 0.0, 2.0, 3.0, 0.0, 0.75]


def test_values(model, monkeypatch):
    values = array.array("d", [1.0, 2.0])
    seen = []
    spy = partial(recorded, seen, lib.BIYOS_addlinterm)
    monkeypatch.setattr(lib, "BIYOS_addlinterm", spy)
    model.addlinterm([1, 1], [2, 0], values)
    assert len(seen) == 1
    assert ctypes.addressof(seen[0][4]) == values.buffer_info()[0]
    assert terms(model)[1] == [0, 1, 2, 0, 2, 0, 1]


def test_sequences(model):
    read_only = memoryview(array.array("i", [1]).tobytes()).cast("i")
    model.addlinterm(read_only, (2,), array.array("d", [4]))
    model.addlinterm((Number(0), Number(0)), range(1, 3), [1, 2])
    off, var, coef = terms(model)
    assert off == [0, 3, 4, 6]
    assert var == [0, 1, 2, 2, 0, 1]
    assert coef == [1.0, 1.0, 5.0, 4.0, 4.0, 5.0]


def test_views(env, model):
    x = model.addvar(2)
    f = model.addfun(2)
    model.addlinterm(f, [x[1], 0], [1.0, 2.0])
    model.addlinterm([f[0], f[0]], x, [3.0, 4.0])
    found = ([3, 3, 4], [3, 4, 0], [3.0, 5.0, 2.0])
    assert indexed(model.getrlinterm(f)) == found
    assert indexed(model.getclinterm(x[1:])) == ([3], [4], [5.0])
    assert indexed(model.getrlinterm([f[0]])) == ([3, 3], [3, 4], [3.0, 5.0])
    other = biyos.Model(env)
    y = other.addvar(1)
    g = other.addfun(1)
    entry = "an entry of var is a variable of another model"
    with pytest.raises(ValueError, match=entry):
        model.addlinterm([0], [y[0]], [1.0])
    with pytest.raises(ValueError, match="fun is a block of another"):
        model.addlinterm(g, [0], [1.0])
    with pytest.raises(ValueError, match="var is a variable of another"):
        model.getclinterm([y[0]])
    with pytest.raises(TypeError, match="is a function, not a variable"):
        model.addlinterm([0], [f[0]], [1.0])
    with pytest.raises(TypeError, match="var is a function, not a"):
        model.getclinterm([f[0]])


def test_gone(model, monkeypatch):
    x = model.addvar(1)
    f = model.addfun(1)
    before = terms(model)
    function, variable = f[0], x[0]
    model._gen += 1
    seen = []
    names = ["BIYOS_addlinterm"]
    for form in ("sp", "rg"):
        names += [f"BIYOS_getnclinterm{form}", f"BIYOS_getclinterm{form}"]
    for name in names:
        spy = partial(recorded, seen, getattr(lib, name))
        monkeypatch.setattr(lib, name, spy)
    block = "the block predates a deletion"
    view = "the function predates a deletion"
    stale = "the variable predates a deletion"
    add = model.addlinterm
    cases = [
        (block, partial(add, f, [0], [1.0])),
        (view, partial(add, [function], [0], [1.0])),
        (stale, partial(add, [0], [variable], [1.0])),
        (stale, partial(model.getclinterm, [variable])),
        (stale, partial(model.getnclinterm, [variable])),
    ]
    for message, call in cases:
        with pytest.raises(ValueError, match=message):
            call()
    assert seen == []
    assert terms(model) == before


def test_numbers(model):
    with pytest.raises(TypeError, match="an entry of coef must be a number"):
        model.addlinterm([0], [0], ["1"])
    with pytest.raises(TypeError, match="an entry of var must be an"):
        model.addlinterm([0], [0.0], [1.0])
    with pytest.raises(TypeError, match="an entry of fun must be an"):
        model.addlinterm([0.0], [0], [1.0])
    with pytest.raises(OverflowError, match="var: 4294967296 does not fit"):
        model.getclinterm(range(2**32, 2**32 + 1))
    assert terms(model) == SORTED


def test_by_variable(model):
    assert indexed(model.getclinterm()) == spread(HELD_VARS)
    model.addlinterm(*ADDED)
    assert indexed(model.getclinterm()) == spread(MERGED_VARS)


def test_by_function(model):
    assert indexed(model.getrlinterm()) == spread(SORTED, 0, 0)
    assert indexed(model.getrlinterm([2, 0, 2])) == (
        [2, 2, 0, 0, 0, 2, 2],
        [0, 1, 0, 1, 2, 0, 1],
        [4.0, 5.0, 1.0, 0.0, 3.0, 4.0, 5.0],
    )
    assert indexed(model.getrlinterm(range(1, 2))) == ([], [], [])


def test_range(model):
    tail = ([0, 2, 3], [0, 2, 0], [0.0, 5.0, 3.0])
    assert indexed(model.getclinterm(range(1, 3))) == spread(tail, 1)
    assert indexed(model.getclinterm(range(2, 3))) == ([0], [2], [3.0])
    assert indexed(model.getclinterm(range(3, 3))) == ([], [], [])


def test_counts(model):
    assert model.getnrlinterm() == 5
    assert model.getnclinterm() == 5
    assert model.getnrlinterm([2, 2, 1]) == 4
    assert model.getnclinterm(range(1, 3)) == 3
    assert model.getnclinterm([]) == 0


def test_transpose(env):
    draw = random.Random(20261121)
    for _ in range(30):
        with biyos.Model(env) as model:
            n_vars = draw.randint(1, 7)
            n_funs = draw.randint(0, 6)
            model.addvar(n_vars)
            model.addfun(n_funs)
            drawn = random_terms(draw, n_vars, n_funs)
            model.addlinterm(*spread(drawn, 0, 0))
            for _ in range(3):
                fun = draw.sample(range(n_funs), draw.randint(0, n_funs))
                added = random_terms(draw, n_vars, len(fun))
                owners, var, coef = spread(added, 0, 0)
                model.addlinterm([fun[j] for j in owners], var, coef)
            by_function = terms(model)
            whole = transposed(by_function, 0, n_vars)
            assert indexed(model.getclinterm()) == spread(whole)
            assert indexed(model.getrlinterm()) == spread(by_function, 0, 0)
            pos = draw.randint(0, n_vars)
            n = draw.randint(0, n_vars - pos)
            part = transposed(by_function, pos, n)
            found = indexed(model.getclinterm(range(pos, pos + n)))
            assert found == spread(part, pos)
            expect_lists(draw, model, "rlinterm", n_funs)
            expect_lists(draw, model, "clinterm", n_vars)


def expect_lists(draw, model, name, count):
    size = draw.randint(0, 8) if count > 0 else 0
    listed = [draw.randrange(count) for _ in range(size)]
    read = getattr(model, f"get{name}")
    found = indexed(read(listed))
    wanted = ([], [], [])
    for index in listed:
        one = indexed(read(range(index, index + 1)))
        for column, more in zip(wanted, one):
            column += more
    assert found == wanted
    assert getattr(model, f"getn{name}")(listed) == len(found[2])


def test_growing(model):
    grow = partial(model.addvar, 2)
    expected = indexed(model.getclinterm())
    assert indexed(model.getclinterm([0, Growing(1, grow), 2])) == expected
    assert model.getnvar() == 7


def test_stale(model, monkeypatch):
    monkeypatch.setattr(model, "getnvar", lambda: 0)
    spy = Room(lib.BIYOS_getclintermrg, read_lengths)
    monkeypatch.setattr(lib, "BIYOS_getclintermrg", spy)
    assert indexed(model.getclinterm(range(3))) == spread(HELD_VARS)
    assert indexed(model.getclinterm()) == ([], [], [])


def test_outside(model, monkeypatch):
    limit(monkeypatch, biyos._model.model, 1)
    message = refusal(model.getclinterm, range(2**31 - 1))
    assert message == "invalid argument: variable 3: unknown"
    message = refusal(model.getclinterm, range(-1, 3))
    assert message == "invalid argument: variable -1: unknown"
    message = refusal(model.getrlinterm, [0, 3])
    assert message == "invalid argument: function 3: unknown"
    message = refusal(model.getnrlinterm, range(4, 4))
    assert message == "invalid argument: function 4: unknown"


def test_locked(env, model):
    seen = []

    def change(event):
        try:
            model.addlinterm([0], [0], [1.0])
        except biyos.Error as error:
            seen.append(str(error))
        seen.append(indexed(model.getclinterm([2])))

    env.addhook(change, EventKind.MESSAGE, EventLevel.ERROR)
    with pytest.raises(biyos.Error):
        env.setopt("seed", -1)
    assert seen == [LOCKED, ([0], [2], [3.0])]
    assert terms(model) == SORTED


def test_closed(env):
    model = biyos.Model(env)
    model.close()
    calls = (
        partial(model.addlinterm, [], [], []),
        model.getclinterm,
        model.getrlinterm,
        model.getnclinterm,
    )
    for call in calls:
        with pytest.raises(ValueError, match="the model is closed"):
            call()


def test_copy(model):
    model.addlinterm(*ADDED)
    copy = model.copy()
    assert terms(copy) == MERGED
    copy.addlinterm([1], [0], [1.0])
    assert terms(model) == MERGED
    assert indexed(copy.getclinterm([0])) == (
        [0, 1, 2],
        [0, 0, 0],
        [1.0, 1.0, 3.0],
    )
    copy.close()
