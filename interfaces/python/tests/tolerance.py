from functools import partial

import pytest

import biyos
from biyos import EventKind, EventLevel
from refusal import LOCKED, refusal

INFINITY = float("inf")
NAN = float("nan")
UNSET = ([None] * 3, [None] * 3)


class Calls:
    def __init__(self, model, short):
        self.set = getattr(model, f"set{short}tol")
        self.get = getattr(model, f"get{short}tol")
        self.delete = getattr(model, f"del{short}tol")

    def every(self):
        return self.get(range(3))


def attempt(seen, calls):
    for call in calls:
        try:
            call()
        except biyos.Error as error:
            seen.append(str(error))


def expect_own(env, model, short, noun):
    tol = Calls(model, short)
    assert tol.every() == UNSET
    tol.set([1], atol=[0.5])
    tol.set(range(2), rtol=[0.25, 0.0])
    tol.set([2, 2], [1.0, 2.0], [-0.0, 0.125])
    tol.set([0], None, None)
    assert tol.every() == ([None, 0.5, 2.0], [0.25, 0.0, 0.125])
    assert tol.get([2, 1]) == ([2.0, 0.5], [0.125, 0.0])
    tol.delete(range(1, 3))
    tol.delete([0, 0])
    assert tol.every() == UNSET


def expect_refused(env, model, short, noun):
    tol = Calls(model, short)
    tol.set([0], [1.0], [2.0])
    record = f"invalid argument: {noun} 1: "
    message = refusal(tol.set, [0, 1], [3.0, NAN])
    assert message == record + "absolute tolerance is NaN"
    message = refusal(tol.set, [0, 1], None, [3.0, INFINITY])
    assert message == record + "relative tolerance is infinity"
    message = refusal(tol.set, range(2), [3.0, -0.5])
    assert message == record + "absolute tolerance -0.5 is negative"
    unknown = f"invalid argument: {noun} 3: unknown"
    assert refusal(tol.delete, [0, 3]) == unknown
    assert refusal(tol.get, [3]) == unknown
    with pytest.raises(ValueError, match="rtol has length 1, not 2"):
        tol.set([0, 1], rtol=[1.0])
    assert tol.every() == ([1.0, None, None], [2.0, None, None])


def expect_locked(env, model, short, noun):
    tol = Calls(model, short)
    seen = []
    calls = (partial(tol.set, [0], [0.5]), partial(tol.delete, [0]))

    def change(event):
        attempt(seen, calls)
        seen.append(tol.every())

    env.addhook(change, EventKind.MESSAGE, EventLevel.ERROR)
    with pytest.raises(biyos.Error):
        env.setopt("seed", -1)
    assert seen == [LOCKED] * 2 + [UNSET]


def expect_copied(env, model, short, noun):
    Calls(model, short).set([2], [0.5], [0.25])
    with model.copy() as copy:
        tol = Calls(copy, short)
        assert tol.get([2]) == ([0.5], [0.25])
        tol.delete(range(3))
        assert tol.every() == UNSET
    assert Calls(model, short).get([2]) == ([0.5], [0.25])


def expect_closed(env, model, short, noun):
    tol = Calls(model, short)
    model.close()
    for call in (tol.set, tol.get, tol.delete):
        with pytest.raises(ValueError, match="the model is closed"):
            call([])


CHECKS = [
    expect_own,
    expect_refused,
    expect_locked,
    expect_copied,
    expect_closed,
]
