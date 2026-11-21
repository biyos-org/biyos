import gc
import json
import re
import weakref
from functools import partial
from operator import attrgetter, methodcaller

import pytest

import biyos
from biyos import EventKind, EventLevel
from header import lines, macros
from number import Number

CODE = (("code", "-100"),)

LEVELS = {
    "BIYOS_LVL_DBG": EventLevel.DEBUG,
    "BIYOS_LVL_INFO": EventLevel.INFO,
    "BIYOS_LVL_WARN": EventLevel.WARNING,
    "BIYOS_LVL_ERR": EventLevel.ERROR,
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


def test_constants():
    kinds = {f"BIYOS_EVT_{kind.name}": kind for kind in biyos.EventKind}
    found = macros("events.h")
    assert len(found) == lines("events.h", "#define BIYOS_")
    assert set(LEVELS.values()) == set(EventLevel)
    assert found == kinds | LEVELS


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
        env.addhook(partial(record, seen), EventKind.CHECK, EventLevel.DEBUG)
        fail(env)
    assert seen == []


def test_unsubscribe():
    seen = []
    with biyos.Env() as env:
        function = partial(record, seen)
        subscription = env.addhook(
            function, EventKind.MESSAGE, EventLevel.DEBUG
        )
        env.delhook(subscription)
        fail(env)
        with pytest.raises(biyos.Error) as caught:
            env.delhook(subscription)
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
        kind = Number(int(EventKind.MESSAGE))
        lowest = Number(int(EventLevel.DEBUG))
        subscription = env.addhook(partial(record, seen), kind, lowest)
        fail(env)
        env.delhook(Number(subscription))
        fail(env)
    assert len(seen) == 1


@pytest.mark.parametrize("subscription", NUMBERS)
def test_number(subscription):
    with biyos.Env() as env, pytest.raises(TypeError):
        env.delhook(subscription)


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
    raise RuntimeError("subscriber failed")


def refuse(event):
    raise ValueError("second subscriber failed")


def stop(event):
    raise Stop


def read_seed(env, seeds, event):
    seeds.append(env.getopt("seed"))


def raised(env):
    try:
        env.setopt("seed", -1)
    except RuntimeError:
        return True
    return False


def test_raised():
    with biyos.Env() as env:
        env.addhook(explode, EventKind.MESSAGE, EventLevel.DEBUG)
        with pytest.raises(RuntimeError, match="subscriber failed") as caught:
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
