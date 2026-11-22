from collections.abc import Sequence, Sized
from ctypes import Array, c_char_p, c_double, c_int
from numbers import Real
from typing import TYPE_CHECKING, Any, SupportsIndex, TypeGuard, TypeVar

from biyos._blocks import Block, index_of, range_of
from biyos._integers import integer
from biyos._text import text

if TYPE_CHECKING:
    from _typeshed import WriteableBuffer

    from biyos._model import Model

Element = TypeVar("Element", c_double, c_int, c_char_p)


def _sized(name: str, values: Sized, count: int) -> None:
    if len(values) != count:
        raise ValueError(f"{name} has length {len(values)}, not {count}")


def _shared(values: Any) -> "TypeGuard[WriteableBuffer]":
    try:
        view = memoryview(values)
    except TypeError:
        return False
    with view:
        flat = view.ndim == 1 and view.c_contiguous
        return flat and view.format == "d" and not view.readonly


def _real(name: str, value: float) -> float:
    if isinstance(value, bool) or not isinstance(value, Real):
        found = type(value).__name__
        raise TypeError(f"{name} must be a number, not {found}")
    return float(value)


def _array(
    element: type[Element], name: str, items: Sequence[Any], count: int
) -> Array[Element]:
    _sized(name, items, count)
    return (element * count)(*items)


def doubles(
    name: str, values: Sequence[float] | None, count: int
) -> Array[c_double] | None:
    if values is None:
        return None
    _sized(name, values, count)
    if _shared(values):
        return (c_double * count).from_buffer(values)
    entry = f"an entry of {name}"
    items = [_real(entry, value) for value in values]
    return _array(c_double, name, items, count)


def ints(
    name: str, values: Sequence[SupportsIndex] | None, count: int
) -> Array[c_int] | None:
    if values is None:
        return None
    _sized(name, values, count)
    entry = f"an entry of {name}"
    items = [integer(entry, value) for value in values]
    return _array(c_int, name, items, count)


def listed(name: str, values: Sequence[str] | None) -> Sequence[str]:
    if values is None or isinstance(values, str):
        found = type(values).__name__
        raise TypeError(f"{name} must be a sequence of str, not {found}")
    return values


def strings(
    name: str, values: Sequence[str] | None, count: int
) -> Array[c_char_p] | None:
    if values is None:
        return None
    _sized(name, listed(name, values), count)
    entry = f"an entry of {name}"
    items = [text(entry, value) for value in values]
    return _array(c_char_p, name, items, count)


def _indices(
    model: "Model",
    family: type[Block[Any]],
    name: str,
    values: Sequence[SupportsIndex] | Block[Any],
) -> range | list[int]:
    if isinstance(values, Block):
        return range_of(model, family, name, values)
    entry = f"an entry of {name}"
    return [index_of(model, family, entry, value) for value in values]


def span_of(
    model: "Model",
    family: type[Block[Any]],
    name: str,
    values: Sequence[SupportsIndex] | Block[Any],
) -> range | None:
    if isinstance(values, Block):
        return range_of(model, family, name, values)
    if isinstance(values, range) and values.step == 1:
        return values
    return None


def indices_of(
    model: "Model",
    family: type[Block[Any]],
    name: str,
    values: Sequence[SupportsIndex] | Block[Any] | None,
    count: int,
) -> Array[c_int] | None:
    if values is None:
        return None
    _sized(name, values, count)
    items = _indices(model, family, name, values)
    return _array(c_int, name, items, count)
