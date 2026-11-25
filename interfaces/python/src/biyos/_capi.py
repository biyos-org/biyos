import ctypes
import math
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
    c_uint64,
    c_void_p,
)
from importlib.resources import files

inf = math.inf
"""Infinity, ``BIYOS_INF`` of the C header: a bound of ``inf`` or ``-inf``
leaves that side open."""

OK = 0

OPTKND_BOOL = 1
OPTKND_INT = 2
OPTKND_DBL = 3
OPTKND_STR = 4
OPTKND_ENUM = 5
OPTKND_FLG = 6

HOOK = CFUNCTYPE(None, c_void_p, c_void_p)


class ErrorRecord(ctypes.Structure):
    _fields_ = [
        ("size", c_size_t),
        ("status", c_int),
        ("mdl", c_void_p),
        ("msg", c_char_p),
    ]


class RequestRecord(ctypes.Structure):
    _fields_ = [
        ("size", c_size_t),
        ("mdl", c_void_p),
        ("cb", c_int32),
        ("thread", c_int32),
        ("x", c_void_p),
        ("w", c_void_p),
        ("v", c_void_p),
    ]


CALLBACK = CFUNCTYPE(c_int, POINTER(RequestRecord), c_void_p, c_void_p)


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


def _declare_tolerances(lib: ctypes.CDLL, family: str, doubles: type) -> None:
    _declare_pair(lib, f"BIYOS_set{family}tol", doubles, doubles)
    _declare_pair(lib, f"BIYOS_get{family}tol", doubles, doubles)
    _declare_pair(lib, f"BIYOS_del{family}tol")


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
    record = POINTER(ErrorRecord)
    _declare(lib, "BIYOS_lasterror", c_int, c_void_p, record)
    _declare(lib, "BIYOS_interrupt", c_int, c_void_p)
    _declare(lib, "BIYOS_getoptid", c_int, c_void_p, c_char_p, ints)
    _declare(lib, "BIYOS_getoptknd", c_int, c_void_p, c_int32, ints)
    versions = (c_void_p, c_int32, ints, ints, ints)
    _declare(lib, "BIYOS_getoptver", c_int, *versions)
    _declare(lib, "BIYOS_setenvboolopt", c_int, c_void_p, c_int32, c_int32)
    _declare(lib, "BIYOS_setenvintopt", c_int, c_void_p, c_int32, c_int32)
    _declare(lib, "BIYOS_setenvdblopt", c_int, c_void_p, c_int32, c_double)
    _declare(lib, "BIYOS_setenvstropt", c_int, c_void_p, c_int32, c_char_p)
    masks = (c_uint64, c_uint64)
    _declare(lib, "BIYOS_setenvflgopt", c_int, c_void_p, c_int32, *masks)
    _declare(lib, "BIYOS_getenvboolopt", c_int, c_void_p, c_int32, ints)
    _declare(lib, "BIYOS_getenvintopt", c_int, c_void_p, c_int32, ints)
    _declare(lib, "BIYOS_getenvdblopt", c_int, c_void_p, c_int32, doubles)
    _declare(lib, "BIYOS_getenvstropt", c_int, c_void_p, c_int32, strings)
    pointers = (POINTER(c_uint64), POINTER(c_uint64))
    _declare(lib, "BIYOS_getenvflgopt", c_int, c_void_p, c_int32, *pointers)
    _declare_events(lib, ints)
    _declare_model(lib, ints, doubles)
    return lib


