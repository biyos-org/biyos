from ctypes import byref, c_int

import pytest

import biyos
from biyos._capi import OK, lib

INVALID_ARGUMENT = -100

DEFAULTS = {
    "seed": 0,
    "time_limit": float("inf"),
    "check_level": "cheap",
    "presolve": True,
    "reproducer": "",
}


@pytest.fixture
def env():
    with biyos.Environment() as env:
        yield env


def option_count(env):
    count = 0
    kind = c_int()
    while lib.biyos_option_kind(env._handle, count + 1, byref(kind)) == OK:
        count += 1
    return count


def test_complete(env):
    assert option_count(env) == len(DEFAULTS)


@pytest.mark.parametrize("name, default", DEFAULTS.items())
def test_default(env, name, default):
    value = env.get_option(name)
    assert type(value) is type(default)
    assert value == default


def test_unknown_name(env):
    with pytest.raises(biyos.Error) as caught:
        env.get_option("seeds")
    assert str(caught.value) == "invalid argument: option 'seeds': unknown"
    assert caught.value.code == INVALID_ARGUMENT


def test_name_type(env):
    with pytest.raises(TypeError, match="option name must be a str, not int"):
        env.get_option(1)


def test_null_name(env):
    with pytest.raises(ValueError, match="embedded null character"):
        env.get_option("seed\0")
