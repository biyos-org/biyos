from ctypes import c_int
from operator import index


def whole(name, value):
    if isinstance(value, bool):
        raise TypeError(f"{name} must be an int, not bool")
    try:
        return index(value)
    except TypeError:
        found = type(value).__name__
        raise TypeError(f"{name} must be an int, not {found}") from None


def integer(name, value):
    number = whole(name, value)
    if c_int(number).value != number:
        raise OverflowError(f"{name}: {number} does not fit in a C int")
    return number
