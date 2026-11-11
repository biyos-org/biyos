import array
import ctypes
import gc
from functools import partial

import pytest

import biyos
from biyos import EventKind, Level, VariableKind
from biyos._capi import OK, lib
from number import Number

INVALID_ARGUMENT = -100
INFINITY = float("inf")
LOCKED = "invalid argument: model: is read-only while delivering an event"
BUSY = "invalid argument: environment: is read-only while delivering an event"

KINDS = [
    VariableKind.CONTINUOUS,
    VariableKind.BINARY,
    VariableKind.INTEGER,
    VariableKind.SEMICONTINUOUS,
    VariableKind.SEMIINTEGER,
]

WRONG = [
    ([1.5], TypeError, "an entry of kinds must be an int, not float"),
    (["1"], TypeError, "an entry of kinds must be an int, not str"),
    ([True], TypeError, "an entry of kinds must be an int, not bool"),
    ([2**40], OverflowError, "an entry of kinds: 1099511627776 does not fit"),
]

REALS = [("0", "str"), (True, "bool"), (None, "NoneType")]


@pytest.fixture
def env():
    with biyos.Environment() as env:
        yield env


@pytest.fixture
def model(env):
    with biyos.Model(env) as model:
        yield model


@pytest.fixture
def log(monkeypatch):
    log = []
    free = partial(freed, log, lib.biyos_model_free)
    count = partial(reached, log, lib.biyos_variable_count)
    monkeypatch.setattr(lib, "biyos_model_free", free)
    monkeypatch.setattr(lib, "biyos_variable_count", count)
    return log


def keep(texts, event):
    texts.append(event.text)


def spied(function, statuses, *arguments):
    status = function(*arguments)
    statuses.append(status)
    return status


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


def interrupted(function, *arguments):
    function(*arguments)
    raise KeyboardInterrupt


def freed(log, function, handle):
    log.append("free")
    return function(handle)


def reached(log, function, *arguments):
    log.append("call")
    return OK if "free" in log else function(*arguments)


def run(actions):
    while actions:
        action = actions.pop(0)
        action()


def preceded(actions, function, *arguments):
    run(actions)
    return function(*arguments)


def followed(actions, function, *arguments):
    result = function(*arguments)
    run(actions)
    return result


def refusal(call, *arguments):
    with pytest.raises(biyos.Error) as caught:
        call(*arguments)
    assert caught.value.code == INVALID_ARGUMENT
    return str(caught.value)


def test_context(env):
    with biyos.Model(env) as model:
        assert model.variable_count() == 0
    with pytest.raises(ValueError, match="the model is closed"):
        model.variable_count()


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
    spy = partial(spied, lib.biyos_env_free, statuses)
    monkeypatch.setattr(lib, "biyos_env_free", spy)
    env = biyos.Environment()
    env.subscribe(partial(keep, texts), EventKind.MESSAGE, Level.DEBUG)
    env.cycle = biyos.Model(env)
    del env
    gc.collect()
    assert texts == []
    assert statuses == [OK]


def test_window(monkeypatch):
    gc.collect()
    log = []
    model_free = partial(once, log, lib.biyos_model_free)
    env_free = partial(collected, log, lib.biyos_env_free)
    monkeypatch.setattr(lib, "biyos_model_free", model_free)
    monkeypatch.setattr(lib, "biyos_env_free", env_free)
    gc.disable()
    try:
        env = biyos.Environment()
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
    create = partial(interrupted, lib.biyos_model_create)
    monkeypatch.setattr(lib, "biyos_model_create", create)
    spy = partial(spied, lib.biyos_model_free, statuses)
    monkeypatch.setattr(lib, "biyos_model_free", spy)
    env = biyos.Environment()
    with pytest.raises(KeyboardInterrupt):
        biyos.Model(env)
    env.close()
    assert statuses == [OK]
    assert not env._free.alive


def test_interrupted_close(env, monkeypatch):
    log = []
    free = partial(once, log, lib.biyos_model_free)
    monkeypatch.setattr(lib, "biyos_model_free", partial(interrupted, free))
    model = biyos.Model(env)
    handle = model._handle.value
    with pytest.raises(KeyboardInterrupt):
        model.close()
    monkeypatch.setattr(lib, "biyos_model_free", free)
    del model
    gc.collect()
    env.get_option("seed")
    env.close()
    assert log == [handle]
    assert not env._free.alive


def test_put_back(env, monkeypatch):
    statuses = []
    spy = partial(spied, lib.biyos_model_free, statuses)
    monkeypatch.setattr(lib, "biyos_model_free", spy)
    models = [biyos.Model(env)]
    seen = []

    def close_inside(event):
        try:
            models[0].close()
        except biyos.Error as error:
            seen.append(str(error))

    env.subscribe(close_inside, EventKind.MESSAGE, Level.ERROR)
    with pytest.raises(biyos.Error):
        env.set_option("seed", -1)
    assert seen == [LOCKED]
    assert models[0].variable_count() == 0
    models.clear()
    env.get_option("seed")
    assert statuses == [INVALID_ARGUMENT, OK]


