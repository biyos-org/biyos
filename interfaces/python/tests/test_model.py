import array
import ctypes
import gc
from functools import partial

import pytest

import biyos
from biyos import EventKind, EventLevel, VariableKind, _model
from biyos._capi import OK, lib
from number import Growing, Number
from refusal import LOCKED, refusal
from sized import Room, limit
from spies import followed, interrupted, preceded, spied

INFINITY = float("inf")

KINDS = [
    VariableKind.CONTINUOUS,
    VariableKind.BINARY,
    VariableKind.INTEGER,
    VariableKind.SEMICONTINUOUS,
    VariableKind.SEMIINTEGER,
]

WRONG = [
    ([1.5], TypeError, "an entry of knd must be an int, not float"),
    (["1"], TypeError, "an entry of knd must be an int, not str"),
    ([True], TypeError, "an entry of knd must be an int, not bool"),
    ([2**40], OverflowError, "an entry of knd: 1099511627776 does not fit"),
]

REALS = [("0", "str"), (True, "bool"), (None, "NoneType")]


@pytest.fixture
def env():
    with biyos.Env() as env:
        yield env


@pytest.fixture
def model(env):
    with biyos.Model(env) as model:
        yield model


@pytest.fixture
def log(monkeypatch):
    log = []
    free = partial(freed, log, lib.BIYOS_freemdl)
    count = partial(reached, log, lib.BIYOS_getnvar)
    monkeypatch.setattr(lib, "BIYOS_freemdl", free)
    monkeypatch.setattr(lib, "BIYOS_getnvar", count)
    return log


def keep(texts, event):
    texts.append(event.text)


def refused(*arguments):
    return -100


def once(log, function, handle):
    value = getattr(handle, "value", handle)
    seen = value in log
    log.append(value)
    return OK if seen else function(handle)


def collected(log, function, handle):
    gc.collect()
    log.append("env")
    return function(handle)


def freed(log, function, handle):
    log.append("free")
    return function(handle)


def reached(log, function, *arguments):
    log.append("call")
    return OK if "free" in log else function(*arguments)


def read_lengths(handle, first, count, *arrays):
    return [(array, count) for array in arrays]


def test_context(env):
    with biyos.Model(env) as model:
        assert model.getnvar() == 0
    with pytest.raises(ValueError, match="the model is closed"):
        model.getnvar()
    with pytest.raises(ValueError, match="the model is closed"):
        model.getvarbnd([])
    with pytest.raises(ValueError, match="the model is closed"):
        model.getvarknd([])


def test_close(env):
    model = biyos.Model(env)
    model.close()
    model.close()
    env.close()
    assert not env._free.alive


def test_collected(env):
    model = biyos.Model(env)
    free = model._free
    del model
    gc.collect()
    assert not free.alive


def test_cycle(monkeypatch):
    gc.collect()
    statuses = []
    texts = []
    spy = partial(spied, lib.BIYOS_freeenv, statuses)
    monkeypatch.setattr(lib, "BIYOS_freeenv", spy)
    env = biyos.Env()
    env.addhook(partial(keep, texts), EventKind.MESSAGE, EventLevel.DEBUG)
    env.cycle = biyos.Model(env)
    del env
    gc.collect()
    assert texts == []
    assert statuses == [OK]


def test_window(monkeypatch):
    gc.collect()
    log = []
    model_free = partial(once, log, lib.BIYOS_freemdl)
    env_free = partial(collected, log, lib.BIYOS_freeenv)
    monkeypatch.setattr(lib, "BIYOS_freemdl", model_free)
    monkeypatch.setattr(lib, "BIYOS_freeenv", env_free)
    gc.disable()
    try:
        env = biyos.Env()
        model = biyos.Model(env)
        handle = model._handle.value
        model.cycle = model
        del model
        env.close()
    finally:
        gc.enable()
    assert log == [handle, "env"]


