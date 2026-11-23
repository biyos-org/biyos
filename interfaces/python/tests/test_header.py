from ctypes import POINTER, c_uint64

import biyos
from biyos import _capi
from biyos._capi import lib
from header import functions, group, lines, macros

KINDS = {
    "BIYOS_OPTKND_BOOL": "KIND_BOOL",
    "BIYOS_OPTKND_INT": "KIND_INT",
    "BIYOS_OPTKND_DBL": "KIND_DOUBLE",
    "BIYOS_OPTKND_STR": "KIND_STRING",
    "BIYOS_OPTKND_ENUM": "KIND_ENUM",
    "BIYOS_OPTKND_FLG": "KIND_FLAGS",
}

VARIABLES = {
    "BIYOS_VARKND_CONT": biyos.VariableKind.CONTINUOUS,
    "BIYOS_VARKND_BIN": biyos.VariableKind.BINARY,
    "BIYOS_VARKND_INT": biyos.VariableKind.INTEGER,
    "BIYOS_VARKND_SEMICONT": biyos.VariableKind.SEMICONTINUOUS,
    "BIYOS_VARKND_SEMIINT": biyos.VariableKind.SEMIINTEGER,
}

SOURCES = {
    "BIYOS_OPTSRC_MDL": biyos.OptionSource.MODEL,
    "BIYOS_OPTSRC_ENV": biyos.OptionSource.ENVIRONMENT,
    "BIYOS_OPTSRC_DEFAULT": biyos.OptionSource.DEFAULT,
}


OPERATORS = {
    "BIYOS_OP_CONST": biyos.Operator.CONSTANT,
    "BIYOS_OP_VAR": biyos.Operator.VARIABLE,
    "BIYOS_OP_SUM": biyos.Operator.SUM,
    "BIYOS_OP_PROD": biyos.Operator.PRODUCT,
    "BIYOS_OP_NEG": biyos.Operator.NEGATION,
    "BIYOS_OP_DIV": biyos.Operator.DIVISION,
    "BIYOS_OP_CPOW": biyos.Operator.CONSTANT_POWER,
    "BIYOS_OP_POW": biyos.Operator.POWER,
    "BIYOS_OP_EXP": biyos.Operator.EXP,
    "BIYOS_OP_LOG": biyos.Operator.LOG,
    "BIYOS_OP_SQRT": biyos.Operator.SQRT,
    "BIYOS_OP_SIN": biyos.Operator.SIN,
    "BIYOS_OP_COS": biyos.Operator.COS,
    "BIYOS_OP_TAN": biyos.Operator.TAN,
    "BIYOS_OP_ABS": biyos.Operator.ABS,
    "BIYOS_OP_MAX": biyos.Operator.MAX,
    "BIYOS_OP_MIN": biyos.Operator.MIN,
    "BIYOS_OP_ASIN": biyos.Operator.ASIN,
    "BIYOS_OP_ACOS": biyos.Operator.ACOS,
    "BIYOS_OP_ATAN": biyos.Operator.ATAN,
    "BIYOS_OP_SINH": biyos.Operator.SINH,
    "BIYOS_OP_COSH": biyos.Operator.COSH,
    "BIYOS_OP_TANH": biyos.Operator.TANH,
    "BIYOS_OP_ASINH": biyos.Operator.ASINH,
    "BIYOS_OP_ACOSH": biyos.Operator.ACOSH,
    "BIYOS_OP_ATANH": biyos.Operator.ATANH,
    "BIYOS_OP_ENT": biyos.Operator.ENTROPY,
    "BIYOS_OP_SPOW": biyos.Operator.SIGN_POWER,
    "BIYOS_OP_ERF": biyos.Operator.ERF,
    "BIYOS_OP_NODE": biyos.Operator.NODE,
    "BIYOS_OP_KEEP": biyos.Operator.KEEP,
    "BIYOS_OP_REF": biyos.Operator.REFERENCE,
}

SENSES = {
    "BIYOS_OBJSENSE_MIN": biyos.ObjectiveSense.MINIMIZE,
    "BIYOS_OBJSENSE_MAX": biyos.ObjectiveSense.MAXIMIZE,
}

