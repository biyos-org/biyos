import gc
from copy import copy as shallow
from copy import deepcopy
from functools import partial

import pytest

import biyos
from biyos import EventKind, EventLevel
from biyos._capi import OK, lib
from spies import followed, interrupted, preceded, spied

REFUSED = -100

CALLS = [
    "BIYOS_getoptver",
    "BIYOS_addhook",
    "BIYOS_delhook",
    "BIYOS_getnvars",
]


@pytest.fixture
def log(monkeypatch):
    log = []
    for name in ("mdl", "env"):
        entry = f"BIYOS_free{name}"
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
    raise RuntimeError("hook failed")


def stranded(env, monkeypatch):
    model = biyos.Model(env)
    with monkeypatch.context() as patch:
        patch.setattr(lib, "BIYOS_freemdl", halted)
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
    with biyos.Env() as env:
        assert env.getopt("seed") == 0
    with pytest.raises(ValueError, match="the environment is closed"):
        env.getopt("seed")


def test_close():
    env = biyos.Env()
    env.close()
    env.close()
    with pytest.raises(ValueError, match="the environment is closed"):
        env.getopt("seed")


def test_copied():
    with biyos.Env() as env:
        for copier in (shallow, deepcopy):
            with pytest.raises(TypeError, match="cannot be copied"):
                copier(env)
        assert env.getopt("seed") == 0


def test_collected():
    env = biyos.Env()
    free = env._free
    del env
    gc.collect()
    assert not free.alive


def test_deferred(log):
    def close_inside(event):
        env.close()

    env = biyos.Env()
    model = biyos.Model(env)
    env.addhook(close_inside, EventKind.MESSAGE, EventLevel.ERROR)
    with pytest.raises(biyos.Error) as caught:
        env.setopt("seed", -1)
    assert str(caught.value).startswith("invalid argument: option 'seed'")
    assert log == ["call", "mdl", "env"]
    with pytest.raises(ValueError, match="the environment is closed"):
        env.getopt("seed")
    with pytest.raises(ValueError, match="the model is closed"):
        model.getnvars()


def test_gap(log, monkeypatch):
    env = biyos.Env()
    model = biyos.Model(env)
    before_entry("BIYOS_getoptver", [env.close], monkeypatch)
    assert env.getoptver("seed").since == "0.1.0"
    assert log == ["call", "mdl", "env"]
    with pytest.raises(ValueError, match="the model is closed"):
        model.getnvars()


def test_model_gap(log, monkeypatch):
    env = biyos.Env()
    model = biyos.Model(env)
    before_start(env, [env.close], monkeypatch)
    assert model.getnvars() == 0
    assert log == ["call", "mdl", "env"]
    with pytest.raises(ValueError, match="the model is closed"):
        model.getnvars()
    env.close()
    model.close()
    assert log == ["call", "mdl", "env"]


def test_nested(log, monkeypatch):
    env = biyos.Env()
    model = biyos.Model(env)
    before_start(env, [model.getnvars, env.close], monkeypatch)
    assert model.getnvars() == 0
    assert log == ["call", "call", "mdl", "env"]


def test_interrupted(log, monkeypatch):
    env = biyos.Env()
    model = biyos.Model(env)
    count = partial(interrupted, lib.BIYOS_getnvars)
    monkeypatch.setattr(lib, "BIYOS_getnvars", count)
    before_start(env, [env.close], monkeypatch)
    with pytest.raises(KeyboardInterrupt):
        model.getnvars()
    assert log == ["call", "mdl", "env"]


def test_closing(log, monkeypatch):
    env = biyos.Env()
    model = biyos.Model(env)
    models = env._models
    take = partial(preceded, [env.close], models.take)
    monkeypatch.setattr(models, "take", take)
    model.close()
    assert log == ["mdl", "env"]


def test_created(log, monkeypatch):
    env = biyos.Env()
    create = partial(followed, [env.close], lib.BIYOS_newmdl)
    monkeypatch.setattr(lib, "BIYOS_newmdl", create)
    model = biyos.Model(env)
    assert log == ["mdl", "env"]
    with pytest.raises(ValueError, match="the model is closed"):
        model.getnvars()


