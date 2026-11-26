import array
from functools import partial

import pytest

import biyos
from biyos._capi import lib

LINEAR = biyos.Model.addlinterm
FUN = [0, 7, 9]
VAR = [[1, 2, 3], [0, 4, 5, 6, 8], [2, 3]]
COEF = [[1.0, 2.0, 3.0], [4.0, 5.0, 6.0, 7.0, 8.0], [9.0, -1.0]]


@pytest.fixture
def env():
    with biyos.Env() as env:
        yield env


@pytest.fixture
def model(env):
    with biyos.Model(env) as model:
        model.addvar(10)
        model.addfun(10)
        yield model


def held(model):
    return model.getrlinterm(), model.getquadterm()


def built(env, add, *arguments):
    with biyos.Model(env) as model:
        model.addvar(10)
        model.addfun(10)
        add(model, *arguments)
        return held(model)


def repeated(records, nested):
    return [record for record, entry in zip(records, nested) for _ in entry]


def joined(nested):
    return [value for entry in nested for value in entry]


def recorded(seen, name, function, *arguments):
    seen.append(name)
    return function(*arguments)


def spy(monkeypatch, *names):
    seen = []
    for name in names:
        function = partial(recorded, seen, name, getattr(lib, name))
        monkeypatch.setattr(lib, name, function)
    return seen


def test_linear(env):
    nested = built(env, LINEAR, FUN, VAR, COEF)
    flat = repeated(FUN, VAR), joined(VAR), joined(COEF)
    assert nested == built(env, LINEAR, *flat)
    assert nested[0][0] == repeated(FUN, VAR)


def test_twice(env):
    twice = built(env, LINEAR, [7, 7], [[0], [4]], [[1.0], [2.0]])
    assert twice == built(env, LINEAR, [7, 7], [0, 4], [1.0, 2.0])


def test_blocks(model):
    x = model.addvar(3)
    f = model.addfun(2)
    model.addlinterm(f, [x[:2], x[2:]], [array.array("d", [1, 2]), (3,)])
    assert model.getrlinterm(f) == ([10, 10, 11], [10, 11, 12], [1, 2, 3])


def test_set(env):
    unordered = built(env, LINEAR, [0, 0], {2, 3}, [1.0, 1.0])
    assert unordered == built(env, LINEAR, [0, 0], [2, 3], [1.0, 1.0])


def test_range(model):
    start = 2**31
    with pytest.raises(OverflowError, match="the start of fun"):
        model.addlinterm(range(start, start + 1), [[0]], [[1.0]])


def test_calls(model, monkeypatch):
    names = ("BIYOS_addlinterm", "BIYOS_addrlintermsp", "BIYOS_addrlintermrg")
    seen = spy(monkeypatch, *names)
    f = model.addfun(2)
    model.addlinterm([0, 2], [[0], [1]], [[1.0], [2.0]])
    model.addlinterm(range(1, 3), [[0], [1]], [[1.0], [2.0]])
    model.addlinterm(f, [[0], [1]], [[1.0], [2.0]])
    model.addlinterm(f[0], [0], [1.0])
    model.addlinterm([0], [1], [1.0])
    model.addlinterm(f, [0, 1], [1.0, 2.0])
    model.addlinterm([], [], [])
    assert seen == [names[k] for k in (1, 2, 2, 1, 0, 0, 0)]


def test_ragged(model):
    before = held(model)
    short = [COEF[0], COEF[1][:4], COEF[2]]
    with pytest.raises(ValueError, match=r"coef\[1\] has length 4, not 5"):
        model.addlinterm(FUN, VAR, short)
    with pytest.raises(ValueError, match="coef has length 2, not 3"):
        model.addlinterm(FUN, VAR, COEF[:2])
    with pytest.raises(TypeError, match=r"coef\[0\] must be a sequence"):
        model.addlinterm([0, 1], [[1], [2]], [1.0, 2.0])
    with pytest.raises(ValueError, match="var has length 2, not 3"):
        model.addlinterm(FUN, VAR[:2], COEF)
    with pytest.raises(TypeError, match=r"an entry of coef\[0\] must be a"):
        model.addlinterm([0], [[1]], [["1"]])
    assert held(model) == before


def test_named(model):
    x = model.addvar(1)
    f = model.addfun(1)
    with pytest.raises(TypeError, match="an entry of fun is a variable"):
        model.addlinterm([x[0]], [[0]], [[1.0]])
    with pytest.raises(TypeError, match=r"an entry of var\[0\] is a func"):
        model.addlinterm([0], [[f[0]]], [[1.0]])


def test_read_back(env, model):
    model.addlinterm(FUN, VAR, COEF)
    by_function = built(env, LINEAR, *model.getrlinterm())
    by_variable = built(env, LINEAR, *model.getclinterm())
    assert by_function[0] == held(model)[0]
    assert by_variable[0] == held(model)[0]


@pytest.mark.parametrize(
    ("add", "scalar", "listed"),
    [
        (LINEAR, (7, [0, 4], [1.0, 2.0]), ([7], [[0, 4]], [[1.0, 2.0]])),
        (LINEAR, (7, 4, 2.0), ([7], [[4]], [[2.0]])),
    ],
)
def test_scalar(env, add, scalar, listed):
    one = built(env, add, *scalar)
    assert one == built(env, add, *listed)
    assert one != built(env, LINEAR, [], [], [])


def test_single_term(env):
    one = built(env, LINEAR, 0, 1, 1.0)
    assert one[0] == ([0], [1], [1.0])
    assert one == built(env, LINEAR, [0], [1], [1.0])


def test_views(model):
    x = model.addvar(1)
    f = model.addfun(1)
    model.addlinterm(f[0], x[0], 2.0)
    assert model.getrlinterm(f) == ([10], [10], [2.0])


@pytest.mark.parametrize(
    ("add", "arguments", "message"),
    [
        (LINEAR, ([0, 1], 2, [1.0, 2.0]), "var must be a sequence"),
        (LINEAR, ([0, 1], [0, 1], 1.0), "coef must be a sequence"),
        (LINEAR, ([0], [[1]], 1.0), "coef must be a sequence"),
        (LINEAR, (0, [1], 1.0), "coef must be a sequence"),
        (LINEAR, (0, 1, [1.0]), "coef must be a scalar, not list"),
        (LINEAR, (None, 1, 1.0), "^fun must be an int, not NoneType"),
        (LINEAR, (True, 1, 1.0), "^fun must be an int, not bool"),
    ],
)
def test_refused(model, add, arguments, message):
    before = held(model)
    with pytest.raises(TypeError, match=message):
        add(model, *arguments)
    assert held(model) == before
