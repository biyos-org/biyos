from operator import index, itemgetter

import pytest

import biyos
from biyos._convert.arrays import indices_of
from number import Number

STALE = "the block predates a deletion from its model"
GONE = "the variable predates a deletion from its model"


@pytest.fixture
def env():
    with biyos.Env() as env:
        yield env


@pytest.fixture
def model(env):
    with biyos.Model(env) as model:
        yield model


def indices(block):
    return [index(variable) for variable in block]


def test_block(model):
    model.addvar(2)
    x = model.addvar(3)
    assert type(x) is biyos.Variables
    assert len(x) == 3
    assert indices(x) == [2, 3, 4]
    assert all(type(variable) is biyos.Variable for variable in x)


def test_slices(model):
    model.addvar(2)
    x = model.addvar(4)
    assert type(x[1:3]) is biyos.Variables
    assert indices(x[1:3]) == [3, 4]
    assert indices(x[-2:]) == [4, 5]
    assert indices(x[:-3]) == [2]
    assert indices(x[1:][1:]) == [4, 5]
    assert indices(x[::1]) == [2, 3, 4, 5]
    assert indices(x[3:1]) == []
    assert len(x[3:1]) == 0
    assert indices(x[9:]) == []


def test_step(model):
    x = model.addvar(4)
    with pytest.raises(ValueError, match="slice step must be 1, not 2"):
        x[::2]
    with pytest.raises(ValueError, match="slice step must be 1, not -1"):
        x[::-1]


def test_negative(model):
    model.addvar(1)
    x = model.addvar(3)
    assert index(x[-1]) == 3
    assert index(x[-3]) == 1
    assert index(x[Number(0)]) == 1


def test_outside(model):
    x = model.addvar(3)
    with pytest.raises(IndexError, match="index 3 is outside a block of 3"):
        x[3]
    with pytest.raises(IndexError, match="index -4 is outside a block of 3"):
        x[-4]
    with pytest.raises(IndexError, match="index 2147483648 is outside a"):
        x[2**31]
    with pytest.raises(TypeError, match="index must be an int, not float"):
        x[1.0]


def test_view(model):
    x = model.addvar(3)
    model.setvarbnd([x[1]], [0.0], [1.0])
    model.setvarbnd(x[1:], ub=[5.0, 6.0])
    model.setvarbnd([0, x[2]], lb=[-1.0, -2.0])
    found = model.getvar()[1:]
    assert found == ([-1.0, 0.0, -2.0], [biyos.inf, 5.0, 6.0])
    assert model.getvar(x[2:])[1] == [-2.0]


def test_foreign_view(env, model):
    other = biyos.Model(env)
    x = other.addvar(3)
    model.addvar(3)
    entry = "an entry of idx is a variable of another model"
    with pytest.raises(ValueError, match=entry):
        model.setvarbnd([0, x[1]], [0.0, 0.0], [1.0, 1.0])
    with pytest.raises(ValueError, match=entry):
        model.setvarknd([x[0]], [biyos.VariableKind.INTEGER])
    with pytest.raises(ValueError, match=entry):
        model.getvar([x[1]])
    assert model.getvar() == other.getvar()


def test_foreign_block(env, model):
    other = biyos.Model(env)
    x = other.addvar(3)
    model.addvar(3)
    block = "idx is a block of another model"
    with pytest.raises(ValueError, match=block):
        model.setvarbnd(x, None, [1.0] * 3)
    with pytest.raises(ValueError, match=block):
        model.setvarknd(x[1:], [biyos.VariableKind.INTEGER] * 2)
    assert model.getvar() == other.getvar()


def test_closed_block(env, model):
    other = biyos.Model(env)
    x = other.addvar(3)
    other.close()
    model.addvar(3)
    with pytest.raises(ValueError, match="idx is a block of another"):
        model.setvarbnd(x, None, [1.0] * 3)
    assert model.getvar()[2] == [biyos.inf] * 3


