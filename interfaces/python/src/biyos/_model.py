import weakref
from collections.abc import Callable, Sequence
from ctypes import Array, byref, c_char_p, c_double, c_int, c_int64, c_void_p
from enum import IntEnum
from typing import Any, NoReturn, SupportsIndex

from biyos._arrays import (
    doubles,
    indices_of,
    ints,
    listed,
    offsets,
    span_of,
    strings,
)
from biyos._blocks import (
    Block,
    Constraint,
    Constraints,
    Function,
    Functions,
    Objective,
    Objectives,
    Variable,
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


_COUNTS: dict[type[Block[Any]], str] = {
    Variables: "getnvar",
    Functions: "getnfun",
    Constraints: "getncon",
    Objectives: "getnobj",
}


def _checked_env(env: Env) -> Env:
    if not isinstance(env, Env):
        found = type(env).__name__
        raise TypeError(f"env must be an Env, not {found}")
    return env


def _span(
    pos: int, n: int | None, size: Callable[[], int]
) -> tuple[int, bool]:
    given = None if n is None else integer("n", n)
    total = size()
    n = max(total - pos, 0) if given is None else given
    inside = 0 <= pos <= total and 0 <= n <= total - pos
    return n, inside


def _expanded(starts: Array[c_int64], first: int) -> list[int]:
    spans = zip(starts[:-1], starts[1:])
    counts = [end - start for start, end in spans]
    return [first + j for j, count in enumerate(counts) for _ in range(count)]


def _total(starts: Array[c_int64] | None, count: int) -> int:
    if starts is None or count < 0:
        return 0
    return starts[count]


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
    env
        The environment the model is made on.

    See Also
    --------
    BIYOS_newmdl
    """

    def __init__(self, env: Env) -> None:
        self._start(_checked_env(env))
        with self._env._busy():
            self._make(self._env._call, lib.BIYOS_newmdl)
        self._hold()

    def __enter__(self) -> "Model":
        return self

    def __exit__(self, *exc_info: object) -> None:
        self.close()

    def __copy__(self) -> NoReturn:
        raise TypeError("copy a model with Model.copy")

    def __deepcopy__(self, memo: dict[int, object]) -> NoReturn:
        self.__copy__()

    def close(self) -> None:
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

    def _close(self) -> None:
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

    def copy(self, env: Env | None = None) -> "Model":
        """Return a copy of the model, made on an environment.

        The copy holds the same variables, functions, constraints and
        objectives under the same indices and names, and changes apart from
        the model. It reports on its own environment, like any model made
        there.

        Parameters
        ----------
        env
            The environment the copy is made on, the model's own by default.

        Returns
        -------
        copy
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

    def addvar(
        self,
        n: int,
        knd: Sequence[VariableKind] | None = None,
        lb: Sequence[float] | None = None,
        ub: Sequence[float] | None = None,
    ) -> Variables:
        """Add variables after the last one.

        Parameters
        ----------
        n
            The number of variables to add.
        knd
            The kind of each, continuous by default.
        lb, ub
            The bounds of each, the widest of its kind by default.

        Returns
        -------
        vars
            The new variables.

        See Also
        --------
        BIYOS_addvar
        """
        n = integer("n", n)
        arrays = (
            ints("knd", knd, n),
            doubles("lb", lb, n),
            doubles("ub", ub, n),
        )
        first = c_int()
        self._call(lib.BIYOS_addvar, n, *arrays, byref(first))
        return Variables(self, first.value, n, self._gen)

    def setvarbnd(
        self,
        idx: Sequence[int | Variable] | Variables,
        lb: Sequence[float] | None = None,
        ub: Sequence[float] | None = None,
    ) -> None:
        """Set the bounds of variables.

        A variable may appear more than once; its entries apply in order.

        Parameters
        ----------
        idx
            The variables.
        lb, ub
            The new bounds of each; ``None`` leaves that side as it is.

        See Also
        --------
        BIYOS_setvarbndsp
        BIYOS_setvarbndrg
        """
        count = len(idx)
        chosen = self._chosen("BIYOS_setvarbnd", Variables, idx)
        arrays = (
            doubles("lb", lb, count),
            doubles("ub", ub, count),
        )
        self._call(*chosen, *arrays)

    def getvarbnd(
        self, idx: Sequence[int | Variable] | Variables
    ) -> tuple[list[float], list[float]]:
        """Read the bounds of variables.

        Parameters
        ----------
        idx
            The variables, in any order, any of them more than once.

        Returns
        -------
        lb
            The lower bound of each.
        ub
            The upper bound of each.

        See Also
        --------
        BIYOS_getvarbndsp
        BIYOS_getvarbndrg
        """
        bounds = (c_double, c_double)
        lb, ub = self._getlist("BIYOS_getvarbnd", Variables, idx, *bounds)
        return lb, ub

    def setvarknd(
        self,
        idx: Sequence[int | Variable] | Variables,
        knd: Sequence[VariableKind] | None,
    ) -> None:
        """Set the kinds of variables.

        Parameters
        ----------
        idx
            The variables.
        knd
            The new kind of each.

        See Also
        --------
        BIYOS_setvarkndsp
        BIYOS_setvarkndrg
        """
        count = len(idx)
        chosen = self._chosen("BIYOS_setvarknd", Variables, idx)
        arrays = (
            ints("knd", knd, count),
        )
        self._call(*chosen, *arrays)

    def getvarknd(
        self, idx: Sequence[int | Variable] | Variables
    ) -> list[VariableKind]:
        """Read the kinds of variables.

        Parameters
        ----------
        idx
            The variables, in any order, any of them more than once.

        Returns
        -------
        knd
            The kind of each.

        See Also
        --------
        BIYOS_getvarkndsp
        BIYOS_getvarkndrg
        """
        found = self._getlist("BIYOS_getvarknd", Variables, idx, c_int)
        return [VariableKind(value) for value in found[0]]

    def getnvar(self) -> int:
        """Return the number of variables.

        Returns
        -------
        n
            The number of variables.

        See Also
        --------
        BIYOS_getnvar
        """
        count = c_int()
        self._call(lib.BIYOS_getnvar, byref(count))
        return count.value

    def getvar(
        self, idx: Sequence[int | Variable] | Variables | None = None
    ) -> tuple[list[VariableKind], list[float], list[float]]:
        """Read the kinds and bounds of variables.

        Parameters
        ----------
        idx
            The variables, in any order, any of them more than once; every
            variable by default.

        Returns
        -------
        knd
            The kind of each variable.
        lb
            The lower bound of each.
        ub
            The upper bound of each.

        See Also
        --------
        BIYOS_getvarsp
        BIYOS_getvarrg
        """
        every = range(self.getnvar()) if idx is None else idx
        types = (c_int, c_double, c_double)
        found = self._getlist("BIYOS_getvar", Variables, every, *types)
        kinds, lower, upper = found
        return [VariableKind(kind) for kind in kinds], lower, upper

    def addfuns(
        self,
        n: int,
        cst: Sequence[float] | None = None,
        off: Sequence[int] | None = None,
        varidx: Sequence[int | Variable] | Variables | None = None,
        coef: Sequence[float] | None = None,
    ) -> Functions:
        """Add functions after the last one.

        Parameters
        ----------
        n
            The number of functions to add.
        cst
            The constant of each, 0 by default.
        off
            The ``n + 1`` offsets of the functions' linear terms in
            ``varidx`` and ``coef``, from 0 and never decreasing.
            Function ``j`` has the terms from ``off[j]`` to
            ``off[j + 1] - 1``. By default the functions have no terms.
        varidx
            The variable of each term, ``off[n]`` of them.
        coef
            The coefficient of each term, ``off[n]`` of them.

        Returns
        -------
        funs
            The new functions.

        See Also
        --------
        BIYOS_addfuns
        """
        n = integer("n", n)
        starts = offsets("off", off, n + 1)
        size = _total(starts, n)
        arrays = (
            doubles("cst", cst, n),
            starts,
            indices_of(self, Variables, "varidx", varidx, size),
            doubles("coef", coef, size),
        )
        first = c_int()
        self._call(lib.BIYOS_addfuns, n, *arrays, byref(first))
        return Functions(self, first.value, n, self._gen)

    def getnfun(self) -> int:
        """Return the number of functions.

        Returns
        -------
        n
            The number of functions.

        See Also
        --------
        BIYOS_getnfun
        """
        count = c_int()
        self._call(lib.BIYOS_getnfun, byref(count))
        return count.value

    def getfuns(
        self, pos: int | Function = 0, n: int | None = None
    ) -> tuple[list[float], list[int], list[int], list[float]]:
        """Read the constants and linear terms of functions.

        Parameters
        ----------
        pos
            The first function to read.
        n
            The number of functions to read, every one from ``pos`` on by
            default.

        Returns
        -------
        cst
            The constant of each function.
        off
            The ``n + 1`` offsets of the functions' terms in the two lists
            after it, from 0.
        varidx
            The variable of each term.
        coef
            The coefficient of each term.

        See Also
        --------
        BIYOS_getfuns
        """
        pos = index_of(self, Functions, "pos", pos)
        n, inside = _span(pos, n, self.getnfun)
        needed = c_int64()
        if not inside:
            empty = (0, None, None, byref(needed), None, None)
            self._call(lib.BIYOS_getfuns, pos, n, *empty)
        constants = (c_double * n)()
        starts = (c_int64 * (n + 1))()
        learn = (0, constants, starts, byref(needed), None, None)
        self._call(lib.BIYOS_getfuns, pos, n, *learn)
        size = needed.value
        variables = (c_int * size)()
        values = (c_double * size)()
        fill = (size, None, None, byref(needed), variables, values)
        self._call(lib.BIYOS_getfuns, pos, n, *fill)
        terms = (list(starts), list(variables), list(values))
        return (list(constants), *terms)

    def addcst(
        self,
        idx: Sequence[int | Function] | Functions,
        cst: Sequence[float] | None,
    ) -> None:
        """Add to the constants of functions.

        A function may appear more than once; its entries add up, in order.

        Parameters
        ----------
        idx
            The functions.
        cst
            The value each entry adds to its function's constant, finite;
            ``None`` changes nothing. A constant that would leave the finite
            range refuses the call.

        See Also
        --------
        BIYOS_addcstsp
        BIYOS_addcstrg
        """
        n = len(idx)
        chosen = self._chosen("BIYOS_addcst", Functions, idx)
        self._call(*chosen, doubles("cst", cst, n))

    def setcst(
        self,
        idx: Sequence[int | Function] | Functions,
        cst: Sequence[float] | None,
    ) -> None:
        """Set the constants of functions.

        A function may appear more than once; its entries apply in order.

        Parameters
        ----------
        idx
            The functions.
        cst
            The new constant of each, finite; ``None`` changes nothing.

        See Also
        --------
        BIYOS_setcstsp
        BIYOS_setcstrg
        """
        n = len(idx)
        chosen = self._chosen("BIYOS_setcst", Functions, idx)
        self._call(*chosen, doubles("cst", cst, n))

    def getcst(
        self, idx: Sequence[int | Function] | Functions
    ) -> list[float]:
        """Read the constants of functions.

        Parameters
        ----------
        idx
            The functions, in any order, any of them more than once.

        Returns
        -------
        cst
            The constant of each.

        See Also
        --------
        BIYOS_getcstsp
        BIYOS_getcstrg
        """
        found = self._getlist("BIYOS_getcst", Functions, idx, c_double)
        return found[0]

    def addlinterms(
        self,
        funidx: Sequence[int | Function] | Functions,
        off: Sequence[int],
        varidx: Sequence[int | Variable] | Variables,
        coef: Sequence[float],
    ) -> None:
        """Add linear terms to functions of the model.

        Function ``funidx[j]`` gains the terms from ``off[j]`` to
        ``off[j + 1] - 1``. Term ``k`` adds ``coef[k]`` times variable
        ``varidx[k]``. A variable the function holds already adds its
        coefficient to the one held.

        Parameters
        ----------
        funidx
            The functions that gain terms, each listed once.
        off
            The ``len(funidx) + 1`` offsets of the functions' terms in
            ``varidx`` and ``coef``, from 0 and never decreasing.
        varidx
            The variable of each term, ``off[-1]`` of them, each once for
            its function.
        coef
            The coefficient of each term, ``off[-1]`` of them.

        See Also
        --------
        BIYOS_addlinterms
        """
        n = len(funidx)
        starts = offsets("off", off, n + 1)
        size = _total(starts, n)
        arrays = (
            indices_of(self, Functions, "funidx", funidx, n),
            starts,
            indices_of(self, Variables, "varidx", varidx, size),
            doubles("coef", coef, size),
        )
        self._call(lib.BIYOS_addlinterms, n, *arrays)

    def addlinterm(
        self,
        funidx: Sequence[int | Function] | Functions,
        varidx: Sequence[int | Variable] | Variables,
        coef: Sequence[float],
    ) -> None:
        """Add linear terms to functions, as coordinates.

        Term ``k`` adds ``coef[k]`` times variable ``varidx[k]`` to
        function ``funidx[k]``, in any order. A variable the function holds
        already adds its coefficient to the one held.

        Parameters
        ----------
        funidx
            The function of each term.
        varidx
            The variable of each term, each once for its function.
        coef
            The coefficient of each term.

        See Also
        --------
        BIYOS_addlintermsp
        """
        size = len(funidx)
        arrays = (
            indices_of(self, Functions, "funidx", funidx, size),
            indices_of(self, Variables, "varidx", varidx, size),
            doubles("coef", coef, size),
        )
        self._call(lib.BIYOS_addlintermsp, size, *arrays)

    def getnrlinterm(
        self, idx: Sequence[int | Function] | Functions | None = None
    ) -> int:
        """Count the linear terms of functions.

        Parameters
        ----------
        idx
            The functions, in any order, any of them more than once; every
            function by default.

        Returns
        -------
        nnz
            The number of linear terms they hold, a function counted each
            time it appears.

        See Also
        --------
        BIYOS_getnrlintermsp
        BIYOS_getnrlintermrg
        """
        every = range(self.getnfun()) if idx is None else idx
        return self._count("BIYOS_getnrlinterm", Functions, every)

    def getrlinterm(
        self, idx: Sequence[int | Function] | Functions | None = None
    ) -> tuple[list[int], list[int], list[float]]:
        """Read the linear terms of functions, by function.

        Parameters
        ----------
        idx
            The functions, in any order, any of them more than once; every
            function by default.

        Returns
        -------
        funidx
            The function of each term, the terms of ``idx[0]`` first.
        varidx
            The variable of each term, each function's sorted by variable.
        coef
            The coefficient of each term.

        See Also
        --------
        BIYOS_getrlintermsp
        BIYOS_getrlintermrg
        """
        every = range(self.getnfun()) if idx is None else idx
        found, values = self._terms("rlinterm", Functions, every, 0)
        return found[0], found[1], values

    def getnclinterm(
        self, idx: Sequence[int | Variable] | Variables | None = None
    ) -> int:
        """Count the linear terms of variables.

        Parameters
        ----------
        idx
            The variables, in any order, any of them more than once; every
            variable by default.

        Returns
        -------
        nnz
            The number of linear terms that hold them, a variable counted
            each time it appears.

        See Also
        --------
        BIYOS_getnclintermsp
        BIYOS_getnclintermrg
        """
        every = range(self.getnvar()) if idx is None else idx
        return self._count("BIYOS_getnclinterm", Variables, every)

    def getclinterm(
        self, idx: Sequence[int | Variable] | Variables | None = None
    ) -> tuple[list[int], list[int], list[float]]:
        """Read the linear terms of variables, by variable.

        Parameters
        ----------
        idx
            The variables, in any order, any of them more than once; every
            variable by default.

        Returns
        -------
        funidx
            The function of each term, each variable's sorted by function.
        varidx
            The variable of each term, the terms of ``idx[0]`` first.
        coef
            The coefficient of each term.

        See Also
        --------
        BIYOS_getclintermsp
        BIYOS_getclintermrg
        """
        every = range(self.getnvar()) if idx is None else idx
        found, values = self._terms("clinterm", Variables, every, 1)
        return found[0], found[1], values

    def addquadterms(
        self,
        funidx: Sequence[int | Function] | Functions,
        off: Sequence[int],
        varidx1: Sequence[int | Variable] | Variables,
        varidx2: Sequence[int | Variable] | Variables,
        coef: Sequence[float],
    ) -> None:
        """Add quadratic terms to functions of the model.

        Function ``funidx[j]`` gains the terms from ``off[j]`` to
        ``off[j + 1] - 1``. Term ``k`` adds ``coef[k]`` times the
        product of variables ``varidx1[k]`` and ``varidx2[k]``, with no factor
        1/2. The two variables of a pair may come in either order. A pair
        the function holds already adds its coefficient to the one held.

        Parameters
        ----------
        funidx
            The functions that gain terms, each listed once.
        off
            The ``len(funidx) + 1`` offsets of the functions' terms in
            ``varidx1``, ``varidx2`` and ``coef``, from 0 and never
            decreasing.
        varidx1, varidx2
            The two variables of each term, ``off[-1]`` of each.
        coef
            The coefficient of each term, ``off[-1]`` of them.

        See Also
        --------
        BIYOS_addquadterms
        """
        count = len(funidx)
        starts = offsets("off", off, count + 1)
        size = _total(starts, count)
        arrays = (
            indices_of(self, Functions, "funidx", funidx, count),
            starts,
            indices_of(self, Variables, "varidx1", varidx1, size),
            indices_of(self, Variables, "varidx2", varidx2, size),
            doubles("coef", coef, size),
        )
        self._call(lib.BIYOS_addquadterms, count, *arrays)

    def addquadterm(
        self,
        funidx: Sequence[int | Function] | Functions,
        varidx1: Sequence[int | Variable] | Variables,
        varidx2: Sequence[int | Variable] | Variables,
        coef: Sequence[float],
    ) -> None:
        """Add quadratic terms to functions, as coordinates.

        Term ``k`` adds ``coef[k]`` times the product of variables
        ``varidx1[k]`` and ``varidx2[k]`` to function ``funidx[k]``, with
        no factor 1/2, in any order. The two variables of a pair may come
        in either order. A pair the function holds already adds its
        coefficient to the one held.

        Parameters
        ----------
        funidx
            The function of each term.
        varidx1, varidx2
            The two variables of each term, each pair once for its
            function.
        coef
            The coefficient of each term.

        See Also
        --------
        BIYOS_addquadtermsp
        """
        size = len(funidx)
        arrays = (
            indices_of(self, Functions, "funidx", funidx, size),
            indices_of(self, Variables, "varidx1", varidx1, size),
            indices_of(self, Variables, "varidx2", varidx2, size),
            doubles("coef", coef, size),
        )
        self._call(lib.BIYOS_addquadtermsp, size, *arrays)

    def getnquadterm(
        self, idx: Sequence[int | Function] | Functions | None = None
    ) -> int:
        """Count the quadratic terms of functions.

        Parameters
        ----------
        idx
            The functions, in any order, any of them more than once; every
            function by default.

        Returns
        -------
        nnz
            The number of quadratic terms they hold, a function counted each
            time it appears.

        See Also
        --------
        BIYOS_getnquadtermsp
        BIYOS_getnquadtermrg
        """
        every = range(self.getnfun()) if idx is None else idx
        return self._count("BIYOS_getnquadterm", Functions, every)

    def getquadterm(
        self, idx: Sequence[int | Function] | Functions | None = None
    ) -> tuple[list[int], list[int], list[int], list[float]]:
        """Read the quadratic terms of functions.

        Parameters
        ----------
        idx
            The functions, in any order, any of them more than once; every
            function by default.

        Returns
        -------
        funidx
            The function of each term, the terms of ``idx[0]`` first.
        varidx1
            The first variable of each term, never above the second, each
            function's terms sorted by pair.
        varidx2
            The second variable of each term.
        coef
            The coefficient of each term.

        See Also
        --------
        BIYOS_getquadtermsp
        BIYOS_getquadtermrg
        """
        every = range(self.getnfun()) if idx is None else idx
        found, values = self._terms("quadterm", Functions, every, 0, 3)
        functions, firsts, seconds = found
        return functions, firsts, seconds, values

    def addcon(
        self,
        funidx: Sequence[int | Function] | Functions,
        lb: Sequence[float] | None = None,
        ub: Sequence[float] | None = None,
    ) -> Constraints:
        """Add constraints after the last one, one for each function given.

        Parameters
        ----------
        funidx
            The function each new constraint bounds.
        lb, ub
            The bounds of each, unbounded by default.

        Returns
        -------
        cons
            The new constraints.

        See Also
        --------
        BIYOS_addcon
        """
        count = len(funidx)
        arrays = (
            indices_of(self, Functions, "funidx", funidx, count),
            doubles("lb", lb, count),
            doubles("ub", ub, count),
        )
        first = c_int()
        self._call(lib.BIYOS_addcon, count, *arrays, byref(first))
        return Constraints(self, first.value, count, self._gen)

    def setconbnd(
        self,
        idx: Sequence[int | Constraint] | Constraints,
        lb: Sequence[float] | None = None,
        ub: Sequence[float] | None = None,
    ) -> None:
        """Set the bounds of constraints and keep their functions.

        Parameters
        ----------
        idx
            The constraints.
        lb, ub
            The new bounds of each; ``None`` leaves that side as it is.

        See Also
        --------
        BIYOS_setconbndsp
        BIYOS_setconbndrg
        """
        count = len(idx)
        chosen = self._chosen("BIYOS_setconbnd", Constraints, idx)
        arrays = (
            doubles("lb", lb, count),
            doubles("ub", ub, count),
        )
        self._call(*chosen, *arrays)

    def getconbnd(
        self, idx: Sequence[int | Constraint] | Constraints
    ) -> tuple[list[float], list[float]]:
        """Read the bounds of constraints.

        Parameters
        ----------
        idx
            The constraints, in any order, any of them more than once.

        Returns
        -------
        lb
            The lower bound of each.
        ub
            The upper bound of each.

        See Also
        --------
        BIYOS_getconbndsp
        BIYOS_getconbndrg
        """
        bounds = (c_double, c_double)
        lb, ub = self._getlist("BIYOS_getconbnd", Constraints, idx, *bounds)
        return lb, ub

    def getncon(self) -> int:
        """Return the number of constraints.

        Returns
        -------
        n
            The number of constraints.

        See Also
        --------
        BIYOS_getncon
        """
        count = c_int()
        self._call(lib.BIYOS_getncon, byref(count))
        return count.value

    def getcon(
        self, idx: Sequence[int | Constraint] | Constraints | None = None
    ) -> tuple[list[int], list[float], list[float]]:
        """Read the functions and bounds of constraints.

        Parameters
        ----------
        idx
            The constraints, in any order, any of them more than once; every
            constraint by default.

        Returns
        -------
        funidx
            The function each constraint bounds.
        lb
            The lower bound of each.
        ub
            The upper bound of each.

        See Also
        --------
        BIYOS_getconsp
        BIYOS_getconrg
        """
        every = range(self.getncon()) if idx is None else idx
        types = (c_int, c_double, c_double)
        found = self._getlist("BIYOS_getcon", Constraints, every, *types)
        functions, lower, upper = found
        return functions, lower, upper

    def addobj(
        self,
        funidx: Sequence[int | Function] | Functions,
        sense: Sequence[Sense] | None = None,
        prio: Sequence[int] | None = None,
        coef: Sequence[float] | None = None,
    ) -> Objectives:
        """Add objectives after the last one, one for each function given.

        The objectives are optimized in stages, one for each priority, the
        lowest first. A stage minimizes the weighted sum of its objectives,
        each counted negatively when it is maximized, and keeps every
        objective of an earlier stage within its tolerances of the optimum
        found for it. A new objective's tolerances are 0 until ``setobjtol``
        sets them.

        Parameters
        ----------
        funidx
            The function each new objective optimizes.
        sense
            Whether each is minimized or maximized, minimized by default.
        prio
            The priority of each. By default each takes its own index, so
            the objectives are optimized one after another in the order
            they are added.
        coef
            The weight of each in the sum of its stage, finite and
            positive, 1 by default.

        Returns
        -------
        objs
            The new objectives.

        See Also
        --------
        BIYOS_addobj
        """
        count = len(funidx)
        arrays = (
            indices_of(self, Functions, "funidx", funidx, count),
            ints("sense", sense, count),
            ints("prio", prio, count),
            doubles("coef", coef, count),
        )
        first = c_int()
        self._call(lib.BIYOS_addobj, count, *arrays, byref(first))
        return Objectives(self, first.value, count, self._gen)

    def setobjsense(
        self,
        idx: Sequence[int | Objective] | Objectives,
        sense: Sequence[Sense] | None,
    ) -> None:
        """Set the senses of objectives.

        An objective may appear more than once; its entries apply in order.

        Parameters
        ----------
        idx
            The objectives.
        sense
            The new sense of each; ``None`` changes nothing.

        See Also
        --------
        BIYOS_setobjsensesp
        BIYOS_setobjsenserg
        """
        n = len(idx)
        chosen = self._chosen("BIYOS_setobjsense", Objectives, idx)
        arrays = (
            ints("sense", sense, n),
        )
        self._call(*chosen, *arrays)

    def getobjsense(
        self, idx: Sequence[int | Objective] | Objectives
    ) -> list[Sense]:
        """Read the senses of objectives.

        Parameters
        ----------
        idx
            The objectives, in any order, any of them more than once.

        Returns
        -------
        sense
            The sense of each.

        See Also
        --------
        BIYOS_getobjsensesp
        BIYOS_getobjsenserg
        """
        found = self._getlist("BIYOS_getobjsense", Objectives, idx, c_int)
        return [Sense(value) for value in found[0]]

    def setobjprio(
        self,
        idx: Sequence[int | Objective] | Objectives,
        prio: Sequence[int] | None,
    ) -> None:
        """Set the priorities of objectives.

        An objective may appear more than once; its entries apply in order.

        Parameters
        ----------
        idx
            The objectives.
        prio
            The new priority of each; ``None`` changes nothing.

        See Also
        --------
        BIYOS_setobjpriosp
        BIYOS_setobjpriorg
        """
        n = len(idx)
        chosen = self._chosen("BIYOS_setobjprio", Objectives, idx)
        arrays = (
            ints("prio", prio, n),
        )
        self._call(*chosen, *arrays)

    def getobjprio(
        self, idx: Sequence[int | Objective] | Objectives
    ) -> list[int]:
        """Read the priorities of objectives.

        Parameters
        ----------
        idx
            The objectives, in any order, any of them more than once.

        Returns
        -------
        prio
            The priority of each.

        See Also
        --------
        BIYOS_getobjpriosp
        BIYOS_getobjpriorg
        """
        found = self._getlist("BIYOS_getobjprio", Objectives, idx, c_int)
        return found[0]

    def setobjcoef(
        self,
        idx: Sequence[int | Objective] | Objectives,
        coef: Sequence[float] | None,
    ) -> None:
        """Set the weights of objectives.

        An objective may appear more than once; its entries apply in order.

        Parameters
        ----------
        idx
            The objectives.
        coef
            The new weight of each, finite and positive; ``None`` changes
            nothing.

        See Also
        --------
        BIYOS_setobjcoefsp
        BIYOS_setobjcoefrg
        """
        n = len(idx)
        chosen = self._chosen("BIYOS_setobjcoef", Objectives, idx)
        arrays = (
            doubles("coef", coef, n),
        )
        self._call(*chosen, *arrays)

    def getobjcoef(
        self, idx: Sequence[int | Objective] | Objectives
    ) -> list[float]:
        """Read the weights of objectives.

        Parameters
        ----------
        idx
            The objectives, in any order, any of them more than once.

        Returns
        -------
        coef
            The weight of each.

        See Also
        --------
        BIYOS_getobjcoefsp
        BIYOS_getobjcoefrg
        """
        found = self._getlist("BIYOS_getobjcoef", Objectives, idx, c_double)
        return found[0]

    def setobjtol(
        self,
        idx: Sequence[int | Objective] | Objectives,
        atol: Sequence[float] | None = None,
        rtol: Sequence[float] | None = None,
    ) -> None:
        """Set the tolerances of objectives.

        A tolerance is how far an objective may move from its optimum in
        later stages, as a value of its function and as a fraction of the
        size of that optimum: 0 or more, ``inf`` setting no limit. An
        objective may appear more than once; its entries apply in order.

        Parameters
        ----------
        idx
            The objectives.
        atol, rtol
            The new absolute and relative tolerance of each; ``None``
            leaves that tolerance as it is.

        See Also
        --------
        BIYOS_setobjtolsp
        BIYOS_setobjtolrg
        """
        n = len(idx)
        chosen = self._chosen("BIYOS_setobjtol", Objectives, idx)
        arrays = (
            doubles("atol", atol, n),
            doubles("rtol", rtol, n),
        )
        self._call(*chosen, *arrays)

    def getobjtol(
        self, idx: Sequence[int | Objective] | Objectives
    ) -> tuple[list[float], list[float]]:
        """Read the tolerances of objectives.

        Parameters
        ----------
        idx
            The objectives, in any order, any of them more than once.

        Returns
        -------
        atol
            The absolute tolerance of each.
        rtol
            The relative tolerance of each.

        See Also
        --------
        BIYOS_getobjtolsp
        BIYOS_getobjtolrg
        """
        pair = (c_double, c_double)
        atol, rtol = self._getlist("BIYOS_getobjtol", Objectives, idx, *pair)
        return atol, rtol

    def getnobj(self) -> int:
        """Return the number of objectives.

        Returns
        -------
        n
            The number of objectives.

        See Also
        --------
        BIYOS_getnobj
        """
        count = c_int()
        self._call(lib.BIYOS_getnobj, byref(count))
        return count.value

    def getobj(
        self, idx: Sequence[int | Objective] | Objectives | None = None
    ) -> tuple[list[int], list[Sense], list[int], list[float]]:
        """Read objectives.

        Parameters
        ----------
        idx
            The objectives, in any order, any of them more than once; every
            objective by default.

        Returns
        -------
        funidx
            The function each objective optimizes.
        sense
            The sense of each.
        prio
            The priority of each.
        coef
            The weight of each.

        See Also
        --------
        BIYOS_getobjsp
        BIYOS_getobjrg
        """
        every = range(self.getnobj()) if idx is None else idx
        types = (c_int, c_int, c_int, c_double)
        found = self._getlist("BIYOS_getobj", Objectives, every, *types)
        functions, codes, priorities, weights = found
        senses = [Sense(code) for code in codes]
        return functions, senses, priorities, weights

    def setmdlname(self, name: str) -> None:
        """Name the model.

        A name is a nonempty str. It may hold any character but the null
        character, and it is kept as UTF-8.

        Parameters
        ----------
        name
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

    def getmdlname(self) -> str | None:
        """Return the name of the model.

        Returns
        -------
        name
            The name, or None when the model has none.

        See Also
        --------
        BIYOS_getmdlname
        """
        name = c_char_p()
        self._call(lib.BIYOS_getmdlname, byref(name))
        return decoded(name.value)

    def delmdlname(self) -> None:
        """Remove the name of the model, which then has none.

        See Also
        --------
        BIYOS_delmdlname
        """
        self._call(lib.BIYOS_delmdlname)

    def setvarname(
        self, idx: Sequence[int | Variable] | Variables, name: Sequence[str]
    ) -> None:
        """Name variables of the model.

        Variable ``idx[j]`` takes the name ``name[j]``, under the rules of
        ``setmdlname``. A variable listed more than once takes the last of
        its names. Names may repeat: several variables may hold one name,
        and ``getvaridx`` then refuses to choose among them. Functions,
        constraints and objectives are named the same way, each family
        apart.

        Parameters
        ----------
        idx
            The variables to name.
        name
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
        BIYOS_setvarnamesp
        BIYOS_setvarnamerg
        """
        self._setname("BIYOS_setvarname", Variables, idx, name)

    def getvarname(
        self, idx: Sequence[int | Variable] | Variables
    ) -> list[str | None]:
        """Return the names of variables.

        Parameters
        ----------
        idx
            The variables to read, in any order, any of them more than once.

        Returns
        -------
        name
            The name of each, or None for one that has none.

        See Also
        --------
        BIYOS_getvarnamesp
        BIYOS_getvarnamerg
        """
        return self._getname("BIYOS_getvarname", Variables, idx)

    def getvaridx(self, name: Sequence[str]) -> list[int]:
        """Return the indices of the variables with names.

        Parameters
        ----------
        name
            The names, each matched exactly.

        Returns
        -------
        idx
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

    def delvarname(self, idx: Sequence[int | Variable] | Variables) -> None:
        """Remove the names of variables, which then have none.

        Parameters
        ----------
        idx
            The variables that lose their names. One without a name, or
            listed more than once, is left without one.

        Raises
        ------
        Error
            When an index is not that of a variable, and then no name is
            removed.

        See Also
        --------
        BIYOS_delvarnamesp
        BIYOS_delvarnamerg
        """
        self._delname("BIYOS_delvarname", Variables, idx)

    def setfunname(
        self, idx: Sequence[int | Function] | Functions, name: Sequence[str]
    ) -> None:
        """Name functions of the model, as ``setvarname`` names variables.

        Parameters
        ----------
        idx
            The functions to name.
        name
            The new name of each, as many as ``idx``.

        See Also
        --------
        BIYOS_setfunnamesp
        BIYOS_setfunnamerg
        """
        self._setname("BIYOS_setfunname", Functions, idx, name)

    def getfunname(
        self, idx: Sequence[int | Function] | Functions
    ) -> list[str | None]:
        """Return the names of functions, as ``getvarname`` does.

        Parameters
        ----------
        idx
            The functions to read.

        Returns
        -------
        name
            The name of each, or None for one that has none.

        See Also
        --------
        BIYOS_getfunnamesp
        BIYOS_getfunnamerg
        """
        return self._getname("BIYOS_getfunname", Functions, idx)

    def getfunidx(self, name: Sequence[str]) -> list[int]:
        """Return the indices of named functions, as ``getvaridx`` does.

        Parameters
        ----------
        name
            The names, each matched exactly.

        Returns
        -------
        idx
            The index of the function that alone holds each name.

        See Also
        --------
        BIYOS_getfunidx
        """
        return self._getidx(lib.BIYOS_getfunidx, name)

    def delfunname(self, idx: Sequence[int | Function] | Functions) -> None:
        """Remove the names of functions, as ``delvarname`` does.

        Parameters
        ----------
        idx
            The functions that lose their names.

        See Also
        --------
        BIYOS_delfunnamesp
        BIYOS_delfunnamerg
        """
        self._delname("BIYOS_delfunname", Functions, idx)

    def setconname(
        self,
        idx: Sequence[int | Constraint] | Constraints,
        name: Sequence[str],
    ) -> None:
        """Name constraints of the model, as ``setvarname`` names variables.

        Parameters
        ----------
        idx
            The constraints to name.
        name
            The new name of each, as many as ``idx``.

        See Also
        --------
        BIYOS_setconnamesp
        BIYOS_setconnamerg
        """
        self._setname("BIYOS_setconname", Constraints, idx, name)

    def getconname(
        self, idx: Sequence[int | Constraint] | Constraints
    ) -> list[str | None]:
        """Return the names of constraints, as ``getvarname`` does.

        Parameters
        ----------
        idx
            The constraints to read.

        Returns
        -------
        name
            The name of each, or None for one that has none.

        See Also
        --------
        BIYOS_getconnamesp
        BIYOS_getconnamerg
        """
        return self._getname("BIYOS_getconname", Constraints, idx)

    def getconidx(self, name: Sequence[str]) -> list[int]:
        """Return the indices of named constraints, as ``getvaridx`` does.

        Parameters
        ----------
        name
            The names, each matched exactly.

        Returns
        -------
        idx
            The index of the constraint that alone holds each name.

        See Also
        --------
        BIYOS_getconidx
        """
        return self._getidx(lib.BIYOS_getconidx, name)

    def delconname(
        self, idx: Sequence[int | Constraint] | Constraints
    ) -> None:
        """Remove the names of constraints, as ``delvarname`` does.

        Parameters
        ----------
        idx
            The constraints that lose their names.

        See Also
        --------
        BIYOS_delconnamesp
        BIYOS_delconnamerg
        """
        self._delname("BIYOS_delconname", Constraints, idx)

    def setobjname(
        self, idx: Sequence[int | Objective] | Objectives, name: Sequence[str]
    ) -> None:
        """Name objectives of the model, as ``setvarname`` names variables.

        Parameters
        ----------
        idx
            The objectives to name.
        name
            The new name of each, as many as ``idx``.

        See Also
        --------
        BIYOS_setobjnamesp
        BIYOS_setobjnamerg
        """
        self._setname("BIYOS_setobjname", Objectives, idx, name)

    def getobjname(
        self, idx: Sequence[int | Objective] | Objectives
    ) -> list[str | None]:
        """Return the names of objectives, as ``getvarname`` does.

        Parameters
        ----------
        idx
            The objectives to read.

        Returns
        -------
        name
            The name of each, or None for one that has none.

        See Also
        --------
        BIYOS_getobjnamesp
        BIYOS_getobjnamerg
        """
        return self._getname("BIYOS_getobjname", Objectives, idx)

    def getobjidx(self, name: Sequence[str]) -> list[int]:
        """Return the indices of named objectives, as ``getvaridx`` does.

        Parameters
        ----------
        name
            The names, each matched exactly.

        Returns
        -------
        idx
            The index of the objective that alone holds each name.

        See Also
        --------
        BIYOS_getobjidx
        """
        return self._getidx(lib.BIYOS_getobjidx, name)

    def delobjname(self, idx: Sequence[int | Objective] | Objectives) -> None:
        """Remove the names of objectives, as ``delvarname`` does.

        Parameters
        ----------
        idx
            The objectives that lose their names.

        See Also
        --------
        BIYOS_delobjnamesp
        BIYOS_delobjnamerg
        """
        self._delname("BIYOS_delobjname", Objectives, idx)

    def _start(self, env: Env) -> None:
        self._env = env
        self._handle = c_void_p()
        self._calls = 0
        self._closing = False
        self._gen = 0

    def _make(self, call: Callable[..., None], *arguments: object) -> None:
        handle = self._handle
        try:
            call(*arguments, byref(handle))
        finally:
            if handle.value is not None:
                self._env._models.add(handle.value)

    def _copy(self, source: "Model") -> None:
        if not self._env._open():
            raise ValueError("the environment is closed")
        target = self._env._handle
        self._make(source._call, lib.BIYOS_copymdl, target)

    def _hold(self) -> None:
        park = self._env._models.park
        self._free = weakref.finalize(self, park, self._handle.value)

    def _open(self) -> bool:
        if self._closing:
            return False
        return self._free.alive and self._env._open()

    def _chosen(
        self,
        call: str,
        family: type[Block[Any]],
        idx: Sequence[SupportsIndex] | Block[Any],
        outputs: int = 0,
    ) -> tuple[Callable[..., int], int, object]:
        span = span_of(self, family, "idx", idx)
        if span is None:
            n = len(idx)
            indices = indices_of(self, family, "idx", idx, n)
            return getattr(lib, f"{call}sp"), n, indices
        pos = integer("the start of idx", span.start)
        n = integer("idx", len(span))
        function = getattr(lib, f"{call}rg")
        if outputs > 0 and not self._inside(family, span):
            self._call(function, pos, n, *([None] * outputs))
        return function, pos, n

    def _inside(self, family: type[Block[Any]], span: range) -> bool:
        total: int = getattr(self, _COUNTS[family])()
        return 0 <= span.start <= total and len(span) <= total - span.start

    def _setname(
        self,
        call: str,
        family: type[Block[Any]],
        idx: Sequence[SupportsIndex] | Block[Any],
        name: Sequence[str],
    ) -> None:
        chosen = self._chosen(call, family, idx)
        names = strings("name", name, len(idx))
        self._call(*chosen, names)

    def _delname(
        self,
        call: str,
        family: type[Block[Any]],
        idx: Sequence[SupportsIndex] | Block[Any],
    ) -> None:
        self._call(*self._chosen(call, family, idx))

    def _getname(
        self,
        call: str,
        family: type[Block[Any]],
        idx: Sequence[SupportsIndex] | Block[Any],
    ) -> list[str | None]:
        chosen = self._chosen(call, family, idx, 1)
        found = (c_char_p * len(idx))()
        self._call(*chosen, found)
        return [decoded(value) for value in found]

    def _getlist(
        self,
        call: str,
        family: type[Block[Any]],
        idx: Sequence[SupportsIndex] | Block[Any],
        *types: type[c_int | c_double],
    ) -> list[list[Any]]:
        chosen = self._chosen(call, family, idx, len(types))
        found = [(element * len(idx))() for element in types]
        self._call(*chosen, *found)
        return [list(values) for values in found]

    def _count(
        self,
        call: str,
        family: type[Block[Any]],
        idx: Sequence[SupportsIndex] | Block[Any],
    ) -> int:
        nnz = c_int64()
        self._call(*self._chosen(call, family, idx), byref(nnz))
        return nnz.value

    def _terms(
        self,
        name: str,
        family: type[Block[Any]],
        idx: Sequence[SupportsIndex] | Block[Any],
        place: int,
        width: int = 2,
    ) -> tuple[list[list[int]], list[float]]:
        size = self._count(f"BIYOS_getn{name}", family, idx)
        span = span_of(self, family, "idx", idx)
        chosen = self._chosen(f"BIYOS_get{name}", family, idx)
        found = [(c_int * size)() for _ in range(width)]
        values = (c_double * size)()
        if span is None:
            self._call(*chosen, size, None, *found, values)
            return [list(column) for column in found], list(values)
        starts = (c_int64 * (len(span) + 1))()
        others = found[:place] + found[place + 1 :]
        self._call(*chosen, size, starts, *others, values)
        columns = [list(column) for column in found]
        columns[place] = _expanded(starts, span.start)
        return columns, list(values)

    def _getidx(
        self, function: Callable[..., int], name: Sequence[str]
    ) -> list[int]:
        count = len(listed("name", name))
        names = strings("name", name, count)
        found = (c_int * count)()
        self._call(function, count, names, found)
        return list(found)

    def _call(self, function: Callable[..., int], *arguments: object) -> None:
        with self._env._busy():
            self._run(function, *arguments)

    def _run(self, function: Callable[..., int], *arguments: object) -> None:
        self._calls += 1
        try:
            if not self._open():
                raise ValueError("the model is closed")
            self._env._run(function, self._handle, *arguments)
        finally:
            self._calls -= 1
            self._settle()

    def _settle(self) -> None:
        if self._calls == 0 and self._closing:
            self._free()
