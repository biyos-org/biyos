import weakref
from ctypes import byref, c_char_p, c_double, c_int, c_void_p

from biyos import _capi
from biyos._capi import lib
from biyos._error import Error, check


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


def _bool(name, value):
    if not isinstance(value, bool):
        raise _rejected(name, "a bool", value)
    return int(value)


def _int(name, value):
    if isinstance(value, bool) or not isinstance(value, int):
        raise _rejected(name, "an int", value)
    if c_int(value).value != value:
        message = f"option '{name}': {value} does not fit in a C int"
        raise OverflowError(message)
    return value


def _double(name, value):
    if isinstance(value, bool) or not isinstance(value, (int, float)):
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


class Environment:
    def __init__(self):
        handle = c_void_p()
        status = lib.biyos_env_create(byref(handle))
        if status != _capi.OK:
            raise Error("cannot create the environment", status)
        self._handle = handle
        self._free = weakref.finalize(self, lib.biyos_env_free, handle)

    def __enter__(self):
        return self

    def __exit__(self, *exc_info):
        self.close()

    def close(self):
        if not self._free.alive:
            return
        check(self._handle, lib.biyos_env_free(self._handle))
        self._free.detach()

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

    def _lookup(self, name):
        option = c_int()
        kind = c_int()
        self._call(lib.biyos_option_id, _name(name), byref(option))
        self._call(lib.biyos_option_kind, option, byref(kind))
        return option.value, kind.value

    def _call(self, function, *arguments):
        if not self._free.alive:
            raise ValueError("the environment is closed")
        check(self._handle, function(self._handle, *arguments))
