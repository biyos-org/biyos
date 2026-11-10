import gc
from ctypes import CFUNCTYPE, POINTER, byref, c_int, c_void_p

import pytest

import biyos
from biyos._capi import lib

MESSAGE = 1
ERROR = 4
SUBSCRIBER = CFUNCTYPE(None, c_void_p, c_void_p)
SUBSCRIBE = CFUNCTYPE(
    c_int, c_void_p, SUBSCRIBER, c_void_p, c_int, c_int, POINTER(c_int)
)
subscribe = SUBSCRIBE(("biyos_subscribe", lib))


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


def test_collected():
    env = biyos.Environment()
    free = env._free
    del env
    gc.collect()
    assert not free.alive


def test_refused_close():
    refused = []

    def close_inside(event, user_data):
        try:
            env.close()
        except biyos.Error as error:
            refused.append(error)

    callback = SUBSCRIBER(close_inside)
    with biyos.Environment() as env:
        subscription = c_int()
        status = subscribe(
            env._handle, callback, None, MESSAGE, ERROR, byref(subscription)
        )
        assert status == 0
        with pytest.raises(biyos.Error) as caught:
            env.set_option("seed", -1)
        assert str(caught.value).startswith("invalid argument: option 'seed'")
        assert len(refused) == 1
        assert refused[0].code == -100
        message = "invalid argument: environment: is read-only"
        assert str(refused[0]).startswith(message)
        assert env.get_option("seed") == 0
    assert not env._free.alive
