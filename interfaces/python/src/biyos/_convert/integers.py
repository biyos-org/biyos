from operator import index
from typing import SupportsIndex

from biyos._convert.numbers import Offset, c_integer, c_mask, c_offset


def whole(name: str, value: SupportsIndex) -> int:
    if isinstance(value, bool):
        raise TypeError(f"{name} must be an int, not bool")
    try:
        return index(value)
    except TypeError:
        found = type(value).__name__
        raise TypeError(f"{name} must be an int, not {found}") from None


def integer(name: str, value: SupportsIndex) -> int:
    number = whole(name, value)
    if c_integer(number).value != number:
        raise OverflowError(f"{name}: {number} does not fit in a C int")
    return number


def offset(name: str, value: SupportsIndex) -> Offset:
    number = whole(name, value)
    if c_offset(number).value != number:
        raise OverflowError(f"{name}: {number} does not fit in an int64_t")
    return number


def mask(name: str, value: SupportsIndex) -> int:
    number = whole(name, value)
    if c_mask(number).value != number:
        raise OverflowError(f"{name}: {number} does not fit in a BIYOS_mask")
    return number