def test_stale(model):
    x = model.addvar(3)
    model.setvarname(x, ["a", "b", "c"])
    model._gen += 1
    for use in (len, iter, itemgetter(0), itemgetter(slice(1))):
        with pytest.raises(ValueError, match=STALE):
            use(x)
    with pytest.raises(ValueError, match=STALE):
        model.delvarname(x)
    assert model.getvarname(range(3)) == ["a", "b", "c"]


def test_gone(model):
    x = model.addvar(3)
    view = x[1]
    views = iter(x)
    model._gen += 1
    with pytest.raises(ValueError, match=GONE):
        index(view)
    with pytest.raises(ValueError, match=GONE):
        index(next(views))
    with pytest.raises(ValueError, match=GONE):
        model.setvarbnd([view], [0.0], [1.0])
    assert model.getvar()[1] == [-biyos.inf] * 3


def test_fresh(model):
    model.addvar(1)
    model._gen += 1
    x = model.addvar(3)
    assert len(x) == 3
    assert indices(x[1:]) == [2, 3]
    model.setvarbnd(x[1:], [0.0, 0.0])
    assert model.getvar()[1] == [-biyos.inf, -biyos.inf, 0.0, 0.0]


def test_empty(model):
    model.addvar(2)
    x = model.addvar(0)
    assert len(x) == 0
    assert indices(x) == []
    assert indices(x[:]) == []
    with pytest.raises(IndexError, match="index 0 is outside a block of 0"):
        x[0]


def test_sized_block(model):
    x = model.addvar(3)
    with pytest.raises(ValueError, match="columns has length 3, not 5"):
        indices_of(model, biyos.Variables, "columns", x, 5)


def test_subclass(model):
    class Mine(biyos.Variable):
        __slots__ = ()

    class Ours(biyos.Variables):
        __slots__ = ()

    model.addvar(3)
    model.setvarbnd([Mine(model, 1, model._gen)], [3.0])
    model.setvarbnd(Ours(model, 2, 3, model._gen), [4.0])
    assert model.getvar()[1] == [-biyos.inf, 3.0, 4.0]


def test_on(model):
    model.addvar(1)
    x = model.addvar(3)
    copy = model.copy()
    y = x.on(copy)
    assert type(y) is biyos.Variables
    assert indices(y) == [1, 2, 3]
    copy.setvarbnd(y[1:], None, [5.0, 6.0])
    assert copy.getvar()[2] == [biyos.inf, biyos.inf, 5.0, 6.0]
    with pytest.raises(ValueError, match="idx is a block of another"):
        model.setvarbnd(y, None, [1.0] * 3)
    assert model.getvar()[2] == [biyos.inf] * 4
    assert indices(x) == [1, 2, 3]
    copy.close()


def test_smaller(env, model):
    x = model.addvar(3)
    other = biyos.Model(env)
    other.addvar(2)
    few = "mdl holds too few variables: 2, the block needs 3"
    with pytest.raises(ValueError, match=few):
        x.on(other)
    with pytest.raises(ValueError, match=few):
        x[3:].on(other)
    assert indices(x[:2].on(other)) == [0, 1]
    assert len(x[2:2].on(other)) == 0
    other.addvar(1)
    assert indices(x.on(other)) == [0, 1, 2]


def test_target(env, model):
    x = model.addvar(3)
    with pytest.raises(TypeError, match="mdl must be a Model, not int"):
        x.on(1)
    other = biyos.Model(env)
    other.close()
    with pytest.raises(ValueError, match="the model is closed"):
        x.on(other)
    model._gen += 1
    with pytest.raises(ValueError, match=STALE):
        x.on(model)


def test_rebound(model):
    x = model.addvar(3)
    copy = model.copy()
    copy._gen += 2
    y = x.on(copy)
    assert len(y) == 3
    copy._gen += 1
    with pytest.raises(ValueError, match=STALE):
        len(y)
    assert len(x) == 3
    copy.close()
