import weakref
from contextlib import contextmanager
from ctypes import byref, c_char_p, c_double, c_int, c_void_p
from numbers import Real

from biyos import _capi
from biyos._capi import lib
from biyos._error import Error, check
from biyos._handles import Handles
from biyos._integers import integer
from biyos._relay import Relay
from biyos._subscribers import Subscribers
from biyos._versions import Versions, version_text


def _encoded(text):
    if "\0" in text:
        raise ValueError("embedded null character")
    return text.encode()


def _name(name):
    if not isinstance(name, str):
        found = type(name).__name__
        raise TypeError(f"option name must be a str, not {found}")
    return _encoded(name)


def _rejected(name, noun, value):
    found = type(value).__name__
    return TypeError(f"option '{name}' takes {noun}, not {found}")


def _callable(function):
    if not callable(function):
        found = type(function).__name__
        raise TypeError(f"hook must be callable, not {found}")
    return function


def _bool(name, value):
    if not isinstance(value, bool):
        raise _rejected(name, "a bool", value)
    return int(value)


def _int(name, value):
    try:
        return integer(f"option '{name}'", value)
    except TypeError:
        raise _rejected(name, "an int", value) from None


def _double(name, value):
    if isinstance(value, bool) or not isinstance(value, Real):
        raise _rejected(name, "a number", value)
    return float(value)


def _string(name, value):
    if not isinstance(value, str):
        raise _rejected(name, "a str", value)
    return _encoded(value)


_SETTERS = {
    _capi.KIND_BOOL: (lib.BIYOS_setboolopt, _bool),
    _capi.KIND_INT: (lib.BIYOS_setintopt, _int),
    _capi.KIND_DOUBLE: (lib.BIYOS_setdblopt, _double),
    _capi.KIND_STRING: (lib.BIYOS_setstropt, _string),
    _capi.KIND_ENUM: (lib.BIYOS_setstropt, _string),
}

_GETTERS = {
    _capi.KIND_BOOL: (lib.BIYOS_getboolopt, c_int, bool),
    _capi.KIND_INT: (lib.BIYOS_getintopt, c_int, int),
    _capi.KIND_DOUBLE: (lib.BIYOS_getdblopt, c_double, float),
    _capi.KIND_STRING: (lib.BIYOS_getstropt, c_char_p, bytes.decode),
    _capi.KIND_ENUM: (lib.BIYOS_getstropt, c_char_p, bytes.decode),
}


def _release(models, handle):
    models.release()
    return lib.BIYOS_freeenv(handle)


class Env:
    def __init__(self):
        handle = c_void_p()
        status = lib.BIYOS_newenv(byref(handle))
        if status != _capi.OK:
            raise Error("cannot create the environment", status)
        self._handle = handle
        self._models = Handles()
        self._arm()
        self._relay = Relay()
        self._subscribers = Subscribers(self._relay)
        self._calls = 0
        self._closing = False
        self._pending = False

    def __enter__(self):
        return self

    def __exit__(self, *exc_info):
        self.close()

    def __copy__(self):
        raise TypeError("an environment cannot be copied")

    def __deepcopy__(self, memo):
        return self.__copy__()

    def close(self):
        """Free the environment and every model made on it.

        The models still open are closed first, so using one afterwards
        raises ``ValueError``. Closing a freed environment does nothing.
        Closed during a call on it or on one of its models, by a hook or
        a finalizer, it reads as closed at once and is freed when the
        outermost call returns. Such a deferred close never raises.

        Raises
        ------
        Error
            When the free is refused because a model is still allocated, as
            after an interrupted ``Model.close``. An exception raised by a
            hook during the refusal is raised instead. The environment
            stays allocated, and only a later ``close`` or its collection
            tries again; the collection cannot raise and fails silently.
        """
        self._closing = True
        self._pending = True
        if self._calls == 0:
            self._run(self._dispose)

    def setopt(self, name, val):
        option, kind = self._lookup(name)
        setter, convert = _SETTERS[kind]
        self._call(setter, option, convert(name, val))

    def getopt(self, name):
        option, kind = self._lookup(name)
        getter, slot, convert = _GETTERS[kind]
        value = slot()
        self._call(getter, option, byref(value))
        return convert(value.value)

    def getoptver(self, name):
        option = self._option(name)
        numbers = (c_int(), c_int(), c_int())
        pointers = (byref(number) for number in numbers)
        self._call(lib.BIYOS_getoptver, option, *pointers)
        return Versions(*(version_text(number.value) for number in numbers))

    def addhook(self, hk, knd, lvl):
        hk = _callable(hk)
        knd = integer("knd", knd)
        lvl = integer("lvl", lvl)
        subscription = c_int()
        with self._busy():
            self._addhook(hk, knd, lvl, subscription)
        return subscription.value

    def delhook(self, idx):
        idx = integer("idx", idx)
        with self._busy():
            self._delhook(idx)

    def _addhook(self, function, kind, lowest, subscription):
        callback = self._subscribers.callback
        out = byref(subscription)
        key = self._subscribers.add(function)
        try:
            self._call(lib.BIYOS_addhook, callback, key, kind, lowest, out)
        finally:
            self._subscribers.settle(key, subscription.value)

    def _delhook(self, subscription):
        self._call(lib.BIYOS_delhook, subscription)
        self._subscribers.remove(subscription)

    def _lookup(self, name):
        option = self._option(name)
        kind = c_int()
        self._call(lib.BIYOS_getoptknd, option, byref(kind))
        return option, kind.value

    def _option(self, name):
        option = c_int()
        self._call(lib.BIYOS_getoptid, _name(name), byref(option))
        return option.value

    def _call(self, function, *arguments):
        with self._busy():
            if not self._open():
                raise ValueError("the environment is closed")
            self._run(function, self._handle, *arguments)

    @contextmanager
    def _busy(self):
        """Keep the environment allocated while the block runs.

        A close during the block reads as closed at once and frees when the
        last hold ends. Inside the block, check ``_open()`` before calling C,
        register any handle the call creates in a ``finally``, and hold every
        other environment the call uses.
        """
        self._calls += 1
        try:
            yield
        finally:
            self._calls -= 1
            self._settle()

    def _run(self, function, *arguments):
        self._relay.start()
        status = function(*arguments)
        self._settle()
        self._relay.finish(self._handle, status)
        check(self._handle, status)

    def _open(self):
        return self._free.alive and not self._closing

    def _arm(self):
        arguments = (self._models, self._handle)
        self._free = weakref.finalize(self, _release, *arguments)

    def _settle(self):
        if self._relay.idle():
            self._drain()
        if self._calls == 0 and self._pending:
            self._dispose()

    def _drain(self):
        self._calls += 1
        try:
            self._models.drain()
        finally:
            self._calls -= 1

    def _dispose(self):
        self._pending = False
        if self._free.detach() is None:
            return _capi.OK
        status = _release(self._models, self._handle)
        if status == _capi.OK:
            self._subscribers.clear()
        else:
            self._arm()
        return status
