import ctypes
import math
from ctypes import (
    CFUNCTYPE,
    POINTER,
    byref,
    c_char,
    c_char_p,
    c_size_t,
    c_void_p,
)
from importlib.resources import files

from biyos._convert.numbers import (
    c_index,
    c_integer,
    c_mask,
    c_offset,
    c_real,
    c_status,
)

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

_INDICES = POINTER(c_index)
_INTEGERS = POINTER(c_integer)
_OFFSETS = POINTER(c_offset)
_REALS = POINTER(c_real)
_STRINGS = POINTER(c_char_p)


class ErrorRecord(ctypes.Structure):
    _fields_ = [
        ("size", c_size_t),
        ("status", c_status),
        ("mdl", c_void_p),
        ("msg", c_char_p),
    ]


class RequestRecord(ctypes.Structure):
    _fields_ = [
        ("size", c_size_t),
        ("mdl", c_void_p),
        ("cb", c_index),
        ("thread", c_index),
        ("x", c_void_p),
        ("w", c_void_p),
        ("v", c_void_p),
    ]


CALLBACK = CFUNCTYPE(c_status, POINTER(RequestRecord), c_void_p, c_void_p)


def _reader(
    lib: ctypes.CDLL, name: str, result: type | None, *arguments: type
) -> None:
    function = getattr(lib, name)
    function.restype = result
    function.argtypes = arguments


def _declare(lib: ctypes.CDLL, name: str, *arguments: type) -> None:
    _reader(lib, name, c_status, *arguments)


def _declare_pair(lib: ctypes.CDLL, name: str, *values: type) -> None:
    listed = (c_void_p, c_index, _INDICES)
    _declare(lib, f"{name}sp", *listed, *values)
    _declare(lib, f"{name}rg", c_void_p, c_index, c_index, *values)


def _declare_tolerances(lib: ctypes.CDLL, family: str) -> None:
    _declare_pair(lib, f"BIYOS_set{family}tol", _REALS, _REALS)
    _declare_pair(lib, f"BIYOS_get{family}tol", _REALS, _REALS)
    _declare_pair(lib, f"BIYOS_del{family}tol")


def _load() -> ctypes.CDLL:
    package = files("biyos")
    name = (package / "library.txt").read_text()
    lib = ctypes.CDLL(str(package / name))
    _reader(lib, "BIYOS_version", None, _INTEGERS, _INTEGERS, _INTEGERS)
    _declare(lib, "BIYOS_newenv", POINTER(c_void_p))
    _declare(lib, "BIYOS_freeenv", c_void_p)
    record = POINTER(ErrorRecord)
    _declare(lib, "BIYOS_lasterror", c_void_p, record)
    _declare(lib, "BIYOS_interrupt", c_void_p)
    _declare(lib, "BIYOS_getoptid", c_void_p, c_char_p, _INTEGERS)
    _declare(lib, "BIYOS_getoptknd", c_void_p, c_integer, _INTEGERS)
    versions = (c_void_p, c_integer, _INTEGERS, _INTEGERS, _INTEGERS)
    _declare(lib, "BIYOS_getoptver", *versions)
    _declare(lib, "BIYOS_setenvboolopt", c_void_p, c_integer, c_integer)
    _declare(lib, "BIYOS_setenvintopt", c_void_p, c_integer, c_integer)
    _declare(lib, "BIYOS_setenvdblopt", c_void_p, c_integer, c_real)
    _declare(lib, "BIYOS_setenvstropt", c_void_p, c_integer, c_char_p)
    masks = (c_mask, c_mask)
    _declare(lib, "BIYOS_setenvflgopt", c_void_p, c_integer, *masks)
    _declare(lib, "BIYOS_getenvboolopt", c_void_p, c_integer, _INTEGERS)
    _declare(lib, "BIYOS_getenvintopt", c_void_p, c_integer, _INTEGERS)
    _declare(lib, "BIYOS_getenvdblopt", c_void_p, c_integer, _REALS)
    _declare(lib, "BIYOS_getenvstropt", c_void_p, c_integer, _STRINGS)
    pointers = (POINTER(c_mask), POINTER(c_mask))
    _declare(lib, "BIYOS_getenvflgopt", c_void_p, c_integer, *pointers)
    _declare_events(lib)
    _declare_model(lib)
    return lib


