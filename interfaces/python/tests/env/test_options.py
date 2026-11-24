from ctypes import byref, c_int
from fractions import Fraction
from functools import partial

import pytest

import biyos
from biyos import EventKind, EventLevel, OptionSource
from biyos._capi import OPTKND_FLG, OK, lib
from biyos._versions import version_text
from number import Number
from refusal import LOCKED

INVALID_ARGUMENT = -100

DEFAULTS = {
    "seed": 0,
    "maxtime": biyos.inf,
    "checking": biyos.Checking.CHEAP,
    "presolve": True,
    "repro": "",
    "feastol_abs": 1e-6,
    "feastol_rel": 1e-6,
    "inttol": 1e-5,
    "zerotol": 1e-9,
    "maxcputime": biyos.inf,
}

VALUES = {
    "seed": 42,
    "maxtime": 3.5,
    "checking": biyos.Checking.FULL,
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
    ("checking", "full"),
    ("checking", 2.0),
    ("presolve", 1),
    ("repro", b"replay.txt"),
]

NUMBERS = [(0, None), (100, "0.1.0"), (10203, "1.2.3"), (990099, "99.0.99")]

HELD = {
    "seed": "an int",
    "maxtime": "a double",
    "checking": "an enum",
    "presolve": "a bool",
    "repro": "a string",
}

NOT_PAIRS = [3, (1,), (1, 2, 3), [1, 2], "on"]

NOT_MASKS = [("1", 0), (True, 0), (0, 1.0), (0, None)]

WIDE = [(-1, 0), (0, -1), (2**64, 0), (0, 2**64 + 1)]


@pytest.fixture
def env():
    with biyos.Env() as env:
        yield env


@pytest.fixture
def model(env):
    with biyos.Model(env) as model:
        yield model


def as_flags(env, monkeypatch, name):
    option = env._option(name)
    monkeypatch.setattr(env, "_lookup", lambda _: (option, OPTKND_FLG))


def n_opts(env):
    count = 0
    kind = c_int()
    while lib.BIYOS_getoptknd(env._handle, count + 1, byref(kind)) == OK:
        count += 1
    return count


def test_complete(env):
    assert n_opts(env) == len(DEFAULTS)


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
    message = "invalid argument: option 'seed': -1 is outside [0, 2147483647]"
    with pytest.raises(biyos.Error) as caught:
        env.setopt("seed", -1)
    assert str(caught.value) == message
    assert caught.value.code == INVALID_ARGUMENT


@pytest.mark.parametrize("checking", list(biyos.Checking))
def test_enum(env, checking):
    env.setopt("checking", int(checking))
    assert env.getopt("checking") is checking


@pytest.mark.parametrize("number", [4, 0, -1])
def test_bad_enum(env, number):
    env.setopt("checking", biyos.Checking.FULL)
    message = (
        f"invalid argument: option 'checking': {number} "
        "is not one of 1 (none), 2 (cheap), 3 (full)"
    )
    with pytest.raises(biyos.Error) as caught:
        env.setopt("checking", number)
    assert str(caught.value) == message
    assert caught.value.code == INVALID_ARGUMENT
    assert env.getopt("checking") is biyos.Checking.FULL


def test_enum_message(env):
    message = "option 'checking' takes an int, not str"
    with pytest.raises(TypeError, match=message):
        env.setopt("checking", "full")


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


def test_masks():
    masks = biyos.Masks(on=5, off=2)
    assert masks == (5, 2)
    assert (masks.on, masks.off) == (5, 2)
    assert biyos.Masks._fields == ("on", "off")


@pytest.mark.parametrize("name, held", HELD.items())
def test_flags_kind(env, monkeypatch, name, held):
    message = f"invalid argument: option '{name}': is {held}, not flags"
    as_flags(env, monkeypatch, name)
    with pytest.raises(biyos.Error) as caught:
        env.setopt(name, biyos.Masks(1, 2))
    assert str(caught.value) == message
    assert caught.value.code == INVALID_ARGUMENT
    with pytest.raises(biyos.Error) as caught:
        env.getopt(name)
    assert str(caught.value) == message
    assert caught.value.code == INVALID_ARGUMENT