def _declare_events(lib: ctypes.CDLL, ints: type) -> None:
    arguments = (c_void_p, HOOK, c_void_p, c_int32, c_int32, ints)
    _declare(lib, "BIYOS_addenvhook", c_int, *arguments)
    _declare(lib, "BIYOS_delenvhook", c_int, c_void_p, c_int32)
    _declare(lib, "BIYOS_addmdlhook", c_int, *arguments)
    _declare(lib, "BIYOS_delmdlhook", c_int, c_void_p, c_int32)
    _declare(lib, "BIYOS_getevtknd", c_int32, c_void_p)
    _declare(lib, "BIYOS_getevtlvl", c_int32, c_void_p)
    _declare(lib, "BIYOS_getevtmod", c_char_p, c_void_p)
    _declare(lib, "BIYOS_getevttext", c_char_p, c_void_p)
    _declare(lib, "BIYOS_getnevtfld", c_int32, c_void_p)
    _declare(lib, "BIYOS_getevtfldname", c_char_p, c_void_p, c_int32)
    _declare(lib, "BIYOS_getevtfldval", c_char_p, c_void_p, c_int32)
    _declare(lib, "BIYOS_getevtmdl", c_void_p, c_void_p)
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
    read = (c_void_p, c_char_p, c_int32, POINTER(c_void_p))
    _declare(lib, "BIYOS_readmdl", c_int, *read)
    _declare(lib, "BIYOS_writemdl", c_int, c_void_p, c_char_p, c_int32)
    _declare_variables(lib, ints, doubles)
    _declare_functions(lib, ints, doubles)
    _declare_quadratic(lib, ints, doubles)
    _declare_constraints(lib, ints, doubles)
    _declare_objectives(lib, ints, doubles)
    _declare_indicators(lib, ints, doubles)
    _declare_sos(lib, ints, doubles)
    _declare_pwl(lib, ints, doubles)
    _declare_cones(lib, ints, doubles)
    _declare_complements(lib, ints, doubles)
    _declare_names(lib, ints)
    _declare_expressions(lib, ints, doubles)
    _declare_settings(lib, ints, doubles)
    _declare_callbacks(lib, ints)
    _declare_evaluation(lib, ints)


def _declare_variables(lib: ctypes.CDLL, ints: type, doubles: type) -> None:
    variables = (c_void_p, c_int32, ints, doubles, doubles)
    _declare(lib, "BIYOS_addvar", c_int, *variables, ints)
    _declare_pair(lib, "BIYOS_setvarbnd", doubles, doubles)
    _declare_pair(lib, "BIYOS_getvarbnd", doubles, doubles)
    _declare_pair(lib, "BIYOS_setvarknd", ints)
    _declare_pair(lib, "BIYOS_getvarknd", ints)
    _declare_tolerances(lib, "var", doubles)
    _declare(lib, "BIYOS_getnvar", c_int, c_void_p, ints)
    _declare_pair(lib, "BIYOS_getvar", ints, doubles, doubles)
    _declare_pair(lib, "BIYOS_delvar", ints)


def _declare_functions(lib: ctypes.CDLL, ints: type, doubles: type) -> None:
    offsets = POINTER(c_int64)
    _declare(lib, "BIYOS_addfun", c_int, c_void_p, c_int32, ints)
    _declare(lib, "BIYOS_getnfun", c_int, c_void_p, ints)
    for verb in ("add", "set", "get"):
        _declare_pair(lib, f"BIYOS_{verb}cst", doubles)
    _declare_pair(lib, "BIYOS_delfun", ints)
    coordinates = (c_void_p, c_int64, ints, ints, doubles)
    _declare(lib, "BIYOS_addlintermsp", c_int, *coordinates)
    ranged = (c_void_p, c_int32, c_int32, offsets, ints, doubles)
    _declare(lib, "BIYOS_addrlintermrg", c_int, *ranged)
    _declare(lib, "BIYOS_addclintermrg", c_int, *ranged)
    for name in ("rlinterm", "clinterm"):
        _declare_terms(lib, name, ints, doubles)


def _declare_terms(
    lib: ctypes.CDLL, name: str, ints: type, doubles: type, others: int = 1
) -> None:
    offsets = POINTER(c_int64)
    _declare_pair(lib, f"BIYOS_getn{name}", offsets)
    values = (*[ints] * others, doubles)
    listed = (c_void_p, c_int32, ints, c_int64, offsets, ints, *values)
    _declare(lib, f"BIYOS_get{name}sp", c_int, *listed)
    ranged = (c_void_p, c_int32, c_int32, c_int64, offsets)
    _declare(lib, f"BIYOS_get{name}rg", c_int, *ranged, *values)


def _declare_quadratic(lib: ctypes.CDLL, ints: type, doubles: type) -> None:
    offsets = POINTER(c_int64)
    coordinates = (c_void_p, c_int64, ints, ints, ints, doubles)
    _declare(lib, "BIYOS_addquadtermsp", c_int, *coordinates)
    ranged = (c_void_p, c_int32, c_int32, offsets, ints, ints, doubles)
    _declare(lib, "BIYOS_addquadtermrg", c_int, *ranged)
    _declare_terms(lib, "quadterm", ints, doubles, 2)


