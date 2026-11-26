import pytest

import biyos
from biyos import FileFormat
from samples import LP_MPS, STRUCTURE_MPS, written

UNSUPPORTED = -201
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
        model.getname(),
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
    source = written(tmp_path / "source.mps", LP_MPS)
    path = tmp_path / "lp.mps"
    with biyos.Model.read(env, source, FileFormat.MPS) as model:
        model.write(path, FileFormat.MPS)
        with biyos.Model.read(env, path, FileFormat.MPS) as back:
            assert summary(back) == summary(model)


def test_native(env, tmp_path):
    source = written(tmp_path / "source.mps", STRUCTURE_MPS)
    path = tmp_path / "structure.biyos"
    with biyos.Model.read(env, source, FileFormat.MPS) as model:
        model.write(path, FileFormat.NATIVE)
        with biyos.Model.read(env, path, FileFormat.NATIVE) as back:
            assert model.compare(back) is None


def structure(model):
    sets = range(model.getnsos())
    names = model.getsosname(sets)
    return summary(model), model.getind(), model.getsos(), names


def test_structure(env, tmp_path):
    source = written(tmp_path / "source.mps", STRUCTURE_MPS)
    path = tmp_path / "structure.mps"
    with biyos.Model.read(env, source, FileFormat.MPS) as model:
        assert model.getnind() == 2
        assert model.getsosname(range(2)) == ["pick", None]
        model.write(path, FileFormat.MPS)
        with biyos.Model.read(env, path, FileFormat.MPS) as back:
            assert structure(back) == structure(model)


def test_unwritable(env, tmp_path):
    path = str(tmp_path / "absent" / "lp.mps")
    with biyos.Model(env) as model, pytest.raises(biyos.Error) as caught:
        model.write(path, FileFormat.MPS)
    assert caught.value.code == FILEIO
    assert str(caught.value) == f"file access: file '{path}': cannot be opened"


def test_fixed_read_only(env, tmp_path):
    path = str(tmp_path / "lp.mps")
    with biyos.Model(env) as model, pytest.raises(biyos.Error) as caught:
        model.write(path, FileFormat.FIXED_MPS)
    assert caught.value.code == UNSUPPORTED
    reason = "fixed MPS files are read, never written"
    assert str(caught.value) == f"unsupported: file '{path}': {reason}"


def test_nl_read_only(env, tmp_path):
    path = str(tmp_path / "lp.nl")
    with biyos.Model(env) as model, pytest.raises(biyos.Error) as caught:
        model.write(path, FileFormat.NL)
    assert caught.value.code == UNSUPPORTED
    message = f"unsupported: file '{path}': NL files are read, never written"
    assert str(caught.value) == message


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
