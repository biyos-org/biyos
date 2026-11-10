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
        self._free()

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
