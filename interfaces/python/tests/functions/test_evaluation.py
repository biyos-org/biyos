import math
from functools import partial

import pytest

import biyos
from biyos import exp, piecewise, sin

CALLBACK = -300
BUSY = -202
STEP = 1e-6
POINTS = [[0.3, -0.2, 0.7, 0.5], [-0.6, 0.4, -1.1, -0.4]]
WEIGHTS = [0.5, -2.0, 1.5]
V = [1.0, -0.5, 2.0, 0.25]
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


def pwl(t):
    if t < 0.0:
        return -t
    return 1.5 * t


def reference(x):
    f0 = 1.5 * x[0] - x[3] + 0.7 * x[0] ** 2 + 0.3 * x[0] * x[3]
    f0 += math.exp(x[1]) * math.sin(x[2]) + x[0] * x[3] ** 2
    f1 = 4.0 + 2.0 * x[1] - 0.4 * x[1] * x[2] + math.exp(pwl(x[3]))
    f2 = 0.5 + x[1] ** 3
    return [f0, f1, f2]


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


def rows(cnt):
    return [k for k, count in enumerate(cnt) for _ in range(count)]


def jacobian(model, x):
    cnt, var = model.getgpat()
    dense = [[0.0] * model.getnvar() for _ in cnt]
    for i, j, value in zip(rows(cnt), var, model.evalg(x)):
        dense[i][j] = value
    return dense


def lagrangian(model, x, w):
    dense = jacobian(model, x)
    n = model.getnvar()
    return [sum(w[k] * row[j] for k, row in enumerate(dense)) for j in range(n)]


def moved(x, j, step):
    return [value + step if k == j else value for k, value in enumerate(x)]


def central(function, x, j):
    ahead = function(moved(x, j, STEP))
    behind = function(moved(x, j, -STEP))
    return [(a - b) / (2 * STEP) for a, b in zip(ahead, behind)]


def lower(model, x, w):
    n = model.getnvar()
    dense = [[0.0] * n for _ in range(n)]
    cnt, var = model.gethpat()
    for j, i, value in zip(rows(cnt), var, model.evalh(x, w)):
        dense[i][j] = value
    return dense


def close(found, expected):
    return found == pytest.approx(expected, rel=1e-6, abs=1e-6)


@pytest.mark.parametrize("x", POINTS)
def test_values(model, x):
    build(model)
    assert close(model.evalf(x), reference(x))


@pytest.mark.parametrize("x", POINTS)
def test_jacobian(model, x):
    build(model)
    dense = jacobian(model, x)
    for j in range(4):
        column = [row[j] for row in dense]
        assert close(column, central(reference, x, j))


@pytest.mark.parametrize("x", POINTS)
def test_hessian(model, x):
    build(model)
    dense = lower(model, x, WEIGHTS)
    gradient = partial(lagrangian, model, w=WEIGHTS)
    for j in range(4):
        column = [dense[i][j] for i in range(j, 4)]
        assert close(column, central(gradient, x, j)[j:])


@pytest.mark.parametrize("x", POINTS)
def test_product(model, x):
    build(model)
    dense = lower(model, x, WEIGHTS)
    full = [[dense[max(i, j)][min(i, j)] for j in range(4)] for i in range(4)]
    expected = [sum(a * b for a, b in zip(row, V)) for row in full]
    assert close(model.evalhv(x, WEIGHTS, V), expected)


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
    assert len(model.evalg(POINTS[0])) == 8
    assert len(model.evalh(POINTS[0], WEIGHTS)) == 6


def test_piecewise(model):
    x = model.addvar(1)
    p = model.addpwl([3], KINKS, HEIGHTS)
    model.addobj(piecewise(p[0], x[0]))
    for kink, height in zip(KINKS, HEIGHTS):
        assert model.evalf([kink]) == [height]
    slopes = [(-2.0, -1.0), (-0.5, -1.0), (1.0, 1.5), (3.0, 1.5)]
    for t, slope in slopes:
        assert model.evalg([t]) == [slope]
        assert close(central(model.evalf, [t], 0), [slope])


def raising(error, req, out, data):
    raise error


def answer(result, req, out, data):
    return result


def test_raised(model):
    cb = build(model)
    error = ValueError("not here")
    model.setcbf(cb, partial(raising, error))
    with pytest.raises(biyos.Error) as caught:
        model.evalf(POINTS[0])
    assert caught.value.code == CALLBACK
    assert caught.value.__cause__ is error
    assert caught.value.model is model
    assert str(caught.value) == "callback: callback 0: function returned -1"
    model.setcbf(cb, values)
    assert close(model.evalf(POINTS[0]), reference(POINTS[0]))


def evaluated(model, x):
    try:
        return model.evalf(x)
    except Exception:
        return None


@pytest.mark.parametrize("stop", [KeyboardInterrupt(), SystemExit(3)])
def test_interrupted(model, stop):
    cb = build(model)
    model.setcbf(cb, partial(raising, stop))
    with pytest.raises(type(stop)) as caught:
        evaluated(model, POINTS[0])
    assert caught.value is stop
    model.setcbf(cb, partial(answer, -4))
    with pytest.raises(biyos.Error) as failed:
        model.evalf(POINTS[0])
    assert failed.value.__cause__ is None


def evaluating(model, req, out, data):
    model.evalf(list(req.x))


def test_inside(model):
    cb = build(model)
    model.setcbf(cb, partial(evaluating, model))
    with pytest.raises(biyos.Error) as caught:
        model.evalf(POINTS[0])
    assert caught.value.code == CALLBACK
    assert caught.value.__cause__.code == BUSY


def test_undefined(model):
    cb = build(model)
    model.setcbf(cb, partial(answer, 1))
    f = model.evalf(POINTS[0])
    assert math.isnan(f[0])
    assert f[1] == pytest.approx(reference(POINTS[0])[1])
    assert math.isnan(f[2])
    model.setcbf(cb, partial(answer, -4))
    with pytest.raises(biyos.Error) as caught:
        model.evalf(POINTS[0])
    assert caught.value.__cause__ is None


def unfinite(req, out, data):
    out[1] = math.nan


def test_unfinite(model):
    cb = build(model)
    model.setcbf(cb, unfinite)
    model.setopt("checking", biyos.Checking.CHEAP)
    with pytest.raises(biyos.Error) as caught:
        model.evalf(POINTS[0])
    assert caught.value.code == CALLBACK
    assert caught.value.__cause__ is None
    failed = "check 'finite output' failed: function output 1 is nan"
    assert str(caught.value) == f"callback: callback 0: {failed}"
    model.setopt("checking", biyos.Checking.NONE)
    assert math.isnan(model.evalf(POINTS[0])[2])


def test_sized(model):
    build(model)
    with pytest.raises(ValueError, match="x has length 2, not 4"):
        model.evalf([0.0, 1.0])
    with pytest.raises(ValueError, match="w has length 1, not 3"):
        model.evalh(POINTS[0], [1.0])
    with pytest.raises(ValueError, match="v has length 3, not 4"):
        model.evalhv(POINTS[0], WEIGHTS, [1.0] * 3)


def test_empty(model):
    assert model.evalf([]) == []
    assert model.evalg([]) == []
    assert model.evalh([], []) == []
    assert model.evalhv([], [], []) == []
    assert model.getgpat() == ([], [])
    assert model.gethpat() == ([], [])
    assert model.getngpat() == model.getnhpat() == 0
