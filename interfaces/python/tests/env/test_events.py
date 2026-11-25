import gc
import json
import re
import weakref
from functools import partial
from operator import attrgetter, methodcaller

import pytest

import biyos
from biyos import EventKind, EventLevel
from biyos._capi import OK
from header import group
from number import Number
from spies import followed, preceded

CODE = (("code", "-100"),)

SEED = "invalid argument: option 'seed': -1 is outside [0, 2147483647]"

LOCKED = "invalid argument: model: is read-only while delivering an event"

ENV_LOCKED = (
    "invalid argument: environment: is read-only while delivering an event"
)

KINDS = {
    "BIYOS_EVTKND_MSG": EventKind.MESSAGE,
    "BIYOS_EVTKND_INV": EventKind.INVARIANT,
    "BIYOS_EVTKND_SPAN": EventKind.SPAN,
}

LEVELS = {
    "BIYOS_EVTLVL_DBG": EventLevel.DEBUG,
    "BIYOS_EVTLVL_INFO": EventLevel.INFO,
    "BIYOS_EVTLVL_WARN": EventLevel.WARNING,
    "BIYOS_EVTLVL_ERR": EventLevel.ERROR,
}

WRONG = [
    ("record", EventKind.MESSAGE, EventLevel.DEBUG),
    (print, True, EventLevel.DEBUG),
    (print, "message", EventLevel.DEBUG),
    (print, EventKind.MESSAGE, 1.0),
    (print, EventKind.MESSAGE, None),
]

NUMBERS = [True, "1", 1.0, None]

READERS = [
    attrgetter("kind"),
    attrgetter("level"),
    attrgetter("module"),
    attrgetter("text"),
    attrgetter("fields"),
    methodcaller("fmttext"),
    methodcaller("fmtjson"),
    attrgetter("model"),
]


class Sink:
    def __call__(self, event):
        pass


def snapshot(event):
    return event.kind, event.level, event.module, event.text, event.fields


def record(seen, event):
    seen.append(snapshot(event))


def keep_lines(lines, event):
    lines.append((event.fmttext(), event.fmtjson()))


def fail(env):
    with pytest.raises(biyos.Error) as caught:
        env.setopt("seed", -1)
    return caught.value


def keep_model(seen, event):
    seen.append(event.model)


def listen(target, hook):
    return target.addhook(hook, EventKind.MESSAGE, EventLevel.DEBUG)


def fail_model(model):
    with pytest.raises(biyos.Error) as caught:
        model.setopt("seed", -1)
    return caught.value


def mark(heard, name, event):
    heard.append(name)


def refuse_inside(model, seen, event):
    try:
        model.addvar(1)
    except biyos.Error as error:
        seen.append(str(error))


def change_model(model, other, env, seen, event):
    changes = [
        partial(model.setopt, "seed", 1),
        partial(listen, model, Sink()),
        partial(model.delhook, 1),
        partial(model.addvar, 1),
        partial(other.setopt, "seed", 1),
        partial(env.setopt, "seed", 1),
        partial(env.delopt, "seed"),
        partial(listen, env, Sink()),
        partial(biyos.Model, env),
    ]
    for call in changes:
        try:
            call()
        except biyos.Error as error:
            seen.append(str(error))
    seen.append(model.getopt("seed"))
    seen.append(model.getnvar())


def test_constants():
    assert set(KINDS.values()) == set(EventKind)
    assert set(LEVELS.values()) == set(EventLevel)
    assert group("BIYOS_EVTKND_") == KINDS
    assert group("BIYOS_EVTLVL_") == LEVELS


def test_error():
    seen = []
    with biyos.Env() as env:
        env.addhook(partial(record, seen), EventKind.MESSAGE, EventLevel.ERROR)
        error = fail(env)
    message = (EventKind.MESSAGE, EventLevel.ERROR, "capi", str(error), CODE)
    assert seen == [message]
    assert type(seen[0][0]) is EventKind
    assert type(seen[0][1]) is EventLevel


def test_kind():
    seen = []
    with biyos.Env() as env:
        hook = partial(record, seen)
        env.addhook(hook, EventKind.INVARIANT, EventLevel.DEBUG)
        fail(env)
    assert seen == []


def test_delhook():
    seen = []
    with biyos.Env() as env:
        function = partial(record, seen)
        idx = env.addhook(function, EventKind.MESSAGE, EventLevel.DEBUG)
        env.delhook(idx)
        fail(env)
        with pytest.raises(biyos.Error) as caught:
            env.delhook(idx)
    assert seen == []
    assert caught.value.code == -100


