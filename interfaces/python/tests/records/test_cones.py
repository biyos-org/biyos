import array
import ctypes
from functools import partial
from operator import index

import pytest

import biyos
from biyos import ConeKind, EventKind, EventLevel, _model
from biyos._capi import lib
from number import Number
from refusal import LOCKED, refusal
from sized import Room, limit
from tolerance import CHECKS

KINDS = [ConeKind.ROTATED, ConeKind.DUAL_POWER]
OFF = [0, 3, 5]
MEMBERS = [4, 0, 2, 1, 3]
PRMOFF = [0, 0, 2]
PRM = [0.5, 2.0]
SAMPLE = (KINDS, OFF, MEMBERS)
WEIGHTS = (PRMOFF, PRM)

SHAPES = [
    (ConeKind.NONNEGATIVE, 1, []),
    (ConeKind.SECOND_ORDER, 3, []),
    (ConeKind.ROTATED, 2, []),
    (ConeKind.EXPONENTIAL, 3, []),
    (ConeKind.DUAL_EXPONENTIAL, 3, []),
    (ConeKind.POWER, 3, [1.0, 2.0]),
    (ConeKind.DUAL_POWER, 2, [0.25]),
    (ConeKind.SEMIDEFINITE, 6, []),
]

REFUSALS = [
    (([9], [0, 1], [0]), "cone 2: kind 9 is unknown"),
    (([ConeKind.SECOND_ORDER], [0, 0], []), "cone 2: has no member"),
    (([ConeKind.EXPONENTIAL], [0, 2], [0, 1]), "cone 2: dimension 2 is not 3"),
    (([ConeKind.ROTATED], [0, 1], [0]), "cone 2: dimension 1 is below 2"),
    (
        ([ConeKind.SEMIDEFINITE], [0, 4], [0, 1, 2, 3]),
        "cone 2: dimension 4 is not n(n+1)/2",
    ),
    (
        ([ConeKind.NONNEGATIVE], [0, 2], [1, 1]),
        "cone 2: function 1 is repeated",
    ),
    (([ConeKind.NONNEGATIVE], [0, 1], [5]), "cone 2: function 5 is unknown"),
    (
        ([ConeKind.NONNEGATIVE], [0, 1], [0], [0, 1], [1.0]),
        "cone 2: takes no parameter, given 1",
    ),
    (
        ([ConeKind.POWER], [0, 2], [0, 1]),
        "cone 2: takes at least one parameter",
    ),
    (
        ([ConeKind.POWER], [0, 2], [0, 1], [0, 1], [0.0]),
        "cone 2: parameter 0 is 0, not positive",
    ),
    (
        ([ConeKind.DUAL_POWER], [0, 2], [0, 1], [0, 1], [float("nan")]),
        "cone 2: parameter 0 is NaN",
    ),
]


@pytest.fixture
def env():
    with biyos.Env() as env:
        yield env


@pytest.fixture
def model(env):
    with biyos.Model(env) as model:
        model.addfun(5)
        yield model


def add_sample(model):
    return model.addcone(KINDS, OFF, MEMBERS, PRMOFF, PRM)


def read_lengths(handle, pos, n, cap, kinds, starts, members):
    return [(kinds, n), (starts, n + 1), (members, cap)]


def recorded(seen, name, function, *arguments):
    seen.append(name)
    return function(*arguments)


def trace(monkeypatch, seen, call):
    for form in ("sp", "rg"):
        name = f"BIYOS_{call}{form}"
        spy = partial(recorded, seen, form, getattr(lib, name))
        monkeypatch.setattr(lib, name, spy)


def test_add(model):
    added = add_sample(model)
    assert type(added) is biyos.Cones
    assert [index(cone) for cone in added] == [0, 1]
    assert model.getncone() == 2
    assert model.getcone() == SAMPLE
    assert model.getconeprm() == WEIGHTS
    assert all(type(kind) is ConeKind for kind in model.getcone()[0])
    assert model.getnconefun() == 5
    assert model.getnconeprm() == 2


def test_block(model):
    model.addcone([ConeKind.NONNEGATIVE], [0, 1], [0])
    c = add_sample(model)
    assert len(c) == 2
    assert all(type(cone) is biyos.Cone for cone in c)
    assert type(c[1:]) is biyos.Cones
    assert [index(cone) for cone in c] == [1, 2]
    assert len(model.addcone([], [0], [])) == 0
    assert model.getncone() == 3


@pytest.mark.parametrize("kind, dimension, weights", SHAPES)
def test_kinds(env, kind, dimension, weights):
    with biyos.Model(env) as model:
        f = model.addfun(dimension)
        model.addcone([kind], [0, dimension], f, [0, len(weights)], weights)
        found = ([kind], [0, dimension], list(range(dimension)))
        assert model.getcone() == found
        assert model.getconeprm() == ([0, len(weights)], weights)


