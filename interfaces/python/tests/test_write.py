from pathlib import Path

import pytest

import biyos
from biyos import FileFormat

DATA = Path(__file__).resolve().parents[3] / "tests" / "io" / "data" / "mps"
FILEIO = -401
BADARG = -100


@pytest.fixture
def env():
    with biyos.Env() as env:
        yield env


def summary(model):
    variables = range(model.getnvar())
    constraints = range(model.getncon())
    objectives = range(model.getnobj())
    return (
        model.getmdlname(),
        model.getvarname(variables),
        model.getvarbnd(variables),
        model.getvarknd(variables),
        model.getconname(constraints),
        model.getconbnd(constraints),
        model.getobjname(objectives),
        model.getobjsense(objectives),
        model.getcst(range(model.getnfun())),
        model.getrlinterm(),
    )


def test_round_trip(env, tmp_path):
    path = tmp_path / "lp.mps"
    with biyos.Model.read(env, DATA / "lp.mps", FileFormat.MPS) as model:
        model.write(path, FileFormat.MPS)
        with biyos.Model.read(env, path, FileFormat.MPS) as back:
            assert summary(back) == summary(model)


def test_unwritable(env, tmp_path):
    path = str(tmp_path / "absent" / "lp.mps")
    with biyos.Model(env) as model:
        with pytest.raises(biyos.Error) as caught:
            model.write(path, FileFormat.MPS)
    assert caught.value.code == FILEIO
    assert str(caught.value) == f"file access: file '{path}': cannot be opened"


def test_refused(env, tmp_path):
    with biyos.Model(env) as model:
        with pytest.raises(biyos.Error) as caught:
            model.write(tmp_path / "lp.mps", 7)
        assert caught.value.code == BADARG
        with pytest.raises(TypeError):
            model.write(b"lp.mps", FileFormat.MPS)
        with pytest.raises(TypeError):
            model.write(tmp_path / "lp.mps", "MPS")
        with pytest.raises(ValueError):
            model.write("lp\0.mps", FileFormat.MPS)
