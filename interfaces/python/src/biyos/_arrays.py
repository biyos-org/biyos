from ctypes import c_double, c_int, c_int64
from numbers import Real

from biyos._integers import integer, offset


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


def _ordered(name, starts):
    if len(starts) > 0 and starts[0] != 0:
        raise ValueError(f"{name} begins at {starts[0]}, not 0")
    for k in range(1, len(starts)):
        if starts[k] < starts[k - 1]:
            fall = f"{starts[k - 1]} to {starts[k]}"
            raise ValueError(f"{name} decreases from {fall} at entry {k}")


def offsets(name, values, count):
    if values is None:
        return None
    _sized(name, values, count)
    entry = f"an entry of {name}"
    copied = (c_int64 * count)(*(offset(entry, value) for value in values))
    _ordered(name, copied)
    return copied
