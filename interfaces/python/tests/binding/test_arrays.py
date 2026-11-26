from fractions import Fraction

import pytest

import biyos
from biyos._convert.arrays import (
    counts,
    doubles,
    indices_of,
    int64s,
    ints,
    strings,
)
from number import Growing

FAMILIES = [
    biyos.Variables,
    biyos.Functions,
    biyos.Constraints,
    biyos.Objectives,
    biyos.Indicators,
    biyos.Cones,
    biyos.Complements,
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
    with pytest.raises(ValueError, match="cnt has length 2, not 3"):
        counts("cnt", Liar([0, 0]), 3)


def test_counts():
    assert list(counts("cnt", [2, 0, 1], 3)) == [2, 0, 1]
    assert counts("cnt", None, 3) is None
    with pytest.raises(ValueError, match="prmcnt: -1 is negative at entry 2"):
        counts("prmcnt", [0, 1, -1], 3)


def test_expressions(model):
    with pytest.raises(ValueError, match="cnt has length 2, not 3"):
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


def test_one():
    assert list(doubles("lb", 1.5, 1, True)) == [1.5]
    assert list(ints("knd", 2, 1, True)) == [2]
    assert list(int64s("node", 3, 1, True)) == [3]
    assert [bytes(b) for b in strings("name", "x", 1, True)] == [b"x"]


@pytest.mark.parametrize("convert", [doubles, ints, int64s, strings])
def test_one_list(convert):
    with pytest.raises(TypeError, match="^v must be one value for one index"):
        convert("v", [0], 1, True)


def test_one_entry():
    with pytest.raises(TypeError, match="^lb must be a number, not str"):
        doubles("lb", "x", 1, True)
    with pytest.raises(TypeError, match="^an entry of lb must be a number"):
        doubles("lb", ["x"], 1)
    with pytest.raises(TypeError, match="^knd must be an int, not float"):
        ints("knd", 0.5, 1, True)
    with pytest.raises(TypeError, match="^node must be an int, not float"):
        int64s("node", 0.5, 1, True)
    with pytest.raises(TypeError, match="^name must be a str, not int"):
        strings("name", 0, 1, True)


def test_one_view(model):
    x = model.addvar(2)
    assert list(indices_of(model, biyos.Variables, "var", x[1], 1, True)) == [1]
    with pytest.raises(TypeError, match="^var must be one value"):
        indices_of(model, biyos.Variables, "var", x, 2, True)
    f = model.addfun(1)
    with pytest.raises(TypeError, match="^var is a function"):
        indices_of(model, biyos.Variables, "var", f[0], 1, True)


def test_adder_scalar(model):
    kind = biyos.VariableKind.CONTINUOUS
    with pytest.raises(TypeError, match="^lb must be a sequence, not float"):
        model.addvar(1, [kind], 0.0)
