import gc
from copy import copy as shallow
from copy import deepcopy
from functools import partial

import pytest

import biyos
from biyos import EventKind, EventLevel, Sense, VariableKind
from biyos._capi import lib
from spies import Trap, followed, interrupted, preceded, traced

INFINITY = float("inf")
BUSY = (
    "invalid argument: argument 'env':"
    " is read-only while delivering an event"
)
CLOSED = "the model is closed"
REFUSED = -100


@pytest.fixture
def env():
    with biyos.Env() as env:
        yield env


@pytest.fixture
def other():
    with biyos.Env() as other:
        yield other


@pytest.fixture
def model(env):
    with biyos.Model(env) as model:
        add_sample(model)
        yield model


def add_sample(model):
    kinds = [
        VariableKind.CONTINUOUS,
        VariableKind.BINARY,
        VariableKind.INTEGER,
    ]
    model.addvar(3, kinds, [-0.0, 0.0, -INFINITY], [2.5, 1.0, 3.0])
    f = model.addfun(2)
    model.setcst(f, [1.5, 0.1])
    model.addlinterm([0, 0, 1], [2, 0, 1], [-1.0, 2.0, 0.1])
    pairs = ([1, 0, 0], [2, 0, 1], [2, 1, 1], [0.5, -1.0, 3.0])
    model.addquadterm(*pairs)
    model.addcon([0, 1, 0], [-1.0, 0.0, 3.0], [1.0, INFINITY, 3.0])
    senses = [Sense.MAXIMIZE, Sense.MINIMIZE]
    tolerances = ([0.25, 0.0], [0.0, INFINITY])
    o = model.addobj([1, 0], senses, [3, -2], [0.5, 2.0])
    model.setobjtol(o, *tolerances)
    model.addind([1], [1], [0], [0.0], [2.0])


def read(model):
    found = (
        model.getvar(),
        model.getcst(range(model.getnfun())),
        model.getrlinterm(),
        model.getquadterm(),
        model.getcon(),
        model.getobj(),
        model.getobjtol(range(model.getnobj())),
        model.getind(),
    )
    return repr(found)


def freed(log, function, handle):
    status = function(handle)
    log.append(getattr(handle, "value", handle))
    return status


def logged(log, name, function, *arguments):
    log.append(name)
    return function(*arguments)


def reached(log, function, *arguments):
    log.append("copy")
    return REFUSED if "env" in log else function(*arguments)


def before_copy(log, actions, monkeypatch):
    env_free = partial(logged, log, "env", lib.BIYOS_freeenv)
    monkeypatch.setattr(lib, "BIYOS_freeenv", env_free)
    copy = partial(reached, log, lib.BIYOS_copymdl)
    spy = partial(preceded, actions, copy)
    monkeypatch.setattr(lib, "BIYOS_copymdl", spy)


@pytest.fixture(params=["env", "other"])
def place(request):
    return request.getfixturevalue(request.param)


def test_same(env, model):
    copy = model.copy()
    assert copy._env is env
    assert read(copy) == read(model)
    assert copy._handle.value != model._handle.value
    assert id(copy._handle) in env._models._live
    copy.close()


def test_other(env, other, model):
    copy = model.copy(other)
    assert copy._env is other
    assert read(copy) == read(model)
    assert id(copy._handle) in other._models._live
    assert id(copy._handle) not in env._models._live
    copy.close()


def test_apart(place, model):
    before = read(model)
    copy = model.copy(place)
    copy.addvar(1)
    copy.setvarbnd([0], [1.0])
    copy.addcon([1])
    copy.addquadterm([0], [0], [0], [1.0])
    copy.setobjsense([1], [Sense.MAXIMIZE])
    assert read(model) == before
    assert copy.getnvar() == 4
    model.addfun(1)
    assert copy.getnfun() == 2
    copy.close()


def test_source_closed(place, model):
    before = read(model)
    copy = model.copy(place)
    model.close()
    assert read(copy) == before
    copy.addvar(1)
    assert copy.getnvar() == 4
    copy.close()


def test_copy_closed(place, model):
    before = read(model)
    copy = model.copy(place)
    copy.close()
    with pytest.raises(ValueError, match=CLOSED):
        copy.getnvar()
    assert read(model) == before


