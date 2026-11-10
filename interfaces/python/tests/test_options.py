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
    "feasibility_abs": 1e-6,
    "feasibility_rel": 1e-6,
    "integrality_tol": 1e-5,
    "zero_tol": 1e-9,
    "cpu_time_limit": float("inf"),
}

VALUES = {
    "seed": 42,
    "time_limit": 3.5,
    "check_level": "full",
    "presolve": False,
    "reproducer": "replay.txt",
    "feasibility_abs": 1e-7,
    "feasibility_rel": 1e-8,
    "integrality_tol": 1e-4,
    "zero_tol": 1e-12,
    "cpu_time_limit": 60.0,
}

WRONG = [
    ("seed", 1.5),
    ("seed", True),
    ("seed", "3"),
    ("time_limit", True),
    ("time_limit", "10"),
    ("check_level", 2),
    ("presolve", 1),
    ("reproducer", b"replay.txt"),
]


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


def test_name_kind(env):
    with pytest.raises(TypeError, match="option name must be a str, not int"):
        env.get_option(1)


def test_null_name(env):
    with pytest.raises(ValueError, match="embedded null character"):
        env.get_option("seed\0")


@pytest.mark.parametrize("name, value", VALUES.items())
def test_round_trip(env, name, value):
    env.set_option(name, value)
    found = env.get_option(name)
    assert type(found) is type(value)
    assert found == value


def test_int_double(env):
    env.set_option("time_limit", 10)
    value = env.get_option("time_limit")
    assert type(value) is float
    assert value == 10.0


@pytest.mark.parametrize("name, value", WRONG)
def test_wrong_kind(env, name, value):
    with pytest.raises(TypeError):
        env.set_option(name, value)
    assert env.get_option(name) == DEFAULTS[name]


def test_kind_message(env):
    message = "option 'seed' takes an int, not bool"
    with pytest.raises(TypeError, match=message):
        env.set_option("seed", True)


def test_bad_value(env):
    message = (
        "invalid argument: option 'seed': "
        "-1 is outside [0, 2147483647]"
    )
    with pytest.raises(biyos.Error) as caught:
        env.set_option("seed", -1)
    assert str(caught.value) == message
    assert caught.value.code == INVALID_ARGUMENT


def test_bad_enum(env):
    message = (
        "invalid argument: option 'check_level': "
        "'most' is not one of none, cheap, full"
    )
    with pytest.raises(biyos.Error) as caught:
        env.set_option("check_level", "most")
    assert str(caught.value) == message
    assert caught.value.code == INVALID_ARGUMENT


@pytest.mark.parametrize("value", [2**31, 2**32 + 5])
def test_overflow(env, value):
    with pytest.raises(OverflowError):
        env.set_option("seed", value)
    assert env.get_option("seed") == 0


def test_null_value(env):
    with pytest.raises(ValueError, match="embedded null character"):
        env.set_option("reproducer", "replay\0.txt")
    assert env.get_option("reproducer") == ""


def test_unicode(env):
    env.set_option("reproducer", "réplique.txt")
    assert env.get_option("reproducer") == "réplique.txt"
