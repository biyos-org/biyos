import pickle
from ctypes import byref, c_int

import pytest

import biyos
from biyos._capi import lib
from biyos._error import check


def test_pickle():
    with biyos.Environment() as env, pytest.raises(biyos.Error) as caught:
        env.get_option("seeds")
    copy = pickle.loads(pickle.dumps(caught.value))
    assert type(copy) is biyos.Error
    assert str(copy) == str(caught.value)
    assert copy.code == caught.value.code


def test_undecodable():
    option = c_int()
    with biyos.Environment() as env, pytest.raises(biyos.Error) as caught:
        status = lib.biyos_option_id(env._handle, b"seed\xff", byref(option))
        check(env._handle, status)
    message = "invalid argument: option 'seed\ufffd': unknown"
    assert str(caught.value) == message
    assert caught.value.code == -100
