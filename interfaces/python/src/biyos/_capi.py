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


def _declare(lib, name, result, *arguments):
    function = getattr(lib, name)
    function.restype = result
    function.argtypes = arguments


def _load():
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
    _declare(lib, "BIYOS_getoptid", c_int, c_void_p, c_char_p, ints)
    _declare(lib, "BIYOS_getoptknd", c_int, c_void_p, c_int32, ints)
    versions = (c_void_p, c_int32, ints, ints, ints)
    _declare(lib, "BIYOS_getoptver", c_int, *versions)
    _declare(lib, "BIYOS_setboolopt", c_int, c_void_p, c_int32, c_int32)
    _declare(lib, "BIYOS_setintopt", c_int, c_void_p, c_int32, c_int32)
    _declare(lib, "BIYOS_setdblopt", c_int, c_void_p, c_int32, c_double)
    _declare(lib, "BIYOS_setstropt", c_int, c_void_p, c_int32, c_char_p)
    _declare(lib, "BIYOS_getboolopt", c_int, c_void_p, c_int32, ints)
    _declare(lib, "BIYOS_getintopt", c_int, c_void_p, c_int32, ints)
    _declare(lib, "BIYOS_getdblopt", c_int, c_void_p, c_int32, doubles)
    _declare(lib, "BIYOS_getstropt", c_int, c_void_p, c_int32, strings)
    _declare_events(lib, ints)
    _declare_model(lib, ints, doubles)
    return lib


def _declare_events(lib, ints):
    arguments = (c_void_p, HOOK, c_void_p, c_int32, c_int32, ints)
    _declare(lib, "BIYOS_addhook", c_int, *arguments)
    _declare(lib, "BIYOS_delhook", c_int, c_void_p, c_int32)
    _declare(lib, "BIYOS_getevtknd", c_int32, c_void_p)
    _declare(lib, "BIYOS_getevtlvl", c_int32, c_void_p)
    _declare(lib, "BIYOS_getevtmod", c_char_p, c_void_p)
    _declare(lib, "BIYOS_getevttext", c_char_p, c_void_p)
    _declare(lib, "BIYOS_getnevtflds", c_int32, c_void_p)
    _declare(lib, "BIYOS_getevtfldname", c_char_p, c_void_p, c_int32)
    _declare(lib, "BIYOS_getevtfldval", c_char_p, c_void_p, c_int32)
    _declare_formats(lib)


def _declare_formats(lib):
    arguments = (c_void_p, c_size_t, POINTER(c_char), POINTER(c_size_t))
    _declare(lib, "BIYOS_fmttext", c_int, *arguments)
    _declare(lib, "BIYOS_fmtjson", c_int, *arguments)


def _declare_model(lib, ints, doubles):
    _declare(lib, "BIYOS_newmdl", c_int, c_void_p, POINTER(c_void_p))
    copy = (c_void_p, c_void_p, POINTER(c_void_p))
    _declare(lib, "BIYOS_copymdl", c_int, *copy)
    _declare(lib, "BIYOS_freemdl", c_int, c_void_p)
    variables = (c_void_p, c_int32, ints, doubles, doubles)
    _declare(lib, "BIYOS_addvars", c_int, *variables, ints)
    _declare(lib, "BIYOS_setvarbnd", c_int, *variables)
    kinds = (c_void_p, c_int32, ints, ints)
    _declare(lib, "BIYOS_setvarknd", c_int, *kinds)
    _declare(lib, "BIYOS_getnvars", c_int, c_void_p, ints)
    read = (c_void_p, c_int32, c_int32, ints, doubles, doubles)
    _declare(lib, "BIYOS_getvars", c_int, *read)
    _declare_functions(lib, ints, doubles)
    _declare_quadratic(lib, ints, doubles)
    _declare_constraints(lib, ints, doubles)
    _declare_objectives(lib, ints, doubles)
    _declare_names(lib, ints)


def _declare_functions(lib, ints, doubles):
    offsets = POINTER(c_int64)
    add = (c_void_p, c_int32, doubles, offsets, ints, doubles, ints)
    _declare(lib, "BIYOS_addfuns", c_int, *add)
    _declare(lib, "BIYOS_getnfuns", c_int, c_void_p, ints)
    sized = (offsets, ints, doubles)
    read = (c_void_p, c_int32, c_int32, c_int64, doubles, offsets, *sized)
    _declare(lib, "BIYOS_getfuns", c_int, *read)


def _declare_quadratic(lib, ints, doubles):
    offsets = POINTER(c_int64)
    add = (c_void_p, c_int32, ints, offsets, ints, ints, doubles)
    _declare(lib, "BIYOS_addquadterms", c_int, *add)
    sized = (offsets, ints, ints, doubles)
    read = (c_void_p, c_int32, c_int32, c_int64, offsets, *sized)
    _declare(lib, "BIYOS_getquadterms", c_int, *read)


def _declare_constraints(lib, ints, doubles):
    add = (c_void_p, c_int32, ints, doubles, doubles, ints)
    _declare(lib, "BIYOS_addcons", c_int, *add)
    bounds = (c_void_p, c_int32, ints, doubles, doubles)
    _declare(lib, "BIYOS_setconbnd", c_int, *bounds)
    _declare(lib, "BIYOS_getncons", c_int, c_void_p, ints)
    read = (c_void_p, c_int32, c_int32, ints, doubles, doubles)
    _declare(lib, "BIYOS_getcons", c_int, *read)


def _declare_objectives(lib, ints, doubles):
    fields = (ints, ints, doubles, doubles, doubles)
    add = (c_void_p, c_int32, ints, *fields, ints)
    _declare(lib, "BIYOS_addobjs", c_int, *add)
    change = (c_void_p, c_int32, ints, *fields)
    _declare(lib, "BIYOS_setobjs", c_int, *change)
    _declare(lib, "BIYOS_getnobjs", c_int, c_void_p, ints)
    read = (c_void_p, c_int32, c_int32, ints, *fields)
    _declare(lib, "BIYOS_getobjs", c_int, *read)


def _declare_names(lib, ints):
    strings = POINTER(c_char_p)
    _declare(lib, "BIYOS_setmdlname", c_int, c_void_p, c_char_p)
    _declare(lib, "BIYOS_getmdlname", c_int, c_void_p, strings)
    _declare(lib, "BIYOS_delmdlname", c_int, c_void_p)
    names = (c_void_p, c_int32, ints, strings)
    find = (c_void_p, c_int32, strings, ints)
    remove = (c_void_p, c_int32, ints)
    for noun in ("var", "fun", "con", "obj"):
        _declare(lib, f"BIYOS_set{noun}name", c_int, *names)
        _declare(lib, f"BIYOS_get{noun}name", c_int, *names)
        _declare(lib, f"BIYOS_get{noun}idx", c_int, *find)
        _declare(lib, f"BIYOS_del{noun}name", c_int, *remove)


lib = _load()


def version():
    """Return the version of the library.

    Returns
    -------
    str
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
