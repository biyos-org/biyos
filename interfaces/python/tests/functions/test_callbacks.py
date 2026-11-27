import gc
import weakref
from ctypes import addressof, byref, c_double, c_void_p, cast, sizeof
from functools import partial

import pytest

import biyos
from biyos import EventKind, EventLevel, Operator
from biyos._capi import CALLBACK, RequestRecord, lib
from refusal import LOCKED, refusal

SETTERS = (
    "BIYOS_setcbf",
    "BIYOS_setcbg",
    "BIYOS_setcbh",
    "BIYOS_setcbhv",
)


@pytest.fixture
def env():
    with biyos.Env() as env:
        yield env


@pytest.fixture
def model(env):
    with biyos.Model(env) as model:
        model.addvar(3)
        model.addfun(3)
        yield model


@pytest.fixture
def seen(monkeypatch):
    found = []
    for name in SETTERS:
        spy = partial(recorded, found, getattr(lib, name))
        monkeypatch.setattr(lib, name, spy)
    return found


def recorded(found, function, *arguments):
    found.append(cast(arguments[-1], c_void_p).value)
    return function(*arguments)


def doubles(values):
    return None if values is None else (c_double * len(values))(*values)


def start(values):
    return None if values is None else addressof(values)


def evaluate(model, address, cb, size, x, w=None, v=None):
    arrays = [doubles(values) for values in (x, w, v)]
    record = RequestRecord(sizeof(RequestRecord), model._handle.value, cb)
    record.x, record.w, record.v = (start(array) for array in arrays)
    out = (c_double * size)()
    data = c_void_p()
    assert lib.BIYOS_getcbdata(model._handle, cb, byref(data)) == 0
    function = CALLBACK(address)
    status = function(byref(record), start(out) if size else None, data)
    return status, list(out)


def raised(model):
    return model._callbacks.taken(model._handle.value)


def describe(found, req, out, data):
    found.append((req.callback, req.thread, list(req.x), data))
    found.append((req.w, req.v, req.x.readonly, len(out)))
    for k in range(len(out)):
        out[k] = req.x[k] + 0.5


def test_added(model):
    assert model.addcb([0, 2]) == 0
    f = model.addfun(2)
    data = {"scale": 2.0}
    assert model.addcb(f, data=data) == 1
    assert model.getcbdata(0) is None
    assert model.getcbdata(1) is data
    other = []
    model.setcbdata(1, other)
    assert model.getcbdata(1) is other


def test_covered(model):
    unknown = "invalid argument: callback 0: unknown"
    assert refusal(model.getcbfun, 0) == unknown
    model.addcb([2, 0])
    model.addcb(model.addfun(2))
    assert [model.getncbfun(cb) for cb in (0, 1)] == [2, 2]
    assert model.getcbfun(0).indices() == [2, 0]
    assert model.getcbfun(1).indices() == [3, 4]


def test_listed(model):
    assert model.addcb([0], [2, 0]) == 0
    assert model.addcb([1], []) == 1
    assert model.addcb([2], model.addvar(2)) == 2
    assert model.addcb([0]) == 3
    found = [model.getcbvar(cb) for cb in range(3)]
    assert [block.indices() for block in found] == [[2, 0], [], [3, 4]]
    assert model.getcbvar(3) is None


def test_values(model, seen):
    cb = model.addcb([2, 0], data="data")
    found = []
    model.setcbf(cb, partial(describe, found))
    status, values = evaluate(model, seen[-1], cb, 2, [1.0, 2.0, 3.0])
    assert (status, values) == (0, [1.5, 2.5])
    assert found == [(0, 0, [1.0, 2.0, 3.0], "data"), (None, None, True, 2)]


def test_collected(model, seen):
    cb = model.addcb([0])

    def values(req, out, data):
        out[0] = 2.0 * req.x[1]

    alive = weakref.ref(values)
    model.setcbf(cb, values)
    del values
    gc.collect()
    assert alive() is not None
    assert evaluate(model, seen[-1], cb, 1, [0.0, 1.5, 0.0]) == (0, [3.0])