@pytest.mark.parametrize("arguments", WRONG)
def test_wrong(arguments):
    with biyos.Env() as env, pytest.raises(TypeError):
        env.addhook(*arguments)


def test_unknown():
    with biyos.Env() as env, pytest.raises(biyos.Error) as caught:
        env.addhook(print, 9, EventLevel.DEBUG)
    assert caught.value.code == -100


def test_overflow():
    seen = []
    with biyos.Env() as env:
        function = partial(record, seen)
        first = env.addhook(function, EventKind.MESSAGE, EventLevel.DEBUG)
        with pytest.raises(OverflowError):
            env.addhook(print, 2**32 + EventKind.MESSAGE, EventLevel.DEBUG)
        with pytest.raises(OverflowError):
            env.delhook(2**32 + first)
        fail(env)
    assert len(seen) == 1


def test_index():
    seen = []
    with biyos.Env() as env:
        knd = Number(int(EventKind.MESSAGE))
        lvl = Number(int(EventLevel.DEBUG))
        idx = env.addhook(partial(record, seen), knd, lvl)
        fail(env)
        env.delhook(Number(idx))
        fail(env)
    assert len(seen) == 1


@pytest.mark.parametrize("idx", NUMBERS)
def test_number(idx):
    with biyos.Env() as env, pytest.raises(TypeError):
        env.delhook(idx)


@pytest.mark.parametrize("read", READERS)
def test_expired(read):
    kept = []
    with biyos.Env() as env:
        env.addhook(kept.append, EventKind.MESSAGE, EventLevel.DEBUG)
        fail(env)
    with pytest.raises(ValueError, match="the event is no longer valid"):
        read(kept[0])


def test_formats():
    lines = []
    with biyos.Env() as env:
        function = partial(keep_lines, lines)
        env.addhook(function, EventKind.MESSAGE, EventLevel.ERROR)
        error = fail(env)
    text, line = lines[0]
    stamp, rest = text.split(" ", 1)
    assert re.fullmatch(r"\d+\.\d{6}", stamp)
    assert rest == f"error capi: {error} code=-100"
    parsed = json.loads(line)
    assert parsed.pop("time") == float(stamp)
    assert parsed == {
        "kind": "message",
        "level": "error",
        "module": "capi",
        "text": str(error),
        "fields": {"code": "-100"},
    }


def test_kept():
    seen = []
    with biyos.Env() as env:
        env.addhook(partial(record, seen), EventKind.MESSAGE, EventLevel.DEBUG)
        gc.collect()
        fail(env)
    assert len(seen) == 1


def test_cleared():
    sink = Sink()
    alive = weakref.ref(sink)
    env = biyos.Env()
    env.addhook(sink, EventKind.MESSAGE, EventLevel.DEBUG)
    del sink
    env.close()
    assert alive() is None


def test_freed():
    gc.disable()
    try:
        env = biyos.Env()
        env.addhook(Sink(), EventKind.MESSAGE, EventLevel.DEBUG)
        free = env._free
        del env
        assert not free.alive
    finally:
        gc.enable()


class Stop(BaseException):
    pass


def explode(event):
    raise RuntimeError("hook failed")


def refuse(event):
    raise ValueError("second hook failed")


def stop(event):
    raise Stop


def read_seed(env, seeds, event):
    seeds.append(env.getopt("seed"))


def arm(actions, steps, event):
    actions.extend(steps)
    raise RuntimeError("hook failed")


def relayed(relay, hook):
    relay.run(hook, None)
    return OK


def trapped(errors, function, event):
    try:
        function()
    except ValueError as error:
        errors.append(error)


def interrupt():
    raise KeyboardInterrupt


def swallowed(function, *arguments):
    try:
        function(*arguments)
    except KeyboardInterrupt:
        pass


def raised(env):
    try:
        env.setopt("seed", -1)
    except RuntimeError:
        return True
    return False


def test_raised():
    with biyos.Env() as env:
        env.addhook(explode, EventKind.MESSAGE, EventLevel.DEBUG)
        with pytest.raises(RuntimeError, match="hook failed") as caught:
            env.setopt("seed", -1)
        env.setopt("seed", 3)
        seed = env.getopt("seed")
    assert seed == 3
    context = caught.value.__context__
    assert type(context) is biyos.Error
    assert context.code == -100


def test_later():
    seen = []
    with biyos.Env() as env:
        env.addhook(explode, EventKind.MESSAGE, EventLevel.DEBUG)
        env.addhook(refuse, EventKind.MESSAGE, EventLevel.DEBUG)
        env.addhook(partial(record, seen), EventKind.MESSAGE, EventLevel.DEBUG)
        with pytest.raises(RuntimeError):
            env.setopt("seed", -1)
    assert len(seen) == 1


