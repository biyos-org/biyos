import ctypes
from ctypes import (
    CFUNCTYPE,
    POINTER,
    byref,
    c_char,
    c_char_p,
    c_double,
    c_int,
    c_size_t,
    c_void_p,
)
from importlib.resources import files

OK = 0

KIND_BOOL = 1
KIND_INT = 2
KIND_DOUBLE = 3
KIND_STRING = 4
KIND_ENUM = 5

SUBSCRIBER = CFUNCTYPE(None, c_void_p, c_void_p)


def _declare(lib, name, result, *arguments):
    function = getattr(lib, name)
    function.restype = result
    function.argtypes = arguments


def _load():
    package = files("biyos")
    name = (package / "library.txt").read_text()
    lib = ctypes.CDLL(str(package / name))
    ints = POINTER(c_int)
    doubles = POINTER(c_double)
    strings = POINTER(c_char_p)
    _declare(lib, "biyos_version", None, ints, ints, ints)
    _declare(lib, "biyos_env_create", c_int, POINTER(c_void_p))
    _declare(lib, "biyos_env_free", c_int, c_void_p)
    _declare(lib, "biyos_last_error_message", c_char_p, c_void_p)
    _declare(lib, "biyos_option_id", c_int, c_void_p, c_char_p, ints)
    _declare(lib, "biyos_option_kind", c_int, c_void_p, c_int, ints)
    versions = (c_void_p, c_int, ints, ints, ints)
    _declare(lib, "biyos_option_versions", c_int, *versions)
    _declare(lib, "biyos_set_bool_option", c_int, c_void_p, c_int, c_int)
    _declare(lib, "biyos_set_int_option", c_int, c_void_p, c_int, c_int)
    _declare(lib, "biyos_set_double_option", c_int, c_void_p, c_int, c_double)
    _declare(lib, "biyos_set_string_option", c_int, c_void_p, c_int, c_char_p)
    _declare(lib, "biyos_get_bool_option", c_int, c_void_p, c_int, ints)
    _declare(lib, "biyos_get_int_option", c_int, c_void_p, c_int, ints)
    _declare(lib, "biyos_get_double_option", c_int, c_void_p, c_int, doubles)
    _declare(lib, "biyos_get_string_option", c_int, c_void_p, c_int, strings)
    _declare_events(lib, ints)
    _declare_model(lib)
    return lib


def _declare_events(lib, ints):
    subscribe = (c_void_p, SUBSCRIBER, c_void_p, c_int, c_int, ints)
    _declare(lib, "biyos_subscribe", c_int, *subscribe)
    _declare(lib, "biyos_unsubscribe", c_int, c_void_p, c_int)
    _declare(lib, "biyos_event_kind", c_int, c_void_p)
    _declare(lib, "biyos_event_level", c_int, c_void_p)
    _declare(lib, "biyos_event_module", c_char_p, c_void_p)
    _declare(lib, "biyos_event_text", c_char_p, c_void_p)
    _declare(lib, "biyos_event_field_count", c_int, c_void_p)
    _declare(lib, "biyos_event_field_name", c_char_p, c_void_p, c_int)
    _declare(lib, "biyos_event_field_value", c_char_p, c_void_p, c_int)
    _declare_formats(lib)


def _declare_formats(lib):
    arguments = (c_void_p, POINTER(c_char), c_size_t, POINTER(c_size_t))
    _declare(lib, "biyos_format_text", c_int, *arguments)
    _declare(lib, "biyos_format_json", c_int, *arguments)


def _declare_model(lib):
    _declare(lib, "biyos_model_create", c_int, c_void_p, POINTER(c_void_p))
    _declare(lib, "biyos_model_free", c_int, c_void_p)


lib = _load()


def version():
    major = c_int()
    minor = c_int()
    patch = c_int()
    lib.biyos_version(byref(major), byref(minor), byref(patch))
    return f"{major.value}.{minor.value}.{patch.value}"