def test_interrupted_create(monkeypatch):
    statuses = []
    create = partial(interrupted, lib.BIYOS_newmdl)
    monkeypatch.setattr(lib, "BIYOS_newmdl", create)
    spy = partial(spied, lib.BIYOS_freemdl, statuses)
    monkeypatch.setattr(lib, "BIYOS_freemdl", spy)
    env = biyos.Env()
    with pytest.raises(KeyboardInterrupt):
        biyos.Model(env)
    env.close()
    assert statuses == [OK]
    assert not env._free.alive


def test_interrupted_close(env, monkeypatch):
    log = []
    free = partial(once, log, lib.BIYOS_freemdl)
    monkeypatch.setattr(lib, "BIYOS_freemdl", partial(interrupted, free))
    model = biyos.Model(env)
    handle = model._handle.value
    with pytest.raises(KeyboardInterrupt):
        model.close()
    monkeypatch.setattr(lib, "BIYOS_freemdl", free)
    del model
    gc.collect()
    env.getopt("seed")
    env.close()
    assert log == [handle]
    assert not env._free.alive


def test_parked(env, monkeypatch):
    statuses = []
    spy = partial(spied, lib.BIYOS_freemdl, statuses)
    monkeypatch.setattr(lib, "BIYOS_freemdl", spy)
    models = [biyos.Model(env)]
    seen = []

    def close_inside(event):
        models[0].close()
        seen.append(list(statuses))
        try:
            models[0].getnvar()
        except ValueError as error:
            seen.append(str(error))

    env.addhook(close_inside, EventKind.MESSAGE, EventLevel.ERROR)
    with pytest.raises(biyos.Error):
        env.setopt("seed", -1)
    assert seen == [[], "the model is closed"]
    assert statuses == [OK]
    models[0].close()
    env.close()
    assert statuses == [OK]


def test_put_back(env, monkeypatch):
    statuses = []
    spy = partial(spied, lib.BIYOS_freemdl, statuses)
    monkeypatch.setattr(lib, "BIYOS_freemdl", spy)
    models = [biyos.Model(env)]
    with monkeypatch.context() as patch:
        patch.setattr(lib, "BIYOS_freemdl", refused)
        with pytest.raises(biyos.Error):
            models[0].close()
    assert models[0].getnvar() == 0
    models.clear()
    env.getopt("seed")
    assert statuses == [OK]


def test_dropped(env, monkeypatch):
    statuses = []
    spy = partial(spied, lib.BIYOS_freemdl, statuses)
    monkeypatch.setattr(lib, "BIYOS_freemdl", spy)
    models = [biyos.Model(env)]

    def drop(event):
        models.clear()

    env.addhook(drop, EventKind.MESSAGE, EventLevel.ERROR)
    with pytest.raises(biyos.Error):
        env.setopt("seed", -1)
    assert models == []
    assert statuses == [OK]
    env.close()
    assert not env._free.alive


def test_delivery(env, monkeypatch):
    gc.collect()
    statuses = []
    spy = partial(spied, lib.BIYOS_freemdl, statuses)
    monkeypatch.setattr(lib, "BIYOS_freemdl", spy)
    model = biyos.Model(env)
    model.cycle = model
    del model

    def collect(event):
        gc.collect()

    env.addhook(collect, EventKind.MESSAGE, EventLevel.ERROR)
    gc.disable()
    try:
        with pytest.raises(biyos.Error):
            env.setopt("seed", -1)
    finally:
        gc.enable()
    assert statuses == [OK]
    env.close()
    assert not env._free.alive


def test_kept(env, monkeypatch):
    original = lib.BIYOS_freemdl
    statuses = []
    models = [biyos.Model(env)]
    monkeypatch.setattr(lib, "BIYOS_freemdl", refused)
    models.clear()
    env.getopt("seed")
    spy = partial(spied, original, statuses)
    monkeypatch.setattr(lib, "BIYOS_freemdl", spy)
    env.getopt("seed")
    assert statuses == []
    env.close()
    assert statuses == [OK]
    assert not env._free.alive


