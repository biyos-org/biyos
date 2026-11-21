from ctypes import byref, c_int
from fractions import Fraction

import pytest

import biyos
from biyos._capi import OK, lib
from biyos._versions import version_text
from number import Number

INVALID_ARGUMENT = -100

DEFAULTS = {
    "seed": 0,
    "maxtime": float("inf"),
    "check_lvl": "cheap",
    "presolve": True,
    "repro": "",
    "feastol_abs": 1e-6,
    "feastol_rel": 1e-6,
    "inttol": 1e-5,
    "zerotol": 1e-9,
    "maxcputime": float("inf"),
}

VALUES = {
    "seed": 42,
    "maxtime": 3.5,
    "check_lvl": "full",
    "presolve": False,
    "repro": "replay.txt",
    "feastol_abs": 1e-7,
    "feastol_rel": 1e-8,
    "inttol": 1e-4,
    "zerotol": 1e-12,
    "maxcputime": 60.0,
}

WRONG = [
    ("seed", 1.5),
    ("seed", True),
    ("seed", "3"),
    ("maxtime", True),
    ("maxtime", "10"),
    ("check_lvl", 2),
    ("presolve", 1),
    ("repro", b"replay.txt"),
]

NUMBERS = [(0, None), (100, "0.1.0"), (10203, "1.2.3"), (990099, "99.0.99")]


@pytest.fixture
def env():
    with biyos.Env() as env:
        yield env


def option_count(env):
    count = 0
    kind = c_int()
    while lib.BIYOS_getoptknd(env._handle, count + 1, byref(kind)) == OK:
        count += 1
    return count


def test_complete(env):
    assert option_count(env) == len(DEFAULTS)


@pytest.mark.parametrize("name, default", DEFAULTS.items())
def test_default(env, name, default):
    value = env.getopt(name)
    assert type(value) is type(default)
    assert value == default


def test_unknown_name(env):
    with pytest.raises(biyos.Error) as caught:
        env.getopt("seeds")
    assert str(caught.value) == "invalid argument: option 'seeds': unknown"
    assert caught.value.code == INVALID_ARGUMENT


def test_name_kind(env):
    with pytest.raises(TypeError, match="option name must be a str, not int"):
        env.getopt(1)


def test_null_name(env):
    with pytest.raises(ValueError, match="embedded null character"):
        env.getopt("seed\0")


@pytest.mark.parametrize("name, value", VALUES.items())
def test_round_trip(env, name, value):
    env.setopt(name, value)
    found = env.getopt(name)
    assert type(found) is type(value)
    assert found == value


def test_int_double(env):
    env.setopt("maxtime", 10)
    value = env.getopt("maxtime")
    assert type(value) is float
    assert value == 10.0


@pytest.mark.parametrize("name, value", WRONG)
def test_wrong_kind(env, name, value):
    with pytest.raises(TypeError):
        env.setopt(name, value)
    assert env.getopt(name) == DEFAULTS[name]


def test_kind_message(env):
    message = "option 'seed' takes an int, not bool"
    with pytest.raises(TypeError, match=message):
        env.setopt("seed", True)
    message = "option 'seed' takes an int, not str"
    with pytest.raises(TypeError, match=message):
        env.setopt("seed", "3")


def test_index(env):
    env.setopt("seed", Number(5))
    assert env.getopt("seed") == 5


def test_real(env):
    env.setopt("maxtime", Fraction(7, 2))
    assert env.getopt("maxtime") == 3.5


def test_bad_value(env):
    message = (
        "invalid argument: option 'seed': "
        "-1 is outside [0, 2147483647]"
    )
    with pytest.raises(biyos.Error) as caught:
        env.setopt("seed", -1)
    assert str(caught.value) == message
    assert caught.value.code == INVALID_ARGUMENT


def test_bad_enum(env):
    message = (
        "invalid argument: option 'check_lvl': "
        "'most' is not one of none, cheap, full"
    )
    with pytest.raises(biyos.Error) as caught:
        env.setopt("check_lvl", "most")
    assert str(caught.value) == message
    assert caught.value.code == INVALID_ARGUMENT


@pytest.mark.parametrize("value", [2**31, 2**32 + 5])
def test_overflow(env, value):
    with pytest.raises(OverflowError):
        env.setopt("seed", value)
    assert env.getopt("seed") == 0


def test_null_value(env):
    with pytest.raises(ValueError, match="embedded null character"):
        env.setopt("repro", "replay\0.txt")
    assert env.getopt("repro") == ""


def test_unicode(env):
    env.setopt("repro", "réplique.txt")
    assert env.getopt("repro") == "réplique.txt"


@pytest.mark.parametrize("name", DEFAULTS)
def test_versions(env, name):
    versions = env.getoptver(name)
    assert versions == biyos.Versions("0.1.0", None, None)
    assert versions.since == "0.1.0"


@pytest.mark.parametrize("number, text", NUMBERS)
def test_version_text(number, text):
    assert version_text(number) == text


def test_unknown_versions(env):
    with pytest.raises(biyos.Error) as caught:
        env.getoptver("seeds")
    assert caught.value.code == INVALID_ARGUMENT


def test_name_versions(env):
    with pytest.raises(TypeError):
        env.getoptver(1)
