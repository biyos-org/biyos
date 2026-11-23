from fractions import Fraction

import pytest

import biyos
from biyos._arrays import doubles, indices_of, ints, offsets, strings
from number import Growing

FAMILIES = [
    biyos.Variables,
    biyos.Functions,
    biyos.Constraints,
    biyos.Objectives,
]


class Shrinking(Fraction):
    def __float__(self):
        self.items.pop()
        return 0.0


class Encoding(str):
    def encode(self):
        self.items.pop()
        return b"x"


class Liar(list):
    def __len__(self):
        return 3


class Long(biyos.Variables):
    __slots__ = ()

    def __len__(self):
        return 3


def popping():
    items = [0, 1, 2]
    items[0] = Growing(0, items.pop)
    return items


def shrinking():
    entry = Shrinking()
    entry.items = [entry, 1, 2]
    return entry.items


def encoding():
    entry = Encoding("x")
    entry.items = [entry, "y", "z"]
    return entry.items


@pytest.fixture
def model():
    with biyos.Env() as env, biyos.Model(env) as model:
        yield model


def test_popping():
    with pytest.raises(ValueError, match="knd has length 2, not 3"):
        ints("knd", popping(), 3)


def test_shrinking():
    with pytest.raises(ValueError, match="lb has length 2, not 3"):
        doubles("lb", shrinking(), 3)


def test_encoding():
    with pytest.raises(ValueError, match="name has length 2, not 3"):
        strings("name", encoding(), 3)


def test_liar():
    with pytest.raises(ValueError, match="knd has length 2, not 3"):
        ints("knd", Liar([1, 2]), 3)
    with pytest.raises(ValueError, match="lb has length 2, not 3"):
        doubles("lb", Liar([1.0, 2.0]), 3)
    with pytest.raises(ValueError, match="name has length 2, not 3"):
        strings("name", Liar(["x", "y"]), 3)
    with pytest.raises(ValueError, match="off has length 2, not 3"):
        offsets("off", Liar([0, 0]), 3)


def test_expressions(model):
    with pytest.raises(ValueError, match="off has length 2, not 3"):
        model.addexpr(Liar([0, 0]), [], [], None)


@pytest.mark.parametrize("family", FAMILIES)
def test_indices(model, family):
    with pytest.raises(ValueError, match="idx has length 2, not 3"):
        indices_of(model, family, "idx", popping(), 3)
    with pytest.raises(ValueError, match="idx has length 2, not 3"):
        indices_of(model, family, "idx", Liar([0, 1]), 3)


def test_long(model):
    model.addvar(2)
    x = Long(model, 0, 2, model._gen)
    with pytest.raises(ValueError, match="idx has length 2, not 3"):
        indices_of(model, biyos.Variables, "idx", x, 3)
