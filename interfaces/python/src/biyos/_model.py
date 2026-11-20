import weakref
from ctypes import byref, c_double, c_int, c_int64, c_void_p
from enum import IntEnum

from biyos._arrays import doubles, indices_of, ints, offsets
from biyos._blocks import (
    Constraints,
    Functions,
    Objectives,
    Variables,
    index_of,
)
from biyos._capi import lib
from biyos._environment import Env
from biyos._error import Error
from biyos._integers import integer


class VariableKind(IntEnum):
    CONTINUOUS = 1
    BINARY = 2
    INTEGER = 3
    SEMICONTINUOUS = 4
    SEMIINTEGER = 5


class Sense(IntEnum):
    MINIMIZE = 1
    MAXIMIZE = 2


def _environment(env):
    if not isinstance(env, Env):
        found = type(env).__name__
        raise TypeError(f"env must be an Env, not {found}")
    return env


def _span(beg, n, size):
    given = None if n is None else integer("n", n)
    total = size()
    n = max(total - beg, 0) if given is None else given
    inside = 0 <= beg <= total and 0 <= n <= total - beg
    return n, inside


def _total(starts, count):
    if starts is None or count < 0:
        return 0
    return starts[count]


def _fields(count, senses, prios, weights, abs, rel):
    return (
        ints("senses", senses, count),
        ints("prios", prios, count),
        doubles("weights", weights, count),
        doubles("abs", abs, count),
        doubles("rel", rel, count),
    )


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
    env : Env
        The environment the model is made on.
    """

    def __init__(self, env):
        self._start(_environment(env))
        with self._env._busy():
            self._make(self._env._call, lib.BIYOS_newmodel)
        self._hold()

    def __enter__(self):
        return self

    def __exit__(self, *exc_info):
        self.close()

    def __copy__(self):
        raise TypeError("copy a model with Model.copy")

    def __deepcopy__(self, memo):
        return self.__copy__()

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
            self._env._run(lib.BIYOS_freemodel, self._handle)
        except Error:
            models.add(self._handle.value)
            self._hold()
            raise

    def copy(self, env=None):
        """Return a copy of the model, made on an environment.

        The copy holds the same variables, functions, constraints and
        objectives under the same indices, and changes apart from the model.
        It reports on its own environment, like any model made there.

        Parameters
        ----------
        env : Env, optional
            The environment the copy is made on, the model's own by default.

        Returns
        -------
        Model
            The copy.

        Raises
        ------
        TypeError
            When ``env`` is not an ``Env``.
        Error
            While ``env`` delivers an event.
        ValueError
            When the model or ``env`` is closed.
        """
        env = self._env if env is None else _environment(env)
        if not self._open():
            raise ValueError("the model is closed")
        copy = Model.__new__(Model)
        copy._start(env)
        with env._busy():
            copy._copy(self)
        copy._hold()
        return copy

    def addvars(self, n, kinds=None, lb=None, ub=None):
        """Add variables after the last one.

        Parameters
        ----------
        n : int
            The number of variables to add.
        kinds : sequence of VariableKind, optional
            The kind of each, continuous by default.
        lb, ub : sequence of float, optional
            The bounds of each, the widest of its kind by default.

        Returns
        -------
        Variables
            The new variables.
        """
        n = integer("n", n)
        arrays = (
            ints("kinds", kinds, n),
            doubles("lb", lb, n),
            doubles("ub", ub, n),
        )
        first = c_int()
        self._call(lib.BIYOS_addvars, n, *arrays, byref(first))
        return Variables(self, first.value, n, self._version)

    def setvarbounds(self, idx, lb=None, ub=None):
        count = len(idx)
        arrays = (
            indices_of(self, Variables, "idx", idx, count),
            doubles("lb", lb, count),
            doubles("ub", ub, count),
        )
        self._call(lib.BIYOS_setvarbounds, count, *arrays)

    def setvarkinds(self, idx, kinds):
        count = len(idx)
        arrays = (
            indices_of(self, Variables, "idx", idx, count),
            ints("kinds", kinds, count),
        )
        self._call(lib.BIYOS_setvarkinds, count, *arrays)

    def getnvars(self):
        count = c_int()
        self._call(lib.BIYOS_getnvars, byref(count))
        return count.value

    def getvars(self, beg=0, n=None):
        beg = index_of(self, Variables, "beg", beg)
        n, inside = _span(beg, n, self.getnvars)
        if not inside:
            self._call(lib.BIYOS_getvars, beg, n, None, None, None)
        kinds = (c_int * n)()
        lower = (c_double * n)()
        upper = (c_double * n)()
        arrays = (kinds, lower, upper)
        self._call(lib.BIYOS_getvars, beg, n, *arrays)
        return [VariableKind(kind) for kind in kinds], list(lower), list(upper)

    def addfuns(
        self,
        n,
        consts=None,
        starts=None,
        vars=None,
        vals=None,
    ):
        """Add functions after the last one.

        Parameters
        ----------
        n : int
            The number of functions to add.
        consts : sequence of float, optional
            The constant of each, 0 by default.
        starts : sequence of int, optional
            The ``n + 1`` offsets of the functions' linear terms in
            ``vars`` and ``vals``, from 0 and never decreasing.
            Function ``j`` has the terms from ``starts[j]`` to
            ``starts[j + 1] - 1``. By default the functions have no terms.
        vars : sequence of int, optional
            The variable of each term, ``starts[n]`` of them.
        vals : sequence of float, optional
            The coefficient of each term, ``starts[n]`` of them.

        Returns
        -------
        Functions
            The new functions.
        """
        n = integer("n", n)
        starts = offsets("starts", starts, n + 1)
        size = _total(starts, n)
        arrays = (
            doubles("consts", consts, n),
            starts,
            indices_of(self, Variables, "vars", vars, size),
            doubles("vals", vals, size),
        )
        first = c_int()
        self._call(lib.BIYOS_addfuns, n, *arrays, byref(first))
        return Functions(self, first.value, n, self._version)

    def getnfuns(self):
        count = c_int()
        self._call(lib.BIYOS_getnfuns, byref(count))
        return count.value

    def getfuns(self, beg=0, n=None):
        beg = index_of(self, Functions, "beg", beg)
        n, inside = _span(beg, n, self.getnfuns)
        needed = c_int64()
        if not inside:
            empty = (None, None, 0, byref(needed), None, None)
            self._call(lib.BIYOS_getfuns, beg, n, *empty)
        constants = (c_double * n)()
        starts = (c_int64 * (n + 1))()
        learn = (constants, starts, 0, byref(needed), None, None)
        self._call(lib.BIYOS_getfuns, beg, n, *learn)
        size = needed.value
        variables = (c_int * size)()
        values = (c_double * size)()
        fill = (None, None, size, byref(needed), variables, values)
        self._call(lib.BIYOS_getfuns, beg, n, *fill)
        terms = (list(starts), list(variables), list(values))
        return (list(constants), *terms)

    def addquadterms(self, funs, starts, vars1, vars2, vals):
        """Add quadratic terms to functions of the model.

        Function ``funs[j]`` gains the terms from ``starts[j]`` to
        ``starts[j + 1] - 1``. Term ``k`` adds ``vals[k]`` times the
        product of variables ``vars1[k]`` and ``vars2[k]``, with no factor
        1/2. The two variables of a pair may come in either order. A pair
        the function holds already adds its coefficient to the one held.

        Parameters
        ----------
        funs : sequence of int
            The functions that gain terms, each listed once.
        starts : sequence of int
            The ``len(funs) + 1`` offsets of the functions' terms in
            ``vars1``, ``vars2`` and ``vals``, from 0 and never
            decreasing.
        vars1, vars2 : sequence of int
            The two variables of each term, ``starts[-1]`` of each.
        vals : sequence of float
            The coefficient of each term, ``starts[-1]`` of them.
        """
        count = len(funs)
        starts = offsets("starts", starts, count + 1)
        size = _total(starts, count)
        arrays = (
            indices_of(self, Functions, "funs", funs, count),
            starts,
            indices_of(self, Variables, "vars1", vars1, size),
            indices_of(self, Variables, "vars2", vars2, size),
            doubles("vals", vals, size),
        )
        self._call(lib.BIYOS_addquadterms, count, *arrays)

    def getquadterms(self, beg=0, n=None):
        beg = index_of(self, Functions, "beg", beg)
        n, inside = _span(beg, n, self.getnfuns)
        needed = c_int64()
        if not inside:
            empty = (None, 0, byref(needed), None, None, None)
            self._call(lib.BIYOS_getquadterms, beg, n, *empty)
        starts = (c_int64 * (n + 1))()
        learn = (starts, 0, byref(needed), None, None, None)
        self._call(lib.BIYOS_getquadterms, beg, n, *learn)
        size = needed.value
        firsts = (c_int * size)()
        seconds = (c_int * size)()
        values = (c_double * size)()
        fill = (None, size, byref(needed), firsts, seconds, values)
        self._call(lib.BIYOS_getquadterms, beg, n, *fill)
        terms = (list(firsts), list(seconds), list(values))
        return (list(starts), *terms)

    def addcons(self, funs, lb=None, ub=None):
        """Add constraints after the last one, one for each function given.

        Parameters
        ----------
        funs : sequence of int
            The function each new constraint bounds.
        lb, ub : sequence of float, optional
            The bounds of each, unbounded by default.

        Returns
        -------
        Constraints
            The new constraints.
        """
        count = len(funs)
        arrays = (
            indices_of(self, Functions, "funs", funs, count),
            doubles("lb", lb, count),
            doubles("ub", ub, count),
        )
        first = c_int()
        self._call(lib.BIYOS_addcons, count, *arrays, byref(first))
        return Constraints(self, first.value, count, self._version)

    def setconbounds(self, idx, lb=None, ub=None):
        count = len(idx)
        arrays = (
            indices_of(self, Constraints, "idx", idx, count),
            doubles("lb", lb, count),
            doubles("ub", ub, count),
        )
        self._call(lib.BIYOS_setconbounds, count, *arrays)

    def getncons(self):
        count = c_int()
        self._call(lib.BIYOS_getncons, byref(count))
        return count.value

    def getcons(self, beg=0, n=None):
        beg = index_of(self, Constraints, "beg", beg)
        n, inside = _span(beg, n, self.getncons)
        if not inside:
            empty = (None, None, None)
            self._call(lib.BIYOS_getcons, beg, n, *empty)
        functions = (c_int * n)()
        lower = (c_double * n)()
        upper = (c_double * n)()
        arrays = (functions, lower, upper)
        self._call(lib.BIYOS_getcons, beg, n, *arrays)
        return list(functions), list(lower), list(upper)

    def addobjs(
        self,
        funs,
        senses=None,
        prios=None,
        weights=None,
        abs=None,
        rel=None,
    ):
        """Add objectives after the last one, one for each function given.

        The objectives are optimized in stages, one for each priority, the
        lowest first. A stage minimizes the weighted sum of its objectives,
        each counted negatively when it is maximized, and keeps every
        objective of an earlier stage within its tolerances of the optimum
        found for it.

        Parameters
        ----------
        funs : sequence of int
            The function each new objective optimizes.
        senses : sequence of Sense, optional
            Whether each is minimized or maximized, minimized by default.
        prios : sequence of int, optional
            The priority of each. By default each takes its own index, so
            the objectives are optimized one after another in the order
            they are added.
        weights : sequence of float, optional
            The weight of each in the sum of its stage, finite and
            positive, 1 by default.
        abs, rel : sequence of float, optional
            How far each may move from its optimum in later stages, as a
            value of its function and as a fraction of the size of that
            optimum. Each is 0 or more, ``inf`` setting no limit, and 0 by
            default.

        Returns
        -------
        Objectives
            The new objectives.
        """
        count = len(funs)
        arrays = (
            indices_of(self, Functions, "funs", funs, count),
            *_fields(count, senses, prios, weights, abs, rel),
        )
        first = c_int()
        self._call(lib.BIYOS_addobjs, count, *arrays, byref(first))
        return Objectives(self, first.value, count, self._version)

    def setobjs(
        self,
        idx,
        senses=None,
        prios=None,
        weights=None,
        abs=None,
        rel=None,
    ):
        count = len(idx)
        arrays = (
            indices_of(self, Objectives, "idx", idx, count),
            *_fields(count, senses, prios, weights, abs, rel),
        )
        self._call(lib.BIYOS_setobjs, count, *arrays)

    def getnobjs(self):
        count = c_int()
        self._call(lib.BIYOS_getnobjs, byref(count))
        return count.value

    def getobjs(self, beg=0, n=None):
        beg = index_of(self, Objectives, "beg", beg)
        n, inside = _span(beg, n, self.getnobjs)
        if not inside:
            empty = (None,) * 6
            self._call(lib.BIYOS_getobjs, beg, n, *empty)
        functions = (c_int * n)()
        codes = (c_int * n)()
        priorities = (c_int * n)()
        weights = (c_double * n)()
        absolute = (c_double * n)()
        relative = (c_double * n)()
        found = (functions, codes, priorities, weights, absolute, relative)
        self._call(lib.BIYOS_getobjs, beg, n, *found)
        senses = [Sense(code) for code in codes]
        values = (list(weights), list(absolute), list(relative))
        return (list(functions), senses, list(priorities), *values)

    def _start(self, env):
        self._env = env
        self._handle = c_void_p()
        self._calls = 0
        self._closing = False
        self._version = 0

    def _make(self, call, *arguments):
        handle = self._handle
        try:
            call(*arguments, byref(handle))
        finally:
            if handle.value is not None:
                self._env._models.add(handle.value)

    def _copy(self, source):
        if not self._env._open():
            raise ValueError("the environment is closed")
        target = self._env._handle
        self._make(source._call, lib.BIYOS_copymodel, target)

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
