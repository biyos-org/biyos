from ctypes import c_double, c_int
from numbers import Real

from biyos._blocks import Block, index_of, range_of
from biyos._integers import integer


def _sized(name, values, count):
    if len(values) != count:
        raise ValueError(f"{name} has length {len(values)}, not {count}")


def _shared(values):
    try:
        view = memoryview(values)
    except TypeError:
        return False
    with view:
        flat = view.ndim == 1 and view.c_contiguous
        return flat and view.format == "d" and not view.readonly


def _real(name, value):
    if isinstance(value, bool) or not isinstance(value, Real):
        found = type(value).__name__
        raise TypeError(f"{name} must be a number, not {found}")
    return value


def doubles(name, values, count):
    if values is None:
        return None
    _sized(name, values, count)
    if _shared(values):
        return (c_double * count).from_buffer(values)
    entry = f"an entry of {name}"
    return (c_double * count)(*(_real(entry, value) for value in values))


def ints(name, values, count):
    if values is None:
        return None
    _sized(name, values, count)
    entry = f"an entry of {name}"
    return (c_int * count)(*(integer(entry, value) for value in values))


def indices_of(model, family, name, values, count):
    _sized(name, values, count)
    if isinstance(values, Block):
        return (c_int * count)(*range_of(model, family, name, values))
    entry = f"an entry of {name}"
    found = (index_of(model, family, entry, value) for value in values)
    return (c_int * count)(*found)
