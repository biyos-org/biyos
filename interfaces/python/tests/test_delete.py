from functools import partial
from operator import index

import pytest

import biyos
from biyos._capi import lib
from refusal import refusal

IN_USE = -203
STALE = "the block predates a deletion from its model"
NAMES = ["r0", "r1", "r2"]
NOUNS = {
    "con": "constraint",
    "obj": "objective",
    "ind": "indicator",
    "sos": "set",
    "cone": "cone",
    "ccon": "complement",
}


@pytest.fixture
def env():
    with biyos.Env() as env:
        yield env


@pytest.fixture
def model(env):
    with biyos.Model(env) as model:
        yield model


def build(model):
    x = model.addvar(3, [biyos.VariableKind.BINARY] * 3)
    f = model.addfun(3)
    starts = [0, 1, 2, 3]
    blocks = {
        "var": x,
        "con": model.addcon(f),
        "obj": model.addobj(f),
        "ind": model.addind(x, [1, 0, 1], f),
        "sos": model.addsos([biyos.SOSKind.SOS1] * 3, starts, x),
        "cone": model.addcone([biyos.ConeKind.NONNEGATIVE] * 3, starts, f),
        "ccon": model.addccon(f, [f[1], f[2], f[0]]),
    }
    for short in NOUNS:
        getattr(model, f"set{short}name")(blocks[short], NAMES)
    return blocks


def unmapped(maps, call, *args):
    maps.append(args[-1])
    return call(*args)


def delete(model, short, idx):
    return getattr(model, f"del{short}")(idx)


def count(model, short):
    return getattr(model, f"getn{short}")()


def names(model, short):
    return getattr(model, f"get{short}name")(range(count(model, short)))


@pytest.mark.parametrize("short", NOUNS)
def test_listed(model, short):
    build(model)
    assert delete(model, short, [2, 0]) == [-1, 0, -1]
    assert names(model, short) == ["r1"]


@pytest.mark.parametrize("short", NOUNS)
def test_ranged(model, short):
    blocks = build(model)
    assert delete(model, short, blocks[short][1:]) == [0, -1, -1]
    assert names(model, short) == ["r0"]
    assert delete(model, short, range(1)) == [-1]
    assert count(model, short) == 0


@pytest.mark.parametrize("short", NOUNS)
def test_stale(model, short):
    blocks = build(model)
    view = blocks["var"][0]
    delete(model, short, [1])
    with pytest.raises(ValueError, match=STALE):
        delete(model, short, blocks[short])
    with pytest.raises(ValueError, match=STALE):
        model.getvarbnd(blocks["var"])
    with pytest.raises(ValueError, match="the variable predates a deletion"):
        index(view)
    assert count(model, short) == 2


@pytest.mark.parametrize("short", NOUNS)
def test_empty(model, short):
    blocks = build(model)
    assert delete(model, short, []) == [0, 1, 2]
    assert delete(model, short, blocks[short][3:]) == [0, 1, 2]
    assert names(model, short) == NAMES
    assert len(blocks["var"]) == 3


@pytest.mark.parametrize("short", NOUNS)
def test_refused(model, short):
    blocks = build(model)
    noun = NOUNS[short]
    repeated = f"invalid argument: argument 'idx': {noun} 1 is repeated"
    assert refusal(delete, model, short, [1, 0, 1]) == repeated
    unknown = f"invalid argument: {noun} 3: unknown"
    assert refusal(delete, model, short, [0, 3]) == unknown
    assert refusal(delete, model, short, range(2, 4)) == unknown
    assert names(model, short) == NAMES
    assert len(blocks[short]) == 3


@pytest.mark.parametrize("short", NOUNS)
def test_unmapped(model, monkeypatch, short):
    build(model)
    maps = []
    for form in ("sp", "rg"):
        name = f"BIYOS_del{short}{form}"
        spy = partial(unmapped, maps, getattr(lib, name))
        monkeypatch.setattr(lib, name, spy)
    assert delete(model, short, [2]) == [0, 1, -1]
    assert delete(model, short, range(1)) == [-1, 0]
    assert maps == [None, None]


def functions(model):
    f = model.addfun(4)
    model.setfunname(f[:3], NAMES)
    model.addcst(f, [0.0, 1.0, 2.0, 3.0])
    model.addcon(f[3:])
    return f


def test_functions(model):
    f = functions(model)
    assert model.delfun([2, 0]) == [-1, 0, -1, 1]
    assert model.getfunname(range(2)) == ["r1", None]
    assert model.getcst(range(2)) == [1.0, 3.0]
    assert model.getcon()[0] == [1]
    with pytest.raises(ValueError, match=STALE):
        model.getfunname(f)
    assert model.delfun(range(1)) == [-1, 0]
    assert model.getcon()[0] == [0]


def test_used(model):
    f = functions(model)
    for idx in ([3], [1, 3], f[2:]):
        with pytest.raises(biyos.Error) as caught:
            model.delfun(idx)
        assert caught.value.code == IN_USE
        message = "in use: function 3: used by constraint 0"
        assert str(caught.value) == message
    assert model.delfun([]) == [0, 1, 2, 3]
    assert model.getfunname(f) == NAMES + [None]
    assert model.getcst(f) == [0.0, 1.0, 2.0, 3.0]
    assert model.getcon()[0] == [3]
