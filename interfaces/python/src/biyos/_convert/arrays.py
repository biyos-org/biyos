import numbers
from array import array
from collections.abc import Sequence, Sized
from ctypes import Array, c_char_p
from typing import (
    TYPE_CHECKING,
    Any,
    SupportsIndex,
    TypeAlias,
    TypeGuard,
    TypeVar,
    cast,
)

from biyos._convert.integers import integer, offset
from biyos._convert.numbers import (
    Index,
    Real,
    c_index,
    c_integer,
    c_offset,
    c_real,
)
from biyos._convert.text import text
from biyos._model.blocks import Block, Indices, View, idx_of, index_of

if TYPE_CHECKING:
    from _typeshed import WriteableBuffer

    from biyos._model.model import Model

Element = TypeVar("Element", c_real, c_integer, c_offset, c_char_p)
Value = TypeVar("Value")
Entries: TypeAlias = list[tuple[str, Sequence[Any]]]


def _sized(name: str, values: Sized, count: int) -> None:
    if len(values) != count:
        raise ValueError(f"{name} has length {len(values)}, not {count}")


def _single(values: object) -> bool:
    return isinstance(values, str) or not isinstance(values, Sized)


def _alone(name: str, values: object) -> None:
    if not _single(values):
        found = type(values).__name__
        raise TypeError(f"{name} must be one value for one index, not {found}")


def _wrapped(
    name: str, values: Value | Sequence[Value], one: bool
) -> Sequence[Value]:
    if one:
        _alone(name, values)
        return [cast(Value, values)]
    return sequence(name, values)


def _entry(name: str, one: bool) -> str:
    return name if one else f"an entry of {name}"


def counted(idx: object) -> tuple[bool, int]:
    if isinstance(idx, Sized):
        return False, len(idx)
    return True, 1


def picked(
    model: "Model",
    family: type[Block[View]],
    idx: SupportsIndex | Sequence[SupportsIndex] | Block[View],
) -> Sequence[SupportsIndex] | Block[View]:
    if isinstance(idx, Sized):
        return idx
    return [index_of(model, family, "idx", idx)]


def _shared(values: Any) -> "TypeGuard[WriteableBuffer]":
    try:
        view = memoryview(values)
    except TypeError:
        return False
    with view:
        flat = view.ndim == 1 and view.c_contiguous
        return flat and view.format == "d" and not view.readonly


def _real(name: str, value: Real) -> Real:
    if isinstance(value, bool) or not isinstance(value, numbers.Real):
        found = type(value).__name__
        raise TypeError(f"{name} must be a number, not {found}")
    return float(value)


def _array(
    element: type[Element], name: str, items: Sequence[Any], count: int
) -> Array[Element]:
    _sized(name, items, count)
    return (element * count)(*items)


def doubles(
    name: str,
    values: Real | Sequence[Real] | None,
    count: int,
    one: bool = False,
) -> Array[c_real] | None:
    if values is None:
        return None
    values = _wrapped(name, values, one)
    _sized(name, values, count)
    if _shared(values):
        return (c_real * count).from_buffer(values)
    entry = _entry(name, one)
    items = [_real(entry, value) for value in values]
    return _array(c_real, name, items, count)


def ints(
    name: str,
    values: SupportsIndex | Sequence[SupportsIndex] | None,
    count: int,
    one: bool = False,
) -> Array[c_integer] | None:
    if values is None:
        return None
    values = _wrapped(name, values, one)
    _sized(name, values, count)
    entry = _entry(name, one)
    items = [integer(entry, value) for value in values]
    return _array(c_integer, name, items, count)


def listed(name: str, values: Sequence[str] | None) -> Sequence[str]:
    if values is None or isinstance(values, str):
        found = type(values).__name__
        raise TypeError(f"{name} must be a sequence of str, not {found}")
    return values


