import gc
from copy import copy as shallow
from copy import deepcopy
from functools import partial

import pytest

import biyos
from biyos import EventKind, Level, VariableKind
from biyos._capi import lib
from spies import interrupted, preceded

INFINITY = float("inf")
BUSY = (
    "invalid argument: argument 'env':"
    " is read-only while delivering an event"
)
CLOSED = "the model is closed"
REFUSED = -100


@pytest.fixture
def env():
    with biyos.Environment() as env:
        yield env


@pytest.fixture
def other():
    with biyos.Environment() as other:
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
    model.add_variables(3, kinds, [-0.0, 0.0, -INFINITY], [2.5, 1.0, 3.0])
    model.add_functions(2, [1.5, 0.1], [0, 2, 3], [2, 0, 1], [-1.0, 2.0, 0.1])
    model.add_constraints([0, 1, 0], [-1.0, 0.0, 3.0], [1.0, INFINITY, 3.0])


def read(model):
    found = (
        model.get_variables(),
        model.get_functions(),
        model.get_constraints(),
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
    env_free = partial(logged, log, "env", lib.biyos_env_free)
    monkeypatch.setattr(lib, "biyos_env_free", env_free)
    copy = partial(reached, log, lib.biyos_model_copy)
    spy = partial(preceded, actions, copy)
    monkeypatch.setattr(lib, "biyos_model_copy", spy)


@pytest.fixture(params=["env", "other"])
def place(request):
    return request.getfixturevalue(request.param)


def test_same(env, model):
    copy = model.copy()
    assert copy._env is env
    assert read(copy) == read(model)
    assert copy._handle.value != model._handle.value
    assert copy._handle.value in env._models._live
    copy.close()


def test_other(env, other, model):
    copy = model.copy(other)
    assert copy._env is other
    assert read(copy) == read(model)
    assert copy._handle.value in other._models._live
    assert copy._handle.value not in env._models._live
    copy.close()


def test_apart(place, model):
    before = read(model)
    copy = model.copy(place)
    copy.add_variables(1)
    copy.set_variable_bounds([0], [1.0])
    copy.add_constraints([1])
    assert read(model) == before
    assert copy.variable_count() == 4
    model.add_functions(1)
    assert copy.function_count() == 2
    copy.close()


def test_source_closed(place, model):
    before = read(model)
    copy = model.copy(place)
    model.close()
    assert read(copy) == before
    copy.add_variables(1)
    assert copy.variable_count() == 4
    copy.close()


def test_copy_closed(place, model):
    before = read(model)
    copy = model.copy(place)
    copy.close()
    with pytest.raises(ValueError, match=CLOSED):
        copy.variable_count()
    assert read(model) == before


def test_envs(env, model):
    first = biyos.Environment()
    second = biyos.Environment()
    copy = model.copy(first)
    kept = model.copy(second)
    env.close()
    with pytest.raises(ValueError, match=CLOSED):
        model.variable_count()
    first.close()
    with pytest.raises(ValueError, match=CLOSED):
        copy.variable_count()
    assert kept.variable_count() == 3
    second.close()
    assert not second._free.alive


def test_collected(env, other, model, monkeypatch):
    log = []
    spy = partial(freed, log, lib.biyos_model_free)
    monkeypatch.setattr(lib, "biyos_model_free", spy)
    copy = model.copy(other)
    handle = copy._handle.value
    del copy
    gc.collect()
    env.get_option("seed")
    assert log == []
    other.get_option("seed")
    assert log == [handle]


def test_interrupted(other, model, monkeypatch):
    log = []
    interrupt = partial(interrupted, lib.biyos_model_copy)
    monkeypatch.setattr(lib, "biyos_model_copy", interrupt)
    spy = partial(freed, log, lib.biyos_model_free)
    monkeypatch.setattr(lib, "biyos_model_free", spy)
    with pytest.raises(KeyboardInterrupt):
        model.copy(other)
    [handle] = other._models._live
    other.close()
    assert log == [handle]
    assert not other._free.alive


def test_closed(other, model, monkeypatch):
    calls = []
    monkeypatch.setattr(lib, "biyos_model_copy", calls.append)
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
    assert model.variable_count() == 3


def test_wrong_env(model):
    with pytest.raises(TypeError, match="env must be an Environment, not int"):
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
            seen.append(copy.variable_count())
            copy.close()

    env.subscribe(copy_inside, EventKind.MESSAGE, Level.ERROR)
    with pytest.raises(biyos.Error):
        env.set_option("seed", -1)
    assert seen == [BUSY, BUSY, 3, 0]
    assert len(env._models._live) == 1
    assert len(other._models._live) == 1
    outside.close()


def test_gap(model, monkeypatch):
    log = []
    other = biyos.Environment()
    before_copy(log, [other.close], monkeypatch)
    copy = model.copy(other)
    assert log == ["copy", "env"]
    assert not other._free.alive
    with pytest.raises(ValueError, match=CLOSED):
        copy.variable_count()
    assert model.variable_count() == 3


def test_source_gap(other, monkeypatch):
    log = []
    env = biyos.Environment()
    model = biyos.Model(env)
    add_sample(model)
    before_copy(log, [env.close], monkeypatch)
    copy = model.copy(other)
    assert log == ["copy", "env"]
    with pytest.raises(ValueError, match=CLOSED):
        model.variable_count()
    assert copy.variable_count() == 3