def test_closes(monkeypatch):
    statuses = []
    spy = partial(spied, lib.BIYOS_freemdl, statuses)
    monkeypatch.setattr(lib, "BIYOS_freemdl", spy)
    env = biyos.Env()
    first = biyos.Model(env)
    second = biyos.Model(env)
    second.close()
    env.close()
    assert statuses == [OK, OK]
    assert not env._free.alive
    with pytest.raises(ValueError, match="the model is closed"):
        first.getnvar()
    with pytest.raises(ValueError, match="the model is closed"):
        first.addvar(1)
    first.close()
    second.close()
    assert statuses == [OK, OK]


def test_with():
    with biyos.Env() as env:
        model = biyos.Model(env)
        model.addvar(1)
    assert not env._free.alive
    with pytest.raises(ValueError, match="the model is closed"):
        model.getvar()


def test_unmasked():
    with pytest.raises(KeyError, match="inside"):
        with biyos.Env() as env:
            model = biyos.Model(env)
            model.addvar(1)
            raise KeyError("inside")
    assert not env._free.alive


def test_busy_close(log):
    env = biyos.Env()
    model = biyos.Model(env)
    seen = []

    def close_inside(event):
        env.close()
        try:
            model.getnvar()
        except ValueError as error:
            seen.append(str(error))

    env.addhook(close_inside, EventKind.MESSAGE, EventLevel.ERROR)
    message = refusal(model.setvarbnd, [0], [0.0], [1.0])
    assert message == "invalid argument: variable 0: unknown"
    assert seen == ["the model is closed"]
    assert log == ["free"]
    assert not env._free.alive


def test_gap(log, monkeypatch):
    env = biyos.Env()
    model = biyos.Model(env)
    start = partial(preceded, [model.close], env._relay.start)
    monkeypatch.setattr(env._relay, "start", start)
    assert model.getnvar() == 0
    assert log == ["call", "free"]
    with pytest.raises(ValueError, match="the model is closed"):
        model.getnvar()
    env.close()
    assert log == ["call", "free"]


def test_early(log, monkeypatch):
    env = biyos.Env()
    model = biyos.Model(env)
    opened = partial(preceded, [model.close], model._open)
    monkeypatch.setattr(model, "_open", opened)
    with pytest.raises(ValueError, match="the model is closed"):
        model.getnvar()
    assert log == ["free"]
    env.close()
    assert log == ["free"]


def test_checked(log, monkeypatch):
    env = biyos.Env()
    model = biyos.Model(env)
    opened = partial(followed, [model.close], model._open)
    monkeypatch.setattr(model, "_open", opened)
    assert model.getnvar() == 0
    assert log == ["call", "free"]
    env.close()
    assert log == ["call", "free"]


def test_nested(log, monkeypatch):
    env = biyos.Env()
    model = biyos.Model(env)
    actions = [model.getnvar, model.close]
    start = partial(preceded, actions, env._relay.start)
    monkeypatch.setattr(env._relay, "start", start)
    assert model.getnvar() == 0
    assert log == ["call", "call", "free"]
    env.close()
    assert log == ["call", "call", "free"]


def test_drained(log, monkeypatch):
    env = biyos.Env()
    model = biyos.Model(env)
    actions = [model.close, partial(env.getopt, "seed")]
    start = partial(preceded, actions, env._relay.start)
    monkeypatch.setattr(env._relay, "start", start)
    assert model.getnvar() == 0
    assert log == ["call", "free"]
    env.close()
    assert log == ["call", "free"]


def test_interrupted_call(log, monkeypatch):
    env = biyos.Env()
    model = biyos.Model(env)
    count = partial(interrupted, lib.BIYOS_getnvar)
    monkeypatch.setattr(lib, "BIYOS_getnvar", count)
    start = partial(preceded, [model.close], env._relay.start)
    monkeypatch.setattr(env._relay, "start", start)
    with pytest.raises(KeyboardInterrupt):
        model.getnvar()
    assert log == ["call", "free"]
    env.close()
    assert log == ["call", "free"]


def test_deferred(log):
    env = biyos.Env()
    model = biyos.Model(env)

    def close_inside(event):
        model.close()

    env.addhook(close_inside, EventKind.MESSAGE, EventLevel.ERROR)
    message = refusal(model.setvarbnd, [0], [0.0], [1.0])
    assert message == "invalid argument: variable 0: unknown"
    assert log == ["free"]
    with pytest.raises(ValueError, match="the model is closed"):
        model.getnvar()
    env.close()
    assert log == ["free"]