def _declare_constraints(lib: ctypes.CDLL, ints: type, doubles: type) -> None:
    add = (c_void_p, c_int32, ints, doubles, doubles, ints)
    _declare(lib, "BIYOS_addcon", c_int, *add)
    _declare_pair(lib, "BIYOS_setconbnd", doubles, doubles)
    _declare_pair(lib, "BIYOS_getconbnd", doubles, doubles)
    _declare_tolerances(lib, "con", doubles)
    _declare(lib, "BIYOS_getncon", c_int, c_void_p, ints)
    _declare_pair(lib, "BIYOS_getcon", ints, doubles, doubles)
    _declare_pair(lib, "BIYOS_delcon", ints)


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
    _declare_pair(lib, "BIYOS_delobj", ints)


def _declare_indicators(lib: ctypes.CDLL, ints: type, doubles: type) -> None:
    fields = (ints, ints, ints, doubles, doubles)
    _declare(lib, "BIYOS_addind", c_int, c_void_p, c_int32, *fields, ints)
    _declare_pair(lib, "BIYOS_setindbnd", doubles, doubles)
    _declare_pair(lib, "BIYOS_getindbnd", doubles, doubles)
    _declare_pair(lib, "BIYOS_setindvar", ints, ints)
    _declare_pair(lib, "BIYOS_getindvar", ints, ints)
    _declare(lib, "BIYOS_getnind", c_int, c_void_p, ints)
    _declare_pair(lib, "BIYOS_getind", *fields)
    _declare_pair(lib, "BIYOS_delind", ints)


def _declare_sos(lib: ctypes.CDLL, ints: type, doubles: type) -> None:
    offsets = POINTER(c_int64)
    add = (c_void_p, c_int32, ints, offsets, ints, doubles, ints)
    _declare(lib, "BIYOS_addsos", c_int, *add)
    _declare(lib, "BIYOS_getnsos", c_int, c_void_p, ints)
    _declare_pair(lib, "BIYOS_getnsosvar", offsets)
    _declare_pair(lib, "BIYOS_getsos", c_int64, ints, offsets, ints, doubles)
    _declare_pair(lib, "BIYOS_delsos", ints)


def _declare_pwl(lib: ctypes.CDLL, ints: type, doubles: type) -> None:
    offsets = POINTER(c_int64)
    add = (c_void_p, c_int32, offsets, doubles, doubles, ints)
    _declare(lib, "BIYOS_addpwl", c_int, *add)
    _declare(lib, "BIYOS_getnpwl", c_int, c_void_p, ints)
    _declare_pair(lib, "BIYOS_getnpwlpt", offsets)
    _declare_pair(lib, "BIYOS_getpwl", c_int64, offsets, doubles, doubles)
    node = (c_void_p, c_int32, c_int64, offsets)
    _declare(lib, "BIYOS_addpwlnode", c_int, *node)


def _declare_cones(lib: ctypes.CDLL, ints: type, doubles: type) -> None:
    offsets = POINTER(c_int64)
    add = (c_void_p, c_int32, ints, offsets, ints, offsets, doubles, ints)
    _declare(lib, "BIYOS_addcone", c_int, *add)
    _declare_tolerances(lib, "cone", doubles)
    _declare(lib, "BIYOS_getncone", c_int, c_void_p, ints)
    _declare_pair(lib, "BIYOS_getnconefun", offsets)
    _declare_pair(lib, "BIYOS_getnconeprm", offsets)
    _declare_pair(lib, "BIYOS_getcone", c_int64, ints, offsets, ints)
    _declare_pair(lib, "BIYOS_getconeprm", c_int64, offsets, doubles)
    _declare_pair(lib, "BIYOS_delcone", ints)


def _declare_complements(lib: ctypes.CDLL, ints: type, doubles: type) -> None:
    add = (c_void_p, c_int32, ints, ints, doubles, doubles, ints)
    _declare(lib, "BIYOS_addccon", c_int, *add)
    for name in ("bnd", "tol"):
        _declare_pair(lib, f"BIYOS_setccon{name}", doubles, doubles)
        _declare_pair(lib, f"BIYOS_getccon{name}", doubles, doubles)
    _declare_pair(lib, "BIYOS_delccontol")
    _declare(lib, "BIYOS_getnccon", c_int, c_void_p, ints)
    _declare_pair(lib, "BIYOS_getccon", ints, ints, doubles, doubles)
    _declare_pair(lib, "BIYOS_delccon", ints)