def test_copied(env, seen):
    data = ["shared"]
    source = biyos.Model(env)
    source.addvar(1)
    source.addfun(1)
    cb = source.addcb([0], data=data)
    source.setcbf(cb, partial(describe, []))
    copy = source.copy()
    source.close()
    del source
    gc.collect()
    assert copy.getcbdata(cb) is data
    assert evaluate(copy, seen[-1], cb, 1, [1.0]) == (0, [1.5])
    copy.close()


def answer(result, req, out, data):
    if isinstance(result, BaseException):
        raise result
    return result


def test_results(model, seen):
    cb = model.addcb([0])
    cases = [(None, 0), (0, 0), (3, 1), (-7, -1), (2**70, 1), (-(2**70), -1)]
    for result, status in cases:
        model.setcbf(cb, partial(answer, result))
        assert evaluate(model, seen[-1], cb, 1, [0.0] * 3)[0] == status
    error = ValueError("not here")
    for result in (error, True, 1.5):
        model.setcbf(cb, partial(answer, result))
        assert evaluate(model, seen[-1], cb, 1, [0.0] * 3)[0] == -1
    assert raised(model).args[0].startswith("the result of")
    model.setcbf(cb, partial(answer, error))
    evaluate(model, seen[-1], cb, 1, [0.0] * 3)
    assert raised(model) is error


def failing(errors, req, out, data):
    raise errors.pop(0)


def test_keyed(model, seen):
    cb = model.addcb([0])
    errors = [ValueError("first"), ValueError("second")]
    expected = list(errors)
    model.setcbf(cb, partial(failing, errors))
    copy = model.copy()
    for target in (model, copy):
        assert evaluate(target, seen[-1], cb, 1, [0.0] * 3)[0] == -1
    assert raised(copy) is expected[1]
    assert raised(model) is expected[0]
    assert raised(model) is None
    copy.close()


def keep(found, req, out, data):
    found.extend((req, out))


def test_released(model, seen):
    cb = model.addcb([0])
    found = []
    model.setcbhv(cb, partial(keep, found))
    x = [1.0, 2.0, 3.0]
    assert evaluate(model, seen[-1], cb, 3, x, [1.0], x)[0] == 0
    req, out = found
    for view in (req.x, req.w, req.v, out):
        with pytest.raises(ValueError, match="released memoryview"):
            view[0]


def derivatives(found, req, out, data):
    found.append((list(req.w or []), list(req.v or [])))
    for k in range(len(out)):
        out[k] = k + 1.0


def test_patterns(model, seen):
    cb = model.addcb([0, 1])
    found = []
    function = partial(derivatives, found)
    model.setcbg(cb, [1, 0, 1], [2, 0, 2], function)
    x = [0.0] * 3
    assert evaluate(model, seen[-1], cb, 3, x) == (0, [1.0, 2.0, 3.0])
    model.setcbg(cb, [], [], function)
    assert evaluate(model, seen[-1], cb, 0, x) == (0, [])
    model.setcbh(cb, [0, 0, 0], [2, 0, 2], function)
    w = [2.0, 3.0]
    assert evaluate(model, seen[-1], cb, 3, x, w) == (0, [1.0, 2.0, 3.0])
    model.setcbhv(cb, function)
    v = [4.0, 5.0, 6.0]
    assert evaluate(model, seen[-1], cb, 3, x, [2.0, 3.0], v)[0] == 0
    assert found == [([], []), ([], []), ([2.0, 3.0], []), ([2.0, 3.0], v)]


def gathered(found, req, out, data):
    found.append((list(req.x), list(req.v or []), len(out)))
    for k in range(len(out)):
        out[k] = req.x[k] * (req.v[k] if req.v else 1.0)


