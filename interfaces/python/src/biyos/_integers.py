from ctypes import c_int
from operator import index


def integer(name, value):
    if isinstance(value, bool):
        raise TypeError(f"{name} must be an int, not bool")
    try:
        number = index(value)
    except TypeError:
        found = type(value).__name__
        raise TypeError(f"{name} must be an int, not {found}") from None
    if c_int(number).value != number:
        raise OverflowError(f"{name}: {number} does not fit in a C int")
    return number