def test_range(model):
    add_sample(model)
    assert model.getcone(range(1, 2)) == ([KINDS[1]], [0, 2], [1, 3])
    found = model.getcone([1, 0, 1])
    kinds = [KINDS[1], KINDS[0], KINDS[1]]
    assert found == (kinds, [0, 2, 5, 7], [1, 3, 4, 0, 2, 1, 3])
    assert model.getcone(range(2, 2)) == ([], [0], [])
    assert model.getcone([]) == ([], [0], [])
    assert model.getconeprm([1, 1, 0]) == ([0, 2, 4, 4], PRM * 2)
    assert model.getconeprm(range(0, 1)) == ([0, 0], [])
    assert model.getnconefun([1, 0, 1]) == 7
    assert model.getnconefun(range(1, 2)) == 2
    assert model.getnconeprm([0]) == 0
    assert model.getnconeprm(range(2)) == 2


def test_forms(model, monkeypatch):
    c = add_sample(model)
    seen = []
    for call in ("getnconefun", "getcone", "getnconeprm", "getconeprm"):
        trace(monkeypatch, seen, call)
    model.getcone(c)
    model.getconeprm(range(1, 2))
    assert seen == ["rg"] * 4
    seen.clear()
    model.getcone([1, 0])
    model.getconeprm(range(0, 2, 2))
    assert seen == ["sp"] * 4


def test_defaults(model):
    model.addcone(
        [ConeKind.SECOND_ORDER, ConeKind.NONNEGATIVE], [0, 2, 3], [0, 1, 2]
    )
    assert model.getconeprm() == ([0, 0, 0], [])
    model.addcone([ConeKind.POWER], [0, 2], [3, 4], [0, 1], [0.5])
    assert model.getconeprm() == ([0, 0, 0, 1], [0.5])


def test_shared(model):
    model.addcon([1])
    model.addcone([ConeKind.SECOND_ORDER], [0, 2], [1, 0])
    model.addcone([ConeKind.NONNEGATIVE], [0, 2], [2, 1])
    assert model.getcone()[2] == [1, 0, 2, 1]
    assert model.getcon()[0] == [1]
    assert model.getnfun() == 5


def test_views(env, model):
    f = model.addfun(3)
    c = model.addcone([ConeKind.SECOND_ORDER], [0, 3], f)
    model.addcone([ConeKind.NONNEGATIVE], [0, 2], [f[2], 0])
    assert model.getcone()[2] == [5, 6, 7, 7, 0]
    assert model.getcone([c[0]]) == (
        [ConeKind.SECOND_ORDER],
        [0, 3],
        [5, 6, 7],
    )
    other = biyos.Model(env)
    g = other.addfun(2)
    d = other.addcone([ConeKind.NONNEGATIVE], [0, 2], g)
    x = model.addvar(1)
    entry = "an entry of funidx is a function of another model"
    with pytest.raises(ValueError, match=entry):
        model.addcone([ConeKind.NONNEGATIVE], [0, 2], [0, g[1]])
    with pytest.raises(ValueError, match="funidx is a block of another"):
        model.addcone([ConeKind.NONNEGATIVE], [0, 2], g)
    with pytest.raises(ValueError, match="is a variable, not a function"):
        model.addcone([ConeKind.NONNEGATIVE], [0, 1], [x[0]])
    with pytest.raises(ValueError, match="is a cone of another model"):
        model.getcone([d[0]])
    with pytest.raises(ValueError, match="idx is a block of another model"):
        model.getconeprm(d)
    with pytest.raises(ValueError, match="is a function, not a cone"):
        model.getcone([f[0]])
    with pytest.raises(ValueError, match="of functions, not cones"):
        model.getnconefun(f)
    assert model.getncone() == 2
    other.close()


def test_gone(model):
    c = add_sample(model)
    view = c[0]
    model._gen += 1
    with pytest.raises(ValueError, match="the cone predates a deletion"):
        index(view)
    with pytest.raises(ValueError, match="the cone predates a deletion"):
        model.getcone([view])
    with pytest.raises(ValueError, match="the block predates a deletion"):
        model.getconeprm(c)
    assert model.getcone() == SAMPLE


def kept(found, function, *arguments):
    found.append(arguments)
    return function(*arguments)


def test_values(model, monkeypatch):
    weights = array.array("d", PRM)
    found = []
    spy = partial(kept, found, lib.BIYOS_addcone)
    monkeypatch.setattr(lib, "BIYOS_addcone", spy)
    model.addcone(KINDS, OFF, MEMBERS, PRMOFF, weights)
    assert ctypes.addressof(found[0][6]) == weights.buffer_info()[0]
    assert model.getconeprm() == WEIGHTS


def test_stale(model, monkeypatch):
    add_sample(model)
    monkeypatch.setattr(model, "getncone", lambda: 0)
    spy = Room(lib.BIYOS_getconerg, read_lengths)
    monkeypatch.setattr(lib, "BIYOS_getconerg", spy)
    assert model.getcone(range(2)) == SAMPLE
    assert model.getcone() == ([], [0], [])
    assert model.getconeprm() == ([0], [])


