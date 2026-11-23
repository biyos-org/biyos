from ctypes import POINTER, c_uint64

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
    "BIYOS_KND_FLG": "KIND_FLAGS",
}

VARIABLES = {
    "BIYOS_VAR_CONT": biyos.VariableKind.CONTINUOUS,
    "BIYOS_VAR_BIN": biyos.VariableKind.BINARY,
    "BIYOS_VAR_INT": biyos.VariableKind.INTEGER,
    "BIYOS_VAR_SEMICONT": biyos.VariableKind.SEMICONTINUOUS,
    "BIYOS_VAR_SEMIINT": biyos.VariableKind.SEMIINTEGER,
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


def test_operators():
    found = macros("biyos.h")
    codes = {name: value for name, value in found.items() if "_OP_" in name}
    assert len(codes) == lines("biyos.h", "#define BIYOS_OP_")
    assert set(OPERATORS.values()) == set(biyos.Operator)
    assert codes == OPERATORS


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


def test_masks():
    masks = [c_uint64, c_uint64]
    assert list(lib.BIYOS_setenvflgopt.argtypes[2:]) == masks
    assert list(lib.BIYOS_setmdlflgopt.argtypes[2:]) == masks
    pointers = [POINTER(c_uint64), POINTER(c_uint64)]
    assert list(lib.BIYOS_getenvflgopt.argtypes[2:]) == pointers
    assert list(lib.BIYOS_getmdlflgopt.argtypes[2:]) == pointers