def _declare_events(lib: ctypes.CDLL) -> None:
    arguments = (c_void_p, HOOK, c_void_p, c_integer, c_integer, _INDICES)
    _declare(lib, "BIYOS_addenvhook", *arguments)
    _declare(lib, "BIYOS_delenvhook", c_void_p, c_index)
    _declare(lib, "BIYOS_addmdlhook", *arguments)
    _declare(lib, "BIYOS_delmdlhook", c_void_p, c_index)
    _reader(lib, "BIYOS_getevtknd", c_integer, c_void_p)
    _reader(lib, "BIYOS_getevtlvl", c_integer, c_void_p)
    _reader(lib, "BIYOS_getevtmod", c_char_p, c_void_p)
    _reader(lib, "BIYOS_getevttext", c_char_p, c_void_p)
    _reader(lib, "BIYOS_getnevtfld", c_index, c_void_p)
    _reader(lib, "BIYOS_getevtfldname", c_char_p, c_void_p, c_index)
    _reader(lib, "BIYOS_getevtfldval", c_char_p, c_void_p, c_index)
    _reader(lib, "BIYOS_getevtmdl", c_void_p, c_void_p)
    _declare_formats(lib)


def _declare_formats(lib: ctypes.CDLL) -> None:
    arguments = (c_void_p, c_size_t, POINTER(c_char), POINTER(c_size_t))
    _declare(lib, "BIYOS_fmttext", *arguments)
    _declare(lib, "BIYOS_fmtjson", *arguments)


def _declare_model(lib: ctypes.CDLL) -> None:
    _declare(lib, "BIYOS_newmdl", c_void_p, POINTER(c_void_p))
    copy = (c_void_p, c_void_p, POINTER(c_void_p))
    _declare(lib, "BIYOS_copymdl", *copy)
    _declare(lib, "BIYOS_freemdl", c_void_p)
    read = (c_void_p, c_char_p, c_integer, POINTER(c_void_p))
    _declare(lib, "BIYOS_readmdl", *read)
    _declare(lib, "BIYOS_writemdl", c_void_p, c_char_p, c_integer)
    _declare_variables(lib)
    _declare_functions(lib)
    _declare_quadratic(lib)
    _declare_constraints(lib)
    _declare_objectives(lib)
    _declare_indicators(lib)
    _declare_sos(lib)
    _declare_pwl(lib)
    _declare_cones(lib)
    _declare_complements(lib)
    _declare_names(lib)
    _declare_expressions(lib)
    _declare_settings(lib)
    _declare_callbacks(lib)
    _declare_evaluation(lib)


def _declare_variables(lib: ctypes.CDLL) -> None:
    variables = (c_void_p, c_index, _INTEGERS, _REALS, _REALS)
    _declare(lib, "BIYOS_addvar", *variables, _INDICES)
    _declare_pair(lib, "BIYOS_setvarbnd", _REALS, _REALS)
    _declare_pair(lib, "BIYOS_getvarbnd", _REALS, _REALS)
    _declare_pair(lib, "BIYOS_setvarknd", _INTEGERS)
    _declare_pair(lib, "BIYOS_getvarknd", _INTEGERS)
    _declare_tolerances(lib, "var")
    _declare(lib, "BIYOS_getnvar", c_void_p, _INDICES)
    _declare_pair(lib, "BIYOS_getvar", _INTEGERS, _REALS, _REALS)
    _declare_pair(lib, "BIYOS_delvar", _INDICES)


