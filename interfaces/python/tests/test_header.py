import biyos
from biyos import _capi
from biyos._capi import lib
from header import functions, group, lines, macros

KINDS = {
    "BIYOS_KND_BOOL": "KIND_BOOL",
    "BIYOS_KND_INT": "KIND_INT",
    "BIYOS_KND_DBL": "KIND_DOUBLE",
    "BIYOS_KND_STR": "KIND_STRING",
    "BIYOS_KND_ENUM": "KIND_ENUM",
}

VARIABLES = {
    "BIYOS_VAR_CONT": biyos.VariableKind.CONTINUOUS,
    "BIYOS_VAR_BIN": biyos.VariableKind.BINARY,
    "BIYOS_VAR_INT": biyos.VariableKind.INTEGER,
    "BIYOS_VAR_SEMICONT": biyos.VariableKind.SEMICONTINUOUS,
    "BIYOS_VAR_SEMIINT": biyos.VariableKind.SEMIINTEGER,
}


def declared(name):
    return getattr(lib, name).argtypes is not None


def test_functions():
    names = functions("biyos.h")
    assert len(names) == lines("biyos.h", "BIYOS_API ")
    assert [name for name in names if not declared(name)] == []


def test_kinds():
    found = macros("biyos.h")
    kinds = {name: value for name, value in found.items() if "_KND_" in name}
    assert len(kinds) == lines("biyos.h", "#define BIYOS_KND_")
    names = [name for name in dir(_capi) if name.startswith("KIND_")]
    assert sorted(names) == sorted(KINDS.values())
    paired = {macro: getattr(_capi, name) for macro, name in KINDS.items()}
    assert kinds == paired


def test_variable_kinds():
    found = macros("biyos.h")
    kinds = {name: value for name, value in found.items() if "_VAR_" in name}
    assert len(kinds) == lines("biyos.h", "#define BIYOS_VAR_")
    assert set(VARIABLES.values()) == set(biyos.VariableKind)
    assert kinds == VARIABLES


def test_senses():
    senses, defined = group("biyos.h", "BIYOS_MINIMIZE")
    assert len(senses) == defined
    names = {f"BIYOS_{sense.name}": sense for sense in biyos.Sense}
    assert senses == names


def test_levels():
    levels, defined = group("biyos.h", "BIYOS_CHECKLVL_NONE")
    assert len(levels) == defined
    names = {
        f"BIYOS_CHECKLVL_{level.name}": level for level in biyos.CheckLevel
    }
    assert levels == names


def test_ok():
    assert macros("biyos.h")["BIYOS_OK"] == _capi.OK
