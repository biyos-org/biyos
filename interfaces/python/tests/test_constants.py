import random

import pytest

import biyos
from refusal import refusal

LARGEST = 1.7976931348623157e308
SEEDS = [1, 2, 3, 5, 8, 13, 21, 34]


@pytest.fixture
def model():
    with biyos.Env() as env, biyos.Model(env) as model:
        yield model


@pytest.mark.parametrize("seed", SEEDS)
def test_sums(model, seed):
    draw = random.Random(seed)
    funs = model.addfuns(5, [draw.randint(-8, 8) / 4 for _ in range(5)])
    idx = [draw.randrange(5) for _ in range(draw.randrange(11))]
    cst = [draw.randint(-8, 8) / 4 for _ in idx]
    wanted = model.getcst(funs)
    for k, value in zip(idx, cst):
        wanted[k] += value
    model.addcst(idx, cst)
    assert model.getcst(funs) == wanted


def test_last(model):
    funs = model.addfuns(2)
    model.setcst([0, 1, 0], [1.0, 2.0, 3.0])
    assert model.getcst(funs) == [3.0, 2.0]
    model.addcst(funs[1:], [0.5])
    assert model.getcst([1, 1, 0]) == [2.5, 2.5, 3.0]


def test_refused(model):
    funs = model.addfuns(2, [LARGEST, -1.0])
    message = refusal(model.addcst, [1, 0], [1.0, float("nan")])
    assert message == "invalid argument: function 0: constant is NaN"
    message = refusal(model.addcst, [1, 0, 0], [1.0, -1.0, LARGEST])
    assert message == "invalid argument: function 0: constant sum is infinity"
    message = refusal(model.setcst, range(1, 3), [0.0, 0.0])
    assert message == "invalid argument: function 2: unknown"
    assert model.getcst(funs) == [LARGEST, -1.0]


def test_none(model):
    funs = model.addfuns(2, [1.5, -1.0])
    model.addcst(funs, None)
    model.setcst([1, 0], None)
    assert model.getcst(funs) == [1.5, -1.0]
    message = refusal(model.addcst, [0, 2], None)
    assert message == "invalid argument: function 2: unknown"
    message = refusal(model.setcst, [0, 2], None)
    assert message == "invalid argument: function 2: unknown"
