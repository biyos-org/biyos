from functools import partial
from operator import index

import pytest

import biyos
from biyos import EventKind, EventLevel
from refusal import LOCKED, refusal

NAN = float("nan")
UNSET = ([None] * 3, [None] * 3)

BOUNDED = [0, 1, 2]
PARTNERS = [1, 1, 0]
LOWER = [0.0, -1.0, -biyos.inf]
UPPER = [biyos.inf, 2.0, biyos.inf]
SAMPLE = (BOUNDED, PARTNERS, LOWER, UPPER)


@pytest.fixture
def env():
    with biyos.Env() as env:
        yield env


@pytest.fixture
def model(env):
    with biyos.Model(env) as model:
        model.addfun(3)
        yield model


def add_sample(model):
    return model.addccon(BOUNDED, PARTNERS, LOWER, UPPER)


def tolerances(model):
    return model.getccontol(range(model.getnccon()))


def test_add(model):
    added = add_sample(model)
    assert model.getnccon() == 3
    assert model.getccon() == SAMPLE
    assert type(added) is biyos.Complements
    assert [index(complement) for complement in added] == [0, 1, 2]
    assert all(type(item) is biyos.Complement for item in added)
    assert len(model.addccon([], [])) == 0


def test_range(model):
    add_sample(model)
    found = model.getccon(range(1, 3))
    assert found == tuple(column[1:] for column in SAMPLE)
    found = model.getccon([2, 0, 2])
    assert found == tuple([c[2], c[0], c[2]] for c in SAMPLE)
    assert model.getccon(range(3, 3)) == ([], [], [], [])


def test_defaults(model):
    model.addccon([0, 1], [1, 0])
    model.addccon([2], [2], ub=[1.0])
    assert model.getccon() == (
        [0, 1, 2],
        [1, 0, 2],
        [0.0, 0.0, 0.0],
        [biyos.inf, biyos.inf, 1.0],
    )


def test_shared(model):
    model.addcon([1], [0.0], [2.0])
    model.addccon([1, 1], [1, 2], [-biyos.inf, 0.0])
    assert model.getcon() == ([1], [0.0], [2.0])
    assert model.getccon()[:2] == ([1, 1], [1, 2])
    assert model.getnfun() == 3


def test_bounds(model):
    add_sample(model)
    model.setcconbnd([1, 0], lb=[2.0, 3.0])
    model.setcconbnd(range(2, 3), ub=[5.0])
    found = model.getcconbnd(range(3))
    assert found == ([3.0, 2.0, -biyos.inf], [biyos.inf, 2.0, 5.0])
    assert model.getccon()[:2] == (BOUNDED, PARTNERS)


def test_tolerances(model):
    c = add_sample(model)
    assert tolerances(model) == UNSET
    model.setccontol([1], atol=[0.5])
    model.setccontol(c[:2], rtol=[0.25, 0.0])
    model.setccontol([2, 2], [1.0, 2.0], [-0.0, 0.125])
    model.setccontol([0], None, None)
    assert tolerances(model) == ([None, 0.5, 2.0], [0.25, 0.0, 0.125])
    assert model.getccontol([2, 1]) == ([2.0, 0.5], [0.125, 0.0])
    model.delccontol(c[1:])
    model.delccontol([0, 0])
    assert tolerances(model) == UNSET
    assert model.getccon() == SAMPLE


def test_views(env, model):
    c = add_sample(model)
    f = model.addfun(1)
    x = model.addvar(1)
    model.addccon(f, [f[0]])
    assert model.getccon([3]) == ([3], [3], [0.0], [biyos.inf])
    other = biyos.Model(env)
    g = other.addfun(1)
    d = other.addccon(g, g)
    with pytest.raises(ValueError, match="fun2 is a function of another"):
        model.addccon([0], [g[0]])
    with pytest.raises(TypeError, match="is a variable, not a function"):
        model.addccon([x[0]], [0])
    with pytest.raises(ValueError, match="is a complement of another model"):
        model.setcconbnd([d[0]], [0.0])
    with pytest.raises(TypeError, match="of functions, not complements"):
        model.getccontol(f)
    assert model.getcconbnd(c[1:]) == ([-1.0, -biyos.inf], [2.0, biyos.inf])
    other.close()


