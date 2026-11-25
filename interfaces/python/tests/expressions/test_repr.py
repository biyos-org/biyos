import pytest

import biyos


@pytest.fixture
def env():
    with biyos.Env() as env:
        yield env


@pytest.fixture
def model(env):
    with biyos.Model(env) as model:
        yield model


@pytest.fixture
def x(model):
    return model.addvar(3)


def test_variable(x):
    assert repr(x[2]) == "x[2]"


def test_closed(env):
    with biyos.Model(env) as model:
        x = model.addvar(2)
    assert repr(x[1]) == "x[1]"