def test_nested():
    seeds = []
    with biyos.Env() as env:
        function = partial(read_seed, env, seeds)
        env.addhook(explode, EventKind.MESSAGE, EventLevel.DEBUG)
        env.addhook(function, EventKind.MESSAGE, EventLevel.DEBUG)
        with pytest.raises(RuntimeError):
            env.setopt("seed", -1)
    assert seeds == [0]


def test_base():
    with biyos.Env() as env:
        env.addhook(stop, EventKind.MESSAGE, EventLevel.DEBUG)
        with pytest.raises(Stop):
            env.setopt("seed", -1)


def test_cycle():
    gc.disable()
    try:
        env = biyos.Env()
        env.addhook(explode, EventKind.MESSAGE, EventLevel.DEBUG)
        assert raised(env)
        free = env._free
        del env
        assert not free.alive
    finally:
        gc.enable()


def test_stale():
    with biyos.Env() as env:
        env._relay.run(explode, None)
        assert env.getopt("seed") == 0


def test_finish(monkeypatch):
    with biyos.Env() as env:
        nested = partial(env.getopt, "seed")
        check = partial(preceded, [nested], biyos._env.relay.check)
        monkeypatch.setattr(biyos._env.relay, "check", check)
        env.addhook(explode, EventKind.MESSAGE, EventLevel.DEBUG)
        with pytest.raises(RuntimeError, match="hook failed") as caught:
            env.setopt("seed", -1)
        assert env.getopt("seed") == 0
    assert type(caught.value.__context__) is biyos.Error


def test_succeeded(monkeypatch):
    with biyos.Env() as env:
        relay = env._relay
        relayed = partial(relay.run, explode, None)
        start = partial(followed, [relayed], relay.start)
        monkeypatch.setattr(relay, "start", start)
        nested = partial(env.getopt, "seed")
        check = partial(preceded, [nested], biyos._env.relay.check)
        monkeypatch.setattr(biyos._env.relay, "check", check)
        with pytest.raises(RuntimeError, match="hook failed") as caught:
            env.getopt("seed")
        assert env.getopt("seed") == 0
    assert caught.value.__context__ is None


def test_gap(monkeypatch):
    actions = []
    with biyos.Env() as env:
        settle = partial(preceded, actions, env._settle)
        monkeypatch.setattr(env, "_settle", settle)
        nested = partial(env.getopt, "seed")
        hook = partial(arm, actions, [nested])
        env.addhook(hook, EventKind.MESSAGE, EventLevel.DEBUG)
        with pytest.raises(RuntimeError, match="hook failed") as caught:
            env.setopt("seed", -1)
        assert env.getopt("seed") == 0
    assert type(caught.value.__context__) is biyos.Error


def test_inner():
    errors = []
    with biyos.Env() as env:
        call = partial(env._run, relayed, env._relay, refuse)
        hook = partial(trapped, errors, call)
        env.addhook(hook, EventKind.MESSAGE, EventLevel.DEBUG)
        with pytest.raises(biyos.Error):
            env.setopt("seed", -1)
    assert [str(error) for error in errors] == ["second hook failed"]


def test_aborted(monkeypatch):
    actions = []
    with biyos.Env() as env:
        settle = partial(preceded, actions, env._settle)
        monkeypatch.setattr(env, "_settle", settle)
        nested = partial(swallowed, env.getopt, "seed")
        hook = partial(arm, actions, [nested, interrupt])
        env.addhook(hook, EventKind.MESSAGE, EventLevel.DEBUG)
        with pytest.raises(RuntimeError, match="hook failed"):
            env.setopt("seed", -1)
        assert env.getopt("seed") == 0


def test_model():
    seen = []
    with biyos.Env() as env, biyos.Model(env) as model:
        hook = partial(keep_model, seen)
        env.addhook(hook, EventKind.MESSAGE, EventLevel.ERROR)
        with pytest.raises(biyos.Error):
            model.addvar(1, [biyos.VariableKind.BINARY], ub=[2.0])
        fail(env)
        assert len(seen) == 2
        assert seen[0] is model
        assert seen[1] is None


def test_model_hook():
    mine = []
    every = []
    with biyos.Env() as env, biyos.Model(env) as first:
        second = biyos.Model(env)
        listen(first, partial(keep_model, mine))
        listen(env, partial(keep_model, every))
        fail_model(first)
        fail_model(second)
        fail(env)
        assert mine == [first]
        assert every == [first, second, None]