def test_outside(model, monkeypatch):
    add_sample(model)
    limit(monkeypatch, _model, 1)
    message = refusal(model.getcone, range(2**31 - 1))
    assert message == "invalid argument: cone 2: unknown"
    message = refusal(model.getconeprm, range(1, 2**31 - 1))
    assert message == "invalid argument: cone 2: unknown"
    message = refusal(model.getcone, [0, 2])
    assert message == "invalid argument: cone 2: unknown"
    message = refusal(model.getnconeprm, [-1])
    assert message == "invalid argument: cone -1: unknown"


@pytest.mark.parametrize("arguments, fault", REFUSALS)
def test_refused(model, arguments, fault):
    add_sample(model)
    message = refusal(model.addcone, *arguments)
    assert message == f"invalid argument: {fault}"
    assert model.getcone() == SAMPLE
    assert model.getconeprm() == WEIGHTS


def test_offsets(model):
    with pytest.raises(ValueError, match="off begins at 1, not 0"):
        model.addcone(KINDS, [1, 3, 5], MEMBERS)
    with pytest.raises(ValueError, match="prmoff decreases from 2 to 1"):
        model.addcone(KINDS, OFF, MEMBERS, [0, 2, 1], PRM)
    message = refusal(model.addcone, KINDS, None, [])
    assert message == "invalid argument: argument 'off': is null"
    assert model.getncone() == 0


def test_lengths(model):
    with pytest.raises(ValueError, match="off has length 2, not 3"):
        model.addcone(KINDS, [0, 3], MEMBERS)
    with pytest.raises(ValueError, match="funidx has length 4, not 5"):
        model.addcone(KINDS, OFF, MEMBERS[:4])
    with pytest.raises(ValueError, match="prmoff has length 2, not 3"):
        model.addcone(KINDS, OFF, MEMBERS, [0, 2], PRM)
    with pytest.raises(ValueError, match="prm has length 1, not 2"):
        model.addcone(KINDS, OFF, MEMBERS, PRMOFF, [0.5])
    with pytest.raises(ValueError, match="prm has length 2, not 0"):
        model.addcone(KINDS, OFF, MEMBERS, None, PRM)
    assert model.getncone() == 0


def test_numbers(model):
    with pytest.raises(TypeError, match="an entry of knd must be an"):
        model.addcone([2.0], [0, 1], [0])
    with pytest.raises(TypeError, match="an entry of off must be an"):
        model.addcone([ConeKind.NONNEGATIVE], [0, 1.0], [0])
    with pytest.raises(TypeError, match="an entry of funidx must be an"):
        model.addcone([ConeKind.NONNEGATIVE], [0, 1], [0.0])
    with pytest.raises(TypeError, match="an entry of prm must be a number"):
        model.addcone([ConeKind.POWER], [0, 2], [0, 1], [0, 1], ["1"])
    with pytest.raises(OverflowError, match="idx: 4294967296 does not fit"):
        model.getcone(range(2**32, 2**32 + 1))
    assert model.getncone() == 0
    model.addcone((Number(1),), (0, Number(2)), array.array("i", [1, 0]))
    assert model.getcone() == ([ConeKind.NONNEGATIVE], [0, 2], [1, 0])


def test_locked(env, model):
    seen = []

    def change(event):
        try:
            model.addcone([ConeKind.NONNEGATIVE], [0, 1], [0])
        except biyos.Error as error:
            seen.append(str(error))
        seen.append(model.getcone())
        seen.append(model.getconeprm([1]))

    add_sample(model)
    env.addhook(change, EventKind.MESSAGE, EventLevel.ERROR)
    with pytest.raises(biyos.Error):
        env.setopt("seed", -1)
    assert seen == [LOCKED, SAMPLE, ([0, 2], PRM)]
    assert model.getncone() == 2


def test_closed(env):
    model = biyos.Model(env)
    model.close()
    calls = (
        partial(model.addcone, [], [0], []),
        model.getncone,
        model.getnconefun,
        model.getcone,
        model.getnconeprm,
        model.getconeprm,
    )
    for call in calls:
        with pytest.raises(ValueError, match="the model is closed"):
            call()


def test_on(env, model):
    c = add_sample(model)
    copy = model.copy()
    assert copy.getcone() == SAMPLE
    assert copy.getconeprm() == WEIGHTS
    d = c[1:].on(copy)
    assert type(d) is biyos.Cones
    assert copy.getcone(d) == ([KINDS[1]], [0, 2], [1, 3])
    copy.addcone([ConeKind.NONNEGATIVE], [0, 1], [0])
    assert model.getncone() == 2
    smaller = biyos.Model(env)
    smaller.addfun(1)
    smaller.addcone([ConeKind.NONNEGATIVE], [0, 1], [0])
    few = "mdl holds too few cones: 1, the block needs 2"
    with pytest.raises(ValueError, match=few):
        c.on(smaller)
    assert len(c[:1].on(smaller)) == 1
    copy.close()
    smaller.close()


@pytest.mark.parametrize("check", CHECKS)
def test_tolerances(env, model, check):
    add_sample(model)
    model.addcone([ConeKind.NONNEGATIVE], [0, 1], [0])
    check(env, model, "cone", "cone")