def test_dropped(env, monkeypatch):
    statuses = []
    spy = partial(spied, lib.biyos_model_free, statuses)
    monkeypatch.setattr(lib, "biyos_model_free", spy)
    models = [biyos.Model(env)]

    def drop(event):
        models.clear()

    env.subscribe(drop, EventKind.MESSAGE, Level.ERROR)
    with pytest.raises(biyos.Error):
        env.set_option("seed", -1)
    assert models == []
    assert statuses == [OK]
    env.close()
    assert not env._free.alive


def test_delivery(env, monkeypatch):
    gc.collect()
    statuses = []
    spy = partial(spied, lib.biyos_model_free, statuses)
    monkeypatch.setattr(lib, "biyos_model_free", spy)
    model = biyos.Model(env)
    model.cycle = model
    del model

    def collect(event):
        gc.collect()

    env.subscribe(collect, EventKind.MESSAGE, Level.ERROR)
    gc.disable()
    try:
        with pytest.raises(biyos.Error):
            env.set_option("seed", -1)
    finally:
        gc.enable()
    assert statuses == [OK]
    env.close()
    assert not env._free.alive


def test_kept(env, monkeypatch):
    original = lib.biyos_model_free
    statuses = []
    models = [biyos.Model(env)]
    monkeypatch.setattr(lib, "biyos_model_free", refused)
    models.clear()
    env.get_option("seed")
    spy = partial(spied, original, statuses)
    monkeypatch.setattr(lib, "biyos_model_free", spy)
    env.get_option("seed")
    assert statuses == []
    env.close()
    assert statuses == [OK]
    assert not env._free.alive


def test_closes(monkeypatch):
    statuses = []
    spy = partial(spied, lib.biyos_model_free, statuses)
    monkeypatch.setattr(lib, "biyos_model_free", spy)
    env = biyos.Environment()
    first = biyos.Model(env)
    second = biyos.Model(env)
    second.close()
    env.close()
    assert statuses == [OK, OK]
    assert not env._free.alive
    with pytest.raises(ValueError, match="the model is closed"):
        first.variable_count()
    with pytest.raises(ValueError, match="the model is closed"):
        first.add_variables(1)
    first.close()
    second.close()
    assert statuses == [OK, OK]


def test_with():
    with biyos.Environment() as env:
        model = biyos.Model(env)
        model.add_variables(1)
    assert not env._free.alive
    with pytest.raises(ValueError, match="the model is closed"):
        model.get_variables()


def test_unmasked():
    with pytest.raises(KeyError, match="inside"):
        with biyos.Environment() as env:
            model = biyos.Model(env)
            model.add_variables(1)
            raise KeyError("inside")
    assert not env._free.alive


def test_busy_close(env, model):
    seen = []

    def close_inside(event):
        try:
            env.close()
        except biyos.Error as error:
            seen.append(str(error))

    model.add_variables(1)
    env.subscribe(close_inside, EventKind.MESSAGE, Level.ERROR)
    with pytest.raises(biyos.Error):
        env.set_option("seed", -1)
    assert seen == [BUSY]
    model.add_variables(1)
    assert model.variable_count() == 2
    assert env.get_option("seed") == 0


def test_gap(log, monkeypatch):
    env = biyos.Environment()
    model = biyos.Model(env)
    start = partial(preceded, [model.close], env._relay.start)
    monkeypatch.setattr(env._relay, "start", start)
    assert model.variable_count() == 0
    assert log == ["call", "free"]
    with pytest.raises(ValueError, match="the model is closed"):
        model.variable_count()
    env.close()
    assert log == ["call", "free"]


def test_early(log, monkeypatch):
    env = biyos.Environment()
    model = biyos.Model(env)
    opened = partial(preceded, [model.close], model._open)
    monkeypatch.setattr(model, "_open", opened)
    with pytest.raises(ValueError, match="the model is closed"):
        model.variable_count()
    assert log == ["free"]
    env.close()
    assert log == ["free"]


def test_checked(log, monkeypatch):
    env = biyos.Environment()
    model = biyos.Model(env)
    opened = partial(followed, [model.close], model._open)
    monkeypatch.setattr(model, "_open", opened)
    assert model.variable_count() == 0
    assert log == ["call", "free"]
    env.close()
    assert log == ["call", "free"]