def test_reentered(monkeypatch):
    log = []
    free = partial(once, log, lib.BIYOS_freemdl)
    monkeypatch.setattr(lib, "BIYOS_freemdl", free)
    env = biyos.Env()
    model = biyos.Model(env)
    handle = model._handle.value
    models = env._models
    take = partial(preceded, [model.close], models.take)
    monkeypatch.setattr(models, "take", take)
    model.close()
    env.close()
    assert log == [handle]


def test_interrupted_take(monkeypatch):
    statuses = []
    spy = partial(spied, lib.BIYOS_freemdl, statuses)
    monkeypatch.setattr(lib, "BIYOS_freemdl", spy)
    env = biyos.Env()
    model = biyos.Model(env)
    models = env._models
    monkeypatch.setattr(models, "take", partial(interrupted, models.take))
    with pytest.raises(KeyboardInterrupt):
        model.close()
    monkeypatch.delattr(models, "take")
    model.close()
    env.close()
    assert statuses == [OK]
    assert not env._free.alive


def test_holds_env():
    model = biyos.Model(biyos.Env())
    gc.collect()
    model.addvar(1)
    assert model.getnvar() == 1
    model.close()


def test_wrong_env():
    with pytest.raises(TypeError, match="env must be an Env, not int"):
        biyos.Model(1)


def test_closed_env():
    env = biyos.Env()
    env.close()
    with pytest.raises(ValueError, match="the environment is closed"):
        biyos.Model(env)


def test_add(model):
    lower = [-INFINITY, 0.0, -3.0, 2.0, 1.0]
    upper = [INFINITY, 1.0, 7.0, 10.0, 4.0]
    added = model.addvar(5, KINDS, lower, upper)
    assert type(added) is biyos.Variables
    assert len(added) == 5
    assert model.getnvar() == 5
    found = model.getvar()
    assert found == (KINDS, lower, upper)
    assert all(type(kind) is VariableKind for kind in found[0])
    found = model.getvar([2])
    assert found == ([VariableKind.INTEGER], [-3.0], [7.0])
    assert model.getvar(range(3, 5)) == (KINDS[3:], lower[3:], upper[3:])
    assert model.getvar(range(5, 5)) == ([], [], [])


def test_defaults(model):
    kinds = [VariableKind.BINARY, VariableKind.INTEGER]
    model.addvar(2)
    model.addvar(2, kinds)
    model.addvar(2, kinds, lb=[0.5, 0.75])
    model.addvar(2, kinds, ub=[0.5, 0.75])
    found = model.getvar()
    low, high = -INFINITY, INFINITY
    assert found[0] == [VariableKind.CONTINUOUS] * 2 + kinds * 3
    assert found[1] == [low, low, 0.0, low, 0.5, 0.75, 0.0, low]
    assert found[2] == [high, high, 1.0, high, 1.0, high, 0.5, 0.75]


def test_sequences(model):
    read_only = memoryview(array.array("d", [4.0, 5.0]).tobytes()).cast("d")
    model.addvar(2, (1, 3), range(2), array.array("d", [2.0, 3.0]))
    model.addvar(2, array.array("i", [4, 5]), read_only, None)
    kinds = [VariableKind(kind) for kind in (1, 3, 4, 5)]
    assert model.getvar() == (
        kinds,
        [0.0, 1.0, 4.0, 5.0],
        [2.0, 3.0, INFINITY, INFINITY],
    )


def test_buffer(model, monkeypatch):
    lower = array.array("d", [0.0, 1.0])
    upper = array.array("d", [2.0, 3.0])
    seen = []
    original = lib.BIYOS_addvar

    def spy(handle, count, kinds, low, high, first):
        seen.append((ctypes.addressof(low), ctypes.addressof(high)))
        return original(handle, count, kinds, low, high, first)

    monkeypatch.setattr(lib, "BIYOS_addvar", spy)
    model.addvar(2, None, lower, upper)
    assert seen == [(lower.buffer_info()[0], upper.buffer_info()[0])]
    assert model.getvar()[1:] == ([0.0, 1.0], [2.0, 3.0])