def test_envs(env, model):
    first = biyos.Env()
    second = biyos.Env()
    copy = model.copy(first)
    kept = model.copy(second)
    env.close()
    with pytest.raises(ValueError, match=CLOSED):
        model.getnvar()
    first.close()
    with pytest.raises(ValueError, match=CLOSED):
        copy.getnvar()
    assert kept.getnvar() == 3
    second.close()
    assert not second._free.alive


def test_collected(env, other, model, monkeypatch):
    log = []
    spy = partial(freed, log, lib.BIYOS_freemdl)
    monkeypatch.setattr(lib, "BIYOS_freemdl", spy)
    copy = model.copy(other)
    handle = copy._handle.value
    del copy
    gc.collect()
    env.getopt("seed")
    assert log == []
    other.getopt("seed")
    assert log == [handle]


def test_interrupted(other, model, monkeypatch):
    log = []
    interrupt = partial(interrupted, lib.BIYOS_copymdl)
    monkeypatch.setattr(lib, "BIYOS_copymdl", interrupt)
    spy = partial(freed, log, lib.BIYOS_freemdl)
    monkeypatch.setattr(lib, "BIYOS_freemdl", spy)
    with pytest.raises(KeyboardInterrupt):
        model.copy(other)
    [handle] = other._models._live.values()
    other.close()
    assert log == [handle.value]
    assert not other._free.alive


def test_interrupted_make(other, model, monkeypatch):
    log = []
    trap = Trap(biyos.Model._make)
    copy = partial(followed, [trap.arm], lib.BIYOS_copymdl)
    monkeypatch.setattr(lib, "BIYOS_copymdl", copy)
    spy = partial(freed, log, lib.BIYOS_freemdl)
    monkeypatch.setattr(lib, "BIYOS_freemdl", spy)
    with traced(trap), pytest.raises(KeyboardInterrupt):
        model.copy(other)
    [handle] = other._models._live.values()
    other.close()
    assert log == [handle.value]
    assert not other._free.alive


def test_closed(other, model, monkeypatch):
    calls = []
    monkeypatch.setattr(lib, "BIYOS_copymdl", calls.append)
    other.close()
    with pytest.raises(ValueError, match="the environment is closed"):
        model.copy(other)
    model.close()
    with pytest.raises(ValueError, match=CLOSED):
        model.copy()
    assert calls == []


def test_closed_env(env, model):
    env.close()
    with pytest.raises(ValueError, match=CLOSED):
        model.copy()


def test_module(model):
    for copier in (shallow, deepcopy):
        with pytest.raises(TypeError, match="copy a model with Model.copy"):
            copier(model)
    assert model.getnvar() == 3


def test_wrong_env(model):
    with pytest.raises(TypeError, match="env must be an Env, not int"):
        model.copy(1)


def test_locked(env, other, model):
    outside = biyos.Model(other)
    seen = []

    def copy_inside(event):
        for source, place in ((model, env), (outside, env)):
            try:
                source.copy(place)
            except biyos.Error as error:
                seen.append(str(error))
        for source, place in ((model, other), (outside, other)):
            copy = source.copy(place)
            seen.append(copy.getnvar())
            copy.close()

    env.addhook(copy_inside, EventKind.MESSAGE, EventLevel.ERROR)
    with pytest.raises(biyos.Error):
        env.setopt("seed", -1)
    assert seen == [BUSY, BUSY, 3, 0]
    assert len(env._models._live) == 1
    assert len(other._models._live) == 1
    outside.close()


def test_gap(model, monkeypatch):
    log = []
    other = biyos.Env()
    before_copy(log, [other.close], monkeypatch)
    copy = model.copy(other)
    assert log == ["copy", "env"]
    assert not other._free.alive
    with pytest.raises(ValueError, match=CLOSED):
        copy.getnvar()
    assert model.getnvar() == 3


def test_source_gap(other, monkeypatch):
    log = []
    env = biyos.Env()
    model = biyos.Model(env)
    add_sample(model)
    before_copy(log, [env.close], monkeypatch)
    copy = model.copy(other)
    assert log == ["copy", "env"]
    with pytest.raises(ValueError, match=CLOSED):
        model.getnvar()
    assert copy.getnvar() == 3