def test_reentered(log, monkeypatch):
    env = biyos.Env()
    model = biyos.Model(env)
    before_entry("BIYOS_freeenv", [env.close], monkeypatch)
    env.close()
    assert log == ["mdl", "env"]
    model.close()
    env.close()
    assert log == ["mdl", "env"]


def test_held(log):
    env = biyos.Env()
    model = biyos.Model(env)
    with env._busy():
        env.close()
        with pytest.raises(ValueError, match="the environment is closed"):
            env.getopt("seed")
        assert log == []
    assert log == ["mdl", "env"]
    env.close()
    model.close()
    assert log == ["mdl", "env"]


def test_draining(monkeypatch):
    statuses = []
    env_free = partial(spied, lib.BIYOS_freeenv, statuses)
    monkeypatch.setattr(lib, "BIYOS_freeenv", env_free)
    env = biyos.Env()
    model = biyos.Model(env)
    before_start(env, [model.close], monkeypatch)
    models = env._models
    take = partial(followed, [env.close], models.take)
    monkeypatch.setattr(models, "take", take)
    assert model.getnvars() == 0
    assert statuses == [OK]
    assert not env._free.alive


def test_refused(monkeypatch):
    env = biyos.Env()
    handle = stranded(env, monkeypatch)
    with pytest.raises(biyos.Error, match="has a model that is not freed"):
        env.close()
    assert env._free.alive
    lib.BIYOS_freemdl(handle)
    env.close()
    assert not env._free.alive


def test_deferred_refusal(monkeypatch):
    statuses = []
    env_free = partial(spied, lib.BIYOS_freeenv, statuses)
    monkeypatch.setattr(lib, "BIYOS_freeenv", env_free)
    env = biyos.Env()
    handle = stranded(env, monkeypatch)
    with env._busy():
        env.close()
    with pytest.raises(ValueError, match="the environment is closed"):
        env.getopt("seed")
    assert statuses == [REFUSED]
    assert env._free.alive
    lib.BIYOS_freemdl(handle)
    env.close()
    assert statuses == [REFUSED, OK]


def test_explicit_retry(monkeypatch):
    statuses = []
    env_free = partial(spied, lib.BIYOS_freeenv, statuses)
    monkeypatch.setattr(lib, "BIYOS_freeenv", env_free)
    events = []
    env = biyos.Env()
    env.addhook(events.append, EventKind.MESSAGE, EventLevel.ERROR)
    other = biyos.Model(env)
    handle = stranded(env, monkeypatch)
    with pytest.raises(biyos.Error, match="has a model that is not freed"):
        env.close()
    other.close()
    with pytest.raises(ValueError, match="the environment is closed"):
        env.getopt("seed")
    assert statuses == [REFUSED]
    with pytest.raises(biyos.Error, match="has a model that is not freed"):
        env.close()
    assert statuses == [REFUSED, REFUSED]
    assert len(events) == 2
    lib.BIYOS_freemdl(handle)
    env.close()
    assert statuses == [REFUSED, REFUSED, OK]


def test_relayed(monkeypatch):
    env = biyos.Env()
    handle = stranded(env, monkeypatch)
    env.addhook(raising, EventKind.MESSAGE, EventLevel.ERROR)
    with pytest.raises(RuntimeError, match="hook failed"):
        env.close()
    assert env._relay._raised is None
    assert env._free.alive
    lib.BIYOS_freemdl(handle)
    env.close()
    assert not env._free.alive


def test_hooking(log, monkeypatch):
    env = biyos.Env()
    before_entry("BIYOS_addhook", [env.close], monkeypatch)
    with pytest.raises(biyos.Error, match="event kind 99: unknown"):
        env.addhook(print, 99, EventLevel.ERROR)
    assert log == ["call", "env"]


def test_unhooking(log, monkeypatch):
    env = biyos.Env()
    idx = env.addhook(print, EventKind.MESSAGE, EventLevel.ERROR)
    before_entry("BIYOS_delhook", [env.close], monkeypatch)
    env.delhook(idx)
    assert log == ["call", "call", "env"]


def test_closed_hook():
    env = biyos.Env()
    env.close()
    with pytest.raises(ValueError, match="the environment is closed"):
        env.addhook(print, EventKind.MESSAGE, EventLevel.ERROR)