def test_full_masks(env, monkeypatch):
    as_flags(env, monkeypatch, "seed")
    with pytest.raises(biyos.Error, match="is an int, not flags"):
        env.setopt("seed", (2**64 - 1, 0))


@pytest.mark.parametrize("value", NOT_PAIRS)
def test_not_pair(env, monkeypatch, value):
    as_flags(env, monkeypatch, "seed")
    found = type(value).__name__
    message = rf"option 'seed' takes a pair \(on, off\), not {found}"
    with pytest.raises(TypeError, match=message):
        env.setopt("seed", value)
    monkeypatch.undo()
    assert env.getopt("seed") == 0


@pytest.mark.parametrize("value", NOT_MASKS)
def test_not_mask(env, monkeypatch, value):
    as_flags(env, monkeypatch, "seed")
    message = "option 'seed' o(n|ff) must be an int"
    with pytest.raises(TypeError, match=message):
        env.setopt("seed", value)
    monkeypatch.undo()
    assert env.getopt("seed") == 0


@pytest.mark.parametrize("value", WIDE)
def test_wide_mask(env, monkeypatch, value):
    as_flags(env, monkeypatch, "seed")
    with pytest.raises(OverflowError, match="does not fit in a BIYOS_mask"):
        env.setopt("seed", value)
    monkeypatch.undo()
    assert env.getopt("seed") == 0


@pytest.mark.parametrize("name, default", DEFAULTS.items())
def test_model_default(model, name, default):
    value = model.getopt(name)
    assert type(value) is type(default)
    assert value == default
    assert model.getoptsrc(name) is OptionSource.DEFAULT


@pytest.mark.parametrize("name, value", VALUES.items())
def test_model_value(env, model, name, value):
    model.setopt(name, value)
    found = model.getopt(name)
    assert type(found) is type(value)
    assert found == value
    assert model.getoptsrc(name) is OptionSource.MODEL
    assert env.getopt(name) == DEFAULTS[name]


@pytest.mark.parametrize("name, value", WRONG)
def test_model_kind(model, name, value):
    with pytest.raises(TypeError):
        model.setopt(name, value)
    assert model.getoptsrc(name) is OptionSource.DEFAULT


def test_model_flags(env, model, monkeypatch):
    as_flags(env, monkeypatch, "seed")
    with pytest.raises(biyos.Error, match="is an int, not flags"):
        model.setopt("seed", biyos.Masks(1, 2))
    with pytest.raises(biyos.Error, match="is an int, not flags"):
        model.getopt("seed")
    with pytest.raises(TypeError, match="takes a pair"):
        model.setopt("seed", 3)
    monkeypatch.undo()
    assert model.getoptsrc("seed") is OptionSource.DEFAULT


def test_model_name(model):
    with pytest.raises(biyos.Error) as caught:
        model.getopt("seeds")
    assert str(caught.value) == "invalid argument: option 'seeds': unknown"
    assert caught.value.code == INVALID_ARGUMENT
    assert caught.value.model is model
    with pytest.raises(biyos.Error) as caught:
        model.setopt("nope", 1)
    assert caught.value.model is model
    with pytest.raises(TypeError, match="option name must be a str, not int"):
        model.delopt(1)


def test_sources(env, model):
    assert model.getoptsrc("maxtime") is OptionSource.DEFAULT
    env.setopt("maxtime", 10.0)
    assert model.getoptsrc("maxtime") is OptionSource.ENVIRONMENT
    model.setopt("maxtime", 5.0)
    assert model.getoptsrc("maxtime") is OptionSource.MODEL
    model.delopt("maxtime")
    assert model.getoptsrc("maxtime") is OptionSource.ENVIRONMENT
    env.setopt("seed", 0)
    assert model.getoptsrc("seed") is OptionSource.ENVIRONMENT


