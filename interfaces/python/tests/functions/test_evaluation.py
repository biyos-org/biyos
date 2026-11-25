import pytest

import biyos
from biyos import exp, piecewise, sin

KINKS = [-1.0, 0.0, 2.0]
HEIGHTS = [1.0, 0.0, 3.0]


@pytest.fixture
def env():
    with biyos.Env() as env:
        yield env


@pytest.fixture
def model(env):
    with biyos.Model(env) as model:
        yield model


def values(req, out, data):
    x = req.x
    out[0] = x[0] * x[3] * x[3]
    out[1] = x[1] ** 3


def gradients(req, out, data):
    x = req.x
    out[0] = x[3] * x[3]
    out[1] = 2.0 * x[0] * x[3]
    out[2] = 3.0 * x[1] * x[1]


def hessian(req, out, data):
    x, w = req.x, req.w
    out[0] = 2.0 * w[0] * x[3]
    out[1] = 2.0 * w[0] * x[0]
    out[2] = 6.0 * w[1] * x[1]


def product(req, out, data):
    x, w, v = req.x, req.w, req.v
    out[0] = 2.0 * w[0] * x[3] * v[3]
    out[1] = 6.0 * w[1] * x[1] * v[1]
    out[3] = 2.0 * w[0] * (x[3] * v[0] + x[0] * v[3])


def build(model):
    x = model.addvar(4)
    p = model.addpwl([3], KINKS, HEIGHTS)
    e0 = 1.5 * x[0] - x[3] + exp(x[1]) * sin(x[2])
    e1 = 4 + 2 * x[1] + exp(piecewise(p[0], x[3]))
    funs = model.getobj(model.addobj([e0, e1]))[0]
    f = model.addfun(1)
    model.addcst(f, [0.5])
    quad = ([funs[0], funs[0], funs[1]], [0, 0, 1], [0, 3, 2])
    model.addquadterm(*quad, [0.7, 0.3, -0.4])
    cb = model.addcb([funs[0], f[0]])
    model.setcbf(cb, values)
    model.setcbg(cb, [funs[0], funs[0], f[0]], [0, 3, 1], gradients)
    model.setcbh(cb, [0, 3, 1], [3, 3, 1], hessian)
    model.setcbhv(cb, product)
    return cb


def test_patterns(model):
    build(model)
    assert model.getgpat() == ([4, 3, 1], [0, 1, 2, 3, 1, 2, 3, 1])
    assert model.gethpat() == ([2, 2, 1, 1], [0, 3, 1, 2, 2, 3])
    assert model.getngpat() == 8
    assert model.getnhpat() == 6
    funs = model.getobj()[0]
    assert model.getngpat([2, 0, 2]) == 6
    assert model.getngpat(funs) == 7
    assert model.getngpat(range(1, 3)) == 4
    assert model.getnhpat([3, 1]) == 3
    assert model.getnhpat(range(2, 4)) == 2
    assert model.getgpat([2, 0, 2]) == ([1, 4, 1], [1, 0, 1, 2, 3, 1])
    assert model.getgpat(funs) == model.getgpat(range(2))
    assert model.getgpat(range(1, 3)) == ([3, 1], [1, 2, 3, 1])
    assert model.gethpat([3, 1]) == ([1, 2], [3, 1, 2])
    assert model.gethpat(range(4)) == model.gethpat([0, 1, 2, 3])


def test_empty(model):
    assert model.getgpat() == ([], [])
    assert model.gethpat() == ([], [])
    assert model.getngpat() == model.getnhpat() == 0
