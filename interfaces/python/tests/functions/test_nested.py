import array
from functools import partial

import pytest

import biyos
from biyos._capi import lib
from listed import indexed

LINEAR = biyos.Model.addlinterm
COLUMN = biyos.Model.addclinterm
QUADRATIC = biyos.Model.addquadterm
FUN = [0, 7, 9]
VAR = [[1, 2, 3], [0, 4, 5, 6, 8], [2, 3]]
COEF = [[1.0, 2.0, 3.0], [4.0, 5.0, 6.0, 7.0, 8.0], [9.0, -1.0]]
SECONDS = [[1, 2, 4], [0, 4, 3, 6, 8], [2, 3]]


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
    return indexed(model.getrlinterm()), indexed(model.getquadterm())


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


def test_column(env):
    var = [4, 5]
    fun = [[0], [1, 2]]
    coef = [[9.0], [8.0, 7.0]]
    by_variable = built(env, COLUMN, var, fun, coef)
    flat = joined(fun), repeated(var, fun), joined(coef)
    assert by_variable == built(env, LINEAR, *flat)


def test_quadratic(env):
    nested = built(env, QUADRATIC, FUN, VAR, SECONDS, COEF)
    flat = repeated(FUN, VAR), joined(VAR), joined(SECONDS), joined(COEF)
    assert nested == built(env, QUADRATIC, *flat)


def test_blocks(model):
    x = model.addvar(3)
    f = model.addfun(2)
    model.addlinterm(f, [x[:2], x[2:]], [array.array("d", [1, 2]), (3,)])
    assert indexed(model.getrlinterm(f)) == (
        [10, 10, 11],
        [10, 11, 12],
        [1, 2, 3],
    )


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


def test_column_calls(model, monkeypatch):
    names = ("BIYOS_addclintermsp", "BIYOS_addclintermrg")
    seen = spy(monkeypatch, *names)
    x = model.addvar(2)
    model.addclinterm([0, 2], [[0], [1]], [[1.0], [2.0]])
    model.addclinterm(x, [[0], [1]], [[1.0], [2.0]])
    model.addclinterm(range(3), [[0], [1], []], [[1.0], [2.0], []])
    model.addclinterm(x[1], 2, 1.0)
    assert seen == [names[0], names[1], names[1], names[0]]


def test_quadratic_calls(model, monkeypatch):
    names = ("BIYOS_addquadterm", "BIYOS_addquadtermsp", "BIYOS_addquadtermrg")
    seen = spy(monkeypatch, *names)
    model.addquadterm([0, 2], [[0], [1]], [[0], [1]], [[1.0], [2.0]])
    model.addquadterm(range(2), [[0], [1]], [[0], [1]], [[1.0], [2.0]])
    model.addquadterm([0], [1], [1], [1.0])
    model.addquadterm(0, 1, 1, 1.0)
    assert seen == [names[1], names[2], names[0], names[1]]


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
    with pytest.raises(ValueError, match=r"var2\[2\] has length 1, not 2"):
        model.addquadterm(FUN, VAR, [*SECONDS[:2], [2]], COEF)
    with pytest.raises(ValueError, match=r"coef\[0\] has length 2, not 1"):
        model.addclinterm([0], [[1]], [[1.0, 2.0]])
    with pytest.raises(TypeError, match=r"an entry of coef\[0\] must be a"):
        model.addlinterm([0], [[1]], [["1"]])
    assert held(model) == before


def test_named(model):
    x = model.addvar(1)
    f = model.addfun(1)
    with pytest.raises(TypeError, match="an entry of fun is a variable"):
        model.addlinterm([x[0]], [[0]], [[1.0]])
    with pytest.raises(TypeError, match="^fun is a variable"):
        model.addquadterm(x[0], 0, 0, 1.0)
    with pytest.raises(TypeError, match="an entry of var is a function"):
        model.addclinterm([f[0]], [[0]], [[1.0]])
    with pytest.raises(TypeError, match=r"an entry of var\[0\] is a func"):
        model.addlinterm([0], [[f[0]]], [[1.0]])
    with pytest.raises(TypeError, match=r"an entry of fun\[0\] is a vari"):
        model.addclinterm([0], [[x[0]]], [[1.0]])


def test_read_back(env, model):
    model.addlinterm(FUN, VAR, COEF)
    model.addquadterm(FUN, VAR, SECONDS, COEF)
    by_function = built(env, LINEAR, *indexed(model.getrlinterm()))
    by_variable = built(env, LINEAR, *indexed(model.getclinterm()))
    quadratic = built(env, QUADRATIC, *indexed(model.getquadterm()))
    assert by_function[0] == held(model)[0]
    assert by_variable[0] == held(model)[0]
    assert quadratic[1] == held(model)[1]


@pytest.mark.parametrize(
    ("add", "scalar", "listed"),
    [
        (LINEAR, (7, [0, 4], [1.0, 2.0]), ([7], [[0, 4]], [[1.0, 2.0]])),
        (LINEAR, (7, 4, 2.0), ([7], [[4]], [[2.0]])),
        (COLUMN, (2, [0, 1], [1.0, 2.0]), ([2], [[0, 1]], [[1.0, 2.0]])),
        (COLUMN, (2, 1, 3.0), ([2], [[1]], [[3.0]])),
        (
            QUADRATIC,
            (0, [0, 1], [0, 2], [1.0, 2.0]),
            ([0], [[0, 1]], [[0, 2]], [[1.0, 2.0]]),
        ),
        (QUADRATIC, (0, 1, 2, 3.0), ([0], [[1]], [[2]], [[3.0]])),
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
    model.addclinterm(x[0], [f[0]], [3.0])
    assert indexed(model.getrlinterm(f)) == ([10], [10], [5.0])


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
        (QUADRATIC, ([0], [0], 1, [1.0]), "var2 must be a sequence"),
        (QUADRATIC, ([0], [[0]], 1, [[1.0]]), "var2 must be a sequence"),
        (QUADRATIC, (0, 0, [1], 1.0), "var2 must be a scalar"),
        (COLUMN, ([0, 1], 2, [1.0, 2.0]), "fun must be a sequence"),
        (COLUMN, ([0], [1], [1.0]), r"fun\[0\] must be a sequence"),
        (COLUMN, (0, [1], 2.0), "coef must be a sequence"),
    ],
)
def test_refused(model, add, arguments, message):
    before = held(model)
    with pytest.raises(TypeError, match=message):
        add(model, *arguments)
    assert held(model) == before
