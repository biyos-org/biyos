import ctypes
import sys
from ctypes import POINTER, byref, c_char_p, c_double, c_int, c_void_p
from importlib.resources import files

OK = 0

KIND_BOOL = 1
KIND_INT = 2
KIND_DOUBLE = 3
KIND_STRING = 4
KIND_ENUM = 5


def _file_name():
    if sys.platform == "win32":
        return "biyos.dll"
    if sys.platform == "darwin":
        return "libbiyos.dylib"
    return "libbiyos.so"


def _declare(lib, name, result, *arguments):
    function = getattr(lib, name)
    function.restype = result
    function.argtypes = arguments


def _load():
    lib = ctypes.CDLL(str(files("biyos") / _file_name()))
    ints = POINTER(c_int)
    doubles = POINTER(c_double)
    strings = POINTER(c_char_p)
    _declare(lib, "biyos_version", None, ints, ints, ints)
    _declare(lib, "biyos_env_create", c_int, POINTER(c_void_p))
    _declare(lib, "biyos_env_free", None, c_void_p)
    _declare(lib, "biyos_last_error_message", c_char_p, c_void_p)
    _declare(lib, "biyos_option_id", c_int, c_void_p, c_char_p, ints)
    _declare(lib, "biyos_option_kind", c_int, c_void_p, c_int, ints)
    _declare(lib, "biyos_set_bool_option", c_int, c_void_p, c_int, c_int)
    _declare(lib, "biyos_set_int_option", c_int, c_void_p, c_int, c_int)
    _declare(lib, "biyos_set_double_option", c_int, c_void_p, c_int, c_double)
    _declare(lib, "biyos_set_string_option", c_int, c_void_p, c_int, c_char_p)
    _declare(lib, "biyos_get_bool_option", c_int, c_void_p, c_int, ints)
    _declare(lib, "biyos_get_int_option", c_int, c_void_p, c_int, ints)
    _declare(lib, "biyos_get_double_option", c_int, c_void_p, c_int, doubles)
    _declare(lib, "biyos_get_string_option", c_int, c_void_p, c_int, strings)
    return lib


lib = _load()


def version():
    major = c_int()
    minor = c_int()
    patch = c_int()
    lib.biyos_version(byref(major), byref(minor), byref(patch))
    return f"{major.value}.{minor.value}.{patch.value}"