def test_model_event():
    seen = []
    with biyos.Env() as env, biyos.Model(env) as model:
        listen(model, partial(record, seen))
        error = fail_model(model)
    assert seen == [(EventKind.MESSAGE, EventLevel.ERROR, "capi", SEED, CODE)]
    assert str(error) == SEED


def test_model_order():
    heard = []
    with biyos.Env() as env, biyos.Model(env) as model:
        listen(env, partial(mark, heard, "environment"))
        listen(model, partial(mark, heard, "first"))
        listen(model, partial(mark, heard, "second"))
        fail_model(model)
    assert heard == ["first", "second", "environment"]


def test_model_ids():
    with biyos.Env() as env, biyos.Model(env) as first:
        second = biyos.Model(env)
        ids = [listen(first, Sink()), listen(first, Sink())]
        ids.extend([listen(second, Sink()), listen(env, Sink())])
        first.delhook(1)
        ids.append(listen(first, Sink()))
        with pytest.raises(biyos.Error) as caught:
            first.delhook(1)
        with pytest.raises(biyos.Error):
            second.delhook(2)
        second.delhook(1)
    assert ids == [1, 2, 1, 1, 3]
    assert str(caught.value) == "invalid argument: hook 1: unknown"


@pytest.mark.parametrize("arguments", WRONG)
def test_model_wrong(arguments):
    with biyos.Env() as env, biyos.Model(env) as model:
        with pytest.raises(TypeError):
            model.addhook(*arguments)
        assert model._hooks._functions == {}


def test_model_unknown():
    with biyos.Env() as env, biyos.Model(env) as model:
        with pytest.raises(biyos.Error) as caught:
            model.addhook(Sink(), 9, EventLevel.DEBUG)
        assert model._hooks._functions == {}
    assert caught.value.code == -100


def test_model_closed():
    env = biyos.Env()
    model = biyos.Model(env)
    model.close()
    with pytest.raises(ValueError, match="the model is closed"):
        listen(model, Sink())
    with pytest.raises(ValueError, match="the model is closed"):
        model.delhook(1)
    env.close()


def test_model_free():
    sink = Sink()
    alive = weakref.ref(sink)
    with biyos.Env() as env:
        model = biyos.Model(env)
        listen(model, sink)
        del sink
        model.close()
        assert alive() is None


def test_model_cycle():
    with biyos.Env() as env:
        model = biyos.Model(env)
        listen(model, partial(keep_model, [model]))
        alive = weakref.ref(model)
        del model
        gc.collect()
        kept = list(env._models._kept.values())
        assert alive() is None
        assert kept[0](None, 1) is None
        env.getopt("seed")
        assert env._models._kept == {}


def test_model_copy():
    seen = []
    with biyos.Env() as env, biyos.Env() as other:
        model = biyos.Model(env)
        listen(model, partial(keep_model, seen))
        near = model.copy()
        far = model.copy(other)
        fail_model(near)
        fail_model(far)
        ids = [listen(near, Sink()), listen(far, Sink())]
        far.close()
    assert seen == []
    assert ids == [1, 1]


def test_model_locked():
    seen = []
    with biyos.Env() as env, biyos.Model(env) as model:
        other = biyos.Model(env)
        listen(model, partial(change_model, model, other, env, seen))
        fail_model(model)
    assert seen == [LOCKED] * 5 + [ENV_LOCKED] * 4 + [0, 0]


def test_model_error():
    seen = []
    with biyos.Env() as env, biyos.Model(env) as model:
        listen(model, partial(refuse_inside, model, seen))
        error = fail_model(model)
    assert seen == [LOCKED]
    assert str(error) == SEED
    assert error.model is model


def test_model_raised():
    with biyos.Env() as env, biyos.Model(env) as model:
        model.addhook(explode, EventKind.MESSAGE, EventLevel.DEBUG)
        with pytest.raises(RuntimeError, match="hook failed") as caught:
            model.setopt("seed", -1)
        model.setopt("seed", 3)
        seed = model.getopt("seed")
    assert seed == 3
    context = caught.value.__context__
    assert type(context) is biyos.Error
    assert context.code == -100


def test_model_silent():
    seen = []
    heard = []
    shared = []
    with biyos.Env() as env, biyos.Model(env) as model:
        listen(model, partial(refuse_inside, model, seen))
        listen(model, partial(keep_model, heard))
        listen(env, partial(keep_model, shared))
        fail_model(model)
        assert heard == [model]
        assert shared == [model]
    assert seen == [LOCKED]
