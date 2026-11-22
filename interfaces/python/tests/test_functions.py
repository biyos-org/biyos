from functools import partial
from operator import index

import pytest

import biyos
from biyos import EventKind, EventLevel
from refusal import LOCKED, refusal


@pytest.fixture
def env():
    with biyos.Env() as env:
        yield env


@pytest.fixture
def model(env):
    with biyos.Model(env) as model:
        model.addvar(3)
        yield model


def test_block(model):
    model.addfun(2)
    f = model.addfun(3)
    assert len(f) == 3
    assert all(type(function) is biyos.Function for function in f)
    assert type(f[1:]) is biyos.Functions
    assert [index(function) for function in f[1:]] == [3, 4]
    assert index(f[-1]) == 4
    assert len(model.addfun(0)) == 0


def test_empty(model):
    model.addfun(2)
    f = model.addfun(3)
    assert type(f) is biyos.Functions
    assert [index(function) for function in f] == [2, 3, 4]
    assert len(model.addfun(0)) == 0
    assert model.getnfun() == 5
    assert model.getcst(range(5)) == [0.0] * 5
    assert model.getnrlinterm() == 0
    assert model.getnquadterm() == 0
    message = refusal(model.addfun, -1)
    assert message == "invalid argument: argument 'n': -1 is negative"
    with pytest.raises(TypeError, match="n must be an int, not float"):
        model.addfun(1.0)
    with pytest.raises(OverflowError, match="n: 4294967296 does not fit"):
        model.addfun(2**32)
    assert model.getnfun() == 5


def test_gone(model):
    f = model.addfun(2)
    view = f[0]
    model._gen += 1
    with pytest.raises(ValueError, match="the function predates a deletion"):
        index(view)
    with pytest.raises(ValueError, match="the block predates a deletion"):
        len(f)


def test_locked(env, model):
    seen = []

    def change(event):
        try:
            model.addfun(1)
        except biyos.Error as error:
            seen.append(str(error))
        seen.append(model.getnfun())

    model.addfun(3)
    env.addhook(change, EventKind.MESSAGE, EventLevel.ERROR)
    with pytest.raises(biyos.Error):
        env.setopt("seed", -1)
    assert seen == [LOCKED, 3]
    assert model.getnfun() == 3


def test_closed(env):
    model = biyos.Model(env)
    model.close()
    for call in (partial(model.addfun, 1), model.getnfun):
        with pytest.raises(ValueError, match="the model is closed"):
            call()


def test_on(env, model):
    f = model.addfun(3)
    copy = model.copy()
    g = f.on(copy)
    assert type(g) is biyos.Functions
    copy.addcon(g)
    assert copy.getcon()[0] == [0, 1, 2]
    with pytest.raises(ValueError, match="funidx is a block of another"):
        model.addcon(g)
    smaller = biyos.Model(env)
    smaller.addfun(2)
    few = "mdl holds too few functions: 2, the block needs 3"
    with pytest.raises(ValueError, match=few):
        f.on(smaller)
    assert [index(function) for function in f[:2].on(smaller)] == [0, 1]
    copy.close()
