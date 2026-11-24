from functools import partial
from operator import index

import pytest

import biyos
from biyos import EventKind, EventLevel, VariableKind
from refusal import LOCKED, refusal

INFINITY = float("inf")
NAN = float("nan")
IN_USE = -203
BINARY = VariableKind.BINARY
CONTINUOUS = VariableKind.CONTINUOUS

VARIABLES = [0, 2, 0]
VALUES = [1, 0, 0]
FUNCTIONS = [1, 1, 0]
LOWER = [-INFINITY, 1.0, 0.0]
UPPER = [4.0, 1.0, INFINITY]
SAMPLE = (VARIABLES, VALUES, FUNCTIONS, LOWER, UPPER)


@pytest.fixture
def env():
    with biyos.Env() as env:
        yield env


@pytest.fixture
def model(env):
    with biyos.Model(env) as model:
        model.addvar(3, [BINARY, CONTINUOUS, BINARY])
        model.addfun(2)
        yield model


def add_sample(model):
    return model.addind(VARIABLES, VALUES, FUNCTIONS, LOWER, UPPER)


def test_add(model):
    added = add_sample(model)
    assert model.getnind() == 3
    assert model.getind() == SAMPLE
    assert type(added) is biyos.Indicators
    assert [index(indicator) for indicator in added] == [0, 1, 2]
    assert all(type(item) is biyos.Indicator for item in added)
    assert len(model.addind([], [], [])) == 0


def test_range(model):
    add_sample(model)
    found = model.getind(range(1, 3))
    assert found == tuple(column[1:] for column in SAMPLE)
    found = model.getind([2, 0, 2])
    assert found == tuple([c[2], c[0], c[2]] for c in SAMPLE)
    assert model.getind(range(3, 3)) == ([], [], [], [], [])


def test_defaults(model):
    model.addind([0, 2], [1, 0], [0, 1])
    model.addind([2], [1], [1], lb=[0.5])
    found = model.getind()
    assert found[3] == [-INFINITY, -INFINITY, 0.5]
    assert found[4] == [INFINITY] * 3


def test_shared(model):
    model.addcon([1], [0.0], [2.0])
    model.addind([0, 2], [1, 1], [1, 1], [3.0, -1.0], [3.0, 1.0])
    assert model.getcon() == ([1], [0.0], [2.0])
    assert model.getind()[2:] == ([1, 1], [3.0, -1.0], [3.0, 1.0])
    assert model.getnfun() == 2


def test_bounds(model):
    add_sample(model)
    model.setindbnd([1, 0], lb=[2.0, 3.0])
    model.setindbnd(range(2, 3), ub=[5.0])
    assert model.getindbnd(range(3)) == ([3.0, 2.0, 0.0], [4.0, 1.0, 5.0])
    assert model.getind()[:3] == (VARIABLES, VALUES, FUNCTIONS)


def test_switch(model):
    i = add_sample(model)
    model.setindvar([2, 1], [2, 0], [1, 1])
    model.setindvar(i[:1], val=[0])
    assert model.getindvar(i) == ([0, 0, 2], [0, 1, 1])
    assert model.getind()[2:] == (FUNCTIONS, LOWER, UPPER)


def test_views(env, model):
    i = add_sample(model)
    x = model.addvar(1, [BINARY])
    f = model.addfun(1)
    model.addind(x, [1], f)
    assert model.getind([3]) == ([3], [1], [2], [-INFINITY], [INFINITY])
    other = biyos.Model(env)
    y = other.addvar(1, [BINARY])
    g = other.addfun(1)
    j = other.addind(y, [1], g)
    with pytest.raises(ValueError, match="varidx is a variable of another"):
        model.addind([y[0]], [1], [0])
    with pytest.raises(ValueError, match="is a function, not a variable"):
        model.addind([f[0]], [1], [0])
    with pytest.raises(ValueError, match="of variables, not functions"):
        model.addind([0], [1], x)
    with pytest.raises(ValueError, match="is an indicator of another model"):
        model.setindbnd([j[0]], [0.0])
    with pytest.raises(ValueError, match="of functions, not indicators"):
        model.getindvar(f)
    assert model.getindvar(i[1:]) == ([2, 0], [0, 0])
    other.close()


