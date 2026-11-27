from ctypes import c_char_p

import pytest

import biyos
from biyos import ConeKind, SpecialOrderedSetKind, VariableKind
from biyos._capi import lib
from biyos._convert.numbers import c_index

STALE = "the block predates a deletion from its model"

FAMILIES = {
    "var": biyos.Variables,
    "fun": biyos.Functions,
    "con": biyos.Constraints,
    "obj": biyos.Objectives,
    "ind": biyos.Indicators,
    "sos": biyos.SpecialOrderedSets,
    "cone": biyos.Cones,
    "ccon": biyos.Complements,
}


@pytest.fixture
def env():
    with biyos.Env() as env:
        yield env


@pytest.fixture
def model(env):
    with biyos.Model(env) as model:
        build(model)
        yield model


@pytest.fixture
def other(env):
    with biyos.Model(env) as other:
        build(other)
        yield other


def build(model):
    x = model.addvar(4, [VariableKind.BINARY] * 4)
    f = model.addfun(4)
    g = f[:3]
    shuffled = [f[2], f[0], f[1]]
    sizes = [1, 1, 1]
    model.addcon(g, [0.0] * 3, [1.0] * 3)
    model.addobj(shuffled)
    model.addind(x[:3], [1, 0, 1], shuffled)
    model.addsos([SpecialOrderedSetKind.ONE] * 3, sizes, x[:3])
    model.addcone([ConeKind.NONNEGATIVE] * 3, sizes, g)
    model.addccon(shuffled, [f[1], f[2], f[0]])
    for short in FAMILIES:
        count = getattr(model, f"getn{short}")()
        names = [f"{short}{count - 1 - k}" for k in range(count)]
        getattr(model, f"set{short}name")(range(count), names)


def renamed(model, other, short, block):
    names = [f"new{k}" for k in range(len(block))]
    getattr(model, f"set{short}name")(block, names)
    getattr(other, f"set{short}name")(block.indices(), names)
    assert model.compare(other) is None


def stale(model, block):
    model.delccon([0])
    with pytest.raises(ValueError, match=STALE):
        len(block)


def looked_up(model, short, names):
    n = len(names)
    texts = (c_char_p * n)(*[name.encode() for name in names])
    found = (c_index * n)()
    call = getattr(lib, f"BIYOS_get{short}idx")
    assert call(model._handle, n, texts, found) == 0
    return list(found)


@pytest.mark.parametrize("short", FAMILIES)
def test_lookup(model, other, short):
    names = [f"{short}2", f"{short}0", f"{short}2"]
    found = getattr(model, f"get{short}idx")(names)
    assert type(found) is FAMILIES[short]
    assert found.indices() == looked_up(model, short, names)
    renamed(model, other, short, found)
    stale(model, found)


def test_no_names(model):
    found = model.getvaridx([])
    assert type(found) is biyos.Variables
    assert found.indices() == []