def test_refused(model):
    add_sample(model)
    message = refusal(model.addccon, [3], [0])
    assert message == "invalid argument: complement 3: function 3 is unknown"
    message = refusal(model.addccon, [0, 0], [0, 3])
    assert message == "invalid argument: complement 4: function 3 is unknown"
    message = refusal(model.addccon, [0], [0], [NAN])
    assert message == "invalid argument: complement 3: lower bound is NaN"
    message = refusal(model.addccon, [0], None)
    assert message == "invalid argument: argument 'fun2': is null"
    message = refusal(model.setcconbnd, [0], [biyos.inf])
    assert message == "invalid argument: complement 0: lower bound is infinity"
    message = refusal(model.setccontol, [1], [NAN])
    assert message == (
        "invalid argument: complement 1: absolute tolerance is NaN"
    )
    message = refusal(model.setccontol, [1], None, [biyos.inf])
    assert message == (
        "invalid argument: complement 1: relative tolerance is infinity"
    )
    message = refusal(model.setccontol, range(1, 2), [-0.5])
    assert message == (
        "invalid argument: complement 1: absolute tolerance -0.5 is negative"
    )
    message = refusal(model.delccontol, [0, 3])
    assert message == "invalid argument: complement 3: unknown"
    message = refusal(model.getccon, [3])
    assert message == "invalid argument: complement 3: unknown"
    assert model.getccon() == SAMPLE
    assert tolerances(model) == UNSET


def test_lengths(model):
    with pytest.raises(ValueError, match="fun2 has length 1, not 2"):
        model.addccon([0, 2], [1])
    with pytest.raises(ValueError, match="lb has length 3, not 2"):
        model.addccon([0, 2], [1, 1], [0.0] * 3)
    with pytest.raises(ValueError, match="ub has length 1, not 2"):
        model.setcconbnd([0, 1], ub=[1.0])
    with pytest.raises(ValueError, match="rtol has length 1, not 2"):
        model.setccontol([0, 1], rtol=[1.0])
    assert model.getnccon() == 0


def test_locked(env, model):
    seen = []

    def change(event):
        for call in (
            partial(model.addccon, [0], [1]),
            partial(model.setcconbnd, [0], [0.0]),
            partial(model.setccontol, [0], [0.5]),
            partial(model.delccontol, [0]),
        ):
            try:
                call()
            except biyos.Error as error:
                seen.append(str(error))
        seen.append(model.getccon())

    add_sample(model)
    env.addhook(change, EventKind.MESSAGE, EventLevel.ERROR)
    with pytest.raises(biyos.Error):
        env.setopt("seed", -1)
    assert seen == [LOCKED] * 4 + [SAMPLE]
    assert model.getnccon() == 3


def test_closed(env):
    model = biyos.Model(env)
    model.close()
    calls = (
        partial(model.addccon, [0], [0]),
        partial(model.setcconbnd, []),
        partial(model.getccontol, []),
        partial(model.delccontol, []),
        model.getnccon,
        model.getccon,
    )
    for call in calls:
        with pytest.raises(ValueError, match="the model is closed"):
            call()


def test_copy(model):
    c = add_sample(model)
    model.setccontol(c[2:], [0.5], [0.25])
    copy = model.copy()
    assert copy.getccon() == SAMPLE
    assert copy.getccontol([2]) == ([0.5], [0.25])
    copy.setcconbnd(c[1:].on(copy), [0.0, 0.0])
    copy.delccontol(c.on(copy))
    assert copy.getcconbnd(range(3))[0] == [0.0, 0.0, 0.0]
    assert model.getccon() == SAMPLE
    assert model.getccontol([2]) == ([0.5], [0.25])
    copy.close()
