import weakref
from ctypes import byref, c_double, c_int, c_int64, c_void_p
from enum import IntEnum

from biyos._arrays import doubles, ints, offsets
from biyos._capi import lib
from biyos._environment import Environment
from biyos._error import Error
from biyos._integers import integer


class VariableKind(IntEnum):
    CONTINUOUS = 1
    BINARY = 2
    INTEGER = 3
    SEMICONTINUOUS = 4
    SEMIINTEGER = 5


def _environment(env):
    if not isinstance(env, Environment):
        found = type(env).__name__
        raise TypeError(f"env must be an Environment, not {found}")
    return env


def _span(first, count, size):
    first = integer("first", first)
    given = None if count is None else integer("count", count)
    total = size()
    count = max(total - first, 0) if given is None else given
    inside = 0 <= first <= total and 0 <= count <= total - first
    return first, count, inside


def _total(starts, count):
    if starts is None or count < 0:
        return 0
    return starts[count]


class Model:
    """A model of a problem, made on an environment.

    The model keeps its environment alive and reports its failures there.
    It is closed by ``close``, on leaving a ``with`` block, or when its
    environment is closed. Using it once closed raises ``ValueError``, and
    closing it again does nothing. ``close`` raises ``biyos.Error`` while the
    environment delivers an event, and the model stays open.

    Parameters
    ----------
    env : Environment
        The environment the model is made on.
    """

    def __init__(self, env):
        env = _environment(env)
        handle = c_void_p()
        models = env._models
        try:
            env._call(lib.biyos_model_create, byref(handle))
        finally:
            if handle.value is not None:
                models.add(handle.value)
        self._env = env
        self._handle = handle
        self._hold()

    def __enter__(self):
        return self

    def __exit__(self, *exc_info):
        self.close()

    def close(self):
        if not self._open():
            return
        models = self._env._models
        models.take(self._handle.value)
        self._free.detach()
        try:
            self._env._run(lib.biyos_model_free, self._handle)
        except Error:
            models.add(self._handle.value)
            self._hold()
            raise

    def add_variables(self, count, kinds=None, lower=None, upper=None):
        count = integer("count", count)
        arrays = (
            ints("kinds", kinds, count),
            doubles("lower", lower, count),
            doubles("upper", upper, count),
        )
        self._call(lib.biyos_add_variables, count, *arrays)

    def set_variable_bounds(self, indices, lower=None, upper=None):
        count = len(indices)
        arrays = (
            ints("indices", indices, count),
            doubles("lower", lower, count),
            doubles("upper", upper, count),
        )
        self._call(lib.biyos_set_variable_bounds, count, *arrays)

    def set_variable_kinds(self, indices, kinds):
        count = len(indices)
        arrays = (ints("indices", indices, count), ints("kinds", kinds, count))
        self._call(lib.biyos_set_variable_kinds, count, *arrays)

    def variable_count(self):
        count = c_int()
        self._call(lib.biyos_variable_count, byref(count))
        return count.value

    def get_variables(self, first=0, count=None):
        first, count, inside = _span(first, count, self.variable_count)
        if not inside:
            self._call(lib.biyos_get_variables, first, count, None, None, None)
        kinds = (c_int * count)()
        lower = (c_double * count)()
        upper = (c_double * count)()
        arrays = (kinds, lower, upper)
        self._call(lib.biyos_get_variables, first, count, *arrays)
        return [VariableKind(kind) for kind in kinds], list(lower), list(upper)

    def add_rows(
        self,
        count,
        lower=None,
        upper=None,
        starts=None,
        columns=None,
        values=None,
    ):
        count = integer("count", count)
        starts = offsets("starts", starts, count + 1)
        size = _total(starts, count)
        arrays = (
            doubles("lower", lower, count),
            doubles("upper", upper, count),
            starts,
            ints("columns", columns, size),
            doubles("values", values, size),
        )
        self._call(lib.biyos_add_rows, count, *arrays)

    def set_row_bounds(self, indices, lower=None, upper=None):
        count = len(indices)
        arrays = (
            ints("indices", indices, count),
            doubles("lower", lower, count),
            doubles("upper", upper, count),
        )
        self._call(lib.biyos_set_row_bounds, count, *arrays)

    def row_count(self):
        count = c_int()
        self._call(lib.biyos_row_count, byref(count))
        return count.value

    def get_rows(self, first=0, count=None):
        first, count, inside = _span(first, count, self.row_count)
        if not inside:
            self._call(lib.biyos_get_rows, first, count, None, None)
        lower = (c_double * count)()
        upper = (c_double * count)()
        self._call(lib.biyos_get_rows, first, count, lower, upper)
        return list(lower), list(upper)

    def get_entries(self, first=0, count=None):
        first, count, inside = _span(first, count, self.row_count)
        needed = c_int64()
        if not inside:
            empty = (None, 0, byref(needed), None, None)
            self._call(lib.biyos_get_entries, first, count, *empty)
        starts = (c_int64 * (count + 1))()
        learn = (starts, 0, byref(needed), None, None)
        self._call(lib.biyos_get_entries, first, count, *learn)
        size = needed.value
        columns = (c_int * size)()
        values = (c_double * size)()
        fill = (None, size, byref(needed), columns, values)
        self._call(lib.biyos_get_entries, first, count, *fill)
        return list(starts), list(columns), list(values)

    def _hold(self):
        park = self._env._models.park
        self._free = weakref.finalize(self, park, self._handle.value)

    def _open(self):
        return self._free.alive and not self._env._models.closed

    def _call(self, function, *arguments):
        if not self._open():
            raise ValueError("the model is closed")
        self._env._run(function, self._handle, *arguments)