def test_gathered(model):
    cb = model.addcb([1], [2, 0])
    found = []
    function = partial(gathered, found)
    model.setcbf(cb, function)
    model.setcbhv(cb, function)
    x = [1.0, 2.0, 3.0]
    assert model.evalf(x)[1] == 3.0
    assert model.evalhv(x, [0.0, 1.0, 0.0], [4.0, 5.0, 6.0]) == [4.0, 0.0, 18.0]
    assert found == [([3.0, 1.0], [], 1), ([3.0, 1.0], [6.0, 4.0], 2)]


def test_blocks(model, seen):
    fun = model.addfun(2)
    cb = model.addcb(fun)
    model.setcbg(cb, fun, [1, 2], answer)
    model.setcbg(cb, [fun[1], fun[1]], [0, 0], answer)
    message = refusal(model.setcbg, cb, [fun[0], 0], [0, 0], answer)
    uncovered = "callback 0: Jacobian function 0 is not covered"
    assert message == "invalid argument: " + uncovered


def attach_exp(model, idx):
    node = model.addexpr([2], [Operator.VARIABLE, Operator.EXP], [1, 1])
    model.attachexpr(idx, node * len(idx))


def test_expression(env, model):
    with biyos.Model(env) as plain:
        plain.addvar(3)
        plain.addfun(3)
        for built in (model, plain):
            attach_exp(built, [1])
        assert model.addcb([1, 0]) == 0
        for built in (model, plain):
            attach_exp(built, [0, 1])
        assert model.getnexpr([1]) == (5, 3)
        assert model.getexpr() == plain.getexpr()


def test_shared(model):
    attach_exp(model, [1])
    assert model.addcb([1, 0]) == 0
    assert model.addcb([2, 1]) == 1
    attach_exp(model, [1])
    assert model.getnexpr([1]) == (5, 3)


def test_refused(model):
    cb = model.addcb([0])
    bad = "invalid argument: "
    repeated = "argument 'fun': function 1 is repeated"
    cases = [
        (model.addcb, ([],), "argument 'm': 0 is not positive"),
        (model.addcb, ([1, 2, 1],), repeated),
        (model.addcb, ([3],), "function 3: unknown"),
        (model.setcbf, (1, answer), "callback 1: unknown"),
        (model.setcbg, (-1, [], [], answer), "callback -1: unknown"),
        (model.setcbh, (2, [], [], answer), "callback 2: unknown"),
        (model.setcbhv, (1, answer), "callback 1: unknown"),
        (model.setcbdata, (1, None), "callback 1: unknown"),
        (model.getcbdata, (1,), "callback 1: unknown"),
        (model.getncbfun, (1,), "callback 1: unknown"),
        (model.getcbfun, (-1,), "callback -1: unknown"),
        (model.getcbvar, (1,), "callback 1: unknown"),
        (model.addcb, ([1], [0, 0]), "argument 'var': variable 0 is repeated"),
        (model.addcb, ([1], [3]), "variable 3: unknown"),
    ]
    for call, arguments, message in cases:
        assert refusal(call, *arguments) == bad + message
    uncovered = "callback 0: Jacobian function 1 is not covered"
    assert refusal(model.setcbg, cb, [0, 1], [2, 1], answer) == bad + uncovered
    unknown = "callback 0: Jacobian of function 0: variable 3 is unknown"
    assert refusal(model.setcbg, cb, [0, 0], [2, 3], answer) == bad + unknown
    pair = "callback 0: Hessian pair (1, 0) has its second variable below"
    message = refusal(model.setcbh, cb, [1], [0], answer)
    assert message == bad + pair + " its first"


def test_unlisted(model):
    cb = model.addcb([0, 1], [2, 0])
    model.setcbg(cb, [1, 0], [0, 2], answer)
    model.setcbh(cb, [0, 2], [2, 2], answer)
    bad = "invalid argument: callback 0: "
    jacobian = "Jacobian of function 1: variable 1 is not listed"
    assert refusal(model.setcbg, cb, [0, 1], [2, 1], answer) == bad + jacobian
    hessian = "Hessian variable 1 is not listed"
    assert refusal(model.setcbh, cb, [0], [1], answer) == bad + hessian