def test_deleted(env, model):
    env.setopt("presolve", False)
    model.setopt("presolve", True)
    model.delopt("presolve")
    assert model.getopt("presolve") is False
    model.delopt("presolve")
    assert model.getopt("presolve") is False
    model.delopt("seed")
    assert model.getoptsrc("seed") is OptionSource.DEFAULT


def test_shared(env, model):
    env.setopt("checking", biyos.Checking.FULL)
    assert model.getopt("checking") is biyos.Checking.FULL
    model.setopt("checking", biyos.Checking.NONE)
    env.setopt("checking", biyos.Checking.CHEAP)
    assert model.getopt("checking") is biyos.Checking.NONE
    with biyos.Model(env) as other:
        assert other.getopt("checking") is biyos.Checking.CHEAP


def test_model_refused(env, model):
    message = "invalid argument: option 'seed': -1 is outside [0, 2147483647]"
    env.setopt("seed", 7)
    model.setopt("seed", 3)
    with pytest.raises(biyos.Error) as caught:
        model.setopt("seed", -1)
    assert str(caught.value) == message
    assert caught.value.model is model
    assert model.getopt("seed") == 3
    model.delopt("seed")
    with pytest.raises(biyos.Error):
        model.setopt("seed", -1)
    assert model.getoptsrc("seed") is OptionSource.ENVIRONMENT
    assert model.getopt("seed") == 7


def test_copied(env, model):
    model.setopt("maxtime", 5.0)
    model.setopt("repro", "replay.txt")
    env.setopt("seed", 9)
    with model.copy() as copy:
        model.setopt("maxtime", 6.0)
        assert copy.getopt("maxtime") == 5.0
        assert copy.getopt("repro") == "replay.txt"
        assert copy.getoptsrc("repro") is OptionSource.MODEL
        assert copy.getopt("seed") == 9
        assert copy.getoptsrc("seed") is OptionSource.ENVIRONMENT


def test_moved(env, model):
    model.setopt("maxtime", 5.0)
    env.setopt("seed", 9)
    with biyos.Env() as other:
        other.setopt("seed", 4)
        with model.copy(other) as moved:
            assert moved.getopt("maxtime") == 5.0
            assert moved.getopt("seed") == 4
            assert moved.getoptsrc("seed") is OptionSource.ENVIRONMENT
            assert moved.getoptsrc("presolve") is OptionSource.DEFAULT


def test_model_strings(env, model):
    env.setopt("repro", "a.txt")
    assert model.getopt("repro") == "a.txt"
    model.setopt("repro", "b.txt")
    env.setopt("repro", "c.txt")
    assert model.getopt("repro") == "b.txt"
    assert env.getopt("repro") == "c.txt"
    model.setopt("repro", "réplique.txt")
    assert model.getopt("repro") == "réplique.txt"
    with pytest.raises(ValueError, match="embedded null character"):
        model.setopt("repro", "replay\0.txt")
    assert model.getopt("repro") == "réplique.txt"


def test_model_locked(env, model):
    seen = []

    def change(event):
        changes = [partial(model.setopt, "seed", 3)]
        changes.append(partial(model.delopt, "seed"))
        for call in changes:
            try:
                call()
            except biyos.Error as error:
                seen.append(str(error))
        seen.append(model.getopt("seed"))
        seen.append(model.getoptsrc("seed"))

    model.setopt("seed", 5)
    env.addhook(change, EventKind.MESSAGE, EventLevel.ERROR)
    with pytest.raises(biyos.Error):
        env.setopt("seed", -1)
    assert seen == [LOCKED, LOCKED, 5, OptionSource.MODEL]


@pytest.mark.parametrize("closing", ["model", "env"])
def test_model_closed(closing):
    env = biyos.Env()
    model = biyos.Model(env)
    {"model": model, "env": env}[closing].close()
    calls = [partial(model.setopt, "seed", 1), partial(model.getopt, "seed")]
    calls.append(partial(model.delopt, "seed"))
    calls.append(partial(model.getoptsrc, "seed"))
    for call in calls:
        with pytest.raises(ValueError, match="the model is closed"):
            call()
    env.close()