def _declare_functions(lib: ctypes.CDLL) -> None:
    _declare(lib, "BIYOS_addfun", c_void_p, c_index, _INDICES)
    _declare(lib, "BIYOS_getnfun", c_void_p, _INDICES)
    for verb in ("add", "set", "get"):
        _declare_pair(lib, f"BIYOS_{verb}cst", _REALS)
    _declare_pair(lib, "BIYOS_delfun", _INDICES)
    _declare_pair(lib, "BIYOS_clrfun")
    coordinates = (c_void_p, c_offset, _INDICES, _INDICES, _REALS)
    _declare(lib, "BIYOS_addlinterm", *coordinates)
    ranged = (c_void_p, c_index, c_index, _OFFSETS, _INDICES, _REALS)
    _declare(lib, "BIYOS_addrlintermrg", *ranged)
    _declare(lib, "BIYOS_addclintermrg", *ranged)
    for name in ("rlinterm", "clinterm"):
        _declare_terms(lib, name)


def _declare_terms(lib: ctypes.CDLL, name: str, others: int = 1) -> None:
    _declare_pair(lib, f"BIYOS_getn{name}", _OFFSETS)
    values = (*[_INDICES] * others, _REALS)
    listed = (c_void_p, c_index, _INDICES, c_offset, _OFFSETS, _INDICES)
    _declare(lib, f"BIYOS_get{name}sp", *listed, *values)
    ranged = (c_void_p, c_index, c_index, c_offset, _OFFSETS)
    _declare(lib, f"BIYOS_get{name}rg", *ranged, *values)


def _declare_quadratic(lib: ctypes.CDLL) -> None:
    coordinates = (c_void_p, c_offset, _INDICES, _INDICES, _INDICES, _REALS)
    _declare(lib, "BIYOS_addquadterm", *coordinates)
    ranged = (c_void_p, c_index, c_index, _OFFSETS, _INDICES, _INDICES)
    _declare(lib, "BIYOS_addquadtermrg", *ranged, _REALS)
    _declare_terms(lib, "quadterm", 2)


def _declare_constraints(lib: ctypes.CDLL) -> None:
    add = (c_void_p, c_index, _INDICES, _REALS, _REALS, _INDICES)
    _declare(lib, "BIYOS_addcon", *add)
    _declare_pair(lib, "BIYOS_setconbnd", _REALS, _REALS)
    _declare_pair(lib, "BIYOS_getconbnd", _REALS, _REALS)
    _declare_tolerances(lib, "con")
    _declare(lib, "BIYOS_getncon", c_void_p, _INDICES)
    _declare_pair(lib, "BIYOS_getcon", _INDICES, _REALS, _REALS)
    _declare_pair(lib, "BIYOS_delcon", _INDICES)


def _declare_objectives(lib: ctypes.CDLL) -> None:
    fields = (_INTEGERS, _INTEGERS, _REALS)
    add = (c_void_p, c_index, _INDICES, *fields, _INDICES)
    _declare(lib, "BIYOS_addobj", *add)
    named = (("sense", _INTEGERS), ("prio", _INTEGERS), ("coef", _REALS))
    for name, values in named:
        _declare_pair(lib, f"BIYOS_setobj{name}", values)
        _declare_pair(lib, f"BIYOS_getobj{name}", values)
    _declare_pair(lib, "BIYOS_setobjtol", _REALS, _REALS)
    _declare_pair(lib, "BIYOS_getobjtol", _REALS, _REALS)
    _declare(lib, "BIYOS_getnobj", c_void_p, _INDICES)
    _declare_pair(lib, "BIYOS_getobj", _INDICES, *fields)
    _declare_pair(lib, "BIYOS_delobj", _INDICES)


def _declare_indicators(lib: ctypes.CDLL) -> None:
    fields = (_INDICES, _INTEGERS, _INDICES, _REALS, _REALS)
    _declare(lib, "BIYOS_addind", c_void_p, c_index, *fields, _INDICES)
    _declare_pair(lib, "BIYOS_setindbnd", _REALS, _REALS)
    _declare_pair(lib, "BIYOS_getindbnd", _REALS, _REALS)
    _declare_pair(lib, "BIYOS_setindvar", _INDICES, _INTEGERS)
    _declare_pair(lib, "BIYOS_getindvar", _INDICES, _INTEGERS)
    _declare(lib, "BIYOS_getnind", c_void_p, _INDICES)
    _declare_pair(lib, "BIYOS_getind", *fields)
    _declare_pair(lib, "BIYOS_delind", _INDICES)