def test_lengths(model):
    cb = model.addcb([0, 1])
    with pytest.raises(ValueError, match="var has length 1, not 2"):
        model.setcbg(cb, [0, 1], [0], answer)
    with pytest.raises(ValueError, match="var2 has length 1, not 2"):
        model.setcbh(cb, [0, 1], [1], answer)
    with pytest.raises(TypeError, match="f must be callable, not int"):
        model.setcbf(cb, 3)
    with pytest.raises(TypeError, match="g must be callable, not None"):
        model.setcbg(cb, [], [], None)
    with pytest.raises(TypeError, match="cb must be an int, not float"):
        model.setcbdata(0.0, None)


def changes(model):
    return (
        partial(model.addcb, [1]),
        partial(model.setcbf, 0, answer),
        partial(model.setcbg, 0, [], [], answer),
        partial(model.setcbh, 0, [], [], answer),
        partial(model.setcbhv, 0, answer),
        partial(model.setcbdata, 0, None),
        partial(model.delcb, [0]),
    )


def test_locked(env, model):
    model.addcb([0], data="kept")
    found = []

    def change(event):
        for call in changes(model):
            try:
                call()
            except biyos.Error as error:
                found.append(str(error))
        found.append(model.getcbdata(0))
        found.append(model.getcbfun(0).indices())

    env.addhook(change, EventKind.MESSAGE, EventLevel.ERROR)
    with pytest.raises(biyos.Error):
        env.setopt("seed", -1)
    assert found == [LOCKED] * 7 + ["kept", [0]]


def test_closed(env):
    model = biyos.Model(env)
    model.close()
    calls = (
        *changes(model),
        partial(model.getcbdata, 0),
        partial(model.getncbfun, 0),
        partial(model.getcbfun, 0),
    )
    for call in calls:
        with pytest.raises(ValueError, match="the model is closed"):
            call()


def constant(value, req, out, data):
    for k in range(len(out)):
        out[k] = value


def test_deleted(model):
    for cb, covered in enumerate(([0], [1, 2], [2])):
        assert model.addcb(covered) == cb
        model.setcbf(cb, partial(constant, 10.0 * (cb + 1)))
    assert model.evalf([0.0] * 3) == [10.0, 20.0, 50.0]
    assert model.delcb([2, 0]) == [-1, 0, -1]
    assert model.getcbfun(0).indices() == [1, 2]
    assert model.evalf([0.0] * 3) == [0.0, 20.0, 20.0]
    assert model.delfun([0]) == [-1, 0, 1]
    assert model.getcbfun(0).indices() == [0, 1]
    assert model.evalf([0.0] * 3) == [20.0, 20.0]
    assert model.delcb(range(1)) == [-1]
    assert model.delvar([2]) == [0, 1, -1]


def test_delete_refused(model):
    model.addcb([0])
    bad = "invalid argument: "
    assert refusal(model.delcb, [1]) == bad + "callback 1: unknown"
    repeated = "argument 'idx': callback 0 is repeated"
    assert refusal(model.delcb, [0, 0]) == bad + repeated
    with pytest.raises(TypeError, match="an entry of idx must be an int"):
        model.delcb([0.0])
    assert model.getcbfun(0).indices() == [0]


def test_delete_kept(env, model):
    model.addcb([0])
    model.addcb([1])
    model.setcbf(0, partial(constant, 1.0))

    def values(req, out, data):
        out[0] = 3.0

    alive = weakref.ref(values)
    model.setcbf(1, values)
    del values
    copy = model.copy()
    assert model.delcb([1]) == [0, -1]
    gc.collect()
    assert alive() is not None
    assert copy.evalf([0.0] * 3) == [1.0, 3.0, 0.0]
    assert model.evalf([0.0] * 3) == [1.0, 0.0, 0.0]
    copy.close()
