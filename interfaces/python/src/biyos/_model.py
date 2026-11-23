import weakref
from collections.abc import Callable, Sequence
from ctypes import Array, byref, c_char_p, c_double, c_int, c_int64, c_void_p
from enum import IntEnum
from math import isnan
from typing import Any, NoReturn, SupportsIndex

from biyos._arrays import (
    doubles,
    indices_of,
    int64s,
    ints,
    listed,
    offsets,
    span_of,
    strings,
)
from biyos._blocks import (
    Block,
    Complement,
    Complements,
    Cone,
    Cones,
    Constraint,
    Constraints,
    Function,
    Functions,
    Indicator,
    Indicators,
    Objective,
    Objectives,
    SOS,
    SOSs,
    Variable,
    Variables,
    index_of,
)
from biyos import _capi, _values
from biyos._capi import lib
from biyos._env import Env
from biyos._error import Error
from biyos._integers import integer
from biyos._operators import Operator, node_values, postfix_values
from biyos._options import OptionSource
from biyos._text import decoded, text
from biyos._values import Value


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


class SOSKind(IntEnum):
    """The kinds of special ordered sets, as ``BIYOS_SOSKND_SOS1`` and
    ``BIYOS_SOSKND_SOS2`` number them.

    Attributes
    ----------
    SOS1
        At most one member is nonzero.
    SOS2
        At most two members are nonzero, adjacent in the order of their
        weights.
    """

    SOS1 = 1
    SOS2 = 2


class ConeKind(IntEnum):
    """The kinds of cones, as ``BIYOS_CONE_`` numbers them.

    The C API reference defines each kind, its member order, its dimensions
    and its parameters.

    Attributes
    ----------
    NONNEGATIVE
        The nonnegative orthant.
    SECOND_ORDER
        The second-order cone.
    ROTATED
        The rotated second-order cone.
    EXPONENTIAL
        The exponential cone.
    DUAL_EXPONENTIAL
        The dual exponential cone.
    POWER
        The power cone, with a weight for each of its first members.
    DUAL_POWER
        The dual power cone, with a weight for each of its first members.
    SEMIDEFINITE
        The semidefinite cone, of a matrix's lower triangle.
    """

    NONNEGATIVE = 1
    SECOND_ORDER = 2
    ROTATED = 3
    EXPONENTIAL = 4
    DUAL_EXPONENTIAL = 5
    POWER = 6
    DUAL_POWER = 7
    SEMIDEFINITE = 8


_COUNTS: dict[type[Block[Any]], str] = {
    Variables: "getnvar",
    Functions: "getnfun",
    Constraints: "getncon",
    Objectives: "getnobj",
    Indicators: "getnind",
    SOSs: "getnsos",
    Cones: "getncone",
    Complements: "getnccon",
}


_SETTERS: dict[int, Callable[..., int]] = {
    _capi.KIND_BOOL: lib.BIYOS_setmdlboolopt,
    _capi.KIND_INT: lib.BIYOS_setmdlintopt,
    _capi.KIND_DOUBLE: lib.BIYOS_setmdldblopt,
    _capi.KIND_STRING: lib.BIYOS_setmdlstropt,
    _capi.KIND_ENUM: lib.BIYOS_setmdlintopt,
    _capi.KIND_FLAGS: lib.BIYOS_setmdlflgopt,
}

_GETTERS: dict[int, Callable[..., int]] = {
    _capi.KIND_BOOL: lib.BIYOS_getmdlboolopt,
    _capi.KIND_INT: lib.BIYOS_getmdlintopt,
    _capi.KIND_DOUBLE: lib.BIYOS_getmdldblopt,
    _capi.KIND_STRING: lib.BIYOS_getmdlstropt,
    _capi.KIND_ENUM: lib.BIYOS_getmdlintopt,
    _capi.KIND_FLAGS: lib.BIYOS_getmdlflgopt,
}


def _checked_env(env: Env) -> Env:
    if not isinstance(env, Env):
        found = type(env).__name__
        raise TypeError(f"env must be an Env, not {found}")
    return env


def _expanded(starts: Array[c_int64], first: int) -> list[int]:
    spans = zip(starts[:-1], starts[1:])
    counts = [end - start for start, end in spans]
    return [first + j for j, count in enumerate(counts) for _ in range(count)]


def _or_none(values: list[float]) -> list[float | None]:
    return [None if isnan(value) else value for value in values]


