import gc
from copy import copy as shallow
from copy import deepcopy
from functools import partial

import pytest

import biyos
from biyos import EventKind, Level
from biyos._capi import OK, lib
from spies import followed, interrupted, preceded, spied

REFUSED = -100

CALLS = [
    "biyos_option_versions",
    "biyos_subscribe",
    "biyos_unsubscribe",
    "biyos_variable_count",
]


@pytest.fixture
def log(monkeypatch):
    log = []
    for name in ("model", "env"):
        entry = f"biyos_{name}_free"
        spy = partial(freed, log, name, getattr(lib, entry))
        monkeypatch.setattr(lib, entry, spy)
    for entry in CALLS:
        spy = partial(reached, log, getattr(lib, entry))
        monkeypatch.setattr(lib, entry, spy)
    return log


def freed(log, name, function, handle):
    seen = name in log
    log.append(name)
    return OK if seen else function(handle)


def reached(log, function, *arguments):
    log.append("call")
    return OK if "env" in log else function(*arguments)


def halted(*arguments):
    raise KeyboardInterrupt


def raising(event):
    raise RuntimeError("subscriber failed")


def stranded(env, monkeypatch):
    model = biyos.Model(env)
    with monkeypatch.context() as patch:
        patch.setattr(lib, "biyos_model_free", halted)
        with pytest.raises(KeyboardInterrupt):
            model.close()
    return model._handle.value


def before_entry(entry, actions, monkeypatch):
    spy = partial(preceded, actions, getattr(lib, entry))
    monkeypatch.setattr(lib, entry, spy)


def before_start(env, actions, monkeypatch):
    start = partial(preceded, actions, env._relay.start)
    monkeypatch.setattr(env._relay, "start", start)


def test_context():
    with biyos.Environment() as env:
        assert env.get_option("seed") == 0
    with pytest.raises(ValueError, match="the environment is closed"):
        env.get_option("seed")


def test_close():
    env = biyos.Environment()
    env.close()
    env.close()
    with pytest.raises(ValueError, match="the environment is closed"):
        env.get_option("seed")


def test_copied():
    with biyos.Environment() as env:
        for copier in (shallow, deepcopy):
            with pytest.raises(TypeError, match="cannot be copied"):
                copier(env)
        assert env.get_option("seed") == 0


def test_collected():
    env = biyos.Environment()
    free = env._free
    del env
    gc.collect()
    assert not free.alive


def test_deferred(log):
    def close_inside(event):
        env.close()

    env = biyos.Environment()
    model = biyos.Model(env)
    env.subscribe(close_inside, EventKind.MESSAGE, Level.ERROR)
    with pytest.raises(biyos.Error) as caught:
        env.set_option("seed", -1)
    assert str(caught.value).startswith("invalid argument: option 'seed'")
    assert log == ["call", "model", "env"]
    with pytest.raises(ValueError, match="the environment is closed"):
        env.get_option("seed")
    with pytest.raises(ValueError, match="the model is closed"):
        model.variable_count()


def test_gap(log, monkeypatch):
    env = biyos.Environment()
    model = biyos.Model(env)
    before_entry("biyos_option_versions", [env.close], monkeypatch)
    assert env.option_versions("seed").since == "0.1.0"
    assert log == ["call", "model", "env"]
    with pytest.raises(ValueError, match="the model is closed"):
        model.variable_count()


def test_model_gap(log, monkeypatch):
    env = biyos.Environment()
    model = biyos.Model(env)
    before_start(env, [env.close], monkeypatch)
    assert model.variable_count() == 0
    assert log == ["call", "model", "env"]
    with pytest.raises(ValueError, match="the model is closed"):
        model.variable_count()
    env.close()
    model.close()
    assert log == ["call", "model", "env"]


def test_nested(log, monkeypatch):
    env = biyos.Environment()
    model = biyos.Model(env)
    before_start(env, [model.variable_count, env.close], monkeypatch)
    assert model.variable_count() == 0
    assert log == ["call", "call", "model", "env"]


def test_interrupted(log, monkeypatch):
    env = biyos.Environment()
    model = biyos.Model(env)
    count = partial(interrupted, lib.biyos_variable_count)
    monkeypatch.setattr(lib, "biyos_variable_count", count)
    before_start(env, [env.close], monkeypatch)
    with pytest.raises(KeyboardInterrupt):
        model.variable_count()
    assert log == ["call", "model", "env"]


def test_closing(log, monkeypatch):
    env = biyos.Environment()
    model = biyos.Model(env)
    models = env._models
    take = partial(preceded, [env.close], models.take)
    monkeypatch.setattr(models, "take", take)
    model.close()
    assert log == ["model", "env"]


