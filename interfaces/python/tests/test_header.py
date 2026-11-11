import biyos
from biyos import _capi
from biyos._capi import lib
from header import functions, lines, macros


def declared(name):
    return getattr(lib, name).argtypes is not None


def test_functions():
    names = functions("biyos.h")
    assert len(names) == lines("biyos.h", "BIYOS_API ")
    assert [name for name in names if not declared(name)] == []


def test_kinds():
    found = macros("options.h")
    kinds = {name: value for name, value in found.items() if "_KIND_" in name}
    assert len(kinds) == lines("options.h", "#define BIYOS_KIND_")
    names = [name for name in dir(_capi) if name.startswith("KIND_")]
    assert kinds == {f"BIYOS_{name}": getattr(_capi, name) for name in names}


def test_variable_kinds():
    found = macros("model.h")
    kinds = {name: value for name, value in found.items() if "_VAR_" in name}
    assert len(kinds) == lines("model.h", "#define BIYOS_VAR_")
    names = {f"BIYOS_VAR_{kind.name}": kind for kind in biyos.VariableKind}
    assert kinds == names


def test_senses():
    found = macros("model.h")
    senses = {name: found[name] for name in found if "_VAR_" not in name}
    kinds = lines("model.h", "#define BIYOS_VAR_")
    assert len(senses) == lines("model.h", "#define BIYOS_") - kinds
    names = {f"BIYOS_{sense.name}": sense for sense in biyos.Sense}
    assert senses == names


def test_ok():
    assert macros("biyos.h")["BIYOS_OK"] == _capi.OK