def test_bounds(model):
    model.addvar(2)
    model.setvarbnd([1, 0], lb=[1.0, 2.0])
    model.setvarbnd([1], ub=[3.0])
    assert model.getvar()[1:] == ([2.0, 1.0], [INFINITY, 3.0])
    model.setvarbnd([0, 0], [5.0, 6.0], [7.0, 8.0])
    assert model.getvar([0])[1:] == ([6.0], [8.0])


def test_kinds(model):
    model.addvar(2, lb=[0.0, 0.0], ub=[1.0, 1.0])
    model.setvarknd([1], [VariableKind.BINARY])
    model.setvarknd([0, 1], None)
    assert model.getvar()[0] == [
        VariableKind.CONTINUOUS,
        VariableKind.BINARY,
    ]


def test_listed(model):
    lower = [-INFINITY, 0.0, -3.0, 2.0, 1.0]
    upper = [INFINITY, 1.0, 7.0, 10.0, 4.0]
    x = model.addvar(5, KINDS, lower, upper)
    idx = [4, 0, 4, 2]
    found = model.getvarbnd(idx)
    assert found == ([1.0, -INFINITY, 1.0, -3.0], [4.0, INFINITY, 4.0, 7.0])
    knd = model.getvarknd(idx)
    assert knd == [KINDS[4], KINDS[0], KINDS[4], KINDS[2]]
    assert all(type(kind) is VariableKind for kind in knd)
    assert model.getvarbnd(x[3:]) == (lower[3:], upper[3:])
    assert model.getvarknd([x[1], Number(3)]) == [KINDS[1], KINDS[3]]
    assert model.getvarbnd([]) == ([], [])
    assert model.getvarknd([]) == []


def test_setters(model):
    model.addvar(3)
    idx = [2, 0, 2]
    knd = [
        VariableKind.INTEGER,
        VariableKind.SEMICONTINUOUS,
        VariableKind.BINARY,
    ]
    model.setvarbnd(idx, [0.0, -1.0, 0.5], [1.0, 2.0, 1.0])
    model.setvarknd(idx, knd)
    assert model.getvarbnd(idx) == ([0.5, -1.0, 0.5], [1.0, 2.0, 1.0])
    assert model.getvarknd(idx) == [knd[2], knd[1], knd[2]]
    assert model.getvarknd([1]) == [VariableKind.CONTINUOUS]


def test_listed_refusals(env, model):
    x = model.addvar(2)
    message = refusal(model.getvarbnd, [0, 2])
    assert message == "invalid argument: variable 2: unknown"
    message = refusal(model.getvarknd, [1, -1])
    assert message == "invalid argument: variable -1: unknown"
    other = biyos.Model(env)
    y = other.addvar(1)
    with pytest.raises(ValueError, match="idx is a variable of another"):
        model.getvarbnd([y[0]])
    with pytest.raises(ValueError, match="idx is a block of another"):
        model.getvarknd(y)
    f = model.addfuns(1)
    with pytest.raises(ValueError, match="is a function, not a variable"):
        model.getvarbnd([f[0]])
    with pytest.raises(TypeError, match="an entry of idx must be an int"):
        model.getvarknd([0.0])
    model._gen += 1
    with pytest.raises(ValueError, match="the block predates a deletion"):
        model.getvarbnd(x)
    other.close()


def test_refused(model):
    model.addvar(1)
    binary = [VariableKind.BINARY]
    message = refusal(model.setvarknd, [0], binary)
    assert message == (
        "invalid argument: variable 0: bounds [-inf, inf] are outside"
        " [0, 1], the widest of its kind"
    )
    message = refusal(model.setvarbnd, [3], [0.0], [1.0])
    assert message == "invalid argument: variable 3: unknown"
    message = refusal(model.addvar, 1, None, [INFINITY])
    assert message == "invalid argument: variable 1: lower bound is infinity"
    message = refusal(model.getvar, range(2, 3))
    assert message == "invalid argument: variable 2: unknown"
    message = refusal(model.getvar, [-1])
    assert message == "invalid argument: variable -1: unknown"
    continuous = [VariableKind.CONTINUOUS]
    assert model.getvar() == (continuous, [-INFINITY], [INFINITY])