CONES = {
    "BIYOS_CONE_NONNEG": biyos.ConeKind.NONNEGATIVE,
    "BIYOS_CONE_SOC": biyos.ConeKind.SECOND_ORDER,
    "BIYOS_CONE_RSOC": biyos.ConeKind.ROTATED,
    "BIYOS_CONE_EXP": biyos.ConeKind.EXPONENTIAL,
    "BIYOS_CONE_DEXP": biyos.ConeKind.DUAL_EXPONENTIAL,
    "BIYOS_CONE_POW": biyos.ConeKind.POWER,
    "BIYOS_CONE_DPOW": biyos.ConeKind.DUAL_POWER,
    "BIYOS_CONE_PSD": biyos.ConeKind.SEMIDEFINITE,
}


def declared(name):
    return getattr(lib, name).argtypes is not None


def test_functions():
    names = functions("biyos.h")
    assert len(names) == lines("biyos.h", "BIYOS_API ")
    assert [name for name in names if not declared(name)] == []


def test_kinds():
    found = macros("biyos.h")
    kinds = {name: value for name, value in found.items() if "_OPTKND_" in name}
    assert len(kinds) == lines("biyos.h", "#define BIYOS_OPTKND_")
    names = [name for name in dir(_capi) if name.startswith("KIND_")]
    assert sorted(names) == sorted(KINDS.values())
    paired = {macro: getattr(_capi, name) for macro, name in KINDS.items()}
    assert kinds == paired


def test_variable_kinds():
    found = macros("biyos.h")
    kinds = {name: value for name, value in found.items() if "_VARKND_" in name}
    assert len(kinds) == lines("biyos.h", "#define BIYOS_VARKND_")
    assert set(VARIABLES.values()) == set(biyos.VariableKind)
    assert kinds == VARIABLES


def test_operators():
    found = macros("biyos.h")
    codes = {name: value for name, value in found.items() if "_OP_" in name}
    assert len(codes) == lines("biyos.h", "#define BIYOS_OP_")
    assert set(OPERATORS.values()) == set(biyos.Operator)
    assert codes == OPERATORS


def test_cone_kinds():
    found = macros("biyos.h")
    kinds = {name: value for name, value in found.items() if "_CONE_" in name}
    assert len(kinds) == lines("biyos.h", "#define BIYOS_CONE_")
    assert set(CONES.values()) == set(biyos.ConeKind)
    assert kinds == CONES


def test_senses():
    senses, defined = group("biyos.h", "BIYOS_OBJSENSE_MIN")
    assert len(senses) == defined
    assert set(SENSES.values()) == set(biyos.ObjectiveSense)
    assert senses == SENSES


def test_sos_kinds():
    kinds, defined = group("biyos.h", "BIYOS_SOSKND_SOS1")
    assert len(kinds) == defined
    names = {f"BIYOS_SOSKND_{kind.name}": kind for kind in biyos.SOSKind}
    assert kinds == names


def test_levels():
    levels, defined = group("biyos.h", "BIYOS_CHECKLVL_NONE")
    assert len(levels) == defined
    names = {
        f"BIYOS_CHECKLVL_{level.name}": level for level in biyos.CheckLevel
    }
    assert levels == names


def test_sources():
    sources, defined = group("biyos.h", "BIYOS_OPTSRC_MDL")
    assert len(sources) == defined
    assert set(SOURCES.values()) == set(biyos.OptionSource)
    assert sources == SOURCES


def test_ok():
    assert macros("biyos.h")["BIYOS_OK"] == _capi.OK


def test_masks():
    masks = [c_uint64, c_uint64]
    assert list(lib.BIYOS_setenvflgopt.argtypes[2:]) == masks
    assert list(lib.BIYOS_setmdlflgopt.argtypes[2:]) == masks
    pointers = [POINTER(c_uint64), POINTER(c_uint64)]
    assert list(lib.BIYOS_getenvflgopt.argtypes[2:]) == pointers
    assert list(lib.BIYOS_getmdlflgopt.argtypes[2:]) == pointers
