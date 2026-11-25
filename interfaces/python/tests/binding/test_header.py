from ctypes import POINTER, c_uint64, sizeof

import biyos
from biyos import _capi, _header
from biyos._capi import lib
from biyos._convert import numbers
from header import group

VARIABLES = {
    "BIYOS_VARKND_CONT": biyos.VariableKind.CONTINUOUS,
    "BIYOS_VARKND_BIN": biyos.VariableKind.BINARY,
    "BIYOS_VARKND_INT": biyos.VariableKind.INTEGER,
    "BIYOS_VARKND_SEMICONT": biyos.VariableKind.SEMICONTINUOUS,
    "BIYOS_VARKND_SEMIINT": biyos.VariableKind.SEMIINTEGER,
}

SOURCES = {
    "BIYOS_OPTSRC_MDL": biyos.OptionSource.MODEL,
    "BIYOS_OPTSRC_ENV": biyos.OptionSource.ENV,
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
    "BIYOS_OP_PWL": biyos.Operator.PIECEWISE_LINEAR,
    "BIYOS_OP_KEEP": biyos.Operator.KEEP,
    "BIYOS_OP_REF": biyos.Operator.REFERENCE,
}

SENSES = {
    "BIYOS_OBJSENSE_MIN": biyos.ObjectiveSense.MINIMIZE,
    "BIYOS_OBJSENSE_MAX": biyos.ObjectiveSense.MAXIMIZE,
}

CONES = {
    "BIYOS_CONEKND_NONNEG": biyos.ConeKind.NONNEGATIVE,
    "BIYOS_CONEKND_SOC": biyos.ConeKind.SECOND_ORDER,
    "BIYOS_CONEKND_RSOC": biyos.ConeKind.ROTATED,
    "BIYOS_CONEKND_EXP": biyos.ConeKind.EXPONENTIAL,
    "BIYOS_CONEKND_DEXP": biyos.ConeKind.DUAL_EXPONENTIAL,
    "BIYOS_CONEKND_POW": biyos.ConeKind.POWER,
    "BIYOS_CONEKND_DPOW": biyos.ConeKind.DUAL_POWER,
    "BIYOS_CONEKND_PSD": biyos.ConeKind.SEMIDEFINITE,
}

SIZES = {
    "c_real": 8,
    "c_index": 4,
    "c_integer": 4,
    "c_offset": 8,
    "c_mask": 8,
}

ALIASES = {"Real": "c_real", "Index": "c_index", "Offset": "c_offset"}


def declared(name):
    return getattr(lib, name).argtypes is not None


def test_functions():
    assert [name for name in _header.FUNCTIONS if not declared(name)] == []


def test_kinds():
    names = [name for name in dir(_capi) if name.startswith("OPTKND_")]
    paired = {f"BIYOS_{name}": getattr(_capi, name) for name in names}
    assert group("BIYOS_OPTKND_") == paired


def test_variable_kinds():
    assert set(VARIABLES.values()) == set(biyos.VariableKind)
    assert group("BIYOS_VARKND_") == VARIABLES


def test_operators():
    assert set(OPERATORS.values()) == set(biyos.Operator)
    assert group("BIYOS_OP_") == OPERATORS


def test_cone_kinds():
    assert set(CONES.values()) == set(biyos.ConeKind)
    assert group("BIYOS_CONEKND_") == CONES


def test_senses():
    assert set(SENSES.values()) == set(biyos.ObjectiveSense)
    assert group("BIYOS_OBJSENSE_") == SENSES


def test_formats():
    names = {f"BIYOS_FILEFMT_{fmt.name}": fmt for fmt in biyos.FileFormat}
    assert group("BIYOS_FILEFMT_") == names


def test_sos_kinds():
    kinds = biyos.SpecialOrderedSetKind
    names = {f"BIYOS_SOSKND_{kind.name}": kind for kind in kinds}
    assert group("BIYOS_SOSKND_") == names


def test_checking():
    names = {f"BIYOS_CHK_{value.name}": value for value in biyos.Checking}
    assert group("BIYOS_CHK_") == names


def test_sources():
    assert set(SOURCES.values()) == set(biyos.OptionSource)
    assert group("BIYOS_OPTSRC_") == SOURCES


def test_ok():
    assert _header.BIYOS_OK == _capi.OK


def test_infinity():
    assert _header.BIYOS_INF == biyos.inf


def test_masks():
    masks = [c_uint64, c_uint64]
    assert list(lib.BIYOS_setenvflgopt.argtypes[2:]) == masks
    assert list(lib.BIYOS_setmdlflgopt.argtypes[2:]) == masks
    pointers = [POINTER(c_uint64), POINTER(c_uint64)]
    assert list(lib.BIYOS_getenvflgopt.argtypes[2:]) == pointers
    assert list(lib.BIYOS_getmdlflgopt.argtypes[2:]) == pointers


def test_sizes():
    found = {name: sizeof(getattr(numbers, name)) for name in SIZES}
    assert found == SIZES


def test_signs():
    assert numbers.c_index(-1).value == -1
    assert numbers.c_integer(-1).value == -1
    assert numbers.c_offset(-1).value == -1
    assert numbers.c_mask(-1).value == 2**64 - 1


def held(name):
    return type(getattr(numbers, name)(0).value)


def test_aliases():
    for alias, name in ALIASES.items():
        assert getattr(numbers, alias) is held(name)
