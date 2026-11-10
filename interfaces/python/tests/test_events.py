import gc
import json
import re
import weakref
from functools import partial
from operator import attrgetter, methodcaller

import pytest

import biyos
from biyos import EventKind, Level
from header import lines, macros

CODE = (("code", "-100"),)

WRONG = [
    ("record", EventKind.MESSAGE, Level.DEBUG),
    (print, True, Level.DEBUG),
    (print, "message", Level.DEBUG),
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
    methodcaller("format_text"),
    methodcaller("format_json"),
]


class Sink:
    def __call__(self, event):
        pass


def snapshot(event):
    return event.kind, event.level, event.module, event.text, event.fields


def record(seen, event):
    seen.append(snapshot(event))


def keep_lines(lines, event):
    lines.append((event.format_text(), event.format_json()))


def fail(env):
    with pytest.raises(biyos.Error) as caught:
        env.set_option("seed", -1)
    return caught.value


def test_constants():
    kinds = {f"BIYOS_EVENT_{kind.name}": kind for kind in biyos.EventKind}
    levels = {f"BIYOS_LEVEL_{level.name}": level for level in biyos.Level}
    found = macros("events.h")
    assert len(found) == lines("events.h", "#define BIYOS_")
    assert found == kinds | levels


def test_error():
    seen = []
    with biyos.Environment() as env:
        env.subscribe(partial(record, seen), EventKind.MESSAGE, Level.ERROR)
        error = fail(env)
    message = (EventKind.MESSAGE, Level.ERROR, "capi", str(error), CODE)
    assert seen == [message]
    assert type(seen[0][0]) is EventKind
    assert type(seen[0][1]) is Level


def test_kind():
    seen = []
    with biyos.Environment() as env:
        env.subscribe(partial(record, seen), EventKind.CHECK, Level.DEBUG)
        fail(env)
    assert seen == []


def test_unsubscribe():
    seen = []
    with biyos.Environment() as env:
        function = partial(record, seen)
        subscription = env.subscribe(function, EventKind.MESSAGE, Level.DEBUG)
        env.unsubscribe(subscription)
        fail(env)
        with pytest.raises(biyos.Error) as caught:
            env.unsubscribe(subscription)
    assert seen == []
    assert caught.value.code == -100


@pytest.mark.parametrize("arguments", WRONG)
def test_wrong(arguments):
    with biyos.Environment() as env, pytest.raises(TypeError):
        env.subscribe(*arguments)


def test_unknown():
    with biyos.Environment() as env, pytest.raises(biyos.Error) as caught:
        env.subscribe(print, 9, Level.DEBUG)
    assert caught.value.code == -100


def test_overflow():
    seen = []
    with biyos.Environment() as env:
        function = partial(record, seen)
        first = env.subscribe(function, EventKind.MESSAGE, Level.DEBUG)
        with pytest.raises(OverflowError):
            env.subscribe(print, 2**32 + EventKind.MESSAGE, Level.DEBUG)
        with pytest.raises(OverflowError):
            env.unsubscribe(2**32 + first)
        fail(env)
    assert len(seen) == 1


@pytest.mark.parametrize("subscription", NUMBERS)
def test_number(subscription):
    with biyos.Environment() as env, pytest.raises(TypeError):
        env.unsubscribe(subscription)


@pytest.mark.parametrize("read", READERS)
def test_expired(read):
    kept = []
    with biyos.Environment() as env:
        env.subscribe(kept.append, EventKind.MESSAGE, Level.DEBUG)
        fail(env)
    with pytest.raises(ValueError, match="the event is no longer valid"):
        read(kept[0])


def test_formats():
    lines = []
    with biyos.Environment() as env:
        function = partial(keep_lines, lines)
        env.subscribe(function, EventKind.MESSAGE, Level.ERROR)
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
    with biyos.Environment() as env:
        env.subscribe(partial(record, seen), EventKind.MESSAGE, Level.DEBUG)
        gc.collect()
        fail(env)
    assert len(seen) == 1


def test_cleared():
    sink = Sink()
    alive = weakref.ref(sink)
    env = biyos.Environment()
    env.subscribe(sink, EventKind.MESSAGE, Level.DEBUG)
    del sink
    env.close()
    assert alive() is None


def test_freed():
    gc.disable()
    try:
        env = biyos.Environment()
        env.subscribe(Sink(), EventKind.MESSAGE, Level.DEBUG)
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
    seeds.append(env.get_option("seed"))


def raised(env):
    try:
        env.set_option("seed", -1)
    except RuntimeError:
        return True
    return False


def test_raised():
    with biyos.Environment() as env:
        env.subscribe(explode, EventKind.MESSAGE, Level.DEBUG)
        with pytest.raises(RuntimeError, match="subscriber failed") as caught:
            env.set_option("seed", -1)
        env.set_option("seed", 3)
        seed = env.get_option("seed")
    assert seed == 3
    context = caught.value.__context__
    assert type(context) is biyos.Error
    assert context.code == -100


def test_later():
    seen = []
    with biyos.Environment() as env:
        env.subscribe(explode, EventKind.MESSAGE, Level.DEBUG)
        env.subscribe(refuse, EventKind.MESSAGE, Level.DEBUG)
        env.subscribe(partial(record, seen), EventKind.MESSAGE, Level.DEBUG)
        with pytest.raises(RuntimeError):
            env.set_option("seed", -1)
    assert len(seen) == 1


def test_nested():
    seeds = []
    with biyos.Environment() as env:
        function = partial(read_seed, env, seeds)
        env.subscribe(explode, EventKind.MESSAGE, Level.DEBUG)
        env.subscribe(function, EventKind.MESSAGE, Level.DEBUG)
        with pytest.raises(RuntimeError):
            env.set_option("seed", -1)
    assert seeds == [0]


def test_base():
    with biyos.Environment() as env:
        env.subscribe(stop, EventKind.MESSAGE, Level.DEBUG)
        with pytest.raises(Stop):
            env.set_option("seed", -1)


def test_cycle():
    gc.disable()
    try:
        env = biyos.Environment()
        env.subscribe(explode, EventKind.MESSAGE, Level.DEBUG)
        assert raised(env)
        free = env._free
        del env
        assert not free.alive
    finally:
        gc.enable()


def test_stale():
    with biyos.Environment() as env:
        env._relay.run(explode, None)
        assert env.get_option("seed") == 0
