from collections.abc import Callable
from ctypes import c_char_p, c_double, c_int, c_uint64
from numbers import Real
from typing import Any, SupportsIndex

from biyos import _capi
from biyos._convert.integers import integer, mask
from biyos._convert.text import encoded
from biyos._env.options import ENUMS, Masks

Value = bool | int | float | str | Masks


def _rejected(name: str, noun: str, value: object) -> TypeError:
    found = type(value).__name__
    return TypeError(f"option '{name}' takes {noun}, not {found}")


def _bool(name: str, value: bool) -> int:
    if not isinstance(value, bool):
        raise _rejected(name, "a bool", value)
    return int(value)


def _int(name: str, value: SupportsIndex) -> int:
    try:
        return integer(f"option '{name}'", value)
    except TypeError:
        raise _rejected(name, "an int", value) from None


def _double(name: str, value: float) -> float:
    if isinstance(value, bool) or not isinstance(value, Real):
        raise _rejected(name, "a number", value)
    return float(value)


def _string(name: str, value: str) -> bytes:
    if not isinstance(value, str):
        raise _rejected(name, "a str", value)
    return encoded(value)


def _masks(name: str, value: object) -> tuple[int, int]:
    if not isinstance(value, tuple) or len(value) != 2:
        raise _rejected(name, "a pair (on, off)", value)
    on, off = value
    return mask(f"option '{name}' on", on), mask(f"option '{name}' off", off)


_CONVERTERS: dict[int, Callable[..., object]] = {
    _capi.OPTKND_BOOL: _bool,
    _capi.OPTKND_INT: _int,
    _capi.OPTKND_DBL: _double,
    _capi.OPTKND_STR: _string,
    _capi.OPTKND_ENUM: _int,
}

_READERS: dict[int, tuple[type, Callable[..., Value]]] = {
    _capi.OPTKND_BOOL: (c_int, bool),
    _capi.OPTKND_INT: (c_int, int),
    _capi.OPTKND_DBL: (c_double, float),
    _capi.OPTKND_STR: (c_char_p, bytes.decode),
    _capi.OPTKND_ENUM: (c_int, int),
}


def arguments(name: str, kind: int, val: object) -> tuple[object, ...]:
    if kind == _capi.OPTKND_FLG:
        return _masks(name, val)
    return (_CONVERTERS[kind](name, val),)


def slots(kind: int) -> tuple[Any, ...]:
    if kind == _capi.OPTKND_FLG:
        return c_uint64(), c_uint64()
    slot, _ = _READERS[kind]
    return (slot(),)


def value(name: str, kind: int, found: tuple[Any, ...]) -> Value:
    if kind == _capi.OPTKND_FLG:
        on, off = found
        return Masks(on.value, off.value)
    if kind == _capi.OPTKND_ENUM:
        return ENUMS[name](found[0].value)
    _, convert = _READERS[kind]
    return convert(found[0].value)
