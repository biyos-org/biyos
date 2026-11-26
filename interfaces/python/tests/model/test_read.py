from functools import partial

import pytest

import biyos
from biyos import EventKind, EventLevel, FileFormat
from samples import LP_MPS, LP_NL, ROWS_MPS, written

FILEIO = -401
BADARG = -100


@pytest.fixture
def env():
    with biyos.Env() as env:
        yield env


def record(heard, event):
    heard.append((event.module, event.fields))


def test_counts(env, tmp_path):
    path = written(tmp_path / "lp.mps", LP_MPS)
    with biyos.Model.read(env, path, FileFormat.MPS) as model:
        assert model.getname() == "small"
        assert model.getnvar() == 2
        assert model.getncon() == 3
        assert model.getnobj() == 1
        assert model.getvarname(range(2)) == ["x", "y"]
        assert model.getconname(range(3)) == ["limit", "floor", "balance"]
        assert model.getobjname(range(1)) == ["cost"]


def test_nl(env, tmp_path):
    path = written(tmp_path / "lp.nl", LP_NL)
    with biyos.Model.read(env, path, FileFormat.NL) as model:
        assert model.getnvar() == 2
        assert model.getncon() == 2
        assert model.getnobj() == 1
        assert model.getvarbnd(range(2)) == ([0.0, 0.0], [10.0, float("inf")])
        assert model.getobjsense(range(1)) == [biyos.ObjectiveSense.MINIMIZE]


def test_str_path(env, tmp_path):
    path = str(written(tmp_path / "rows.mps", ROWS_MPS))
    with biyos.Model.read(env, path, FileFormat.MPS) as model:
        assert model.getname() is None
        assert model.getncon() == 3


def test_missing(env, tmp_path):
    path = str(tmp_path / "absent.mps")
    with pytest.raises(biyos.Error) as caught:
        biyos.Model.read(env, path, FileFormat.MPS)
    assert caught.value.code == FILEIO
    assert str(caught.value) == f"file access: file '{path}': cannot be opened"


def test_unknown_format(env, tmp_path):
    path = written(tmp_path / "lp.mps", LP_MPS)
    with pytest.raises(biyos.Error) as caught:
        biyos.Model.read(env, path, 7)
    assert caught.value.code == BADARG


def test_refused(env, tmp_path):
    path = written(tmp_path / "lp.mps", LP_MPS)
    with pytest.raises(TypeError):
        biyos.Model.read(env, b"lp.mps", FileFormat.MPS)
    with pytest.raises(TypeError):
        biyos.Model.read(env, path, "MPS")
    with pytest.raises(ValueError):
        biyos.Model.read(env, "lp\0.mps", FileFormat.MPS)
    with pytest.raises(TypeError):
        biyos.Model.read(None, path, FileFormat.MPS)


def test_warning(env, tmp_path):
    path = tmp_path / "warn.mps"
    path.write_text("NAME t\nROWS\n N f\nCOLUMNS\n x f 1\nENDATA\n")
    heard = []
    env.addhook(partial(record, heard), EventKind.MESSAGE, EventLevel.WARNING)
    with biyos.Model.read(env, path, FileFormat.MPS):
        pass
    assert heard == [("io", (("file", str(path)), ("line", "6")))]
