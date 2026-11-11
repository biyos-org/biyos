from operator import index, itemgetter

import pytest

import biyos
from biyos._arrays import indices_of
from number import Number

INFINITY = float("inf")
STALE = "the block predates a deletion from its model"
GONE = "the variable predates a deletion from its model"


@pytest.fixture
def env():
    with biyos.Environment() as env:
        yield env


@pytest.fixture
def model(env):
    with biyos.Model(env) as model:
        yield model


def indices(block):
    return [index(variable) for variable in block]


def test_block(model):
    model.add_variables(2)
    x = model.add_variables(3)
    assert type(x) is biyos.Variables
    assert len(x) == 3
    assert indices(x) == [2, 3, 4]
    assert all(type(variable) is biyos.Variable for variable in x)


def test_slices(model):
    model.add_variables(2)
    x = model.add_variables(4)
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
    x = model.add_variables(4)
    with pytest.raises(ValueError, match="slice step must be 1, not 2"):
        x[::2]
    with pytest.raises(ValueError, match="slice step must be 1, not -1"):
        x[::-1]


def test_negative(model):
    model.add_variables(1)
    x = model.add_variables(3)
    assert index(x[-1]) == 3
    assert index(x[-3]) == 1
    assert index(x[Number(0)]) == 1


def test_outside(model):
    x = model.add_variables(3)
    with pytest.raises(IndexError, match="index 3 is outside a block of 3"):
        x[3]
    with pytest.raises(IndexError, match="index -4 is outside a block of 3"):
        x[-4]
    with pytest.raises(IndexError, match="index 2147483648 is outside a"):
        x[2**31]
    with pytest.raises(TypeError, match="index must be an int, not float"):
        x[1.0]


def test_view(model):
    x = model.add_variables(3)
    model.set_variable_bounds([x[1]], [0.0], [1.0])
    model.set_variable_bounds(x[1:], upper=[5.0, 6.0])
    model.set_variable_bounds([0, x[2]], lower=[-1.0, -2.0])
    found = model.get_variables()[1:]
    assert found == ([-1.0, 0.0, -2.0], [INFINITY, 5.0, 6.0])
    assert model.get_variables(x[2])[1] == [-2.0]


def test_foreign_view(env, model):
    other = biyos.Model(env)
    x = other.add_variables(3)
    model.add_variables(3)
    entry = "an entry of indices is a variable of another model"
    with pytest.raises(ValueError, match=entry):
        model.set_variable_bounds([0, x[1]], [0.0, 0.0], [1.0, 1.0])
    with pytest.raises(ValueError, match=entry):
        model.set_variable_kinds([x[0]], [biyos.VariableKind.INTEGER])
    with pytest.raises(ValueError, match="first is a variable of another"):
        model.get_variables(x[1], 1)
    assert model.get_variables() == other.get_variables()


def test_foreign_block(env, model):
    other = biyos.Model(env)
    x = other.add_variables(3)
    model.add_variables(3)
    block = "indices is a block of another model"
    with pytest.raises(ValueError, match=block):
        model.set_variable_bounds(x, None, [1.0] * 3)
    with pytest.raises(ValueError, match=block):
        model.set_variable_kinds(x[1:], [biyos.VariableKind.INTEGER] * 2)
    assert model.get_variables() == other.get_variables()


def test_closed_block(env, model):
    other = biyos.Model(env)
    x = other.add_variables(3)
    other.close()
    model.add_variables(3)
    with pytest.raises(ValueError, match="indices is a block of another"):
        model.set_variable_bounds(x, None, [1.0] * 3)
    assert model.get_variables()[2] == [INFINITY] * 3


def test_stale(model):
    x = model.add_variables(3)
    model._version += 1
    for use in (len, iter, itemgetter(0), itemgetter(slice(1))):
        with pytest.raises(ValueError, match=STALE):
            use(x)


def test_gone(model):
    x = model.add_variables(3)
    view = x[1]
    views = iter(x)
    model._version += 1
    with pytest.raises(ValueError, match=GONE):
        index(view)
    with pytest.raises(ValueError, match=GONE):
        index(next(views))
    with pytest.raises(ValueError, match=GONE):
        model.set_variable_bounds([view], [0.0], [1.0])
    assert model.get_variables()[1] == [-INFINITY] * 3


def test_fresh(model):
    model.add_variables(1)
    model._version += 1
    x = model.add_variables(3)
    assert len(x) == 3
    assert indices(x[1:]) == [2, 3]
    model.set_variable_bounds(x[1:], [0.0, 0.0])
    assert model.get_variables()[1] == [-INFINITY, -INFINITY, 0.0, 0.0]


def test_empty(model):
    model.add_variables(2)
    x = model.add_variables(0)
    assert len(x) == 0
    assert indices(x) == []
    assert indices(x[:]) == []
    with pytest.raises(IndexError, match="index 0 is outside a block of 0"):
        x[0]


def test_sized_block(model):
    x = model.add_variables(3)
    with pytest.raises(ValueError, match="columns has length 3, not 5"):
        indices_of(model, biyos.Variables, "columns", x, 5)


def test_subclass(model):
    class Mine(biyos.Variable):
        __slots__ = ()

    class Ours(biyos.Variables):
        __slots__ = ()

    model.add_variables(3)
    model.set_variable_bounds([Mine(model, 1, model._version)], [3.0])
    model.set_variable_bounds(Ours(model, 2, 1, model._version), [4.0])
    assert model.get_variables()[1] == [-INFINITY, 3.0, 4.0]
