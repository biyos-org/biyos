import weakref
from ctypes import byref, c_char_p, c_double, c_int, c_int64, c_void_p
from enum import IntEnum

from biyos._arrays import doubles, indices_of, ints, listed, offsets, strings
from biyos._blocks import (
    Constraints,
    Functions,
    Objectives,
    Variables,
    index_of,
)
from biyos._capi import lib
from biyos._env import Env
from biyos._error import Error
from biyos._integers import integer
from biyos._text import decoded, text


class VariableKind(IntEnum):
    """The kinds of variables, as ``BIYOS_VAR_`` numbers them.

    Attributes
    ----------
    CONTINUOUS
        Any value between its bounds.
    BINARY
        0 or 1, between its bounds.
    INTEGER
        Any integer between its bounds.
    SEMICONTINUOUS
        0, or any value between its bounds.
    SEMIINTEGER
        0, or any integer between its bounds.
    """

    CONTINUOUS = 1
    BINARY = 2
    INTEGER = 3
    SEMICONTINUOUS = 4
    SEMIINTEGER = 5


class Sense(IntEnum):
    """The senses of objectives, as ``BIYOS_MINIMIZE`` and ``BIYOS_MAXIMIZE``
    number them.

    Attributes
    ----------
    MINIMIZE
        The function is minimized.
    MAXIMIZE
        The function is maximized.
    """

    MINIMIZE = 1
    MAXIMIZE = 2


def _checked_env(env):
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


def _fields(count, sense, prio, coef, atol, rtol):
    return (
        ints("sense", sense, count),
        ints("prio", prio, count),
        doubles("coef", coef, count),
        doubles("atol", atol, count),
        doubles("rtol", rtol, count),
    )