def _total(starts: Array[c_int64] | None, count: int) -> int:
    if starts is None:
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

        The copy holds the same variables, functions, constraints,
        objectives, indicators, special ordered sets, cones and complements
        under the same indices and names, and changes apart from the model.
        It keeps the options set on the model itself and reads every other
        option from its own environment, on which it reports, like any model
        made there.

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

    def setopt(self, name: str, val: Value) -> None:
        """Set an option on the model alone.

        The model reads its own value before its environment's, and the
        other models of the environment keep sharing the environment's.

        Parameters
        ----------
        name
            The name of the option, such as ``"maxtime"``.
        val
            The new value, of the type ``Env.setopt`` takes for the option.

        Raises
        ------
        TypeError
            When ``val`` does not match the option's kind.
        OverflowError
            When a mask of a flags option is negative or wider than 64 bits.
        Error
            When the name is unknown, the value is refused or the option
            has environment scope.
        ValueError
            When the model is closed.

        See Also
        --------
        BIYOS_setmdlboolopt, BIYOS_setmdlintopt,
        BIYOS_setmdldblopt, BIYOS_setmdlstropt,
        BIYOS_setmdlflgopt
        """
        option, kind = self._lookup(name)
        arguments = _values.arguments(name, kind, val)
        self._call(_SETTERS[kind], option, *arguments)

    def getopt(self, name: str) -> Value:
        """Read the value of an option the model uses.

        It is the model's own value, else its environment's, else the
        option's default.

        Parameters
        ----------
        name
            The name of the option.

        Returns
        -------
        val
            The value, of the type ``Env.getopt`` gives for the option.

        Raises
        ------
        Error
            When the name is unknown or the option has environment scope.
        ValueError
            When the model is closed.

        See Also
        --------
        BIYOS_getmdlboolopt, BIYOS_getmdlintopt,
        BIYOS_getmdldblopt, BIYOS_getmdlstropt,
        BIYOS_getmdlflgopt
        """
        option, kind = self._lookup(name)
        found = _values.slots(kind)
        self._call(_GETTERS[kind], option, *map(byref, found))
        return _values.value(name, kind, found)

    def delopt(self, name: str) -> None:
        """Remove the model's own value of an option.

        The model then reads its environment's value again. Removing an
        option the model does not hold does nothing.

        Parameters
        ----------
        name
            The name of the option.

        Raises
        ------
        Error
            When the name is unknown or the option has environment scope.
        ValueError
            When the model is closed.

        See Also
        --------
        BIYOS_delmdlopt
        """
        option, _ = self._lookup(name)
        self._call(lib.BIYOS_delmdlopt, option)

    def getoptsrc(self, name: str) -> OptionSource:
        """Tell where the value the model reads of an option comes from.

        Parameters
        ----------
        name
            The name of the option.

        Returns
        -------
        src
            The source. An option set on the environment to its default
            value comes from the environment.

        Raises
        ------
        Error
            When the name is unknown or the option has environment scope.
        ValueError
            When the model is closed.

        See Also
        --------
        BIYOS_getmdloptsrc
        """
        option, _ = self._lookup(name)
        src = c_int()
        self._call(lib.BIYOS_getmdloptsrc, option, byref(src))
        return OptionSource(src.value)

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

    def addfun(self, n: int) -> Functions:
        """Add functions after the last one, each with the constant 0 and
        no terms.

        Parameters
        ----------
        n
            The number of functions to add.

        Returns
        -------
        funs
            The new functions.

        See Also
        --------
        BIYOS_addfun
        """
        n = integer("n", n)
        first = c_int()
        self._call(lib.BIYOS_addfun, n, byref(first))
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

    def addvarnode(self, var: int | Variable) -> int:
        """Return the node of a variable, added the first time.

        Parameters
        ----------
        var
            The variable.

        Returns
        -------
        node
            The id of the variable's node, the same at every call.

        See Also
        --------
        BIYOS_addvarnode
        """
        var = index_of(self, Variables, "var", var)
        node = c_int64()
        self._call(lib.BIYOS_addvarnode, var, byref(node))
        return node.value

    def addnode(
        self,
        op: Operator,
        arg: Sequence[int] | None = None,
        val: Sequence[float] | None = None,
    ) -> int:
        """Add a node, an operator applied to nodes the model holds.

        The node is added as given: a sum among the children of a sum stays
        a child, and so does a product among those of a product. A node is
        shared by passing its id again.

        Parameters
        ----------
        op
            The operator, any but ``Operator.VARIABLE`` and
            ``Operator.NODE``.
        arg
            The ids of the children, in order, none by default.
        val
            The values the operator takes, None when it takes none: the
            value of a constant, the coefficient of a product, the exponent
            of a constant power or a sign power, or the constant of a sum
            followed by the weight of each child.

        Returns
        -------
        node
            The id of the new node.

        See Also
        --------
        BIYOS_addnode
        """
        code = integer("op", op)
        n = 0 if arg is None else len(arg)
        arrays = (
            int64s("arg", arg, n),
            doubles("val", val, node_values(code, n)),
        )
        node = c_int64()
        self._call(lib.BIYOS_addnode, code, n, *arrays, byref(node))
        return node.value

    def addexpr(
        self,
        off: Sequence[int],
        code: Sequence[Operator],
        arg: Sequence[int],
        val: Sequence[float] | None = None,
    ) -> list[int]:
        """Add expressions given in postfix.

        Expression ``k`` is the entries from ``off[k]`` to
        ``off[k + 1] - 1`` of ``code`` and ``arg``, each operand before the
        operator that takes it. A sum that is an operand of a sum built in
        the same expression is merged into it, and so is a product that is
        an operand of a product. A call that fails adds no node.

        Parameters
        ----------
        off
            The offsets of the expressions' entries, one more than there are
            expressions, from 0 and never decreasing.
        code
            The code of each entry, ``off[-1]`` of them: an operator, or
            ``Operator.NODE``.
        arg
            The argument of each entry, ``off[-1]`` of them: the variable of
            ``Operator.VARIABLE``, the id of a node the model held before
            the call for ``Operator.NODE``, and the number of operands of
            any other operator, 0 for ``Operator.CONSTANT``.
        val
            The values the operators take, as ``addnode`` takes them, one
            after another in the order of the entries. None when they take
            none.

        Returns
        -------
        node
            The id of the top node of each expression.

        See Also
        --------
        BIYOS_addexpr
        """
        n = len(off) - 1
        if n < 0:
            raise ValueError("off has length 0, not at least 1")
        starts = offsets("off", off, n + 1)
        size = _total(starts, n)
        codes = ints("code", code, size)
        args = int64s("arg", arg, size)
        values = doubles("val", val, postfix_values(codes, args))
        nodes = (c_int64 * n)()
        self._call(lib.BIYOS_addexpr, n, starts, codes, args, values, nodes)
        return list(nodes)

    def attachexpr(
        self,
        idx: Sequence[int | Function] | Functions,
        node: Sequence[int],
    ) -> None:
        """Add expressions to functions of the model.

        Function ``idx[k]`` becomes ``f + e``, where ``e`` is node
        ``node[k]``. The constant, linear and quadratic terms of the
        expression's top sum join the function's, and the rest becomes its
        expression, or is summed with the expression it has.

        Parameters
        ----------
        idx
            The functions that gain an expression, each listed once.
        node
            The node each function gains, by id.

        See Also
        --------
        BIYOS_attachexprsp
        BIYOS_attachexprrg
        """
        n = len(idx)
        chosen = self._chosen("BIYOS_attachexpr", Functions, idx)
        self._call(*chosen, int64s("node", node, n))

    def clrfun(self, idx: Sequence[int | Function] | Functions) -> None:
        """Empty functions of the model, so they can be built again.

        Each function given has the constant 0, no term and no expression
        once the call returns. Its expression's nodes stay in the graph.

        Parameters
        ----------
        idx
            The functions, any of them more than once.

        See Also
        --------
        BIYOS_clrfunsp
        BIYOS_clrfunrg
        """
        self._call(*self._chosen("BIYOS_clrfun", Functions, idx))

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

    def addind(
        self,
        varidx: Sequence[int | Variable] | Variables,
        val: Sequence[int],
        funidx: Sequence[int | Function] | Functions,
        lb: Sequence[float] | None = None,
        ub: Sequence[float] | None = None,
    ) -> Indicators:
        """Add indicators after the last one, one for each variable given.

        An indicator bounds a function while a binary variable takes a
        value: when variable ``varidx[j]`` equals ``val[j]``, function
        ``funidx[j]`` lies within its bounds, and otherwise it is free. A
        variable stays binary while an indicator uses it.

        Parameters
        ----------
        varidx
            The binary variable that switches each new indicator on.
        val
            The value of each variable that switches its indicator on, 0 or
            1, as many as ``varidx``.
        funidx
            The function each indicator bounds, as many as ``varidx``.
        lb, ub
            The bounds of each, unbounded by default.

        Returns
        -------
        inds
            The new indicators.

        See Also
        --------
        BIYOS_addind
        """
        count = len(varidx)
        arrays = (
            indices_of(self, Variables, "varidx", varidx, count),
            ints("val", val, count),
            indices_of(self, Functions, "funidx", funidx, count),
            doubles("lb", lb, count),
            doubles("ub", ub, count),
        )
        first = c_int()
        self._call(lib.BIYOS_addind, count, *arrays, byref(first))
        return Indicators(self, first.value, count, self._gen)

    def setindbnd(
        self,
        idx: Sequence[int | Indicator] | Indicators,
        lb: Sequence[float] | None = None,
        ub: Sequence[float] | None = None,
    ) -> None:
        """Set the bounds of indicators and keep the rest.

        Parameters
        ----------
        idx
            The indicators.
        lb, ub
            The new bounds of each; ``None`` leaves that side as it is.

        See Also
        --------
        BIYOS_setindbndsp
        BIYOS_setindbndrg
        """
        count = len(idx)
        chosen = self._chosen("BIYOS_setindbnd", Indicators, idx)
        arrays = (
            doubles("lb", lb, count),
            doubles("ub", ub, count),
        )
        self._call(*chosen, *arrays)

    def getindbnd(
        self, idx: Sequence[int | Indicator] | Indicators
    ) -> tuple[list[float], list[float]]:
        """Read the bounds of indicators.

        Parameters
        ----------
        idx
            The indicators, in any order, any of them more than once.

        Returns
        -------
        lb
            The lower bound of each.
        ub
            The upper bound of each.

        See Also
        --------
        BIYOS_getindbndsp
        BIYOS_getindbndrg
        """
        bounds = (c_double, c_double)
        lb, ub = self._getlist("BIYOS_getindbnd", Indicators, idx, *bounds)
        return lb, ub

    def setindvar(
        self,
        idx: Sequence[int | Indicator] | Indicators,
        varidx: Sequence[int | Variable] | Variables | None = None,
        val: Sequence[int] | None = None,
    ) -> None:
        """Set the variables that switch indicators on, and their values.

        Parameters
        ----------
        idx
            The indicators.
        varidx
            The new binary variable of each; ``None`` leaves the variables
            as they are.
        val
            The new value of each, 0 or 1; ``None`` leaves the values as
            they are.

        See Also
        --------
        BIYOS_setindvarsp
        BIYOS_setindvarrg
        """
        count = len(idx)
        chosen = self._chosen("BIYOS_setindvar", Indicators, idx)
        arrays = (
            indices_of(self, Variables, "varidx", varidx, count),
            ints("val", val, count),
        )
        self._call(*chosen, *arrays)

    def getindvar(
        self, idx: Sequence[int | Indicator] | Indicators
    ) -> tuple[list[int], list[int]]:
        """Read the variables that switch indicators on, and their values.

        Parameters
        ----------
        idx
            The indicators, in any order, any of them more than once.

        Returns
        -------
        varidx
            The variable of each.
        val
            The value of each.

        See Also
        --------
        BIYOS_getindvarsp
        BIYOS_getindvarrg
        """
        pair = (c_int, c_int)
        found = self._getlist("BIYOS_getindvar", Indicators, idx, *pair)
        variables, values = found
        return variables, values

    def getnind(self) -> int:
        """Return the number of indicators.

        Returns
        -------
        n
            The number of indicators.

        See Also
        --------
        BIYOS_getnind
        """
        count = c_int()
        self._call(lib.BIYOS_getnind, byref(count))
        return count.value

    def getind(
        self, idx: Sequence[int | Indicator] | Indicators | None = None
    ) -> tuple[list[int], list[int], list[int], list[float], list[float]]:
        """Read indicators.

        Parameters
        ----------
        idx
            The indicators, in any order, any of them more than once; every
            indicator by default.

        Returns
        -------
        varidx
            The variable that switches each indicator on.
        val
            The value of each variable that switches its indicator on.
        funidx
            The function each indicator bounds.
        lb
            The lower bound of each.
        ub
            The upper bound of each.

        See Also
        --------
        BIYOS_getindsp
        BIYOS_getindrg
        """
        every = range(self.getnind()) if idx is None else idx
        types = (c_int, c_int, c_int, c_double, c_double)
        found = self._getlist("BIYOS_getind", Indicators, every, *types)
        variables, values, functions, lower, upper = found
        return variables, values, functions, lower, upper

    def addsos(
        self,
        knd: Sequence[SOSKind],
        off: Sequence[int],
        varidx: Sequence[int | Variable] | Variables,
        coef: Sequence[float] | None = None,
    ) -> SOSs:
        """Add special ordered sets after the last one.

        The set ``k`` holds the members from ``off[k]`` to ``off[k + 1] - 1``,
        kept in that order whatever their weights. In a set of kind
        ``SOS1`` at most one member is nonzero; in a set of kind ``SOS2`` at
        most two are, adjacent in the order of their weights. A variable is
        a member of a set at most once and may belong to several sets.

        Parameters
        ----------
        knd
            The kind of each new set.
        off
            The offsets of the sets' members, one more than there are sets,
            from 0 and never decreasing.
        varidx
            The variable of each member, ``off[-1]`` of them.
        coef
            The weight of each member, finite and distinct within a set. By
            default the members of each set weigh 1, 2, 3 and so on, in the
            order given.

        Returns
        -------
        sets
            The new sets.

        See Also
        --------
        BIYOS_addsos
        """
        if off is None:
            raise TypeError("off must be a sequence of int, not NoneType")
        count = len(knd)
        starts = offsets("off", off, count + 1)
        size = _total(starts, count)
        arrays = (
            ints("knd", knd, count),
            starts,
            indices_of(self, Variables, "varidx", varidx, size),
            doubles("coef", coef, size),
        )
        first = c_int()
        self._call(lib.BIYOS_addsos, count, *arrays, byref(first))
        return SOSs(self, first.value, count, self._gen)

    def getnsos(self) -> int:
        """Return the number of special ordered sets.

        Returns
        -------
        n
            The number of sets.

        See Also
        --------
        BIYOS_getnsos
        """
        count = c_int()
        self._call(lib.BIYOS_getnsos, byref(count))
        return count.value

    def getnsosvar(self, idx: Sequence[int | SOS] | SOSs | None = None) -> int:
        """Count the members of special ordered sets.

        Parameters
        ----------
        idx
            The sets, in any order, any of them more than once; every set by
            default.

        Returns
        -------
        nnz
            The number of members they hold, a set counted each time it
            appears.

        See Also
        --------
        BIYOS_getnsosvarsp
        BIYOS_getnsosvarrg
        """
        every = range(self.getnsos()) if idx is None else idx
        return self._count("BIYOS_getnsosvar", SOSs, every)

    def getsos(
        self, idx: Sequence[int | SOS] | SOSs | None = None
    ) -> tuple[list[SOSKind], list[int], list[int], list[float]]:
        """Read special ordered sets, as ``addsos`` takes them.

        Parameters
        ----------
        idx
            The sets, in any order, any of them more than once; every set by
            default.

        Returns
        -------
        knd
            The kind of each set.
        off
            The offsets of their members, one more than there are sets, from
            0.
        varidx
            The variable of each member, each set's in the order added.
        coef
            The weight of each member.

        See Also
        --------
        BIYOS_getsossp
        BIYOS_getsosrg
        """
        every = range(self.getnsos()) if idx is None else idx
        size = self.getnsosvar(every)
        count = len(every)
        kinds = (c_int * count)()
        starts = (c_int64 * (count + 1))()
        members = (c_int * size)()
        weights = (c_double * size)()
        chosen = self._chosen("BIYOS_getsos", SOSs, every)
        self._call(*chosen, size, kinds, starts, members, weights)
        found = [SOSKind(code) for code in kinds]
        return found, list(starts), list(members), list(weights)

    def addcone(
        self,
        knd: Sequence[ConeKind],
        off: Sequence[int],
        funidx: Sequence[int | Function] | Functions,
        prmoff: Sequence[int] | None = None,
        prm: Sequence[float] | None = None,
    ) -> Cones:
        """Add cones after the last one.

        Cone ``k`` places the functions from ``funidx[off[k]]`` to
        ``funidx[off[k + 1] - 1]``, in that order, in a cone of kind
        ``knd[k]`` whose parameters are those from ``prm[prmoff[k]]`` to
        ``prm[prmoff[k + 1] - 1]``. A call that fails adds no cone.

        Parameters
        ----------
        knd
            The kind of each cone.
        off
            The offsets of the cones' members, one more than there are
            cones, from 0 and never decreasing.
        funidx
            The members of each cone, ``off[-1]`` of them, a function at
            most once in a cone.
        prmoff
            The offsets of the cones' parameters, as ``off`` gives the
            members'. None when no cone takes one.
        prm
            The parameters of each cone, ``prmoff[-1]`` of them: the
            weights of a power cone, each finite and positive.

        Returns
        -------
        cones
            The new cones.

        See Also
        --------
        BIYOS_addcone
        """
        n = len(knd)
        starts = offsets("off", off, n + 1)
        weights = offsets("prmoff", prmoff, n + 1)
        arrays = (
            ints("knd", knd, n),
            starts,
            indices_of(self, Functions, "funidx", funidx, _total(starts, n)),
            weights,
            doubles("prm", prm, _total(weights, n)),
        )
        first = c_int()
        self._call(lib.BIYOS_addcone, n, *arrays, byref(first))
        return Cones(self, first.value, n, self._gen)

    def getncone(self) -> int:
        """Return the number of cones.

        Returns
        -------
        n
            The number of cones.

        See Also
        --------
        BIYOS_getncone
        """
        count = c_int()
        self._call(lib.BIYOS_getncone, byref(count))
        return count.value

    def getnconefun(
        self, idx: Sequence[int | Cone] | Cones | None = None
    ) -> int:
        """Count the members of cones.

        Parameters
        ----------
        idx
            The cones, in any order, any of them more than once; every cone
            by default.

        Returns
        -------
        nnz
            The number of members they hold, a cone counted each time it
            appears.

        See Also
        --------
        BIYOS_getnconefunsp
        BIYOS_getnconefunrg
        """
        every = range(self.getncone()) if idx is None else idx
        return self._count("BIYOS_getnconefun", Cones, every)

    def getcone(
        self, idx: Sequence[int | Cone] | Cones | None = None
    ) -> tuple[list[ConeKind], list[int], list[int]]:
        """Read the kinds and members of cones.

        Parameters
        ----------
        idx
            The cones, in any order, any of them more than once; every cone
            by default.

        Returns
        -------
        knd
            The kind of each cone.
        off
            The offsets of their members, one more than there are cones,
            from 0.
        funidx
            The members of each cone in order, those of entry ``j`` from
            ``funidx[off[j]]`` to ``funidx[off[j + 1] - 1]``.

        See Also
        --------
        BIYOS_getconesp
        BIYOS_getconerg
        """
        every = range(self.getncone()) if idx is None else idx
        size = self._count("BIYOS_getnconefun", Cones, every)
        chosen = self._chosen("BIYOS_getcone", Cones, every)
        kinds = (c_int * len(every))()
        starts = (c_int64 * (len(every) + 1))()
        members = (c_int * size)()
        self._call(*chosen, size, kinds, starts, members)
        found = [ConeKind(kind) for kind in kinds]
        return found, list(starts), list(members)

    def getnconeprm(
        self, idx: Sequence[int | Cone] | Cones | None = None
    ) -> int:
        """Count the parameters of cones.

        Parameters
        ----------
        idx
            The cones, in any order, any of them more than once; every cone
            by default.

        Returns
        -------
        nnz
            The number of parameters they hold, a cone counted each time it
            appears.

        See Also
        --------
        BIYOS_getnconeprmsp
        BIYOS_getnconeprmrg
        """
        every = range(self.getncone()) if idx is None else idx
        return self._count("BIYOS_getnconeprm", Cones, every)

    def getconeprm(
        self, idx: Sequence[int | Cone] | Cones | None = None
    ) -> tuple[list[int], list[float]]:
        """Read the parameters of cones.

        Parameters
        ----------
        idx
            The cones, in any order, any of them more than once; every cone
            by default.

        Returns
        -------
        prmoff
            The offsets of their parameters, one more than there are cones,
            from 0.
        prm
            The parameters of each cone in order, those of entry ``j`` from
            ``prm[prmoff[j]]`` to ``prm[prmoff[j + 1] - 1]``.

        See Also
        --------
        BIYOS_getconeprmsp
        BIYOS_getconeprmrg
        """
        every = range(self.getncone()) if idx is None else idx
        size = self._count("BIYOS_getnconeprm", Cones, every)
        chosen = self._chosen("BIYOS_getconeprm", Cones, every)
        starts = (c_int64 * (len(every) + 1))()
        values = (c_double * size)()
        self._call(*chosen, size, starts, values)
        return list(starts), list(values)

    def addccon(
        self,
        funidx1: Sequence[int | Function] | Functions,
        funidx2: Sequence[int | Function] | Functions,
        lb: Sequence[float] | None = None,
        ub: Sequence[float] | None = None,
    ) -> Complements:
        """Add complements after the last one, one for each pair given.

        A complement ``lb <= f1(x) <= ub`` complementary to ``f2(x)`` holds
        in one of three cases: ``f1`` at ``lb`` and ``f2 >= 0``, ``f1`` at
        ``ub`` and ``f2 <= 0``, or ``f1`` strictly between and ``f2 = 0``.
        A function may appear in any number of complements.

        Parameters
        ----------
        funidx1
            The bounded function ``f1`` of each new complement.
        funidx2
            The function ``f2`` complementary to it, as many as
            ``funidx1``.
        lb
            The lower bound of each on ``f1``, 0 by default.
        ub
            The upper bound of each on ``f1``, unbounded by default.

        Returns
        -------
        ccons
            The new complements.

        See Also
        --------
        BIYOS_addccon
        """
        count = len(funidx1)
        arrays = (
            indices_of(self, Functions, "funidx1", funidx1, count),
            indices_of(self, Functions, "funidx2", funidx2, count),
            doubles("lb", lb, count),
            doubles("ub", ub, count),
        )
        first = c_int()
        self._call(lib.BIYOS_addccon, count, *arrays, byref(first))
        return Complements(self, first.value, count, self._gen)

    def setcconbnd(
        self,
        idx: Sequence[int | Complement] | Complements,
        lb: Sequence[float] | None = None,
        ub: Sequence[float] | None = None,
    ) -> None:
        """Set the bounds of complements and keep the rest.

        Parameters
        ----------
        idx
            The complements.
        lb, ub
            The new bounds of each; ``None`` leaves that side as it is.

        See Also
        --------
        BIYOS_setcconbndsp
        BIYOS_setcconbndrg
        """
        count = len(idx)
        chosen = self._chosen("BIYOS_setcconbnd", Complements, idx)
        arrays = (
            doubles("lb", lb, count),
            doubles("ub", ub, count),
        )
        self._call(*chosen, *arrays)

    def getcconbnd(
        self, idx: Sequence[int | Complement] | Complements
    ) -> tuple[list[float], list[float]]:
        """Read the bounds of complements.

        Parameters
        ----------
        idx
            The complements, in any order, any of them more than once.

        Returns
        -------
        lb
            The lower bound of each.
        ub
            The upper bound of each.

        See Also
        --------
        BIYOS_getcconbndsp
        BIYOS_getcconbndrg
        """
        bounds = (c_double, c_double)
        lb, ub = self._getlist("BIYOS_getcconbnd", Complements, idx, *bounds)
        return lb, ub

    def setccontol(
        self,
        idx: Sequence[int | Complement] | Complements,
        atol: Sequence[float] | None = None,
        rtol: Sequence[float] | None = None,
    ) -> None:
        """Set the tolerances of complements.

        A complement's tolerances stand for the options ``feastol_abs`` and
        ``feastol_rel`` for that complement alone: each finite and at least
        0. A complement that held none keeps the options for the tolerance
        not given. A complement may appear more than once; its entries
        apply in order.

        Parameters
        ----------
        idx
            The complements.
        atol, rtol
            The new absolute and relative tolerance of each; ``None``
            leaves that tolerance as it is.

        See Also
        --------
        BIYOS_setccontolsp
        BIYOS_setccontolrg
        """
        n = len(idx)
        chosen = self._chosen("BIYOS_setccontol", Complements, idx)
        arrays = (
            doubles("atol", atol, n),
            doubles("rtol", rtol, n),
        )
        self._call(*chosen, *arrays)

    def getccontol(
        self, idx: Sequence[int | Complement] | Complements
    ) -> tuple[list[float | None], list[float | None]]:
        """Read the tolerances of complements.

        Parameters
        ----------
        idx
            The complements, in any order, any of them more than once.

        Returns
        -------
        atol
            The absolute tolerance of each, None where it holds none.
        rtol
            The relative tolerance of each, None where it holds none.

        See Also
        --------
        BIYOS_getccontolsp
        BIYOS_getccontolrg
        """
        pair = (c_double, c_double)
        atol, rtol = self._getlist("BIYOS_getccontol", Complements, idx, *pair)
        return _or_none(atol), _or_none(rtol)

    def delccontol(
        self, idx: Sequence[int | Complement] | Complements
    ) -> None:
        """Return complements to ``feastol_abs`` and ``feastol_rel``.

        Parameters
        ----------
        idx
            The complements that lose their tolerances.

        See Also
        --------
        BIYOS_delccontolsp
        BIYOS_delccontolrg
        """
        self._call(*self._chosen("BIYOS_delccontol", Complements, idx))

    def getnccon(self) -> int:
        """Return the number of complements.

        Returns
        -------
        n
            The number of complements.

        See Also
        --------
        BIYOS_getnccon
        """
        count = c_int()
        self._call(lib.BIYOS_getnccon, byref(count))
        return count.value

    def getccon(
        self, idx: Sequence[int | Complement] | Complements | None = None
    ) -> tuple[list[int], list[int], list[float], list[float]]:
        """Read the functions and bounds of complements.

        Parameters
        ----------
        idx
            The complements, in any order, any of them more than once; every
            complement by default.

        Returns
        -------
        funidx1
            The bounded function of each complement.
        funidx2
            The function complementary to it.
        lb
            The lower bound of each.
        ub
            The upper bound of each.

        See Also
        --------
        BIYOS_getcconsp
        BIYOS_getcconrg
        """
        every = range(self.getnccon()) if idx is None else idx
        types = (c_int, c_int, c_double, c_double)
        found = self._getlist("BIYOS_getccon", Complements, every, *types)
        bounded, partners, lower, upper = found
        return bounded, partners, lower, upper

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
        and ``getvaridx`` then refuses to choose among them. Every other
        family is named the same way, each apart.

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

    def setindname(
        self, idx: Sequence[int | Indicator] | Indicators, name: Sequence[str]
    ) -> None:
        """Name indicators of the model, as ``setvarname`` names variables.

        Parameters
        ----------
        idx
            The indicators to name.
        name
            The new name of each, as many as ``idx``.

        See Also
        --------
        BIYOS_setindnamesp
        BIYOS_setindnamerg
        """
        self._setname("BIYOS_setindname", Indicators, idx, name)

    def getindname(
        self, idx: Sequence[int | Indicator] | Indicators
    ) -> list[str | None]:
        """Return the names of indicators, as ``getvarname`` does.

        Parameters
        ----------
        idx
            The indicators to read.

        Returns
        -------
        name
            The name of each, or None for one that has none.

        See Also
        --------
        BIYOS_getindnamesp
        BIYOS_getindnamerg
        """
        return self._getname("BIYOS_getindname", Indicators, idx)

    def getindidx(self, name: Sequence[str]) -> list[int]:
        """Return the indices of named indicators, as ``getvaridx`` does.

        Parameters
        ----------
        name
            The names, each matched exactly.

        Returns
        -------
        idx
            The index of the indicator that alone holds each name.

        See Also
        --------
        BIYOS_getindidx
        """
        return self._getidx(lib.BIYOS_getindidx, name)

    def delindname(self, idx: Sequence[int | Indicator] | Indicators) -> None:
        """Remove the names of indicators, as ``delvarname`` does.

        Parameters
        ----------
        idx
            The indicators that lose their names.

        See Also
        --------
        BIYOS_delindnamesp
        BIYOS_delindnamerg
        """
        self._delname("BIYOS_delindname", Indicators, idx)

    def setsosname(
        self, idx: Sequence[int | SOS] | SOSs, name: Sequence[str]
    ) -> None:
        """Name special ordered sets, as ``setvarname`` names variables.

        Parameters
        ----------
        idx
            The sets to name.
        name
            The new name of each, as many as ``idx``.

        See Also
        --------
        BIYOS_setsosnamesp
        BIYOS_setsosnamerg
        """
        self._setname("BIYOS_setsosname", SOSs, idx, name)

    def getsosname(self, idx: Sequence[int | SOS] | SOSs) -> list[str | None]:
        """Return the names of special ordered sets, as ``getvarname`` does.

        Parameters
        ----------
        idx
            The sets to read.

        Returns
        -------
        name
            The name of each, or None for one that has none.

        See Also
        --------
        BIYOS_getsosnamesp
        BIYOS_getsosnamerg
        """
        return self._getname("BIYOS_getsosname", SOSs, idx)

    def getsosidx(self, name: Sequence[str]) -> list[int]:
        """Return the indices of named special ordered sets, as
        ``getvaridx`` does.

        Parameters
        ----------
        name
            The names, each matched exactly.

        Returns
        -------
        idx
            The index of the set that alone holds each name.

        See Also
        --------
        BIYOS_getsosidx
        """
        return self._getidx(lib.BIYOS_getsosidx, name)

    def delsosname(self, idx: Sequence[int | SOS] | SOSs) -> None:
        """Remove the names of special ordered sets, as ``delvarname``
        does.

        Parameters
        ----------
        idx
            The sets that lose their names.

        See Also
        --------
        BIYOS_delsosnamesp
        BIYOS_delsosnamerg
        """
        self._delname("BIYOS_delsosname", SOSs, idx)

    def setconename(
        self, idx: Sequence[int | Cone] | Cones, name: Sequence[str]
    ) -> None:
        """Name cones of the model, as ``setvarname`` names variables.

        Parameters
        ----------
        idx
            The cones to name.
        name
            The new name of each, as many as ``idx``.

        See Also
        --------
        BIYOS_setconenamesp
        BIYOS_setconenamerg
        """
        self._setname("BIYOS_setconename", Cones, idx, name)

    def getconename(
        self, idx: Sequence[int | Cone] | Cones
    ) -> list[str | None]:
        """Return the names of cones, as ``getvarname`` does.

        Parameters
        ----------
        idx
            The cones to read.

        Returns
        -------
        name
            The name of each, or None for one that has none.

        See Also
        --------
        BIYOS_getconenamesp
        BIYOS_getconenamerg
        """
        return self._getname("BIYOS_getconename", Cones, idx)

    def getconeidx(self, name: Sequence[str]) -> list[int]:
        """Return the indices of named cones, as ``getvaridx`` does.

        Parameters
        ----------
        name
            The names, each matched exactly.

        Returns
        -------
        idx
            The index of the cone that alone holds each name.

        See Also
        --------
        BIYOS_getconeidx
        """
        return self._getidx(lib.BIYOS_getconeidx, name)

    def delconename(self, idx: Sequence[int | Cone] | Cones) -> None:
        """Remove the names of cones, as ``delvarname`` does.

        Parameters
        ----------
        idx
            The cones that lose their names.

        See Also
        --------
        BIYOS_delconenamesp
        BIYOS_delconenamerg
        """
        self._delname("BIYOS_delconename", Cones, idx)

    def setcconname(
        self,
        idx: Sequence[int | Complement] | Complements,
        name: Sequence[str],
    ) -> None:
        """Name complements of the model, as ``setvarname`` names variables.

        Parameters
        ----------
        idx
            The complements to name.
        name
            The new name of each, as many as ``idx``.

        See Also
        --------
        BIYOS_setcconnamesp
        BIYOS_setcconnamerg
        """
        self._setname("BIYOS_setcconname", Complements, idx, name)

    def getcconname(
        self, idx: Sequence[int | Complement] | Complements
    ) -> list[str | None]:
        """Return the names of complements, as ``getvarname`` does.

        Parameters
        ----------
        idx
            The complements to read.

        Returns
        -------
        name
            The name of each, or None for one that has none.

        See Also
        --------
        BIYOS_getcconnamesp
        BIYOS_getcconnamerg
        """
        return self._getname("BIYOS_getcconname", Complements, idx)

    def getcconidx(self, name: Sequence[str]) -> list[int]:
        """Return the indices of named complements, as ``getvaridx`` does.

        Parameters
        ----------
        name
            The names, each matched exactly.

        Returns
        -------
        idx
            The index of the complement that alone holds each name.

        See Also
        --------
        BIYOS_getcconidx
        """
        return self._getidx(lib.BIYOS_getcconidx, name)

    def delcconname(
        self, idx: Sequence[int | Complement] | Complements
    ) -> None:
        """Remove the names of complements, as ``delvarname`` does.

        Parameters
        ----------
        idx
            The complements that lose their names.

        See Also
        --------
        BIYOS_delcconnamesp
        BIYOS_delcconnamerg
        """
        self._delname("BIYOS_delcconname", Complements, idx)

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
                self._env._owners[handle.value] = self

    def _copy(self, source: "Model") -> None:
        if not self._env._open():
            raise ValueError("the environment is closed")
        target = self._env._handle
        self._make(source._call, lib.BIYOS_copymdl, target)

    def _hold(self) -> None:
        park = self._env._models.park
        self._free = weakref.finalize(self, park, self._handle.value)

    def _lookup(self, name: str) -> tuple[int, int]:
        if not self._open():
            raise ValueError("the model is closed")
        try:
            return self._env._lookup(name)
        except Error as error:
            error.model = self
            raise

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