def _declare_sos(lib: ctypes.CDLL) -> None:
    sets = (_INTEGERS, _OFFSETS, _INDICES, _REALS)
    _declare(lib, "BIYOS_addsos", c_void_p, c_index, *sets, _INDICES)
    _declare(lib, "BIYOS_getnsos", c_void_p, _INDICES)
    _declare_pair(lib, "BIYOS_getnsosvar", _OFFSETS)
    _declare_pair(lib, "BIYOS_getsos", c_offset, *sets)
    _declare_pair(lib, "BIYOS_delsos", _INDICES)


def _declare_pwl(lib: ctypes.CDLL) -> None:
    add = (c_void_p, c_index, _OFFSETS, _REALS, _REALS, _INDICES)
    _declare(lib, "BIYOS_addpwl", *add)
    _declare(lib, "BIYOS_getnpwl", c_void_p, _INDICES)
    _declare_pair(lib, "BIYOS_getnpwlpt", _OFFSETS)
    _declare_pair(lib, "BIYOS_getpwl", c_offset, _OFFSETS, _REALS, _REALS)
    node = (c_void_p, c_index, c_offset, _OFFSETS)
    _declare(lib, "BIYOS_addpwlnode", *node)


def _declare_cones(lib: ctypes.CDLL) -> None:
    cones = (_INTEGERS, _OFFSETS, _INDICES)
    add = (c_void_p, c_index, *cones, _OFFSETS, _REALS, _INDICES)
    _declare(lib, "BIYOS_addcone", *add)
    _declare_tolerances(lib, "cone")
    _declare(lib, "BIYOS_getncone", c_void_p, _INDICES)
    _declare_pair(lib, "BIYOS_getnconefun", _OFFSETS)
    _declare_pair(lib, "BIYOS_getnconeprm", _OFFSETS)
    _declare_pair(lib, "BIYOS_getcone", c_offset, *cones)
    _declare_pair(lib, "BIYOS_getconeprm", c_offset, _OFFSETS, _REALS)
    _declare_pair(lib, "BIYOS_delcone", _INDICES)


def _declare_complements(lib: ctypes.CDLL) -> None:
    fields = (_INDICES, _INDICES, _REALS, _REALS)
    _declare(lib, "BIYOS_addccon", c_void_p, c_index, *fields, _INDICES)
    for name in ("bnd", "tol"):
        _declare_pair(lib, f"BIYOS_setccon{name}", _REALS, _REALS)
        _declare_pair(lib, f"BIYOS_getccon{name}", _REALS, _REALS)
    _declare_pair(lib, "BIYOS_delccontol")
    _declare(lib, "BIYOS_getnccon", c_void_p, _INDICES)
    _declare_pair(lib, "BIYOS_getccon", *fields)
    _declare_pair(lib, "BIYOS_delccon", _INDICES)


def _declare_names(lib: ctypes.CDLL) -> None:
    _declare(lib, "BIYOS_setmdlname", c_void_p, c_char_p)
    _declare(lib, "BIYOS_getmdlname", c_void_p, _STRINGS)
    _declare(lib, "BIYOS_delmdlname", c_void_p)
    find = (c_void_p, c_index, _STRINGS, _INDICES)
    for noun in ("var", "fun", "con", "obj", "ind", "sos", "cone", "ccon"):
        _declare_pair(lib, f"BIYOS_set{noun}name", _STRINGS)
        _declare_pair(lib, f"BIYOS_get{noun}name", _STRINGS)
        _declare(lib, f"BIYOS_get{noun}idx", *find)
        _declare_pair(lib, f"BIYOS_del{noun}name")