def test_nested(log, monkeypatch):
    env = biyos.Environment()
    model = biyos.Model(env)
    actions = [model.variable_count, model.close]
    start = partial(preceded, actions, env._relay.start)
    monkeypatch.setattr(env._relay, "start", start)
    assert model.variable_count() == 0
    assert log == ["call", "call", "free"]
    env.close()
    assert log == ["call", "call", "free"]


def test_drained(log, monkeypatch):
    env = biyos.Environment()
    model = biyos.Model(env)
    actions = [model.close, partial(env.get_option, "seed")]
    start = partial(preceded, actions, env._relay.start)
    monkeypatch.setattr(env._relay, "start", start)
    assert model.variable_count() == 0
    assert log == ["call", "free"]
    env.close()
    assert log == ["call", "free"]


def test_interrupted_call(log, monkeypatch):
    env = biyos.Environment()
    model = biyos.Model(env)
    count = partial(interrupted, lib.biyos_variable_count)
    monkeypatch.setattr(lib, "biyos_variable_count", count)
    start = partial(preceded, [model.close], env._relay.start)
    monkeypatch.setattr(env._relay, "start", start)
    with pytest.raises(KeyboardInterrupt):
        model.variable_count()
    assert log == ["call", "free"]
    env.close()
    assert log == ["call", "free"]


def test_deferred(log):
    env = biyos.Environment()
    model = biyos.Model(env)

    def close_inside(event):
        model.close()

    env.subscribe(close_inside, EventKind.MESSAGE, Level.ERROR)
    message = refusal(model.set_variable_bounds, [0], [0.0], [1.0])
    assert message == "invalid argument: variable 0: unknown"
    assert log == ["free"]
    with pytest.raises(ValueError, match="the model is closed"):
        model.variable_count()
    env.close()
    assert log == ["free"]


def test_reentered(monkeypatch):
    log = []
    free = partial(once, log, lib.biyos_model_free)
    monkeypatch.setattr(lib, "biyos_model_free", free)
    env = biyos.Environment()
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
    spy = partial(spied, lib.biyos_model_free, statuses)
    monkeypatch.setattr(lib, "biyos_model_free", spy)
    env = biyos.Environment()
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
    model = biyos.Model(biyos.Environment())
    gc.collect()
    model.add_variables(1)
    assert model.variable_count() == 1
    model.close()


def test_wrong_env():
    with pytest.raises(TypeError, match="env must be an Environment, not int"):
        biyos.Model(1)


def test_closed_env():
    env = biyos.Environment()
    env.close()
    with pytest.raises(ValueError, match="the environment is closed"):
        biyos.Model(env)


def test_add(model):
    lower = [-INFINITY, 0.0, -3.0, 2.0, 1.0]
    upper = [INFINITY, 1.0, 7.0, 10.0, 4.0]
    model.add_variables(5, KINDS, lower, upper)
    assert model.variable_count() == 5
    found = model.get_variables()
    assert found == (KINDS, lower, upper)
    assert all(type(kind) is VariableKind for kind in found[0])
    assert model.get_variables(2, 1) == ([VariableKind.INTEGER], [-3.0], [7.0])
    assert model.get_variables(3) == (KINDS[3:], lower[3:], upper[3:])
    assert model.get_variables(5) == ([], [], [])


def test_defaults(model):
    kinds = [VariableKind.BINARY, VariableKind.INTEGER]
    model.add_variables(2)
    model.add_variables(2, kinds)
    model.add_variables(2, kinds, lower=[0.5, 0.75])
    model.add_variables(2, kinds, upper=[0.5, 0.75])
    found = model.get_variables()
    low, high = -INFINITY, INFINITY
    assert found[0] == [VariableKind.CONTINUOUS] * 2 + kinds * 3
    assert found[1] == [low, low, 0.0, low, 0.5, 0.75, 0.0, low]
    assert found[2] == [high, high, 1.0, high, 1.0, high, 0.5, 0.75]


def test_sequences(model):
    read_only = memoryview(array.array("d", [4.0, 5.0]).tobytes()).cast("d")
    model.add_variables(2, (1, 3), range(2), array.array("d", [2.0, 3.0]))
    model.add_variables(2, array.array("i", [4, 5]), read_only, None)
    kinds = [VariableKind(kind) for kind in (1, 3, 4, 5)]
    assert model.get_variables() == (
        kinds,
        [0.0, 1.0, 4.0, 5.0],
        [2.0, 3.0, INFINITY, INFINITY],
    )


def test_buffer(model, monkeypatch):
    lower = array.array("d", [0.0, 1.0])
    upper = array.array("d", [2.0, 3.0])
    seen = []
    original = lib.biyos_add_variables

    def spy(handle, count, kinds, low, high):
        seen.append((ctypes.addressof(low), ctypes.addressof(high)))
        return original(handle, count, kinds, low, high)

    monkeypatch.setattr(lib, "biyos_add_variables", spy)
    model.add_variables(2, None, lower, upper)
    assert seen == [(lower.buffer_info()[0], upper.buffer_info()[0])]
    assert model.get_variables()[1:] == ([0.0, 1.0], [2.0, 3.0])


