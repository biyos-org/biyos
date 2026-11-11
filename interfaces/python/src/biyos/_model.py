import weakref
from ctypes import byref, c_double, c_int, c_int64, c_void_p
from enum import IntEnum

from biyos._arrays import doubles, indices_of, ints, offsets
from biyos._blocks import Functions, Variables, index_of
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
    given = None if count is None else integer("count", count)
    total = size()
    count = max(total - first, 0) if given is None else given
    inside = 0 <= first <= total and 0 <= count <= total - first
    return count, inside


def _total(starts, count):
    if starts is None or count < 0:
        return 0
    return starts[count]


class Model:
    """A model of a problem, made on an environment.

    The model keeps its environment alive and reports its failures there.
    It is closed by ``close``, on leaving a ``with`` block, or when its
    environment is closed. Using it once closed raises ``ValueError``, and
    closing it again does nothing. Closed during one of its own calls, by a
    subscriber or a finalizer, it reads as closed at once and is freed when
    the call returns, or when the outermost call returns if the call was
    made from a subscriber. Otherwise ``close`` raises ``biyos.Error`` while
    the environment delivers an event, and the model stays open.

    Parameters
    ----------
    env : Environment
        The environment the model is made on.
    """

    def __init__(self, env):
        env = _environment(env)
        self._env = env
        self._handle = c_void_p()
        self._calls = 0
        self._closing = False
        self._version = 0
        with env._busy():
            self._create()
        self._hold()

    def __enter__(self):
        return self

    def __exit__(self, *exc_info):
        self.close()

    def close(self):
        with self._env._busy():
            self._close()

    def _close(self):
        if not self._open():
            return
        if self._calls > 0:
            self._closing = True
            return
        models = self._env._models
        models.take(self._handle.value)
        if self._free.detach() is None:
            return
        try:
            self._env._run(lib.biyos_model_free, self._handle)
        except Error:
            models.add(self._handle.value)
            self._hold()
            raise

    def add_variables(self, count, kinds=None, lower=None, upper=None):
        """Add variables after the last one.

        Parameters
        ----------
        count : int
            The number of variables to add.
        kinds : sequence of VariableKind, optional
            The kind of each, continuous by default.
        lower, upper : sequence of float, optional
            The bounds of each, the widest of its kind by default.

        Returns
        -------
        Variables
            The new variables.
        """
        count = integer("count", count)
        arrays = (
            ints("kinds", kinds, count),
            doubles("lower", lower, count),
            doubles("upper", upper, count),
        )
        first = c_int()
        self._call(lib.biyos_add_variables, count, *arrays, byref(first))
        return Variables(self, first.value, count, self._version)

    def set_variable_bounds(self, indices, lower=None, upper=None):
        count = len(indices)
        arrays = (
            indices_of(self, Variables, "indices", indices, count),
            doubles("lower", lower, count),
            doubles("upper", upper, count),
        )
        self._call(lib.biyos_set_variable_bounds, count, *arrays)

    def set_variable_kinds(self, indices, kinds):
        count = len(indices)
        arrays = (
            indices_of(self, Variables, "indices", indices, count),
            ints("kinds", kinds, count),
        )
        self._call(lib.biyos_set_variable_kinds, count, *arrays)

    def variable_count(self):
        count = c_int()
        self._call(lib.biyos_variable_count, byref(count))
        return count.value

    def get_variables(self, first=0, count=None):
        first = index_of(self, Variables, "first", first)
        count, inside = _span(first, count, self.variable_count)
        if not inside:
            self._call(lib.biyos_get_variables, first, count, None, None, None)
        kinds = (c_int * count)()
        lower = (c_double * count)()
        upper = (c_double * count)()
        arrays = (kinds, lower, upper)
        self._call(lib.biyos_get_variables, first, count, *arrays)
        return [VariableKind(kind) for kind in kinds], list(lower), list(upper)

    def add_functions(
        self,
        count,
        constants=None,
        starts=None,
        variables=None,
        values=None,
    ):
        """Add functions after the last one.

        Parameters
        ----------
        count : int
            The number of functions to add.
        constants : sequence of float, optional
            The constant of each, 0 by default.
        starts : sequence of int, optional
            The ``count + 1`` offsets of the functions' linear terms in
            ``variables`` and ``values``, from 0 and never decreasing.
            Function ``j`` has the terms from ``starts[j]`` to
            ``starts[j + 1] - 1``. By default the functions have no terms.
        variables : sequence of int, optional
            The variable of each term, ``starts[count]`` of them.
        values : sequence of float, optional
            The coefficient of each term, ``starts[count]`` of them.

        Returns
        -------
        Functions
            The new functions.
        """
        count = integer("count", count)
        starts = offsets("starts", starts, count + 1)
        size = _total(starts, count)
        arrays = (
            doubles("constants", constants, count),
            starts,
            indices_of(self, Variables, "variables", variables, size),
            doubles("values", values, size),
        )
        first = c_int()
        self._call(lib.biyos_add_functions, count, *arrays, byref(first))
        return Functions(self, first.value, count, self._version)

    def function_count(self):
        count = c_int()
        self._call(lib.biyos_function_count, byref(count))
        return count.value

    def get_functions(self, first=0, count=None):
        first = index_of(self, Functions, "first", first)
        count, inside = _span(first, count, self.function_count)
        needed = c_int64()
        if not inside:
            empty = (None, None, 0, byref(needed), None, None)
            self._call(lib.biyos_get_functions, first, count, *empty)
        constants = (c_double * count)()
        starts = (c_int64 * (count + 1))()
        learn = (constants, starts, 0, byref(needed), None, None)
        self._call(lib.biyos_get_functions, first, count, *learn)
        size = needed.value
        variables = (c_int * size)()
        values = (c_double * size)()
        fill = (None, None, size, byref(needed), variables, values)
        self._call(lib.biyos_get_functions, first, count, *fill)
        terms = (list(starts), list(variables), list(values))
        return (list(constants), *terms)

    def _create(self):
        handle = self._handle
        try:
            self._env._call(lib.biyos_model_create, byref(handle))
        finally:
            if handle.value is not None:
                self._env._models.add(handle.value)

    def _hold(self):
        park = self._env._models.park
        self._free = weakref.finalize(self, park, self._handle.value)

    def _open(self):
        if self._closing:
            return False
        return self._free.alive and self._env._open()

    def _call(self, function, *arguments):
        with self._env._busy():
            self._run(function, *arguments)

    def _run(self, function, *arguments):
        self._calls += 1
        try:
            if not self._open():
                raise ValueError("the model is closed")
            self._env._run(function, self._handle, *arguments)
        finally:
            self._calls -= 1
            self._settle()

    def _settle(self):
        if self._calls == 0 and self._closing:
            self._free()
