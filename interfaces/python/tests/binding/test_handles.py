from functools import partial

import pytest

from biyos._capi import OK, lib
from biyos._convert.handles import Handles


def record(freed, handle):
    freed.append(handle)
    return OK


def refuse(handle):
    return -100


class Parked(list):
    def __init__(self, actions):
        super().__init__()
        self.actions = actions

    def pop(self, *arguments):
        while self.actions:
            self.actions.pop(0)()
        return super().pop(*arguments)


@pytest.fixture
def freed(monkeypatch):
    freed = []
    monkeypatch.setattr(lib, "BIYOS_freemdl", partial(record, freed))
    return freed


def test_closed_park(freed):
    handles = Handles()
    handles.add(1)
    handles.closed = True
    handles.park(1, None)
    assert handles._parked == []


def test_stray_park(freed):
    handles = Handles()
    handles.park(1, None)
    assert handles._parked == []


def test_closed_drain(freed):
    handles = Handles()
    handles.add(1)
    handles.park(1, None)
    handles.closed = True
    handles.drain()
    assert freed == []


def test_taken_drain(freed):
    handles = Handles()
    handles.add(1)
    handles.park(1, None)
    assert handles.take(1)
    handles.drain()
    assert freed == []
    assert not handles.take(1)


def test_nested_drain(freed):
    handles = Handles()
    handles._parked = Parked([handles.drain])
    handles.add(1)
    handles.park(1, None)
    handles.drain()
    assert freed == [1]


def test_kept(freed):
    handles = Handles()
    handles.add(1)
    handles.park(1, "callback")
    assert handles._kept == {id(1): "callback"}
    handles.drain()
    assert freed == [1]
    assert handles._kept == {}


def test_refused(monkeypatch):
    monkeypatch.setattr(lib, "BIYOS_freemdl", refuse)
    handles = Handles()
    handles.add(1)
    handles.park(1, "callback")
    handles.drain()
    assert handles._kept == {id(1): "callback"}
    handles.release()
    assert handles._kept == {}