def test_bounds(model):
    model.add_variables(2)
    model.set_variable_bounds([1, 0], lower=[1.0, 2.0])
    model.set_variable_bounds([1], upper=[3.0])
    assert model.get_variables()[1:] == ([2.0, 1.0], [INFINITY, 3.0])
    model.set_variable_bounds([0, 0], [5.0, 6.0], [7.0, 8.0])
    assert model.get_variables(0, 1)[1:] == ([6.0], [8.0])


def test_kinds(model):
    model.add_variables(2, lower=[0.0, 0.0], upper=[1.0, 1.0])
    model.set_variable_kinds([1], [VariableKind.BINARY])
    model.set_variable_kinds([0, 1], None)
    assert model.get_variables()[0] == [
        VariableKind.CONTINUOUS,
        VariableKind.BINARY,
    ]


def test_refused(model):
    model.add_variables(1)
    binary = [VariableKind.BINARY]
    message = refusal(model.set_variable_kinds, [0], binary)
    assert message == (
        "invalid argument: variable 0: bounds [-inf, inf] are outside"
        " [0, 1], the widest of its kind"
    )
    message = refusal(model.set_variable_bounds, [3], [0.0], [1.0])
    assert message == "invalid argument: variable 3: unknown"
    message = refusal(model.add_variables, 1, None, [INFINITY])
    assert message == "invalid argument: variable 1: lower bound is infinity"
    message = refusal(model.get_variables, 2)
    assert message == "invalid argument: variable 2: unknown"
    message = refusal(model.get_variables, 0, -1)
    assert message == "invalid argument: argument 'count': -1 is negative"
    continuous = [VariableKind.CONTINUOUS]
    assert model.get_variables() == (continuous, [-INFINITY], [INFINITY])


def test_lengths(model):
    with pytest.raises(ValueError, match="lower has length 1, not 2"):
        model.add_variables(2, lower=[0.0])
    with pytest.raises(ValueError, match="kinds has length 2, not 1"):
        model.set_variable_kinds([0], [1, 2])
    assert model.variable_count() == 0


@pytest.mark.parametrize("kinds, error, message", WRONG)
def test_wrong(model, kinds, error, message):
    with pytest.raises(error, match=message):
        model.add_variables(1, kinds)
    with pytest.raises(error, match=message.replace("kinds", "indices")):
        model.set_variable_bounds(kinds, [0.0])
    assert model.variable_count() == 0


@pytest.mark.parametrize("value, found", REALS)
def test_reals(model, value, found):
    message = f"an entry of lower must be a number, not {found}"
    with pytest.raises(TypeError, match=message):
        model.add_variables(1, None, [value])
    with pytest.raises(TypeError, match=message):
        model.set_variable_bounds([0], [value])
    assert model.variable_count() == 0


def test_index(model):
    model.add_variables(Number(2), [Number(2), Number(3)])
    model.set_variable_bounds([Number(1)], [-1], [5])
    model.set_variable_kinds([Number(0)], [Number(1)])
    found = model.get_variables(Number(0), Number(2))
    kinds = [VariableKind.CONTINUOUS, VariableKind.INTEGER]
    assert found == (kinds, [0.0, -1.0], [1.0, 5.0])


def test_numbers(model):
    with pytest.raises(TypeError, match="count must be an int, not str"):
        model.add_variables("1")
    with pytest.raises(OverflowError, match="first: 4294967296 does not fit"):
        model.get_variables(2**32)
    assert model.variable_count() == 0


def test_event(env, model):
    texts = []

    def record(event):
        texts.append(event.text)

    env.subscribe(record, EventKind.MESSAGE, Level.ERROR)
    refusal(model.set_variable_bounds, [0], [0.0], [1.0])
    assert texts == ["invalid argument: variable 0: unknown"]


def test_raising(env):
    def explode(event):
        raise RuntimeError("subscriber")

    model = biyos.Model(env)
    env.subscribe(explode, EventKind.MESSAGE, Level.ERROR)
    with pytest.raises(RuntimeError, match="subscriber"):
        model.add_variables(1, None, [INFINITY])
    model.close()


def test_locked(env, model):
    seen = []

    def change(event):
        for call in (partial(model.add_variables, 1), model.close):
            try:
                call()
            except biyos.Error as error:
                seen.append(str(error))
        seen.append(model.variable_count())

    model.add_variables(1)
    env.subscribe(change, EventKind.MESSAGE, Level.ERROR)
    with pytest.raises(biyos.Error):
        env.set_option("seed", -1)
    assert seen == [LOCKED, LOCKED, 1]
    assert model.variable_count() == 1
