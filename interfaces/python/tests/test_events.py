import gc
import weakref
from functools import partial

import pytest

import biyos
from biyos import EventKind, Level
from header import macros

CODE = (("code", "-100"),)

WRONG = [
    ("record", EventKind.MESSAGE, Level.DEBUG),
    (print, True, Level.DEBUG),
    (print, "message", Level.DEBUG),
    (print, EventKind.MESSAGE, 1.0),
    (print, EventKind.MESSAGE, None),
]

NUMBERS = [True, "1", 1.0, None]


class Sink:
    def __call__(self, event):
        pass


def snapshot(event):
    return event.kind, event.level, event.module, event.text, event.fields


def record(seen, event):
    seen.append(snapshot(event))


def fail(env):
    with pytest.raises(biyos.Error) as caught:
        env.set_option("seed", -1)
    return caught.value


def test_constants():
    kinds = {f"BIYOS_EVENT_{kind.name}": kind for kind in biyos.EventKind}
    levels = {f"BIYOS_LEVEL_{level.name}": level for level in biyos.Level}
    assert macros("events.h") == kinds | levels


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


def test_expired():
    kept = []
    with biyos.Environment() as env:
        env.subscribe(kept.append, EventKind.MESSAGE, Level.DEBUG)
        fail(env)
    with pytest.raises(ValueError, match="the event is no longer valid"):
        kept[0].text


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