def test_refused(model):
    add_sample(model)
    message = refusal(model.addind, [1], [1], [0])
    assert message == "invalid argument: indicator 3: variable 1 is not binary"
    message = refusal(model.addind, [0, 3], [1, 1], [0, 0])
    assert message == "invalid argument: indicator 4: variable 3 is unknown"
    message = refusal(model.addind, [0], [2], [0])
    assert message == (
        "invalid argument: indicator 3: value 2 is neither 0 nor 1"
    )
    message = refusal(model.addind, [0], [1], [2])
    assert message == "invalid argument: indicator 3: function 2 is unknown"
    message = refusal(model.addind, [0], [1], [0], [NAN])
    assert message == "invalid argument: indicator 3: lower bound is NaN"
    message = refusal(model.addind, [0], None, [0])
    assert message == "invalid argument: argument 'val': is null"
    message = refusal(model.setindbnd, [0], [INFINITY])
    assert message == "invalid argument: indicator 0: lower bound is infinity"
    message = refusal(model.setindvar, [1], [1])
    assert message == "invalid argument: indicator 1: variable 1 is not binary"
    message = refusal(model.getind, [3])
    assert message == "invalid argument: indicator 3: unknown"
    assert model.getind() == SAMPLE


def test_kind(model):
    add_sample(model)
    with pytest.raises(biyos.Error) as caught:
        model.setvarknd([1, 2], [VariableKind.INTEGER, CONTINUOUS])
    assert caught.value.code == IN_USE
    message = "in use: variable 2: indicator 1 needs it binary"
    assert str(caught.value) == message
    model.setvarknd([2, 1], [BINARY, VariableKind.INTEGER])
    assert model.getvarknd(range(3)) == [BINARY, VariableKind.INTEGER, BINARY]


def test_lengths(model):
    with pytest.raises(ValueError, match="val has length 1, not 2"):
        model.addind([0, 2], [1], [0, 0])
    with pytest.raises(ValueError, match="funidx has length 3, not 2"):
        model.addind([0, 2], [1, 1], [0, 0, 0])
    with pytest.raises(ValueError, match="ub has length 1, not 2"):
        model.setindbnd([0, 1], ub=[1.0])
    assert model.getnind() == 0


def test_locked(env, model):
    seen = []

    def change(event):
        for call in (
            partial(model.addind, [0], [1], [0]),
            partial(model.setindbnd, [0], [0.0]),
            partial(model.setindvar, [0], [2]),
        ):
            try:
                call()
            except biyos.Error as error:
                seen.append(str(error))
        seen.append(model.getind())

    add_sample(model)
    env.addhook(change, EventKind.MESSAGE, EventLevel.ERROR)
    with pytest.raises(biyos.Error):
        env.setopt("seed", -1)
    assert seen == [LOCKED, LOCKED, LOCKED, SAMPLE]
    assert model.getnind() == 3


def test_closed(env):
    model = biyos.Model(env)
    model.close()
    calls = (
        partial(model.addind, [0], [1], [0]),
        partial(model.setindbnd, []),
        partial(model.getindvar, []),
        model.getnind,
        model.getind,
    )
    for call in calls:
        with pytest.raises(ValueError, match="the model is closed"):
            call()


def test_copy(model):
    i = add_sample(model)
    copy = model.copy()
    assert copy.getind() == SAMPLE
    copy.setindbnd(i[1:].on(copy), [0.0, 0.0])
    assert copy.getindbnd(range(3))[0] == [-INFINITY, 0.0, 0.0]
    assert model.getind() == SAMPLE
    copy.close()