def _declare_expressions(lib: ctypes.CDLL) -> None:
    _declare(lib, "BIYOS_addvarnode", c_void_p, c_index, _OFFSETS)
    node = (c_void_p, c_integer, c_offset, _OFFSETS, _REALS, _OFFSETS)
    _declare(lib, "BIYOS_addnode", *node)
    nodes = (_OFFSETS, _INTEGERS, _OFFSETS, _REALS)
    _declare(lib, "BIYOS_addexpr", c_void_p, c_index, *nodes, _OFFSETS)
    _declare_pair(lib, "BIYOS_attachexpr", _OFFSETS)
    _declare_pair(lib, "BIYOS_getnexpr", _OFFSETS, _OFFSETS)
    _declare_pair(lib, "BIYOS_getexpr", c_offset, c_offset, *nodes)


def _declare_settings(lib: ctypes.CDLL) -> None:
    _declare(lib, "BIYOS_setmdlboolopt", c_void_p, c_integer, c_integer)
    _declare(lib, "BIYOS_setmdlintopt", c_void_p, c_integer, c_integer)
    _declare(lib, "BIYOS_setmdldblopt", c_void_p, c_integer, c_real)
    _declare(lib, "BIYOS_setmdlstropt", c_void_p, c_integer, c_char_p)
    masks = (c_mask, c_mask)
    _declare(lib, "BIYOS_setmdlflgopt", c_void_p, c_integer, *masks)
    _declare(lib, "BIYOS_getmdlboolopt", c_void_p, c_integer, _INTEGERS)
    _declare(lib, "BIYOS_getmdlintopt", c_void_p, c_integer, _INTEGERS)
    _declare(lib, "BIYOS_getmdldblopt", c_void_p, c_integer, _REALS)
    _declare(lib, "BIYOS_getmdlstropt", c_void_p, c_integer, _STRINGS)
    pointers = (POINTER(c_mask), POINTER(c_mask))
    _declare(lib, "BIYOS_getmdlflgopt", c_void_p, c_integer, *pointers)
    _declare(lib, "BIYOS_delenvopt", c_void_p, c_integer)
    _declare(lib, "BIYOS_delmdlopt", c_void_p, c_integer)
    _declare(lib, "BIYOS_getmdloptsrc", c_void_p, c_integer, _INTEGERS)


def _declare_callbacks(lib: ctypes.CDLL) -> None:
    added = (c_void_p, c_index, _INDICES, c_void_p, _INDICES)
    _declare(lib, "BIYOS_addcb", *added)
    _declare(lib, "BIYOS_setcbf", c_void_p, c_index, CALLBACK)
    pairs = (c_void_p, c_index, c_offset, _INDICES, _INDICES, CALLBACK)
    _declare(lib, "BIYOS_setcbg", *pairs)
    _declare(lib, "BIYOS_setcbh", *pairs)
    _declare(lib, "BIYOS_setcbhv", c_void_p, c_index, CALLBACK)
    _declare(lib, "BIYOS_setcbdata", c_void_p, c_index, c_void_p)
    data = (c_void_p, c_index, POINTER(c_void_p))
    _declare(lib, "BIYOS_getcbdata", *data)
    _declare(lib, "BIYOS_getncbfun", c_void_p, c_index, _INDICES)
    covered = (c_void_p, c_index, c_index, _INDICES)
    _declare(lib, "BIYOS_getcbfun", *covered)


def _declare_evaluation(lib: ctypes.CDLL) -> None:
    for name, arrays in (("f", 2), ("g", 2), ("h", 3), ("hv", 4)):
        _declare(lib, f"BIYOS_eval{name}", c_void_p, *[_REALS] * arrays)
    for name in ("gpat", "hpat"):
        _declare_pair(lib, f"BIYOS_getn{name}", _OFFSETS)
        _declare_pair(lib, f"BIYOS_get{name}", c_offset, _OFFSETS, _INDICES)


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
    major = c_integer()
    minor = c_integer()
    patch = c_integer()
    lib.BIYOS_version(byref(major), byref(minor), byref(patch))
    return f"{major.value}.{minor.value}.{patch.value}"