def test_outside(model, monkeypatch):
    model.addvar(1)
    limit(monkeypatch, _model, 1)
    message = refusal(model.getvar, range(2**31 - 1))
    assert message == "invalid argument: variable 1: unknown"
    message = refusal(model.getvar, range(2, 2**31 - 1))
    assert message == "invalid argument: variable 2: unknown"


def test_stale(model, monkeypatch):
    model.addvar(2, None, [1.0, 2.0])
    monkeypatch.setattr(model, "getnvar", lambda: 0)
    spy = Room(lib.BIYOS_getvarrg, read_lengths)
    monkeypatch.setattr(lib, "BIYOS_getvarrg", spy)
    assert model.getvar(range(2))[1] == [1.0, 2.0]
    assert model.getvar(range(1, 1))[1] == []


def test_lengths(model):
    with pytest.raises(ValueError, match="lb has length 1, not 2"):
        model.addvar(2, lb=[0.0])
    with pytest.raises(ValueError, match="knd has length 2, not 1"):
        model.setvarknd([0], [1, 2])
    assert model.getnvar() == 0


@pytest.mark.parametrize("kinds, error, message", WRONG)
def test_wrong(model, kinds, error, message):
    with pytest.raises(error, match=message):
        model.addvar(1, kinds)
    with pytest.raises(error, match=message.replace("knd", "idx")):
        model.setvarbnd(kinds, [0.0])
    assert model.getnvar() == 0


@pytest.mark.parametrize("value, found", REALS)
def test_reals(model, value, found):
    message = f"an entry of lb must be a number, not {found}"
    with pytest.raises(TypeError, match=message):
        model.addvar(1, None, [value])
    with pytest.raises(TypeError, match=message):
        model.setvarbnd([0], [value])
    assert model.getnvar() == 0


def test_index(model):
    model.addvar(Number(2), [Number(2), Number(3)])
    model.setvarbnd([Number(1)], [-1], [5])
    model.setvarknd([Number(0)], [Number(1)])
    found = model.getvar([Number(0), Number(1)])
    kinds = [VariableKind.CONTINUOUS, VariableKind.INTEGER]
    assert found == (kinds, [0.0, -1.0], [1.0, 5.0])


def test_numbers(model):
    with pytest.raises(TypeError, match="n must be an int, not str"):
        model.addvar("1")
    with pytest.raises(OverflowError, match="idx: 4294967296 does not fit"):
        model.getvar(range(2**32, 2**32 + 1))
    assert model.getnvar() == 0


def test_event(env, model):
    texts = []

    def record(event):
        texts.append(event.text)

    env.addhook(record, EventKind.MESSAGE, EventLevel.ERROR)
    refusal(model.setvarbnd, [0], [0.0], [1.0])
    assert texts == ["invalid argument: variable 0: unknown"]


def test_raising(env):
    def explode(event):
        raise RuntimeError("hook")

    model = biyos.Model(env)
    env.addhook(explode, EventKind.MESSAGE, EventLevel.ERROR)
    with pytest.raises(RuntimeError, match="hook"):
        model.addvar(1, None, [INFINITY])
    model.close()


def test_locked(env, model):
    seen = []

    def change(event):
        try:
            model.addvar(1)
        except biyos.Error as error:
            seen.append(str(error))
        seen.append(model.getnvar())
        seen.append(model.getvarbnd([0]))
        seen.append(model.getvarknd([0]))

    model.addvar(1)
    env.addhook(change, EventKind.MESSAGE, EventLevel.ERROR)
    with pytest.raises(biyos.Error):
        env.setopt("seed", -1)
    bounds = ([-INFINITY], [INFINITY])
    assert seen == [LOCKED, 1, bounds, [VariableKind.CONTINUOUS]]
    assert model.getnvar() == 1