def _declare_names(lib: ctypes.CDLL, ints: type) -> None:
    strings = POINTER(c_char_p)
    _declare(lib, "BIYOS_setmdlname", c_int, c_void_p, c_char_p)
    _declare(lib, "BIYOS_getmdlname", c_int, c_void_p, strings)
    _declare(lib, "BIYOS_delmdlname", c_int, c_void_p)
    find = (c_void_p, c_int32, strings, ints)
    for noun in ("var", "fun", "con", "obj", "ind", "sos", "cone", "ccon"):
        _declare_pair(lib, f"BIYOS_set{noun}name", strings)
        _declare_pair(lib, f"BIYOS_get{noun}name", strings)
        _declare(lib, f"BIYOS_get{noun}idx", c_int, *find)
        _declare_pair(lib, f"BIYOS_del{noun}name")


def _declare_expressions(lib: ctypes.CDLL, ints: type, doubles: type) -> None:
    offsets = POINTER(c_int64)
    _declare(lib, "BIYOS_addvarnode", c_int, c_void_p, c_int32, offsets)
    node = (c_void_p, c_int32, c_int64, offsets, doubles, offsets)
    _declare(lib, "BIYOS_addnode", c_int, *node)
    postfix = (c_void_p, c_int32, offsets, ints, offsets, doubles, offsets)
    _declare(lib, "BIYOS_addexpr", c_int, *postfix)
    _declare_pair(lib, "BIYOS_attachexpr", offsets)
    _declare_pair(lib, "BIYOS_clrfun")
    _declare_pair(lib, "BIYOS_getnexpr", offsets, offsets)
    read = (c_int64, c_int64, offsets, ints, offsets, doubles)
    _declare_pair(lib, "BIYOS_getexpr", *read)


def _declare_settings(lib: ctypes.CDLL, ints: type, doubles: type) -> None:
    _declare(lib, "BIYOS_setmdlboolopt", c_int, c_void_p, c_int32, c_int32)
    _declare(lib, "BIYOS_setmdlintopt", c_int, c_void_p, c_int32, c_int32)
    _declare(lib, "BIYOS_setmdldblopt", c_int, c_void_p, c_int32, c_double)
    _declare(lib, "BIYOS_setmdlstropt", c_int, c_void_p, c_int32, c_char_p)
    masks = (c_uint64, c_uint64)
    _declare(lib, "BIYOS_setmdlflgopt", c_int, c_void_p, c_int32, *masks)
    _declare(lib, "BIYOS_getmdlboolopt", c_int, c_void_p, c_int32, ints)
    _declare(lib, "BIYOS_getmdlintopt", c_int, c_void_p, c_int32, ints)
    _declare(lib, "BIYOS_getmdldblopt", c_int, c_void_p, c_int32, doubles)
    strings = POINTER(c_char_p)
    _declare(lib, "BIYOS_getmdlstropt", c_int, c_void_p, c_int32, strings)
    pointers = (POINTER(c_uint64), POINTER(c_uint64))
    _declare(lib, "BIYOS_getmdlflgopt", c_int, c_void_p, c_int32, *pointers)
    _declare(lib, "BIYOS_delmdlopt", c_int, c_void_p, c_int32)
    _declare(lib, "BIYOS_getmdloptsrc", c_int, c_void_p, c_int32, ints)


def _declare_callbacks(lib: ctypes.CDLL, ints: type) -> None:
    added = (c_void_p, c_int32, ints, c_void_p, ints)
    _declare(lib, "BIYOS_addcb", c_int, *added)
    _declare(lib, "BIYOS_setcbf", c_int, c_void_p, c_int32, CALLBACK)
    pairs = (c_void_p, c_int32, c_int64, ints, ints, CALLBACK)
    _declare(lib, "BIYOS_setcbg", c_int, *pairs)
    _declare(lib, "BIYOS_setcbh", c_int, *pairs)
    _declare(lib, "BIYOS_setcbhv", c_int, c_void_p, c_int32, CALLBACK)
    _declare(lib, "BIYOS_setcbdata", c_int, c_void_p, c_int32, c_void_p)
    data = (c_void_p, c_int32, POINTER(c_void_p))
    _declare(lib, "BIYOS_getcbdata", c_int, *data)
    _declare(lib, "BIYOS_getncbfun", c_int, c_void_p, c_int32, ints)
    covered = (c_void_p, c_int32, c_int32, ints)
    _declare(lib, "BIYOS_getcbfun", c_int, *covered)


def _declare_evaluation(lib: ctypes.CDLL, ints: type) -> None:
    offsets = POINTER(c_int64)
    for name in ("gpat", "hpat"):
        _declare_pair(lib, f"BIYOS_getn{name}", offsets)
        _declare_pair(lib, f"BIYOS_get{name}", c_int64, offsets, ints)


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
