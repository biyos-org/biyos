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
        raise TypeError(f"subscriber must be callable, not {found}")
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
    _capi.KIND_BOOL: (lib.biyos_set_bool_option, _bool),
    _capi.KIND_INT: (lib.biyos_set_int_option, _int),
    _capi.KIND_DOUBLE: (lib.biyos_set_double_option, _double),
    _capi.KIND_STRING: (lib.biyos_set_string_option, _string),
    _capi.KIND_ENUM: (lib.biyos_set_string_option, _string),
}

_GETTERS = {
    _capi.KIND_BOOL: (lib.biyos_get_bool_option, c_int, bool),
    _capi.KIND_INT: (lib.biyos_get_int_option, c_int, int),
    _capi.KIND_DOUBLE: (lib.biyos_get_double_option, c_double, float),
    _capi.KIND_STRING: (lib.biyos_get_string_option, c_char_p, bytes.decode),
    _capi.KIND_ENUM: (lib.biyos_get_string_option, c_char_p, bytes.decode),
}


def _release(models, handle):
    models.release()
    return lib.biyos_env_free(handle)


class Environment:
    def __init__(self):
        handle = c_void_p()
        status = lib.biyos_env_create(byref(handle))
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
        Closed during a call on it or on one of its models, by a subscriber
        or a finalizer, it reads as closed at once and is freed when the
        outermost call returns. Such a deferred close never raises.

        Raises
        ------
        Error
            When the free is refused because a model is still allocated, as
            after an interrupted ``Model.close``. An exception raised by a
            subscriber during the refusal is raised instead. The environment
            stays allocated, and only a later ``close`` or its collection
            tries again; the collection cannot raise and fails silently.
        """
        self._closing = True
        self._pending = True
        if self._calls == 0:
            self._run(self._dispose)

    def set_option(self, name, value):
        option, kind = self._lookup(name)
        setter, convert = _SETTERS[kind]
        self._call(setter, option, convert(name, value))

    def get_option(self, name):
        option, kind = self._lookup(name)
        getter, slot, convert = _GETTERS[kind]
        value = slot()
        self._call(getter, option, byref(value))
        return convert(value.value)

    def option_versions(self, name):
        option = self._option(name)
        numbers = (c_int(), c_int(), c_int())
        pointers = (byref(number) for number in numbers)
        self._call(lib.biyos_option_versions, option, *pointers)
        return Versions(*(version_text(number.value) for number in numbers))

    def subscribe(self, function, kind, lowest):
        function = _callable(function)
        kind = integer("kind", kind)
        lowest = integer("lowest", lowest)
        subscription = c_int()
        with self._busy():
            self._subscribe(function, kind, lowest, subscription)
        return subscription.value

    def unsubscribe(self, subscription):
        subscription = integer("subscription", subscription)
        with self._busy():
            self._unsubscribe(subscription)

    def _subscribe(self, function, kind, lowest, subscription):
        callback = self._subscribers.callback
        out = byref(subscription)
        key = self._subscribers.add(function)
        try:
            self._call(lib.biyos_subscribe, callback, key, kind, lowest, out)
        finally:
            self._subscribers.settle(key, subscription.value)

    def _unsubscribe(self, subscription):
        self._call(lib.biyos_unsubscribe, subscription)
        self._subscribers.remove(subscription)

    def _lookup(self, name):
        option = self._option(name)
        kind = c_int()
        self._call(lib.biyos_option_kind, option, byref(kind))
        return option, kind.value

    def _option(self, name):
        option = c_int()
        self._call(lib.biyos_option_id, _name(name), byref(option))
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