def strings(
    name: str,
    values: str | Sequence[str] | None,
    count: int,
    one: bool = False,
) -> Array[c_char_p] | None:
    if values is None:
        return None
    values = _wrapped(name, values, one)
    _sized(name, listed(name, values), count)
    entry = _entry(name, one)
    items = [text(entry, value) for value in values]
    return _array(c_char_p, name, items, count)


def _indices(
    model: "Model",
    family: type[Block[View]],
    name: str,
    values: Sequence[SupportsIndex] | Block[View],
    one: bool = False,
) -> "Indices | list[Index]":
    if isinstance(values, Block):
        return idx_of(model, family, name, values)
    entry = _entry(name, one)
    return [index_of(model, family, entry, value) for value in values]


def span_of(
    model: "Model",
    family: type[Block[View]],
    name: str,
    values: Sequence[SupportsIndex] | Block[View],
) -> range | None:
    if isinstance(values, Block):
        idx = idx_of(model, family, name, values)
        return idx if isinstance(idx, range) else None
    if isinstance(values, range) and values.step == 1:
        return values
    return None


def indices_of(
    model: "Model",
    family: type[Block[View]],
    name: str,
    values: SupportsIndex | Sequence[SupportsIndex] | Block[View] | None,
    count: int,
    one: bool = False,
) -> Array[c_index] | None:
    if values is None:
        return None
    if not isinstance(values, Block):
        values = _wrapped(name, values, one)
    elif one:
        _alone(name, values)
    _sized(name, values, count)
    items = _indices(model, family, name, values, one)
    if isinstance(items, memoryview):
        _sized(name, items, count)
        return (c_index * count).from_buffer(items)
    return _array(c_index, name, items, count)


def index_view(count: int) -> "memoryview[int]":
    return memoryview(array(c_index._type_, [0]) * count)


def _counted(name: str, counts: Array[c_offset]) -> None:
    for k, count in enumerate(counts):
        if count < 0:
            raise ValueError(f"{name}: {count} is negative at entry {k}")


def int64s(
    name: str,
    values: SupportsIndex | Sequence[SupportsIndex] | None,
    count: int,
    one: bool = False,
) -> Array[c_offset] | None:
    if values is None:
        return None
    values = _wrapped(name, values, one)
    _sized(name, values, count)
    entry = _entry(name, one)
    items = [offset(entry, value) for value in values]
    return _array(c_offset, name, items, count)


def counts(
    name: str, values: Sequence[SupportsIndex] | None, count: int
) -> Array[c_offset] | None:
    copied = int64s(name, values, count)
    if copied is not None:
        _counted(name, copied)
    return copied


def sequence(name: str, values: object) -> Sequence[Any]:
    if not isinstance(values, Sized):
        found = type(values).__name__
        raise TypeError(f"{name} must be a sequence, not {found}")
    return cast("Sequence[Any]", values)


def nested(name: str, values: object) -> bool:
    entries = sequence(name, values)
    if isinstance(entries, Block):
        return False
    return isinstance(next(iter(entries), None), Sized)


def entries_of(name: str, values: object, count: int) -> Entries:
    found = sequence(name, values)
    _sized(name, found, count)
    parts = [f"{name}[{k}]" for k in range(count)]
    return [(part, sequence(part, entry)) for part, entry in zip(parts, found)]


def matched(name: str, values: object, sizes: Sequence[int]) -> Entries:
    entries = entries_of(name, values, len(sizes))
    for (part, entry), size in zip(entries, sizes):
        _sized(part, entry, size)
    return entries


def nested_indices(
    model: "Model", family: type[Block[View]], entries: Entries
) -> Array[c_index]:
    items = [
        index
        for part, entry in entries
        for index in _indices(model, family, part, entry)
    ]
    return (c_index * len(items))(*items)


def nested_doubles(entries: Entries) -> Array[c_real]:
    items = [
        _real(f"an entry of {part}", value)
        for part, entry in entries
        for value in entry
    ]
    return (c_real * len(items))(*items)