def test_created(log, monkeypatch):
    env = biyos.Environment()
    create = partial(followed, [env.close], lib.biyos_model_create)
    monkeypatch.setattr(lib, "biyos_model_create", create)
    model = biyos.Model(env)
    assert log == ["model", "env"]
    with pytest.raises(ValueError, match="the model is closed"):
        model.variable_count()


def test_reentered(log, monkeypatch):
    env = biyos.Environment()
    model = biyos.Model(env)
    before_entry("biyos_env_free", [env.close], monkeypatch)
    env.close()
    assert log == ["model", "env"]
    model.close()
    env.close()
    assert log == ["model", "env"]


def test_held(log):
    env = biyos.Environment()
    model = biyos.Model(env)
    with env._busy():
        env.close()
        with pytest.raises(ValueError, match="the environment is closed"):
            env.get_option("seed")
        assert log == []
    assert log == ["model", "env"]
    env.close()
    model.close()
    assert log == ["model", "env"]


def test_draining(monkeypatch):
    statuses = []
    env_free = partial(spied, lib.biyos_env_free, statuses)
    monkeypatch.setattr(lib, "biyos_env_free", env_free)
    env = biyos.Environment()
    model = biyos.Model(env)
    before_start(env, [model.close], monkeypatch)
    models = env._models
    take = partial(followed, [env.close], models.take)
    monkeypatch.setattr(models, "take", take)
    assert model.variable_count() == 0
    assert statuses == [OK]
    assert not env._free.alive


def test_refused(monkeypatch):
    env = biyos.Environment()
    handle = stranded(env, monkeypatch)
    with pytest.raises(biyos.Error, match="has a model that is not freed"):
        env.close()
    assert env._free.alive
    lib.biyos_model_free(handle)
    env.close()
    assert not env._free.alive


def test_deferred_refusal(monkeypatch):
    statuses = []
    env_free = partial(spied, lib.biyos_env_free, statuses)
    monkeypatch.setattr(lib, "biyos_env_free", env_free)
    env = biyos.Environment()
    handle = stranded(env, monkeypatch)
    with env._busy():
        env.close()
    with pytest.raises(ValueError, match="the environment is closed"):
        env.get_option("seed")
    assert statuses == [REFUSED]
    assert env._free.alive
    lib.biyos_model_free(handle)
    env.close()
    assert statuses == [REFUSED, OK]


def test_explicit_retry(monkeypatch):
    statuses = []
    env_free = partial(spied, lib.biyos_env_free, statuses)
    monkeypatch.setattr(lib, "biyos_env_free", env_free)
    events = []
    env = biyos.Environment()
    env.subscribe(events.append, EventKind.MESSAGE, Level.ERROR)
    other = biyos.Model(env)
    handle = stranded(env, monkeypatch)
    with pytest.raises(biyos.Error, match="has a model that is not freed"):
        env.close()
    other.close()
    with pytest.raises(ValueError, match="the environment is closed"):
        env.get_option("seed")
    assert statuses == [REFUSED]
    with pytest.raises(biyos.Error, match="has a model that is not freed"):
        env.close()
    assert statuses == [REFUSED, REFUSED]
    assert len(events) == 2
    lib.biyos_model_free(handle)
    env.close()
    assert statuses == [REFUSED, REFUSED, OK]


def test_relayed(monkeypatch):
    env = biyos.Environment()
    handle = stranded(env, monkeypatch)
    env.subscribe(raising, EventKind.MESSAGE, Level.ERROR)
    with pytest.raises(RuntimeError, match="subscriber failed"):
        env.close()
    assert env._relay._raised is None
    assert env._free.alive
    lib.biyos_model_free(handle)
    env.close()
    assert not env._free.alive


def test_subscribing(log, monkeypatch):
    env = biyos.Environment()
    before_entry("biyos_subscribe", [env.close], monkeypatch)
    with pytest.raises(biyos.Error, match="event kind 99: unknown"):
        env.subscribe(print, 99, Level.ERROR)
    assert log == ["call", "env"]


def test_unsubscribing(log, monkeypatch):
    env = biyos.Environment()
    subscription = env.subscribe(print, EventKind.MESSAGE, Level.ERROR)
    before_entry("biyos_unsubscribe", [env.close], monkeypatch)
    env.unsubscribe(subscription)
    assert log == ["call", "call", "env"]


def test_closed_subscribe():
    env = biyos.Environment()
    env.close()
    with pytest.raises(ValueError, match="the environment is closed"):
        env.subscribe(print, EventKind.MESSAGE, Level.ERROR)
