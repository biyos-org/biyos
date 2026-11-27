from ctypes import byref, c_char_p
from operator import index

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

VIEWS = {"var": biyos.Variable, "fun": biyos.Function}

CALLBACKS = [("cbfun", "fun"), ("cbvar", "var")]

RECORDS = [
    ("con", "con", ("fun", None, None)),
    ("obj", "obj", ("fun", None, None, None)),
    ("ind", "ind", ("var", None, "fun", None, None)),
    ("indvar", "ind", ("var", None)),
    ("ccon", "ccon", ("fun", "fun", None, None)),
]


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


def stale(model, *blocks):
    model.delccon([0])
    for block in blocks:
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


def covered(model, call, cb):
    count = c_index()
    counter = getattr(lib, f"BIYOS_getn{call}")
    assert counter(model._handle, cb, byref(count)) == 0
    found = (c_index * count.value)()
    reader = getattr(lib, f"BIYOS_get{call}")
    assert reader(model._handle, cb, count.value, found) == 0
    return list(found)


@pytest.mark.parametrize(("call", "short"), CALLBACKS)
def test_callback(model, call, short):
    model.addcb([2, 0], [3, 1])
    found = getattr(model, f"get{call}")(0)
    assert type(found) is FAMILIES[short]
    assert found.indices() == covered(model, call, 0)
    with model.copy() as copy:
        renamed(model, copy, short, found)
    stale(model, found)


def test_no_names(model):
    found = model.getvaridx([])
    assert type(found) is biyos.Variables
    assert found.indices() == []


def buffer(short, count):
    return None if short is None else (c_index * count)()


def ranged(model, call, columns, count):
    arrays = [buffer(short, count) for short in columns]
    reader = getattr(lib, f"BIYOS_get{call}rg")
    assert reader(model._handle, 0, count, *arrays) == 0
    return [list(array) for array in arrays if array is not None]


def blocks_of(found, columns):
    return [(block, short) for block, short in zip(found, columns) if short]


@pytest.mark.parametrize(("call", "short", "columns"), RECORDS)
def test_record(model, other, call, short, columns):
    count = getattr(model, f"getn{short}")()
    found = blocks_of(getattr(model, f"get{call}")(range(count)), columns)
    assert all(type(block) is FAMILIES[family] for block, family in found)
    listed = [block.indices() for block, _ in found]
    assert listed == ranged(model, call, columns, count)
    for block, family in found:
        renamed(model, other, family, block)
    stale(model, *[block for block, _ in found])


@pytest.mark.parametrize(("call", "short", "columns"), RECORDS)
def test_record_one(model, call, short, columns):
    count = getattr(model, f"getn{short}")()
    every = ranged(model, call, columns, count)
    found = blocks_of(getattr(model, f"get{call}")(0), columns)
    assert all(type(view) is VIEWS[family] for view, family in found)
    assert [index(view) for view, _ in found] == [column[0] for column in every]
