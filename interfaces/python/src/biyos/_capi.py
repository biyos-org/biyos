import ctypes
from ctypes import (
    CFUNCTYPE,
    POINTER,
    byref,
    c_char,
    c_char_p,
    c_double,
    c_int,
    c_int32,
    c_int64,
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

HOOK = CFUNCTYPE(None, c_void_p, c_void_p)


def _declare(
    lib: ctypes.CDLL, name: str, result: type | None, *arguments: type
) -> None:
    function = getattr(lib, name)
    function.restype = result
    function.argtypes = arguments


def _declare_pair(lib: ctypes.CDLL, name: str, *values: type) -> None:
    listed = (c_void_p, c_int32, POINTER(c_int32))
    _declare(lib, f"{name}sp", c_int, *listed, *values)
    _declare(lib, f"{name}rg", c_int, c_void_p, c_int32, c_int32, *values)


def _load() -> ctypes.CDLL:
    package = files("biyos")
    name = (package / "library.txt").read_text()
    lib = ctypes.CDLL(str(package / name))
    ints = POINTER(c_int32)
    doubles = POINTER(c_double)
    strings = POINTER(c_char_p)
    _declare(lib, "BIYOS_version", None, ints, ints, ints)
    _declare(lib, "BIYOS_newenv", c_int, POINTER(c_void_p))
    _declare(lib, "BIYOS_freeenv", c_int, c_void_p)
    _declare(lib, "BIYOS_lasterror", c_char_p, c_void_p)
    _declare(lib, "BIYOS_interrupt", c_int, c_void_p)
    _declare(lib, "BIYOS_getoptid", c_int, c_void_p, c_char_p, ints)
    _declare(lib, "BIYOS_getoptknd", c_int, c_void_p, c_int32, ints)
    versions = (c_void_p, c_int32, ints, ints, ints)
    _declare(lib, "BIYOS_getoptver", c_int, *versions)
    _declare(lib, "BIYOS_setenvboolopt", c_int, c_void_p, c_int32, c_int32)
    _declare(lib, "BIYOS_setenvintopt", c_int, c_void_p, c_int32, c_int32)
    _declare(lib, "BIYOS_setenvdblopt", c_int, c_void_p, c_int32, c_double)
    _declare(lib, "BIYOS_setenvstropt", c_int, c_void_p, c_int32, c_char_p)
    _declare(lib, "BIYOS_getenvboolopt", c_int, c_void_p, c_int32, ints)
    _declare(lib, "BIYOS_getenvintopt", c_int, c_void_p, c_int32, ints)
    _declare(lib, "BIYOS_getenvdblopt", c_int, c_void_p, c_int32, doubles)
    _declare(lib, "BIYOS_getenvstropt", c_int, c_void_p, c_int32, strings)
    _declare_events(lib, ints)
    _declare_model(lib, ints, doubles)
    return lib


def _declare_events(lib: ctypes.CDLL, ints: type) -> None:
    arguments = (c_void_p, HOOK, c_void_p, c_int32, c_int32, ints)
    _declare(lib, "BIYOS_addenvhook", c_int, *arguments)
    _declare(lib, "BIYOS_delenvhook", c_int, c_void_p, c_int32)
    _declare(lib, "BIYOS_getevtknd", c_int32, c_void_p)
    _declare(lib, "BIYOS_getevtlvl", c_int32, c_void_p)
    _declare(lib, "BIYOS_getevtmod", c_char_p, c_void_p)
    _declare(lib, "BIYOS_getevttext", c_char_p, c_void_p)
    _declare(lib, "BIYOS_getnevtfld", c_int32, c_void_p)
    _declare(lib, "BIYOS_getevtfldname", c_char_p, c_void_p, c_int32)
    _declare(lib, "BIYOS_getevtfldval", c_char_p, c_void_p, c_int32)
    _declare_formats(lib)


def _declare_formats(lib: ctypes.CDLL) -> None:
    arguments = (c_void_p, c_size_t, POINTER(c_char), POINTER(c_size_t))
    _declare(lib, "BIYOS_fmttext", c_int, *arguments)
    _declare(lib, "BIYOS_fmtjson", c_int, *arguments)


def _declare_model(lib: ctypes.CDLL, ints: type, doubles: type) -> None:
    _declare(lib, "BIYOS_newmdl", c_int, c_void_p, POINTER(c_void_p))
    copy = (c_void_p, c_void_p, POINTER(c_void_p))
    _declare(lib, "BIYOS_copymdl", c_int, *copy)
    _declare(lib, "BIYOS_freemdl", c_int, c_void_p)
    variables = (c_void_p, c_int32, ints, doubles, doubles)
    _declare(lib, "BIYOS_addvar", c_int, *variables, ints)
    _declare_pair(lib, "BIYOS_setvarbnd", doubles, doubles)
    _declare_pair(lib, "BIYOS_getvarbnd", doubles, doubles)
    _declare_pair(lib, "BIYOS_setvarknd", ints)
    _declare_pair(lib, "BIYOS_getvarknd", ints)
    _declare(lib, "BIYOS_getnvar", c_int, c_void_p, ints)
    _declare_pair(lib, "BIYOS_getvar", ints, doubles, doubles)
    _declare_functions(lib, ints, doubles)
    _declare_quadratic(lib, ints, doubles)
    _declare_constraints(lib, ints, doubles)
    _declare_objectives(lib, ints, doubles)
    _declare_names(lib, ints)


def _declare_functions(lib: ctypes.CDLL, ints: type, doubles: type) -> None:
    offsets = POINTER(c_int64)
    add = (c_void_p, c_int32, doubles, offsets, ints, doubles, ints)
    _declare(lib, "BIYOS_addfuns", c_int, *add)
    _declare(lib, "BIYOS_getnfun", c_int, c_void_p, ints)
    sized = (offsets, ints, doubles)
    read = (c_void_p, c_int32, c_int32, c_int64, doubles, offsets, *sized)
    _declare(lib, "BIYOS_getfuns", c_int, *read)
    for verb in ("add", "set", "get"):
        _declare_pair(lib, f"BIYOS_{verb}cst", doubles)
    terms = (c_void_p, c_int32, ints, offsets, ints, doubles)
    _declare(lib, "BIYOS_addlinterms", c_int, *terms)
    coordinates = (c_void_p, c_int64, ints, ints, doubles)
    _declare(lib, "BIYOS_addlintermsp", c_int, *coordinates)
    ranged = (c_void_p, c_int32, c_int32, offsets, ints, doubles)
    _declare(lib, "BIYOS_addrlintermrg", c_int, *ranged)
    _declare(lib, "BIYOS_addclintermrg", c_int, *ranged)
    for name in ("rlinterm", "clinterm"):
        _declare_terms(lib, name, ints, doubles)


def _declare_terms(
    lib: ctypes.CDLL, name: str, ints: type, doubles: type
) -> None:
    offsets = POINTER(c_int64)
    _declare_pair(lib, f"BIYOS_getn{name}", offsets)
    listed = (c_void_p, c_int32, ints, c_int64, offsets, ints, ints, doubles)
    _declare(lib, f"BIYOS_get{name}sp", c_int, *listed)
    ranged = (c_void_p, c_int32, c_int32, c_int64, offsets, ints, doubles)
    _declare(lib, f"BIYOS_get{name}rg", c_int, *ranged)


def _declare_quadratic(lib: ctypes.CDLL, ints: type, doubles: type) -> None:
    offsets = POINTER(c_int64)
    add = (c_void_p, c_int32, ints, offsets, ints, ints, doubles)
    _declare(lib, "BIYOS_addquadterms", c_int, *add)
    sized = (offsets, ints, ints, doubles)
    read = (c_void_p, c_int32, c_int32, c_int64, offsets, *sized)
    _declare(lib, "BIYOS_getquadtermrg", c_int, *read)


def _declare_constraints(lib: ctypes.CDLL, ints: type, doubles: type) -> None:
    add = (c_void_p, c_int32, ints, doubles, doubles, ints)
    _declare(lib, "BIYOS_addcon", c_int, *add)
    _declare_pair(lib, "BIYOS_setconbnd", doubles, doubles)
    _declare_pair(lib, "BIYOS_getconbnd", doubles, doubles)
    _declare(lib, "BIYOS_getncon", c_int, c_void_p, ints)
    _declare_pair(lib, "BIYOS_getcon", ints, doubles, doubles)


def _declare_objectives(lib: ctypes.CDLL, ints: type, doubles: type) -> None:
    fields = (ints, ints, doubles)
    add = (c_void_p, c_int32, ints, *fields, ints)
    _declare(lib, "BIYOS_addobj", c_int, *add)
    for name, values in (("sense", ints), ("prio", ints), ("coef", doubles)):
        _declare_pair(lib, f"BIYOS_setobj{name}", values)
        _declare_pair(lib, f"BIYOS_getobj{name}", values)
    _declare_pair(lib, "BIYOS_setobjtol", doubles, doubles)
    _declare_pair(lib, "BIYOS_getobjtol", doubles, doubles)
    _declare(lib, "BIYOS_getnobj", c_int, c_void_p, ints)
    _declare_pair(lib, "BIYOS_getobj", ints, *fields)


def _declare_names(lib: ctypes.CDLL, ints: type) -> None:
    strings = POINTER(c_char_p)
    _declare(lib, "BIYOS_setmdlname", c_int, c_void_p, c_char_p)
    _declare(lib, "BIYOS_getmdlname", c_int, c_void_p, strings)
    _declare(lib, "BIYOS_delmdlname", c_int, c_void_p)
    find = (c_void_p, c_int32, strings, ints)
    for noun in ("var", "fun", "con", "obj"):
        _declare_pair(lib, f"BIYOS_set{noun}name", strings)
        _declare_pair(lib, f"BIYOS_get{noun}name", strings)
        _declare(lib, f"BIYOS_get{noun}idx", c_int, *find)
        _declare_pair(lib, f"BIYOS_del{noun}name")


lib = _load()


def version() -> str:
    """Return the version of the library.

    Returns
    -------
    ver
        The version, such as ``"0.1.0"``.

    See Also
    --------
    BIYOS_version
    """
    major = c_int()
    minor = c_int()
    patch = c_int()
    lib.BIYOS_version(byref(major), byref(minor), byref(patch))
    return f"{major.value}.{minor.value}.{patch.value}"
