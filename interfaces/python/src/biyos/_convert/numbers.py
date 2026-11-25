from ctypes import c_double, c_int, c_int32, c_int64, c_uint64
from typing import TypeAlias

Real: TypeAlias = float
"""A real number, ``BIYOS_real`` of the C header."""

Index: TypeAlias = int
"""A count or an index of items, ``BIYOS_index`` of the C header."""

Offset: TypeAlias = int
"""An offset into the terms, members or nodes of a model, or a count of
them, ``BIYOS_offset`` of the C header."""

c_status = c_int
c_real = c_double
c_index = c_int32
c_integer = c_int32
c_offset = c_int64
c_mask = c_uint64
