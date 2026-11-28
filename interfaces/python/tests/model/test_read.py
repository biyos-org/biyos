from functools import partial

import pytest

import biyos
from biyos import EventKind, EventLevel, FileFormat, OptionSource
from samples import LP_MPS, LP_NL, ROWS_MPS, written

FILEIO = -401
BADARG = -100
FIXED_MPS = """NAME          fixed
ROWS
 N  tot cost
 L  cap one
COLUMNS
    x 1       tot cost  1.0            cap one   2.0
ENDATA
"""


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


def test_fixed(env, tmp_path):
    path = written(tmp_path / "fixed.mps", FIXED_MPS)
    with biyos.Model.read(env, path, FileFormat.FIXED_MPS) as model:
        assert model.getvarname(range(1)) == ["x 1"]
        assert model.getconname(range(1)) == ["cap one"]
        assert model.getobjname(range(1)) == ["tot cost"]


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


def test_repro(env, tmp_path):
    path = tmp_path / "run.repro"
    source = written(tmp_path / "lp.mps", LP_MPS)
    env.setopt("seed", 7)
    with biyos.Model.read(env, source, FileFormat.MPS) as model:
        model.setopt("presolve", False)
        model.writerepro(path)
        with biyos.Env() as other:
            other.setopt("maxtime", 5.0)
            with biyos.Model.readrepro(other, str(path)) as back:
                assert model.same(back)
                assert back.getopt("presolve") is False
                assert back.getoptsrc("presolve") is OptionSource.MODEL
            assert other.getopt("seed") == 7
            assert other.getopt("presolve") is True
            assert other.getopt("maxtime") == float("inf")


def test_repro_warning(env, tmp_path):
    path = written(
        tmp_path / "old.repro",
        "REPRO 1\nVERSION 0.0.1\nOPTIONS 1\nnosuch model 1\nBIYOS 1\nEND\n",
    )
    heard = []
    env.addhook(partial(record, heard), EventKind.MESSAGE, EventLevel.WARNING)
    with biyos.Model.readrepro(env, path):
        pass
    file = ("file", str(path))
    lines = [("io", (file, ("line", line))) for line in ("2", "4")]
    assert heard == lines


def test_repro_refused(env, tmp_path):
    with pytest.raises(biyos.Error) as caught:
        biyos.Model.readrepro(env, tmp_path / "absent.repro")
    assert caught.value.code == FILEIO
    with pytest.raises(TypeError):
        biyos.Model.readrepro(env, b"run.repro")
    with pytest.raises(TypeError):
        biyos.Model.readrepro(None, tmp_path / "run.repro")
    with biyos.Model(env) as model:
        with pytest.raises(biyos.Error) as caught:
            model.writerepro(tmp_path)
        assert caught.value.code == FILEIO
        with pytest.raises(ValueError):
            model.writerepro("run\0.repro")