class Model:
    """A model of a problem, made on an environment.

    The model keeps its environment alive and reports its failures there.
    It is closed by ``close``, on leaving a ``with`` block, or when its
    environment is closed. Using it once closed raises ``ValueError``, and
    closing it again does nothing. Closed during one of its own calls, by a
    hook or a finalizer, it reads as closed at once and is freed when
    the call returns, or when the outermost call returns if the call was
    made from a hook. Closed while its environment delivers an event, it
    reads as closed at once and is freed when the outermost call returns.

    Parameters
    ----------
    env : Env
        The environment the model is made on.

    See Also
    --------
    BIYOS_newmdl
    """

    def __init__(self, env):
        self._start(_checked_env(env))
        with self._env._busy():
            self._make(self._env._call, lib.BIYOS_newmdl)
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
        """Free the model.

        Using it afterwards raises ``ValueError``, and closing it again does
        nothing. Closed during one of its own calls or while its environment
        delivers an event, by a hook or a finalizer, it reads as closed at
        once and is freed when the outermost call returns. Such a deferred
        close never raises.

        Raises
        ------
        Error
            When the free is refused. The model then stays open.

        See Also
        --------
        BIYOS_freemdl
        """
        with self._env._busy():
            self._close()

    def _close(self):
        if not self._open():
            return
        if self._calls > 0:
            self._closing = True
            return
        if not self._env._relay.idle():
            self._free()
            return
        models = self._env._models
        models.take(self._handle.value)
        if self._free.detach() is None:
            return
        try:
            self._env._run(lib.BIYOS_freemdl, self._handle)
        except Error:
            models.add(self._handle.value)
            self._hold()
            raise

    def copy(self, env=None):
        """Return a copy of the model, made on an environment.

        The copy holds the same variables, functions, constraints and
        objectives under the same indices and names, and changes apart from
        the model. It reports on its own environment, like any model made
        there.

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

        See Also
        --------
        BIYOS_copymdl
        """
        env = self._env if env is None else _checked_env(env)
        if not self._open():
            raise ValueError("the model is closed")
        copy = Model.__new__(Model)
        copy._start(env)
        with env._busy():
            copy._copy(self)
        copy._hold()
        return copy

    def addvars(self, n, knd=None, lb=None, ub=None):
        """Add variables after the last one.

        Parameters
        ----------
        n : int
            The number of variables to add.
        knd : sequence of VariableKind, optional
            The kind of each, continuous by default.
        lb, ub : sequence of float, optional
            The bounds of each, the widest of its kind by default.

        Returns
        -------
        Variables
            The new variables.

        See Also
        --------
        BIYOS_addvars
        """
        n = integer("n", n)
        arrays = (
            ints("knd", knd, n),
            doubles("lb", lb, n),
            doubles("ub", ub, n),
        )
        first = c_int()
        self._call(lib.BIYOS_addvars, n, *arrays, byref(first))
        return Variables(self, first.value, n, self._gen)

    def setvarbnd(self, idx, lb=None, ub=None):
        """Set the bounds of variables.

        A variable may appear more than once; its entries apply in order.

        Parameters
        ----------
        idx
            The variables, as indices, views or blocks.
        lb, ub
            The new bounds of each; ``None`` leaves that side as it is.

        See Also
        --------
        BIYOS_setvarbnd
        """
        count = len(idx)
        arrays = (
            indices_of(self, Variables, "idx", idx, count),
            doubles("lb", lb, count),
            doubles("ub", ub, count),
        )
        self._call(lib.BIYOS_setvarbnd, count, *arrays)

    def getvarbnd(self, idx):
        """Read the bounds of variables.

        Parameters
        ----------
        idx
            The variables, as indices, views or blocks, in any order and
            any of them more than once.

        Returns
        -------
        list of float
            The lower bound of each.
        list of float
            The upper bound of each.

        See Also
        --------
        BIYOS_getvarbnd
        """
        bounds = (c_double, c_double)
        lb, ub = self._getlist(lib.BIYOS_getvarbnd, Variables, idx, *bounds)
        return lb, ub

    def setvarknd(self, idx, knd):
        """Set the kinds of variables.

        Parameters
        ----------
        idx
            The variables, as indices, views or blocks.
        knd
            The new kind of each, a ``VariableKind``.

        See Also
        --------
        BIYOS_setvarknd
        """
        count = len(idx)
        arrays = (
            indices_of(self, Variables, "idx", idx, count),
            ints("knd", knd, count),
        )
        self._call(lib.BIYOS_setvarknd, count, *arrays)

    def getvarknd(self, idx):
        """Read the kinds of variables.

        Parameters
        ----------
        idx
            The variables, as indices, views or blocks, in any order and
            any of them more than once.

        Returns
        -------
        list of VariableKind
            The kind of each.

        See Also
        --------
        BIYOS_getvarknd
        """
        found = self._getlist(lib.BIYOS_getvarknd, Variables, idx, c_int)
        return [VariableKind(value) for value in found[0]]

    def getnvars(self):
        """Return the number of variables.

        Returns
        -------
        int
            The number of variables.

        See Also
        --------
        BIYOS_getnvars
        """
        count = c_int()
        self._call(lib.BIYOS_getnvars, byref(count))
        return count.value

    def getvars(self, beg=0, n=None):
        """Read the kinds and bounds of variables.

        Parameters
        ----------
        beg
            The first variable to read.
        n
            The number of variables to read, every one from ``beg`` on by
            default.

        Returns
        -------
        list of VariableKind
            The kind of each variable.
        list of float
            The lower bound of each.
        list of float
            The upper bound of each.

        See Also
        --------
        BIYOS_getvars
        """
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
        cst=None,
        off=None,
        varidx=None,
        coef=None,
    ):
        """Add functions after the last one.

        Parameters
        ----------
        n : int
            The number of functions to add.
        cst : sequence of float, optional
            The constant of each, 0 by default.
        off : sequence of int, optional
            The ``n + 1`` offsets of the functions' linear terms in
            ``varidx`` and ``coef``, from 0 and never decreasing.
            Function ``j`` has the terms from ``off[j]`` to
            ``off[j + 1] - 1``. By default the functions have no terms.
        varidx : sequence of int, optional
            The variable of each term, ``off[n]`` of them.
        coef : sequence of float, optional
            The coefficient of each term, ``off[n]`` of them.

        Returns
        -------
        Functions
            The new functions.

        See Also
        --------
        BIYOS_addfuns
        """
        n = integer("n", n)
        off = offsets("off", off, n + 1)
        size = _total(off, n)
        arrays = (
            doubles("cst", cst, n),
            off,
            indices_of(self, Variables, "varidx", varidx, size),
            doubles("coef", coef, size),
        )
        first = c_int()
        self._call(lib.BIYOS_addfuns, n, *arrays, byref(first))
        return Functions(self, first.value, n, self._gen)

    def getnfuns(self):
        """Return the number of functions.

        Returns
        -------
        int
            The number of functions.

        See Also
        --------
        BIYOS_getnfuns
        """
        count = c_int()
        self._call(lib.BIYOS_getnfuns, byref(count))
        return count.value

    def getfuns(self, beg=0, n=None):
        """Read the constants and linear terms of functions.

        Parameters
        ----------
        beg
            The first function to read.
        n
            The number of functions to read, every one from ``beg`` on by
            default.

        Returns
        -------
        list of float
            The constant of each function.
        list of int
            The ``n + 1`` offsets of the functions' terms in the two lists
            after it, from 0.
        list of int
            The variable of each term.
        list of float
            The coefficient of each term.

        See Also
        --------
        BIYOS_getfuns
        """
        beg = index_of(self, Functions, "beg", beg)
        n, inside = _span(beg, n, self.getnfuns)
        needed = c_int64()
        if not inside:
            empty = (0, None, None, byref(needed), None, None)
            self._call(lib.BIYOS_getfuns, beg, n, *empty)
        constants = (c_double * n)()
        starts = (c_int64 * (n + 1))()
        learn = (0, constants, starts, byref(needed), None, None)
        self._call(lib.BIYOS_getfuns, beg, n, *learn)
        size = needed.value
        variables = (c_int * size)()
        values = (c_double * size)()
        fill = (size, None, None, byref(needed), variables, values)
        self._call(lib.BIYOS_getfuns, beg, n, *fill)
        terms = (list(starts), list(variables), list(values))
        return (list(constants), *terms)

    def addquadterms(self, funidx, off, varidx1, varidx2, coef):
        """Add quadratic terms to functions of the model.

        Function ``funidx[j]`` gains the terms from ``off[j]`` to
        ``off[j + 1] - 1``. Term ``k`` adds ``coef[k]`` times the
        product of variables ``varidx1[k]`` and ``varidx2[k]``, with no factor
        1/2. The two variables of a pair may come in either order. A pair
        the function holds already adds its coefficient to the one held.

        Parameters
        ----------
        funidx : sequence of int
            The functions that gain terms, each listed once.
        off : sequence of int
            The ``len(funidx) + 1`` offsets of the functions' terms in
            ``varidx1``, ``varidx2`` and ``coef``, from 0 and never
            decreasing.
        varidx1, varidx2 : sequence of int
            The two variables of each term, ``off[-1]`` of each.
        coef : sequence of float
            The coefficient of each term, ``off[-1]`` of them.

        See Also
        --------
        BIYOS_addquadterms
        """
        count = len(funidx)
        off = offsets("off", off, count + 1)
        size = _total(off, count)
        arrays = (
            indices_of(self, Functions, "funidx", funidx, count),
            off,
            indices_of(self, Variables, "varidx1", varidx1, size),
            indices_of(self, Variables, "varidx2", varidx2, size),
            doubles("coef", coef, size),
        )
        self._call(lib.BIYOS_addquadterms, count, *arrays)

    def getquadterms(self, beg=0, n=None):
        """Read the quadratic terms of functions.

        Parameters
        ----------
        beg
            The first function to read.
        n
            The number of functions to read, every one from ``beg`` on by
            default.

        Returns
        -------
        list of int
            The ``n + 1`` offsets of the functions' terms in the three lists
            after it, from 0.
        list of int
            The first variable of each term.
        list of int
            The second variable of each term.
        list of float
            The coefficient of each term.

        See Also
        --------
        BIYOS_getquadterms
        """
        beg = index_of(self, Functions, "beg", beg)
        n, inside = _span(beg, n, self.getnfuns)
        needed = c_int64()
        if not inside:
            empty = (0, None, byref(needed), None, None, None)
            self._call(lib.BIYOS_getquadterms, beg, n, *empty)
        starts = (c_int64 * (n + 1))()
        learn = (0, starts, byref(needed), None, None, None)
        self._call(lib.BIYOS_getquadterms, beg, n, *learn)
        size = needed.value
        firsts = (c_int * size)()
        seconds = (c_int * size)()
        values = (c_double * size)()
        fill = (size, None, byref(needed), firsts, seconds, values)
        self._call(lib.BIYOS_getquadterms, beg, n, *fill)
        terms = (list(firsts), list(seconds), list(values))
        return (list(starts), *terms)

    def addcons(self, funidx, lb=None, ub=None):
        """Add constraints after the last one, one for each function given.

        Parameters
        ----------
        funidx : sequence of int
            The function each new constraint bounds.
        lb, ub : sequence of float, optional
            The bounds of each, unbounded by default.

        Returns
        -------
        Constraints
            The new constraints.

        See Also
        --------
        BIYOS_addcons
        """
        count = len(funidx)
        arrays = (
            indices_of(self, Functions, "funidx", funidx, count),
            doubles("lb", lb, count),
            doubles("ub", ub, count),
        )
        first = c_int()
        self._call(lib.BIYOS_addcons, count, *arrays, byref(first))
        return Constraints(self, first.value, count, self._gen)

    def setconbnd(self, idx, lb=None, ub=None):
        """Set the bounds of constraints and keep their functions.

        Parameters
        ----------
        idx
            The constraints, as indices, views or blocks.
        lb, ub
            The new bounds of each; ``None`` leaves that side as it is.

        See Also
        --------
        BIYOS_setconbnd
        """
        count = len(idx)
        arrays = (
            indices_of(self, Constraints, "idx", idx, count),
            doubles("lb", lb, count),
            doubles("ub", ub, count),
        )
        self._call(lib.BIYOS_setconbnd, count, *arrays)

    def getconbnd(self, idx):
        """Read the bounds of constraints.

        Parameters
        ----------
        idx
            The constraints, as indices, views or blocks, in any order and
            any of them more than once.

        Returns
        -------
        list of float
            The lower bound of each.
        list of float
            The upper bound of each.

        See Also
        --------
        BIYOS_getconbnd
        """
        bounds = (c_double, c_double)
        lb, ub = self._getlist(lib.BIYOS_getconbnd, Constraints, idx, *bounds)
        return lb, ub

    def getncons(self):
        """Return the number of constraints.

        Returns
        -------
        int
            The number of constraints.

        See Also
        --------
        BIYOS_getncons
        """
        count = c_int()
        self._call(lib.BIYOS_getncons, byref(count))
        return count.value

    def getcons(self, beg=0, n=None):
        """Read the functions and bounds of constraints.

        Parameters
        ----------
        beg
            The first constraint to read.
        n
            The number of constraints to read, every one from ``beg`` on by
            default.

        Returns
        -------
        list of int
            The function each constraint bounds.
        list of float
            The lower bound of each.
        list of float
            The upper bound of each.

        See Also
        --------
        BIYOS_getcons
        """
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
        funidx,
        sense=None,
        prio=None,
        coef=None,
        atol=None,
        rtol=None,
    ):
        """Add objectives after the last one, one for each function given.

        The objectives are optimized in stages, one for each priority, the
        lowest first. A stage minimizes the weighted sum of its objectives,
        each counted negatively when it is maximized, and keeps every
        objective of an earlier stage within its tolerances of the optimum
        found for it.

        Parameters
        ----------
        funidx : sequence of int
            The function each new objective optimizes.
        sense : sequence of Sense, optional
            Whether each is minimized or maximized, minimized by default.
        prio : sequence of int, optional
            The priority of each. By default each takes its own index, so
            the objectives are optimized one after another in the order
            they are added.
        coef : sequence of float, optional
            The weight of each in the sum of its stage, finite and
            positive, 1 by default.
        atol, rtol : sequence of float, optional
            How far each may move from its optimum in later stages, as a
            value of its function and as a fraction of the size of that
            optimum. Each is 0 or more, ``inf`` setting no limit, and 0 by
            default.

        Returns
        -------
        Objectives
            The new objectives.

        See Also
        --------
        BIYOS_addobjs
        """
        count = len(funidx)
        arrays = (
            indices_of(self, Functions, "funidx", funidx, count),
            *_fields(count, sense, prio, coef, atol, rtol),
        )
        first = c_int()
        self._call(lib.BIYOS_addobjs, count, *arrays, byref(first))
        return Objectives(self, first.value, count, self._gen)

    def setobjs(
        self,
        idx,
        sense=None,
        prio=None,
        coef=None,
        atol=None,
        rtol=None,
    ):
        """Set the senses, priorities, weights and tolerances of objectives.

        The functions are kept, and ``None`` leaves a field as it is.

        Parameters
        ----------
        idx
            The objectives, as indices, views or blocks.
        sense
            The new sense of each, a ``Sense``.
        prio
            The new priority of each.
        coef
            The new weight of each, finite and positive.
        atol, rtol
            The new absolute and relative tolerance of each.

        See Also
        --------
        BIYOS_setobjs
        """
        count = len(idx)
        arrays = (
            indices_of(self, Objectives, "idx", idx, count),
            *_fields(count, sense, prio, coef, atol, rtol),
        )
        self._call(lib.BIYOS_setobjs, count, *arrays)

    def setobjsense(self, idx, sense):
        """Set the senses of objectives.

        An objective may appear more than once; its entries apply in order.

        Parameters
        ----------
        idx
            The objectives, as indices, views or blocks.
        sense
            The new sense of each, a ``Sense``; ``None`` changes nothing.

        See Also
        --------
        BIYOS_setobjsense
        """
        n = len(idx)
        arrays = (
            indices_of(self, Objectives, "idx", idx, n),
            ints("sense", sense, n),
        )
        self._call(lib.BIYOS_setobjsense, n, *arrays)

    def getobjsense(self, idx):
        """Read the senses of objectives.

        Parameters
        ----------
        idx
            The objectives, as indices, views or blocks, in any order and
            any of them more than once.

        Returns
        -------
        list of Sense
            The sense of each.

        See Also
        --------
        BIYOS_getobjsense
        """
        found = self._getlist(lib.BIYOS_getobjsense, Objectives, idx, c_int)
        return [Sense(value) for value in found[0]]

    def setobjprio(self, idx, prio):
        """Set the priorities of objectives.

        An objective may appear more than once; its entries apply in order.

        Parameters
        ----------
        idx
            The objectives, as indices, views or blocks.
        prio
            The new priority of each; ``None`` changes nothing.

        See Also
        --------
        BIYOS_setobjprio
        """
        n = len(idx)
        arrays = (
            indices_of(self, Objectives, "idx", idx, n),
            ints("prio", prio, n),
        )
        self._call(lib.BIYOS_setobjprio, n, *arrays)

    def getobjprio(self, idx):
        """Read the priorities of objectives.

        Parameters
        ----------
        idx
            The objectives, as indices, views or blocks, in any order and
            any of them more than once.

        Returns
        -------
        list of int
            The priority of each.

        See Also
        --------
        BIYOS_getobjprio
        """
        found = self._getlist(lib.BIYOS_getobjprio, Objectives, idx, c_int)
        return found[0]

    def setobjcoef(self, idx, coef):
        """Set the weights of objectives.

        An objective may appear more than once; its entries apply in order.

        Parameters
        ----------
        idx
            The objectives, as indices, views or blocks.
        coef
            The new weight of each, finite and positive; ``None`` changes
            nothing.

        See Also
        --------
        BIYOS_setobjcoef
        """
        n = len(idx)
        arrays = (
            indices_of(self, Objectives, "idx", idx, n),
            doubles("coef", coef, n),
        )
        self._call(lib.BIYOS_setobjcoef, n, *arrays)

    def getobjcoef(self, idx):
        """Read the weights of objectives.

        Parameters
        ----------
        idx
            The objectives, as indices, views or blocks, in any order and
            any of them more than once.

        Returns
        -------
        list of float
            The weight of each.

        See Also
        --------
        BIYOS_getobjcoef
        """
        found = self._getlist(lib.BIYOS_getobjcoef, Objectives, idx, c_double)
        return found[0]

    def setobjtol(self, idx, atol=None, rtol=None):
        """Set the tolerances of objectives.

        A tolerance is how far an objective may move from its optimum in
        later stages, as a value of its function and as a fraction of the
        size of that optimum: 0 or more, ``inf`` setting no limit. An
        objective may appear more than once; its entries apply in order.

        Parameters
        ----------
        idx
            The objectives, as indices, views or blocks.
        atol, rtol
            The new absolute and relative tolerance of each; ``None``
            leaves that tolerance as it is.

        See Also
        --------
        BIYOS_setobjtol
        """
        n = len(idx)
        arrays = (
            indices_of(self, Objectives, "idx", idx, n),
            doubles("atol", atol, n),
            doubles("rtol", rtol, n),
        )
        self._call(lib.BIYOS_setobjtol, n, *arrays)

    def getobjtol(self, idx):
        """Read the tolerances of objectives.

        Parameters
        ----------
        idx
            The objectives, as indices, views or blocks, in any order and
            any of them more than once.

        Returns
        -------
        list of float
            The absolute tolerance of each.
        list of float
            The relative tolerance of each.

        See Also
        --------
        BIYOS_getobjtol
        """
        pair = (c_double, c_double)
        atol, rtol = self._getlist(lib.BIYOS_getobjtol, Objectives, idx, *pair)
        return atol, rtol

    def getnobjs(self):
        """Return the number of objectives.

        Returns
        -------
        int
            The number of objectives.

        See Also
        --------
        BIYOS_getnobjs
        """
        count = c_int()
        self._call(lib.BIYOS_getnobjs, byref(count))
        return count.value

    def getobjs(self, beg=0, n=None):
        """Read objectives.

        Parameters
        ----------
        beg
            The first objective to read.
        n
            The number of objectives to read, every one from ``beg`` on by
            default.

        Returns
        -------
        list of int
            The function each objective optimizes.
        list of Sense
            The sense of each.
        list of int
            The priority of each.
        list of float
            The weight of each.
        list of float
            The absolute tolerance of each.
        list of float
            The relative tolerance of each.

        See Also
        --------
        BIYOS_getobjs
        """
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

    def setmdlname(self, name):
        """Name the model.

        A name is a nonempty str. It may hold any character but the null
        character, and it is kept as UTF-8.

        Parameters
        ----------
        name : str
            The new name, which replaces the one the model has.

        Raises
        ------
        TypeError
            When ``name`` is not a str.
        ValueError
            When ``name`` holds a null character or cannot be encoded as
            UTF-8, such as a lone surrogate.
        Error
            When ``name`` is empty.

        See Also
        --------
        BIYOS_setmdlname
        """
        self._call(lib.BIYOS_setmdlname, text("name", name))

    def getmdlname(self):
        """Return the name of the model.

        Returns
        -------
        str or None
            The name, or None when the model has none.

        See Also
        --------
        BIYOS_getmdlname
        """
        name = c_char_p()
        self._call(lib.BIYOS_getmdlname, byref(name))
        return decoded(name.value)

    def delmdlname(self):
        """Remove the name of the model, which then has none.

        See Also
        --------
        BIYOS_delmdlname
        """
        self._call(lib.BIYOS_delmdlname)

    def setvarname(self, idx, name):
        """Name variables of the model.

        Variable ``idx[j]`` takes the name ``name[j]``, under the rules of
        ``setmdlname``. A variable listed more than once takes the last of
        its names. Names may repeat: several variables may hold one name,
        and ``getvaridx`` then refuses to choose among them. Functions,
        constraints and objectives are named the same way, each family
        apart.

        Parameters
        ----------
        idx : sequence of int
            The variables to name.
        name : sequence of str
            The new name of each, as many as ``idx``.

        Raises
        ------
        TypeError
            When ``name`` is a str rather than a sequence of them, or a
            name is not a str.
        ValueError
            When a name holds a null character or cannot be encoded as
            UTF-8, or when ``name`` and ``idx`` differ in length.
        Error
            When an index is not that of a variable or a name is empty.

        See Also
        --------
        BIYOS_setvarname
        """
        self._setname(lib.BIYOS_setvarname, Variables, idx, name)

    def getvarname(self, idx):
        """Return the names of variables.

        Parameters
        ----------
        idx : sequence of int
            The variables to read, in any order, any of them more than once.

        Returns
        -------
        list of str or None
            The name of each, or None for one that has none.

        See Also
        --------
        BIYOS_getvarname
        """
        return self._getname(lib.BIYOS_getvarname, Variables, idx)

    def getvaridx(self, name):
        """Return the indices of the variables with names.

        Parameters
        ----------
        name : sequence of str
            The names, each matched exactly.

        Returns
        -------
        list of int
            The index of the variable that alone holds each name.

        Raises
        ------
        TypeError
            When ``name`` is None, or a str rather than a sequence of them,
            or a name is not a str.
        ValueError
            When a name holds a null character or cannot be encoded as
            UTF-8.
        Error
            When no variable holds a name, or when several do.

        See Also
        --------
        BIYOS_getvaridx
        """
        return self._getidx(lib.BIYOS_getvaridx, name)

    def delvarname(self, idx):
        """Remove the names of variables, which then have none.

        Parameters
        ----------
        idx : sequence of int
            The variables that lose their names. One without a name, or
            listed more than once, is left without one.

        Raises
        ------
        Error
            When an index is not that of a variable, and then no name is
            removed.

        See Also
        --------
        BIYOS_delvarname
        """
        self._delname(lib.BIYOS_delvarname, Variables, idx)

    def setfunname(self, idx, name):
        """Name functions of the model, as ``setvarname`` names variables.

        Parameters
        ----------
        idx : sequence of int
            The functions to name.
        name : sequence of str
            The new name of each, as many as ``idx``.

        See Also
        --------
        BIYOS_setfunname
        """
        self._setname(lib.BIYOS_setfunname, Functions, idx, name)

    def getfunname(self, idx):
        """Return the names of functions, as ``getvarname`` does.

        Parameters
        ----------
        idx : sequence of int
            The functions to read.

        Returns
        -------
        list of str or None
            The name of each, or None for one that has none.

        See Also
        --------
        BIYOS_getfunname
        """
        return self._getname(lib.BIYOS_getfunname, Functions, idx)

    def getfunidx(self, name):
        """Return the indices of named functions, as ``getvaridx`` does.

        Parameters
        ----------
        name : sequence of str
            The names, each matched exactly.

        Returns
        -------
        list of int
            The index of the function that alone holds each name.

        See Also
        --------
        BIYOS_getfunidx
        """
        return self._getidx(lib.BIYOS_getfunidx, name)

    def delfunname(self, idx):
        """Remove the names of functions, as ``delvarname`` does.

        Parameters
        ----------
        idx : sequence of int
            The functions that lose their names.

        See Also
        --------
        BIYOS_delfunname
        """
        self._delname(lib.BIYOS_delfunname, Functions, idx)

    def setconname(self, idx, name):
        """Name constraints of the model, as ``setvarname`` names variables.

        Parameters
        ----------
        idx : sequence of int
            The constraints to name.
        name : sequence of str
            The new name of each, as many as ``idx``.

        See Also
        --------
        BIYOS_setconname
        """
        self._setname(lib.BIYOS_setconname, Constraints, idx, name)

    def getconname(self, idx):
        """Return the names of constraints, as ``getvarname`` does.

        Parameters
        ----------
        idx : sequence of int
            The constraints to read.

        Returns
        -------
        list of str or None
            The name of each, or None for one that has none.

        See Also
        --------
        BIYOS_getconname
        """
        return self._getname(lib.BIYOS_getconname, Constraints, idx)

    def getconidx(self, name):
        """Return the indices of named constraints, as ``getvaridx`` does.

        Parameters
        ----------
        name : sequence of str
            The names, each matched exactly.

        Returns
        -------
        list of int
            The index of the constraint that alone holds each name.

        See Also
        --------
        BIYOS_getconidx
        """
        return self._getidx(lib.BIYOS_getconidx, name)

    def delconname(self, idx):
        """Remove the names of constraints, as ``delvarname`` does.

        Parameters
        ----------
        idx : sequence of int
            The constraints that lose their names.

        See Also
        --------
        BIYOS_delconname
        """
        self._delname(lib.BIYOS_delconname, Constraints, idx)

    def setobjname(self, idx, name):
        """Name objectives of the model, as ``setvarname`` names variables.

        Parameters
        ----------
        idx : sequence of int
            The objectives to name.
        name : sequence of str
            The new name of each, as many as ``idx``.

        See Also
        --------
        BIYOS_setobjname
        """
        self._setname(lib.BIYOS_setobjname, Objectives, idx, name)

    def getobjname(self, idx):
        """Return the names of objectives, as ``getvarname`` does.

        Parameters
        ----------
        idx : sequence of int
            The objectives to read.

        Returns
        -------
        list of str or None
            The name of each, or None for one that has none.

        See Also
        --------
        BIYOS_getobjname
        """
        return self._getname(lib.BIYOS_getobjname, Objectives, idx)

    def getobjidx(self, name):
        """Return the indices of named objectives, as ``getvaridx`` does.

        Parameters
        ----------
        name : sequence of str
            The names, each matched exactly.

        Returns
        -------
        list of int
            The index of the objective that alone holds each name.

        See Also
        --------
        BIYOS_getobjidx
        """
        return self._getidx(lib.BIYOS_getobjidx, name)

    def delobjname(self, idx):
        """Remove the names of objectives, as ``delvarname`` does.

        Parameters
        ----------
        idx : sequence of int
            The objectives that lose their names.

        See Also
        --------
        BIYOS_delobjname
        """
        self._delname(lib.BIYOS_delobjname, Objectives, idx)

    def _start(self, env):
        self._env = env
        self._handle = c_void_p()
        self._calls = 0
        self._closing = False
        self._gen = 0

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
        self._make(source._call, lib.BIYOS_copymdl, target)

    def _hold(self):
        park = self._env._models.park
        self._free = weakref.finalize(self, park, self._handle.value)

    def _open(self):
        if self._closing:
            return False
        return self._free.alive and self._env._open()

    def _setname(self, function, family, idx, name):
        count = len(idx)
        arrays = (
            indices_of(self, family, "idx", idx, count),
            strings("name", name, count),
        )
        self._call(function, count, *arrays)

    def _delname(self, function, family, idx):
        count = len(idx)
        indices = indices_of(self, family, "idx", idx, count)
        self._call(function, count, indices)

    def _getname(self, function, family, idx):
        count = len(idx)
        indices = indices_of(self, family, "idx", idx, count)
        found = (c_char_p * count)()
        self._call(function, count, indices, found)
        return [decoded(value) for value in found]

    def _getlist(self, function, family, idx, *types):
        n = len(idx)
        given = indices_of(self, family, "idx", idx, n)
        found = [(element * n)() for element in types]
        self._call(function, n, given, *found)
        return [list(values) for values in found]

    def _getidx(self, function, name):
        count = len(listed("name", name))
        names = strings("name", name, count)
        found = (c_int * count)()
        self._call(function, count, names, found)
        return list(found)

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
