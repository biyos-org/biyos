import gc
import pickle
from ctypes import byref, c_int

import pytest

import biyos
from biyos import VariableKind
from biyos._capi import lib
from biyos._error import check


def test_pickle():
    with biyos.Env() as env, pytest.raises(biyos.Error) as caught:
        env.getopt("seeds")
    caught.value.__notes__ = ["while reading"]
    copy = pickle.loads(pickle.dumps(caught.value))
    assert type(copy) is biyos.Error
    assert str(copy) == str(caught.value)
    assert copy.code == caught.value.code
    assert copy.model is None
    assert copy.__notes__ == ["while reading"]


def test_undecodable():
    option = c_int()
    with biyos.Env() as env, pytest.raises(biyos.Error) as caught:
        status = lib.BIYOS_getoptid(env._handle, b"seed\xff", byref(option))
        check(env._handle, status, env._owners)
    message = "invalid argument: option 'seed\ufffd': unknown"
    assert str(caught.value) == message
    assert caught.value.code == -100


def fail(model):
    with pytest.raises(biyos.Error) as caught:
        model.addvar(1, [VariableKind.BINARY], ub=[2.0])
    return caught.value


def test_model():
    with biyos.Env() as env, biyos.Model(env) as model:
        error = fail(model)
        assert error.model is model
        assert pickle.loads(pickle.dumps(error)).model is None
        with pytest.raises(biyos.Error) as caught:
            env.getopt("seeds")
        assert caught.value.model is None


def test_gone():
    with biyos.Env() as env:
        model = biyos.Model(env)
        assert fail(model).model is not None
        del model
        gc.collect()
        with pytest.raises(biyos.Error) as caught:
            check(env._handle, -100, env._owners)
        assert caught.value.model is None
        assert "variable 0" in str(caught.value)
