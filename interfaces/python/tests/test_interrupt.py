from concurrent.futures import ThreadPoolExecutor
from functools import partial

import pytest

import biyos
from biyos import EventKind, EventLevel
from biyos._capi import lib


@pytest.fixture
def calls(monkeypatch):
    calls = []
    spy = partial(recorded, calls, lib.BIYOS_interrupt)
    monkeypatch.setattr(lib, "BIYOS_interrupt", spy)
    return calls


def recorded(calls, function, handle):
    calls.append(handle.value)
    return function(handle)


def interrupt_aside(env, event):
    with ThreadPoolExecutor(1) as pool:
        pool.submit(env.interrupt).result()


def test_interrupt(calls):
    with biyos.Env() as env:
        assert env.interrupt() is None
        assert calls == [env._handle.value]
        assert env.getopt("seed") == 0


def test_closed(calls):
    env = biyos.Env()
    env.close()
    with pytest.raises(ValueError, match="the environment is closed"):
        env.interrupt()
    assert calls == []


def test_thread(calls):
    with biyos.Env() as env:
        hook = partial(interrupt_aside, env)
        env.addhook(hook, EventKind.MESSAGE, EventLevel.ERROR)
        with pytest.raises(biyos.Error, match="option 'seed'"):
            env.setopt("seed", -1)
        assert calls == [env._handle.value]
