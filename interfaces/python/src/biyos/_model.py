import weakref
from ctypes import byref, c_double, c_int, c_void_p
from enum import IntEnum

from biyos._arrays import doubles, ints
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


class Model:
    """A model made on an environment, holding the variables of a problem.

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

    def _hold(self):
        park = self._env._models.park
        self._free = weakref.finalize(self, park, self._handle.value)

    def _open(self):
        return self._free.alive and not self._env._models.closed

    def _call(self, function, *arguments):
        if not self._open():
            raise ValueError("the model is closed")
        self._env._run(function, self._handle, *arguments)
