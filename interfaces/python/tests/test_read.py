from pathlib import Path

from functools import partial

import pytest

import biyos
from biyos import EventKind, EventLevel, FileFormat

DATA = Path(__file__).resolve().parents[3] / "tests" / "io" / "data" / "mps"
FILEIO = -401
BADARG = -100


@pytest.fixture
def env():
    with biyos.Env() as env:
        yield env


def record(heard, event):
    heard.append((event.module, event.fields))


def test_counts(env):
    with biyos.Model.read(env, DATA / "lp.mps", FileFormat.MPS) as model:
        assert model.getmdlname() == "small"
        assert model.getnvar() == 2
        assert model.getncon() == 3
        assert model.getnobj() == 1
        assert model.getvarname(range(2)) == ["x", "y"]
        assert model.getconname(range(3)) == ["limit", "floor", "balance"]
        assert model.getobjname(range(1)) == ["cost"]


def test_str_path(env):
    with biyos.Model.read(env, str(DATA / "rows.mps"), FileFormat.MPS) as model:
        assert model.getmdlname() is None
        assert model.getncon() == 3


def test_missing(env):
    path = str(DATA / "absent.mps")
    with pytest.raises(biyos.Error) as caught:
        biyos.Model.read(env, path, FileFormat.MPS)
    assert caught.value.code == FILEIO
    assert str(caught.value) == f"file access: file '{path}': cannot be opened"


def test_unknown_format(env):
    with pytest.raises(biyos.Error) as caught:
        biyos.Model.read(env, DATA / "lp.mps", 7)
    assert caught.value.code == BADARG


def test_refused(env):
    with pytest.raises(TypeError):
        biyos.Model.read(env, b"lp.mps", FileFormat.MPS)
    with pytest.raises(TypeError):
        biyos.Model.read(env, DATA / "lp.mps", "MPS")
    with pytest.raises(ValueError):
        biyos.Model.read(env, "lp\0.mps", FileFormat.MPS)
    with pytest.raises(TypeError):
        biyos.Model.read(None, DATA / "lp.mps", FileFormat.MPS)


def test_warning(env, tmp_path):
    path = tmp_path / "warn.mps"
    path.write_text("NAME t\nROWS\n N f\nCOLUMNS\n x f 1\nENDATA\n")
    heard = []
    env.addhook(partial(record, heard), EventKind.MESSAGE, EventLevel.WARNING)
    with biyos.Model.read(env, path, FileFormat.MPS):
        pass
    assert heard == [("io", (("file", str(path)), ("line", "6")))]
