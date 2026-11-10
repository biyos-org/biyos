from ctypes import c_int


def integer(name, value):
    if isinstance(value, bool) or not isinstance(value, int):
        found = type(value).__name__
        raise TypeError(f"{name} must be an int, not {found}")
    if c_int(value).value != value:
        raise OverflowError(f"{name}: {value} does not fit in a C int")
    return value
