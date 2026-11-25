import weakref
from collections.abc import Callable, Sequence
from ctypes import (
    Array,
    addressof,
    byref,
    c_char_p,
    c_double,
    c_int,
    c_int64,
    c_void_p,
    py_object,
)
from enum import IntEnum
from math import isnan
from os import PathLike, fspath
from typing import Any, NoReturn, SupportsIndex, TypeVar, cast

from biyos import _capi
from biyos._capi import lib
from biyos._convert.arrays import (
    counts,
    doubles,
    indices_of,
    int64s,
    ints,
    listed,
    span_of,
    strings,
)
from biyos._convert.integers import integer, offset
from biyos._convert.text import decoded, text
from biyos._env import values
from biyos._env.env import Env
from biyos._env.event import Event, EventKind, EventLevel
from biyos._env.hooks import Hooks, checked_hook
from biyos._env.options import OptionSource
from biyos._env.values import Value
from biyos._error import Error
from biyos._model.blocks import (
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
    PiecewiseLinear,
    PiecewiseLinears,
    SpecialOrderedSet,
    SpecialOrderedSets,
    Variable,
    Variables,
    index_of,
)
from biyos._model.callbacks import Callbacks, Caller, checked, data_at
from biyos._model.expression import Comparison, Expression, Operand
from biyos._model.operators import Operator, node_values, postfix_values
from biyos._model.postfix import attached, given


class VariableKind(IntEnum):
    """The kinds of variables, as ``BIYOS_VARKND_`` numbers them.

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


class ObjectiveSense(IntEnum):
    """The senses of objectives, as ``BIYOS_OBJSENSE_MIN`` and
    ``BIYOS_OBJSENSE_MAX`` number them.

    Attributes
    ----------
    MINIMIZE
        The function is minimized.
    MAXIMIZE
        The function is maximized.
    """

    MINIMIZE = 1
    MAXIMIZE = 2


class SpecialOrderedSetKind(IntEnum):
    """The kinds of special ordered sets, as ``BIYOS_SOSKND_ONE`` and
    ``BIYOS_SOSKND_TWO`` number them.

    Attributes
    ----------
    ONE
        At most one member is nonzero.
    TWO
        At most two members are nonzero, adjacent in the order of their
        weights.
    """

    ONE = 1
    TWO = 2


class ConeKind(IntEnum):
    """The kinds of cones, as ``BIYOS_CONEKND_`` numbers them.

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


class FileFormat(IntEnum):
    """The file formats, as ``BIYOS_FILEFMT_`` numbers them.

    Attributes
    ----------
    MPS
        Free MPS, for linear and mixed-integer models.
    """

    MPS = 1


_INT_MAX = 2**31 - 1
_Block = TypeVar("_Block", bound=Block[Any])

_COUNTS: dict[type[Block[Any]], str] = {
    Variables: "getnvar",
    Functions: "getnfun",
    Constraints: "getncon",
    Objectives: "getnobj",
    Indicators: "getnind",
    SpecialOrderedSets: "getnsos",
    PiecewiseLinears: "getnpwl",
    Cones: "getncone",
    Complements: "getnccon",
}


_SETTERS: dict[int, Callable[..., int]] = {
    _capi.OPTKND_BOOL: lib.BIYOS_setmdlboolopt,
    _capi.OPTKND_INT: lib.BIYOS_setmdlintopt,
    _capi.OPTKND_DBL: lib.BIYOS_setmdldblopt,
    _capi.OPTKND_STR: lib.BIYOS_setmdlstropt,
    _capi.OPTKND_ENUM: lib.BIYOS_setmdlintopt,
    _capi.OPTKND_FLG: lib.BIYOS_setmdlflgopt,
}

_GETTERS: dict[int, Callable[..., int]] = {
    _capi.OPTKND_BOOL: lib.BIYOS_getmdlboolopt,
    _capi.OPTKND_INT: lib.BIYOS_getmdlintopt,
    _capi.OPTKND_DBL: lib.BIYOS_getmdldblopt,
    _capi.OPTKND_STR: lib.BIYOS_getmdlstropt,
    _capi.OPTKND_ENUM: lib.BIYOS_getmdlintopt,
    _capi.OPTKND_FLG: lib.BIYOS_getmdlflgopt,
}


def _checked_env(env: Env) -> Env:
    if not isinstance(env, Env):
        found = type(env).__name__
        raise TypeError(f"env must be an Env, not {found}")
    return env


def _expanded(counts: Array[c_int64], first: int) -> list[int]:
    return [first + j for j, count in enumerate(counts) for _ in range(count)]


def _or_none(values: list[float]) -> list[float | None]:
    return [None if isnan(value) else value for value in values]


def _renumbered(total: int, gone: set[int]) -> list[int]:
    found: list[int] = []
    kept = 0
    for k in range(total):
        if k in gone:
            found.append(-1)
        else:
            found.append(kept)
            kept += 1
    return found


def _total(counts: Array[c_int64] | None) -> int:
    if counts is None:
        return 0
    return sum(counts)


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
        models.take(self._handle)
        if self._free.detach() is None:
            return
        try:
            self._env._run(lib.BIYOS_freemdl, self._handle)
        except Error:
            models.add(self._handle)
            self._hold()
            raise
        self._hooks.clear()

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

    @classmethod
    def read(
        cls, env: Env, path: str | PathLike[str], fmt: FileFormat
    ) -> "Model":
        """Read a model from a file.

        The format is the one ``fmt`` names, never guessed from the file's
        name. Each oddity the reader reads past is a warning event from
        module ``io``, with the fields ``file`` and ``line``.

        Parameters
        ----------
        env
            The environment the model is made on.
        path
            The path of the file.
        fmt
            The format of the file.

        Returns
        -------
        model
            The model the file holds.

        Raises
        ------
        TypeError
            When ``env`` is not an ``Env``, ``path`` is not a str or a path,
            or ``fmt`` is not an int.
        ValueError
            When ``path`` holds a null character or cannot be encoded as
            UTF-8, or the environment is closed.
        Error
            When ``fmt`` is not a format, the file cannot be read, breaks
            a rule of its format or holds a part the reader does not handle.

        See Also
        --------
        BIYOS_readmdl
        """
        env = _checked_env(env)
        name = text("path", fspath(path))
        number = integer("fmt", fmt)
        model = cls.__new__(cls)
        model._start(env)
        with env._busy():
            model._make(env._call, lib.BIYOS_readmdl, name, number)
        model._hold()
        return model

    def write(self, path: str | PathLike[str], fmt: FileFormat) -> None:
        """Write the model to a file.

        The file is made or replaced, in the format ``fmt`` names. Each
        part the format leaves out is a warning event from module ``io``,
        with the field ``file``.

        Parameters
        ----------
        path
            The path of the file.
        fmt
            The format of the file.

        Raises
        ------
        TypeError
            When ``path`` is not a str or a path, or ``fmt`` is not an int.
        ValueError
            When ``path`` holds a null character or cannot be encoded as
            UTF-8, or the model is closed.
        Error
            When ``fmt`` is not a format, the file cannot be written or the
            format cannot hold a part of the model.

        See Also
        --------
        BIYOS_writemdl
        """
        name = text("path", fspath(path))
        number = integer("fmt", fmt)
        self._call(lib.BIYOS_writemdl, name, number)

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
        arguments = values.arguments(name, kind, val)
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
        found = values.slots(kind)
        self._call(_GETTERS[kind], option, *map(byref, found))
        return values.value(name, kind, found)

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

    def addhook(
        self,
        hk: Callable[[Event], object],
        knd: EventKind,
        lvl: EventLevel,
    ) -> int:
        """Add a hook on one kind of event of the model at a lowest level.

        The hook receives the events of calls on the model alone, before the
        hooks of its environment receive them. It is freed with the model,
        and a copy has none of the model's hooks.

        Parameters
        ----------
        hk
            A callable that takes one ``Event``. It runs on the thread that
            sent the event, and the event is valid only during the call.
        knd
            The kind of events.
        lvl
            The lowest level the hook receives.

        Returns
        -------
        idx
            The id of the hook, for ``delhook``. Ids count from 1 in each
            model.

        Raises
        ------
        TypeError
            When ``hk`` is not callable.
        Error
            When the kind or the level is unknown.
        ValueError
            When the model is closed.

        See Also
        --------
        BIYOS_addmdlhook
        """
        hk = checked_hook(hk)
        kind = integer("knd", knd)
        level = integer("lvl", lvl)
        idx = c_int()
        with self._env._busy():
            self._addhook(hk, kind, level, idx)
        return idx.value

    def delhook(self, idx: int) -> None:
        """Remove a hook of the model.

        Parameters
        ----------
        idx
            The id ``addhook`` gave.

        Raises
        ------
        Error
            When the model has no hook of that id.
        ValueError
            When the model is closed.

        See Also
        --------
        BIYOS_delmdlhook
        """
        idx = integer("idx", idx)
        with self._env._busy():
            self._delhook(idx)

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
        return self._added(Variables, lib.BIYOS_addvar, n, *arrays)

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
        n = len(idx)
        chosen = self._chosen("BIYOS_setvarbnd", Variables, idx)
        arrays = (
            doubles("lb", lb, n),
            doubles("ub", ub, n),
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
        n = len(idx)
        chosen = self._chosen("BIYOS_setvarknd", Variables, idx)
        arrays = (ints("knd", knd, n),)
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

    def setvartol(
        self,
        idx: Sequence[int | Variable] | Variables,
        atol: Sequence[float] | None = None,
        rtol: Sequence[float] | None = None,
    ) -> None:
        """Set the tolerances of variables.

        A variable's tolerances stand for the options ``feastol_abs`` and
        ``feastol_rel`` for that variable alone: each finite and at least 0. A
        variable that held none keeps the options for the tolerance not given. A
        variable may appear more than once; its entries apply in order.

        Parameters
        ----------
        idx
            The variables.
        atol, rtol
            The new absolute and relative tolerance of each; ``None``
            leaves that tolerance as it is.

        See Also
        --------
        BIYOS_setvartolsp
        BIYOS_setvartolrg
        """
        n = len(idx)
        chosen = self._chosen("BIYOS_setvartol", Variables, idx)
        arrays = (
            doubles("atol", atol, n),
            doubles("rtol", rtol, n),
        )
        self._call(*chosen, *arrays)

    def getvartol(
        self, idx: Sequence[int | Variable] | Variables
    ) -> tuple[list[float | None], list[float | None]]:
        """Read the tolerances of variables.

        Parameters
        ----------
        idx
            The variables, in any order, any of them more than once.

        Returns
        -------
        atol
            The absolute tolerance of each, None where it holds none.
        rtol
            The relative tolerance of each, None where it holds none.

        See Also
        --------
        BIYOS_getvartolsp
        BIYOS_getvartolrg
        """
        pair = (c_double, c_double)
        atol, rtol = self._getlist("BIYOS_getvartol", Variables, idx, *pair)
        return _or_none(atol), _or_none(rtol)

    def delvartol(self, idx: Sequence[int | Variable] | Variables) -> None:
        """Return variables to ``feastol_abs`` and ``feastol_rel``.

        Parameters
        ----------
        idx
            The variables that lose their tolerances.

        See Also
        --------
        BIYOS_delvartolsp
        BIYOS_delvartolrg
        """
        self._call(*self._chosen("BIYOS_delvartol", Variables, idx))

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

    def delvar(self, idx: Sequence[int | Variable] | Variables) -> list[int]:
        """Delete variables, with their terms in every function.

        The variables left keep their order and are numbered from 0 again, and
        each term, set, indicator and node refers to them by their new indices.
        A deleted variable's kind, bounds, tolerances, name and terms go with
        it, and a function left with no term keeps its constant. Once any
        variable is deleted, the model refuses every block and view it gave
        before.

        Parameters
        ----------
        idx
            The variables to delete, in any order, none of them twice.

        Returns
        -------
        map
            For each variable the model held, its new index, or -1 when it is
            deleted.

        Raises
        ------
        Error
            When a variable to delete has a node, is a member of a set or is
            the binary of an indicator, or when the model has a callback and
            a variable is to be deleted. The model is then left as it was.

        See Also
        --------
        BIYOS_delvarsp
        BIYOS_delvarrg
        """
        return self._delete("BIYOS_delvar", Variables, idx)

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
        return self._added(Functions, lib.BIYOS_addfun, n)

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

    def getcst(self, idx: Sequence[int | Function] | Functions) -> list[float]:
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

    def delfun(self, idx: Sequence[int | Function] | Functions) -> list[int]:
        """Delete functions that nothing uses.

        The functions left keep their order and are numbered from 0 again, and
        each record and callback refers to them by their new indices. A deleted
        function's name, constant, terms and expression go with it, and the
        nodes of the expression stay. Once any function is deleted, the model
        refuses every block and view it gave before.

        Parameters
        ----------
        idx
            The functions to delete, in any order, none of them twice.

        Returns
        -------
        map
            For each function the model held, its new index, or -1 when it is
            deleted.

        Raises
        ------
        Error
            When a constraint, an objective, an indicator, a cone, a complement
            or a callback uses a function to delete. The model is then left as
            it was.

        See Also
        --------
        BIYOS_delfunsp
        BIYOS_delfunrg
        """
        return self._delete("BIYOS_delfun", Functions, idx)

    def addlinterm(
        self,
        fun: Sequence[int | Function] | Functions,
        var: Sequence[int | Variable] | Variables,
        coef: Sequence[float],
    ) -> None:
        """Add linear terms to functions, as coordinates.

        Term ``k`` adds ``coef[k]`` times variable ``var[k]`` to
        function ``fun[k]``, in any order. A variable the function holds
        already adds its coefficient to the one held.

        Parameters
        ----------
        fun
            The function of each term.
        var
            The variable of each term, each once for its function.
        coef
            The coefficient of each term.

        See Also
        --------
        BIYOS_addlintermsp
        """
        size = len(fun)
        arrays = (
            indices_of(self, Functions, "fun", fun, size),
            indices_of(self, Variables, "var", var, size),
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
        fun
            The function of each term, the terms of ``idx[0]`` first.
        var
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
        fun
            The function of each term, each variable's sorted by function.
        var
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
        fun: Sequence[int | Function] | Functions,
        var1: Sequence[int | Variable] | Variables,
        var2: Sequence[int | Variable] | Variables,
        coef: Sequence[float],
    ) -> None:
        """Add quadratic terms to functions, as coordinates.

        Term ``k`` adds ``coef[k]`` times the product of variables
        ``var1[k]`` and ``var2[k]`` to function ``fun[k]``, with
        no factor 1/2, in any order. The two variables of a pair may come
        in either order. A pair the function holds already adds its
        coefficient to the one held.

        Parameters
        ----------
        fun
            The function of each term.
        var1, var2
            The two variables of each term, each pair once for its
            function.
        coef
            The coefficient of each term.

        See Also
        --------
        BIYOS_addquadtermsp
        """
        size = len(fun)
        arrays = (
            indices_of(self, Functions, "fun", fun, size),
            indices_of(self, Variables, "var1", var1, size),
            indices_of(self, Variables, "var2", var2, size),
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
        fun
            The function of each term, the terms of ``idx[0]`` first.
        var1
            The first variable of each term, never above the second, each
            function's terms sorted by pair.
        var2
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
            The operator, any but ``Operator.VARIABLE``,
            ``Operator.PIECEWISE_LINEAR`` and ``Operator.NODE``:
            ``addvarnode`` and ``addpwlnode`` add the first two.
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
        cnt: Sequence[int],
        code: Sequence[Operator],
        arg: Sequence[int],
        val: Sequence[float] | None = None,
    ) -> list[int]:
        """Add expressions given in postfix.

        Expression ``k`` is the ``cnt[k]`` entries of ``code`` and ``arg``
        that follow those of expression ``k - 1``, each operand before the
        operator that takes it. A sum that is an operand of a sum built in
        the same expression is merged into it, and so is a product that is
        an operand of a product. A call that fails adds no node.

        Parameters
        ----------
        cnt
            The number of entries of each expression, none negative.
        code
            The code of each entry, ``sum(cnt)`` of them: an operator, or
            ``Operator.NODE``.
        arg
            The argument of each entry, ``sum(cnt)`` of them: the variable of
            ``Operator.VARIABLE``, the id of a node the model held before
            the call for ``Operator.NODE``, the pwl of
            ``Operator.PIECEWISE_LINEAR``, and the number of operands of
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
        n = len(cnt)
        sizes = counts("cnt", cnt, n)
        size = _total(sizes)
        codes = ints("code", code, size)
        args = int64s("arg", arg, size)
        values = doubles("val", val, postfix_values(codes, args))
        nodes = (c_int64 * n)()
        self._call(lib.BIYOS_addexpr, n, sizes, codes, args, values, nodes)
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

    def getnexpr(
        self, idx: Sequence[int | Function] | Functions | None = None
    ) -> tuple[int, int]:
        """Count the postfix entries and values of expressions of functions.

        Parameters
        ----------
        idx
            The functions, in any order, any of them more than once; every
            function by default.

        Returns
        -------
        nnz
            The number of entries ``getexpr`` returns for them.
        nval
            The number of values those entries take.

        See Also
        --------
        BIYOS_getnexprsp
        BIYOS_getnexprrg
        """
        every = range(self.getnfun()) if idx is None else idx
        nnz = c_int64()
        nval = c_int64()
        chosen = self._chosen("BIYOS_getnexpr", Functions, every)
        self._call(*chosen, byref(nnz), byref(nval))
        return nnz.value, nval.value

    def getexpr(
        self, idx: Sequence[int | Function] | Functions | None = None
    ) -> tuple[list[int], list[Operator], list[int], list[float]]:
        """Read the expressions of functions, in the postfix of ``addexpr``.

        The expression of a function is what stayed in the graph once its
        terms were split off; a function with no expression reads as the
        constant 0. A node the expressions use more than once is written
        once, followed by ``Operator.KEEP``, and named again by
        ``Operator.REFERENCE``. No entry is ``Operator.NODE``, so
        ``addexpr`` adds the same expressions to any model with at least as
        many variables.

        Parameters
        ----------
        idx
            The functions, in any order, any of them more than once; every
            function by default.

        Returns
        -------
        cnt
            The number of entries of each expression read.
        code
            The code of each entry.
        arg
            The argument of each entry, as ``addexpr`` takes it.
        val
            The values the entries take, one after another in their order.

        See Also
        --------
        BIYOS_getexprsp
        BIYOS_getexprrg
        """
        every = range(self.getnfun()) if idx is None else idx
        nnz, nval = self.getnexpr(every)
        chosen = self._chosen("BIYOS_getexpr", Functions, every)
        counts = (c_int64 * len(every))()
        codes = (c_int * nnz)()
        args = (c_int64 * nnz)()
        values = (c_double * nval)()
        self._call(*chosen, nnz, nval, counts, codes, args, values)
        operators = [Operator(code) for code in codes]
        return list(counts), operators, list(args), list(values)

    def addcb(
        self,
        fun: Sequence[int | Function] | Functions,
        data: object = None,
    ) -> int:
        """Add a callback that covers functions of the model.

        A function's nonlinear part is its expression plus the part of every
        callback that covers it, so a function may be in any number of
        callbacks and hold an expression beside them. The callback has no
        function yet and an empty pattern for each derivative; ``setcbf``,
        ``setcbg``, ``setcbh`` and ``setcbhv`` give them.

        Parameters
        ----------
        fun
            The functions, each listed once. Their order is the order of the
            callback's values.
        data
            The object passed to each of the callback's functions, None by
            default.

        Returns
        -------
        cb
            The index of the new callback, counted from 0.

        Notes
        -----
        Each function of a callback is called with a ``CallbackRequest``, a
        writable ``memoryview`` of floats to fill and the callback's
        ``data``. It returns None or 0 once the array is filled, a positive
        int when its functions are not defined at the point, and a negative
        int to stop the call that asked for it. An exception it raises
        counts as a negative int. The request's arrays and the array to
        fill are valid only during the call, and nothing derived from them
        may outlive it. The model keeps the functions and ``data`` alive,
        and a copy shares them. No call evaluates a callback yet.

        See Also
        --------
        BIYOS_addcb
        """
        n = len(fun)
        indices = indices_of(self, Functions, "fun", fun, n)
        cell = py_object(data)
        cb = c_int()
        self._callbacks.keep(cell)
        self._call(lib.BIYOS_addcb, n, indices, addressof(cell), byref(cb))
        self._callbacks.added(n)
        return cb.value

    def setcbf(self, cb: int, f: Callable[..., object]) -> None:
        """Give a callback the function that evaluates its values.

        Parameters
        ----------
        cb
            The callback.
        f
            The function, as ``addcb`` describes. It writes the callback's
            part of each covered function at the point, in the order given
            to ``addcb``.

        Raises
        ------
        TypeError
            When ``f`` is not callable.

        See Also
        --------
        BIYOS_setcbf
        """
        cb = integer("cb", cb)
        count = self._callbacks.count(cb)
        caller = Caller(checked("f", f), count, count)
        self._callbacks.keep(caller)
        self._call(lib.BIYOS_setcbf, cb, caller.pointer)

    def setcbg(
        self,
        cb: int,
        fun: Sequence[int | Function] | Functions,
        var: Sequence[int | Variable] | Variables,
        g: Callable[..., object],
    ) -> None:
        """Give a callback its Jacobian pattern and the function that
        evaluates it.

        Entry ``k`` is the derivative of the callback's part of function
        ``fun[k]`` with respect to variable ``var[k]``. The entries may come
        in any order, and an entry given more than once has its values
        summed. A variable left out of a function's entries has a zero
        derivative in the callback's part of that function.

        Parameters
        ----------
        cb
            The callback.
        fun
            The function of each entry, one the callback covers.
        var
            The variable of each entry.
        g
            The function, as ``addcb`` describes. It writes the derivative
            of each entry at the point, in the order of the pattern.

        Raises
        ------
        TypeError
            When ``g`` is not callable.

        See Also
        --------
        BIYOS_setcbg
        """
        cb = integer("cb", cb)
        nnz = len(fun)
        functions = indices_of(self, Functions, "fun", fun, nnz)
        variables = indices_of(self, Variables, "var", var, nnz)
        caller = Caller(checked("g", g), self._callbacks.count(cb), nnz)
        pattern = (nnz, functions, variables, caller.pointer)
        self._callbacks.keep(caller)
        self._call(lib.BIYOS_setcbg, cb, *pattern)

    def setcbh(
        self,
        cb: int,
        var1: Sequence[int | Variable] | Variables,
        var2: Sequence[int | Variable] | Variables,
        h: Callable[..., object],
    ) -> None:
        """Give a callback its Hessian pattern and the function that
        evaluates it.

        The pairs may come in any order, and a pair given more than once has
        its values summed. They cover every pair of variables ``i <= j``
        where a second derivative of the callback's part of some covered
        function may be nonzero.

        Parameters
        ----------
        cb
            The callback.
        var1
            The first variable of each pair.
        var2
            The second variable of each pair, no less than the first.
        h
            The function, as ``addcb`` describes. It writes, for each pair,
            the entry of the sum of the Hessians of the callback's parts,
            each weighted by ``req.w``, at the point.

        Raises
        ------
        TypeError
            When ``h`` is not callable.

        See Also
        --------
        BIYOS_setcbh
        """
        cb = integer("cb", cb)
        nnz = len(var1)
        first = indices_of(self, Variables, "var1", var1, nnz)
        second = indices_of(self, Variables, "var2", var2, nnz)
        caller = Caller(checked("h", h), self._callbacks.count(cb), nnz)
        pairs = (nnz, first, second, caller.pointer)
        self._callbacks.keep(caller)
        self._call(lib.BIYOS_setcbh, cb, *pairs)

    def setcbhv(self, cb: int, hv: Callable[..., object]) -> None:
        """Give a callback the function that multiplies its Hessian by a
        vector.

        Parameters
        ----------
        cb
            The callback.
        hv
            The function, as ``addcb`` describes. It writes, for every
            variable, the entry of the product of ``req.v`` with the sum
            of the Hessians of the callback's parts, each weighted by
            ``req.w``, at the point.

        Raises
        ------
        TypeError
            When ``hv`` is not callable.

        See Also
        --------
        BIYOS_setcbhv
        """
        cb = integer("cb", cb)
        function = checked("hv", hv)
        caller = Caller(function, self._callbacks.count(cb), None)
        self._callbacks.keep(caller)
        self._call(lib.BIYOS_setcbhv, cb, caller.pointer)

    def setcbdata(self, cb: int, data: object) -> None:
        """Replace the object a callback's functions receive.

        Parameters
        ----------
        cb
            The callback.
        data
            The new object, for every later call of the callback. A copy of
            the model keeps the object it had when it was made.

        See Also
        --------
        BIYOS_setcbdata
        """
        cell = py_object(data)
        self._callbacks.keep(cell)
        self._call(lib.BIYOS_setcbdata, integer("cb", cb), addressof(cell))

    def getcbdata(self, cb: int) -> object:
        """Return the object a callback's functions receive.

        Parameters
        ----------
        cb
            The callback.

        Returns
        -------
        data
            The object, as last given.

        See Also
        --------
        BIYOS_getcbdata
        """
        found = c_void_p()
        self._call(lib.BIYOS_getcbdata, integer("cb", cb), byref(found))
        return data_at(found.value)

    def getncbfun(self, cb: int) -> int:
        """Return the number of functions a callback covers.

        Parameters
        ----------
        cb
            The callback.

        Returns
        -------
        n
            The number of functions.

        See Also
        --------
        BIYOS_getncbfun
        """
        count = c_int()
        self._call(lib.BIYOS_getncbfun, integer("cb", cb), byref(count))
        return count.value

    def getcbfun(self, cb: int) -> list[int]:
        """Read the functions a callback covers, as ``addcb`` takes them.

        Parameters
        ----------
        cb
            The callback.

        Returns
        -------
        fun
            The functions, in the order given to ``addcb``.

        See Also
        --------
        BIYOS_getcbfun
        """
        n = self.getncbfun(cb)
        found = (c_int * n)()
        self._call(lib.BIYOS_getcbfun, integer("cb", cb), n, found)
        return list(found)

    def addcon(
        self,
        fun: Sequence[int | Function]
        | Functions
        | Comparison
        | Sequence[Comparison],
        lb: Sequence[float] | None = None,
        ub: Sequence[float] | None = None,
    ) -> Constraints:
        """Add constraints after the last one, one for each function given.

        A comparison, or a list of them, adds a function for each, holding
        the expression its constraint bounds, and takes its bounds from the
        comparison. Every expression goes to the model in one ``addexpr``
        call, then ``attachexpr`` splits off its constant, linear and
        quadratic terms. When the model refuses a later step, the nodes and
        functions already added stay in it.

        Parameters
        ----------
        fun
            The function each new constraint bounds, or comparisons such as
            ``x[0] * x[1] <= 3``.
        lb, ub
            The bounds of each, unbounded by default. None with comparisons.

        Returns
        -------
        cons
            The new constraints.

        Raises
        ------
        TypeError
            When ``lb`` or ``ub`` comes with comparisons, or a list mixes
            comparisons with other values.
        ValueError
            When an expression holds a variable of another model, or one
            made before a deletion from the model.

        See Also
        --------
        BIYOS_addcon
        """
        cons = given(fun, Comparison, "a comparison")
        if cons is not None:
            return self._compared(cons, lb, ub)
        funs = cast("Sequence[int | Function] | Functions", fun)
        n = len(funs)
        arrays = (
            indices_of(self, Functions, "fun", funs, n),
            doubles("lb", lb, n),
            doubles("ub", ub, n),
        )
        return self._added(Constraints, lib.BIYOS_addcon, n, *arrays)

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
        n = len(idx)
        chosen = self._chosen("BIYOS_setconbnd", Constraints, idx)
        arrays = (
            doubles("lb", lb, n),
            doubles("ub", ub, n),
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

    def setcontol(
        self,
        idx: Sequence[int | Constraint] | Constraints,
        atol: Sequence[float] | None = None,
        rtol: Sequence[float] | None = None,
    ) -> None:
        """Set the tolerances of constraints.

        A constraint's tolerances stand for the options ``feastol_abs`` and
        ``feastol_rel`` for that constraint alone: each finite and at least 0. A
        constraint that held none keeps the options for the tolerance not given.
        A constraint may appear more than once; its entries apply in order.

        Parameters
        ----------
        idx
            The constraints.
        atol, rtol
            The new absolute and relative tolerance of each; ``None``
            leaves that tolerance as it is.

        See Also
        --------
        BIYOS_setcontolsp
        BIYOS_setcontolrg
        """
        n = len(idx)
        chosen = self._chosen("BIYOS_setcontol", Constraints, idx)
        arrays = (
            doubles("atol", atol, n),
            doubles("rtol", rtol, n),
        )
        self._call(*chosen, *arrays)

    def getcontol(
        self, idx: Sequence[int | Constraint] | Constraints
    ) -> tuple[list[float | None], list[float | None]]:
        """Read the tolerances of constraints.

        Parameters
        ----------
        idx
            The constraints, in any order, any of them more than once.

        Returns
        -------
        atol
            The absolute tolerance of each, None where it holds none.
        rtol
            The relative tolerance of each, None where it holds none.

        See Also
        --------
        BIYOS_getcontolsp
        BIYOS_getcontolrg
        """
        pair = (c_double, c_double)
        atol, rtol = self._getlist("BIYOS_getcontol", Constraints, idx, *pair)
        return _or_none(atol), _or_none(rtol)

    def delcontol(self, idx: Sequence[int | Constraint] | Constraints) -> None:
        """Return constraints to ``feastol_abs`` and ``feastol_rel``.

        Parameters
        ----------
        idx
            The constraints that lose their tolerances.

        See Also
        --------
        BIYOS_delcontolsp
        BIYOS_delcontolrg
        """
        self._call(*self._chosen("BIYOS_delcontol", Constraints, idx))

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
        fun
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

    def delconname(self, idx: Sequence[int | Constraint] | Constraints) -> None:
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

    def delcon(
        self, idx: Sequence[int | Constraint] | Constraints
    ) -> list[int]:
        """Delete constraints.

        The constraints left keep their order and are numbered from 0 again. A
        deleted constraint's tolerances and name go with it, and its function
        stays. Once any constraint is deleted, the model refuses every block
        and view it gave before.

        Parameters
        ----------
        idx
            The constraints to delete, in any order, none of them twice.

        Returns
        -------
        map
            For each constraint the model held, its new index, or -1 when it is
            deleted.

        See Also
        --------
        BIYOS_delconsp
        BIYOS_delconrg
        """
        return self._delete("BIYOS_delcon", Constraints, idx)

    def addobj(
        self,
        fun: Sequence[int | Function]
        | Functions
        | Expression
        | Variable
        | Sequence[Expression | Variable],
        sense: Sequence[ObjectiveSense] | None = None,
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

        An expression or a variable, or a list of them, adds a function for
        each, as ``addcon`` does for comparisons.

        Parameters
        ----------
        fun
            The function each new objective optimizes, or expressions such
            as ``x[0] + biyos.exp(x[1])``.
        sense
            Whether each is minimized or maximized, minimized by default.
        prio
            The priority of each. By default each takes its index at the
            time it is added, so the objectives are optimized one after
            another in the order they are added. Once an objective is
            deleted, a default can repeat a priority already held.
        coef
            The weight of each in the sum of its stage, finite and
            positive, 1 by default.

        Returns
        -------
        objs
            The new objectives.

        Raises
        ------
        TypeError
            When a list mixes expressions with other values.
        ValueError
            When an expression holds a variable of another model, or one
            made before a deletion from the model.

        See Also
        --------
        BIYOS_addobj
        """
        trees = given(fun, Operand, "an expression")
        funs = cast("Sequence[int | Function] | Functions", fun)
        n = len(funs) if trees is None else len(trees)
        arrays = (
            ints("sense", sense, n),
            ints("prio", prio, n),
            doubles("coef", coef, n),
        )
        if trees is not None:
            funs = attached(self, trees)
        idx = indices_of(self, Functions, "fun", funs, n)
        return self._added(Objectives, lib.BIYOS_addobj, n, idx, *arrays)

    def setobjsense(
        self,
        idx: Sequence[int | Objective] | Objectives,
        sense: Sequence[ObjectiveSense] | None,
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
        arrays = (ints("sense", sense, n),)
        self._call(*chosen, *arrays)

    def getobjsense(
        self, idx: Sequence[int | Objective] | Objectives
    ) -> list[ObjectiveSense]:
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
        return [ObjectiveSense(value) for value in found[0]]

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
        arrays = (ints("prio", prio, n),)
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
        arrays = (doubles("coef", coef, n),)
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
    ) -> tuple[list[int], list[ObjectiveSense], list[int], list[float]]:
        """Read objectives.

        Parameters
        ----------
        idx
            The objectives, in any order, any of them more than once; every
            objective by default.

        Returns
        -------
        fun
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
        senses = [ObjectiveSense(code) for code in codes]
        return functions, senses, priorities, weights

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

    def delobj(self, idx: Sequence[int | Objective] | Objectives) -> list[int]:
        """Delete objectives.

        The objectives left keep their order and are numbered from 0 again. A
        deleted objective's name goes with it, and its function stays. Once any
        objective is deleted, the model refuses every block and view it gave
        before.

        Parameters
        ----------
        idx
            The objectives to delete, in any order, none of them twice.

        Returns
        -------
        map
            For each objective the model held, its new index, or -1 when it is
            deleted.

        See Also
        --------
        BIYOS_delobjsp
        BIYOS_delobjrg
        """
        return self._delete("BIYOS_delobj", Objectives, idx)

    def addind(
        self,
        var: Sequence[int | Variable] | Variables,
        val: Sequence[int],
        fun: Sequence[int | Function] | Functions,
        lb: Sequence[float] | None = None,
        ub: Sequence[float] | None = None,
    ) -> Indicators:
        """Add indicators after the last one, one for each variable given.

        An indicator bounds a function while a binary variable takes a
        value: when variable ``var[j]`` equals ``val[j]``, function
        ``fun[j]`` lies within its bounds, and otherwise it is free. A
        variable stays binary while an indicator uses it.

        Parameters
        ----------
        var
            The binary variable that switches each new indicator on.
        val
            The value of each variable that switches its indicator on, 0 or
            1, as many as ``var``.
        fun
            The function each indicator bounds, as many as ``var``.
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
        n = len(var)
        arrays = (
            indices_of(self, Variables, "var", var, n),
            ints("val", val, n),
            indices_of(self, Functions, "fun", fun, n),
            doubles("lb", lb, n),
            doubles("ub", ub, n),
        )
        return self._added(Indicators, lib.BIYOS_addind, n, *arrays)

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
        n = len(idx)
        chosen = self._chosen("BIYOS_setindbnd", Indicators, idx)
        arrays = (
            doubles("lb", lb, n),
            doubles("ub", ub, n),
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
        var: Sequence[int | Variable] | Variables | None = None,
        val: Sequence[int] | None = None,
    ) -> None:
        """Set the variables that switch indicators on, and their values.

        Parameters
        ----------
        idx
            The indicators.
        var
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
        n = len(idx)
        chosen = self._chosen("BIYOS_setindvar", Indicators, idx)
        arrays = (
            indices_of(self, Variables, "var", var, n),
            ints("val", val, n),
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
        var
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
        var
            The variable that switches each indicator on.
        val
            The value of each variable that switches its indicator on.
        fun
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

    def delind(self, idx: Sequence[int | Indicator] | Indicators) -> list[int]:
        """Delete indicators.

        The indicators left keep their order and are numbered from 0 again. A
        deleted indicator's name goes with it, and its variable and function
        stay. Once any indicator is deleted, the model refuses every block and
        view it gave before.

        Parameters
        ----------
        idx
            The indicators to delete, in any order, none of them twice.

        Returns
        -------
        map
            For each indicator the model held, its new index, or -1 when it is
            deleted.

        See Also
        --------
        BIYOS_delindsp
        BIYOS_delindrg
        """
        return self._delete("BIYOS_delind", Indicators, idx)

    def addsos(
        self,
        knd: Sequence[SpecialOrderedSetKind],
        cnt: Sequence[int],
        var: Sequence[int | Variable] | Variables,
        coef: Sequence[float] | None = None,
    ) -> SpecialOrderedSets:
        """Add special ordered sets after the last one.

        The set ``k`` holds the ``cnt[k]`` members that follow those of set
        ``k - 1``, kept in that order whatever their weights. In a set of kind
        ``ONE`` at most one member is nonzero; in a set of kind ``TWO`` at
        most two are, adjacent in the order of their weights. A variable is
        a member of a set at most once and may belong to several sets.

        Parameters
        ----------
        knd
            The kind of each new set.
        cnt
            The number of members of each new set, none negative.
        var
            The variable of each member, ``sum(cnt)`` of them.
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
        if cnt is None:
            raise TypeError("cnt must be a sequence of int, not NoneType")
        n = len(knd)
        sizes = counts("cnt", cnt, n)
        size = _total(sizes)
        arrays = (
            ints("knd", knd, n),
            sizes,
            indices_of(self, Variables, "var", var, size),
            doubles("coef", coef, size),
        )
        return self._added(SpecialOrderedSets, lib.BIYOS_addsos, n, *arrays)

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

    def getnsosvar(
        self,
        idx: Sequence[int | SpecialOrderedSet]
        | SpecialOrderedSets
        | None = None,
    ) -> int:
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
        return self._count("BIYOS_getnsosvar", SpecialOrderedSets, every)

    def getsos(
        self,
        idx: Sequence[int | SpecialOrderedSet]
        | SpecialOrderedSets
        | None = None,
    ) -> tuple[list[SpecialOrderedSetKind], list[int], list[int], list[float]]:
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
        cnt
            The number of members of each set.
        var
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
        n = len(every)
        kinds = (c_int * n)()
        counts = (c_int64 * n)()
        members = (c_int * size)()
        weights = (c_double * size)()
        chosen = self._chosen("BIYOS_getsos", SpecialOrderedSets, every)
        self._call(*chosen, size, kinds, counts, members, weights)
        found = [SpecialOrderedSetKind(code) for code in kinds]
        return found, list(counts), list(members), list(weights)

    def setsosname(
        self,
        idx: Sequence[int | SpecialOrderedSet] | SpecialOrderedSets,
        name: Sequence[str],
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
        self._setname("BIYOS_setsosname", SpecialOrderedSets, idx, name)

    def getsosname(
        self, idx: Sequence[int | SpecialOrderedSet] | SpecialOrderedSets
    ) -> list[str | None]:
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
        return self._getname("BIYOS_getsosname", SpecialOrderedSets, idx)

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

    def delsosname(
        self, idx: Sequence[int | SpecialOrderedSet] | SpecialOrderedSets
    ) -> None:
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
        self._delname("BIYOS_delsosname", SpecialOrderedSets, idx)

    def delsos(
        self, idx: Sequence[int | SpecialOrderedSet] | SpecialOrderedSets
    ) -> list[int]:
        """Delete special ordered sets.

        The sets left keep their order and are numbered from 0 again. A deleted
        set's members and name go with it, and its variables stay. Once any set
        is deleted, the model refuses every block and view it gave before.

        Parameters
        ----------
        idx
            The sets to delete, in any order, none of them twice.

        Returns
        -------
        map
            For each set the model held, its new index, or -1 when it is
            deleted.

        See Also
        --------
        BIYOS_delsossp
        BIYOS_delsosrg
        """
        return self._delete("BIYOS_delsos", SpecialOrderedSets, idx)

    def addpwl(
        self,
        cnt: Sequence[int],
        x: Sequence[float],
        y: Sequence[float],
    ) -> PiecewiseLinears:
        """Add piecewise linear functions after the last one.

        Pwl ``k`` passes through the ``cnt[k]`` points of ``x`` and ``y``
        that follow those of pwl ``k - 1``. It is linear between consecutive
        points, extends its end segments past its first and last points,
        and takes the value ``y[i]`` at ``x[i]`` exactly. A pwl is data: it
        takes no name, is never deleted, and ``addpwlnode`` applies it
        to a node, ``piecewise`` to an expression. A call that fails adds
        no pwl.

        Parameters
        ----------
        cnt
            The number of points of each new pwl, 2 or more.
        x
            The abscissa of each point, ``sum(cnt)`` of them, finite and
            strictly increasing within a pwl.
        y
            The value of each point, finite.

        Returns
        -------
        pwls
            The new pwls.

        See Also
        --------
        BIYOS_addpwl
        """
        if cnt is None:
            raise TypeError("cnt must be a sequence of int, not NoneType")
        n = len(cnt)
        sizes = counts("cnt", cnt, n)
        size = _total(sizes)
        arrays = (sizes, doubles("x", x, size), doubles("y", y, size))
        return self._added(PiecewiseLinears, lib.BIYOS_addpwl, n, *arrays)

    def getnpwl(self) -> int:
        """Return the number of piecewise linear functions.

        Returns
        -------
        n
            The number of pwls.

        See Also
        --------
        BIYOS_getnpwl
        """
        count = c_int()
        self._call(lib.BIYOS_getnpwl, byref(count))
        return count.value

    def getnpwlpt(
        self,
        idx: Sequence[int | PiecewiseLinear] | PiecewiseLinears | None = None,
    ) -> int:
        """Count the points of piecewise linear functions.

        Parameters
        ----------
        idx
            The pwls, in any order, any of them more than once; every pwl by
            default.

        Returns
        -------
        nnz
            The number of points they hold, a pwl counted each time it
            appears.

        See Also
        --------
        BIYOS_getnpwlptsp
        BIYOS_getnpwlptrg
        """
        every = range(self.getnpwl()) if idx is None else idx
        return self._count("BIYOS_getnpwlpt", PiecewiseLinears, every)

    def getpwl(
        self,
        idx: Sequence[int | PiecewiseLinear] | PiecewiseLinears | None = None,
    ) -> tuple[list[int], list[float], list[float]]:
        """Read piecewise linear functions, as ``addpwl`` takes them.

        Parameters
        ----------
        idx
            The pwls, in any order, any of them more than once; every pwl by
            default.

        Returns
        -------
        cnt
            The number of points of each pwl.
        x
            The abscissa of each point, each pwl's in the order added.
        y
            The value of each point.

        See Also
        --------
        BIYOS_getpwlsp
        BIYOS_getpwlrg
        """
        every = range(self.getnpwl()) if idx is None else idx
        size = self.getnpwlpt(every)
        sizes = (c_int64 * len(every))()
        xs = (c_double * size)()
        ys = (c_double * size)()
        chosen = self._chosen("BIYOS_getpwl", PiecewiseLinears, every)
        self._call(*chosen, size, sizes, xs, ys)
        return list(sizes), list(xs), list(ys)

    def addpwlnode(self, pwl: int | PiecewiseLinear, arg: int) -> int:
        """Add the node that applies a piecewise linear function to a node.

        Any number of nodes may apply one pwl. A call that fails adds no
        node.

        Parameters
        ----------
        pwl
            The pwl.
        arg
            The id of the node it applies to.

        Returns
        -------
        node
            The id of the new node.

        See Also
        --------
        BIYOS_addpwlnode
        """
        pwl = index_of(self, PiecewiseLinears, "pwl", pwl)
        node = c_int64()
        self._call(lib.BIYOS_addpwlnode, pwl, offset("arg", arg), byref(node))
        return node.value

    def addcone(
        self,
        knd: Sequence[ConeKind],
        cnt: Sequence[int],
        fun: Sequence[int | Function] | Functions,
        prmcnt: Sequence[int] | None = None,
        prm: Sequence[float] | None = None,
    ) -> Cones:
        """Add cones after the last one.

        Cone ``k`` places the ``cnt[k]`` functions of ``fun`` that follow
        those of cone ``k - 1``, in that order, in a cone of kind ``knd[k]``
        whose parameters are the ``prmcnt[k]`` of ``prm`` that follow its
        parameters. A call that fails adds no cone.

        Parameters
        ----------
        knd
            The kind of each cone.
        cnt
            The number of members of each cone, none negative.
        fun
            The members of each cone, ``sum(cnt)`` of them, a function at
            most once in a cone.
        prmcnt
            The number of parameters of each cone, none negative. None when
            no cone takes one.
        prm
            The parameters of each cone, ``sum(prmcnt)`` of them: the
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
        sizes = counts("cnt", cnt, n)
        weights = counts("prmcnt", prmcnt, n)
        arrays = (
            ints("knd", knd, n),
            sizes,
            indices_of(self, Functions, "fun", fun, _total(sizes)),
            weights,
            doubles("prm", prm, _total(weights)),
        )
        return self._added(Cones, lib.BIYOS_addcone, n, *arrays)

    def setconetol(
        self,
        idx: Sequence[int | Cone] | Cones,
        atol: Sequence[float] | None = None,
        rtol: Sequence[float] | None = None,
    ) -> None:
        """Set the tolerances of cones.

        A cone's tolerances stand for the options ``feastol_abs`` and
        ``feastol_rel`` for that cone alone: each finite and at least 0. A cone
        that held none keeps the options for the tolerance not given. A cone may
        appear more than once; its entries apply in order.

        Parameters
        ----------
        idx
            The cones.
        atol, rtol
            The new absolute and relative tolerance of each; ``None``
            leaves that tolerance as it is.

        See Also
        --------
        BIYOS_setconetolsp
        BIYOS_setconetolrg
        """
        n = len(idx)
        chosen = self._chosen("BIYOS_setconetol", Cones, idx)
        arrays = (
            doubles("atol", atol, n),
            doubles("rtol", rtol, n),
        )
        self._call(*chosen, *arrays)

    def getconetol(
        self, idx: Sequence[int | Cone] | Cones
    ) -> tuple[list[float | None], list[float | None]]:
        """Read the tolerances of cones.

        Parameters
        ----------
        idx
            The cones, in any order, any of them more than once.

        Returns
        -------
        atol
            The absolute tolerance of each, None where it holds none.
        rtol
            The relative tolerance of each, None where it holds none.

        See Also
        --------
        BIYOS_getconetolsp
        BIYOS_getconetolrg
        """
        pair = (c_double, c_double)
        atol, rtol = self._getlist("BIYOS_getconetol", Cones, idx, *pair)
        return _or_none(atol), _or_none(rtol)

    def delconetol(self, idx: Sequence[int | Cone] | Cones) -> None:
        """Return cones to ``feastol_abs`` and ``feastol_rel``.

        Parameters
        ----------
        idx
            The cones that lose their tolerances.

        See Also
        --------
        BIYOS_delconetolsp
        BIYOS_delconetolrg
        """
        self._call(*self._chosen("BIYOS_delconetol", Cones, idx))

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
        cnt
            The number of members of each cone.
        fun
            The members of each cone in order, the ``cnt[j]`` of entry ``j``
            following those of entry ``j - 1``.

        See Also
        --------
        BIYOS_getconesp
        BIYOS_getconerg
        """
        every = range(self.getncone()) if idx is None else idx
        size = self._count("BIYOS_getnconefun", Cones, every)
        chosen = self._chosen("BIYOS_getcone", Cones, every)
        kinds = (c_int * len(every))()
        counts = (c_int64 * len(every))()
        members = (c_int * size)()
        self._call(*chosen, size, kinds, counts, members)
        found = [ConeKind(kind) for kind in kinds]
        return found, list(counts), list(members)

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
        prmcnt
            The number of parameters of each cone.
        prm
            The parameters of each cone in order, the ``prmcnt[j]`` of entry
            ``j`` following those of entry ``j - 1``.

        See Also
        --------
        BIYOS_getconeprmsp
        BIYOS_getconeprmrg
        """
        every = range(self.getncone()) if idx is None else idx
        size = self._count("BIYOS_getnconeprm", Cones, every)
        chosen = self._chosen("BIYOS_getconeprm", Cones, every)
        counts = (c_int64 * len(every))()
        values = (c_double * size)()
        self._call(*chosen, size, counts, values)
        return list(counts), list(values)

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

    def delcone(self, idx: Sequence[int | Cone] | Cones) -> list[int]:
        """Delete cones.

        The cones left keep their order and are numbered from 0 again. A
        deleted cone's members, parameters, tolerances and name go with it, and
        its functions stay. Once any cone is deleted, the model refuses every
        block and view it gave before.

        Parameters
        ----------
        idx
            The cones to delete, in any order, none of them twice.

        Returns
        -------
        map
            For each cone the model held, its new index, or -1 when it is
            deleted.

        See Also
        --------
        BIYOS_delconesp
        BIYOS_delconerg
        """
        return self._delete("BIYOS_delcone", Cones, idx)

    def addccon(
        self,
        fun1: Sequence[int | Function] | Functions,
        fun2: Sequence[int | Function] | Functions,
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
        fun1
            The bounded function ``f1`` of each new complement.
        fun2
            The function ``f2`` complementary to it, as many as
            ``fun1``.
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
        n = len(fun1)
        arrays = (
            indices_of(self, Functions, "fun1", fun1, n),
            indices_of(self, Functions, "fun2", fun2, n),
            doubles("lb", lb, n),
            doubles("ub", ub, n),
        )
        return self._added(Complements, lib.BIYOS_addccon, n, *arrays)

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
        n = len(idx)
        chosen = self._chosen("BIYOS_setcconbnd", Complements, idx)
        arrays = (
            doubles("lb", lb, n),
            doubles("ub", ub, n),
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

    def delccontol(self, idx: Sequence[int | Complement] | Complements) -> None:
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
        fun1
            The bounded function of each complement.
        fun2
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

    def delccon(
        self, idx: Sequence[int | Complement] | Complements
    ) -> list[int]:
        """Delete complements.

        The complements left keep their order and are numbered from 0 again. A
        deleted complement's tolerances and name go with it, and its functions
        stay. Once any complement is deleted, the model refuses every block and
        view it gave before.

        Parameters
        ----------
        idx
            The complements to delete, in any order, none of them twice.

        Returns
        -------
        map
            For each complement the model held, its new index, or -1 when it is
            deleted.

        See Also
        --------
        BIYOS_delcconsp
        BIYOS_delcconrg
        """
        return self._delete("BIYOS_delccon", Complements, idx)

    def _compared(
        self,
        cons: list[Comparison],
        lb: Sequence[float] | None,
        ub: Sequence[float] | None,
    ) -> Constraints:
        if lb is not None or ub is not None:
            raise TypeError("lb and ub come from the comparisons")
        funs = attached(self, [con._body for con in cons])
        lower = [con._lb for con in cons]
        upper = [con._ub for con in cons]
        return self.addcon(funs, lower, upper)

    def _start(self, env: Env) -> None:
        self._env = env
        self._handle = c_void_p()
        self._calls = 0
        self._closing = False
        self._gen = 0
        self._callbacks = Callbacks()
        self._hooks = Hooks(env._relay, env._owners)

    def _make(self, call: Callable[..., None], *arguments: object) -> None:
        models = self._env._models
        handle = self._handle
        models.add(handle)
        try:
            call(*arguments, byref(handle))
        finally:
            if handle.value is None:
                models.take(handle)
            else:
                self._env._owners[handle.value] = self

    def _copy(self, source: "Model") -> None:
        if not self._env._open():
            raise ValueError("the environment is closed")
        target = self._env._handle
        self._callbacks = source._callbacks.copy()
        self._make(source._call, lib.BIYOS_copymdl, target)

    def _hold(self) -> None:
        park = self._env._models.park
        callback = self._hooks.callback
        self._free = weakref.finalize(self, park, self._handle, callback)

    def _addhook(
        self,
        hk: Callable[[Event], object],
        knd: int,
        lvl: int,
        idx: c_int,
    ) -> None:
        callback = self._hooks.callback
        out = byref(idx)
        key = self._hooks.add(hk)
        try:
            self._call(lib.BIYOS_addmdlhook, callback, key, knd, lvl, out)
        finally:
            self._hooks.settle(key, idx.value)

    def _delhook(self, idx: int) -> None:
        self._call(lib.BIYOS_delmdlhook, idx)
        self._hooks.remove(idx)

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
        start = integer("the start of idx", span.start)
        stop = integer("the stop of idx", min(start + len(span), _INT_MAX))
        function = getattr(lib, f"{call}rg")
        if outputs > 0 and not self._inside(family, start, stop):
            self._call(function, start, stop, *([None] * outputs))
        return function, start, stop

    def _inside(
        self, family: type[Block[Any]], start: int, stop: int
    ) -> bool:
        total: int = getattr(self, _COUNTS[family])()
        return 0 <= start <= stop <= total

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

    def _delete(
        self,
        call: str,
        family: type[Block[Any]],
        idx: Sequence[SupportsIndex] | Block[Any],
    ) -> list[int]:
        chosen = self._chosen(call, family, idx)
        gone = self._gone(family, idx)
        total: int = getattr(self, _COUNTS[family])()
        self._call(*chosen, None)
        if len(idx) > 0:
            self._gen += 1
        return _renumbered(total, gone)

    def _gone(
        self,
        family: type[Block[Any]],
        idx: Sequence[SupportsIndex] | Block[Any],
    ) -> set[int]:
        span = span_of(self, family, "idx", idx)
        if span is not None:
            return set(span)
        return {index_of(self, family, "idx", value) for value in idx}

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
        counts = (c_int64 * len(span))()
        others = found[:place] + found[place + 1 :]
        self._call(*chosen, size, counts, *others, values)
        columns = [list(column) for column in found]
        columns[place] = _expanded(counts, span.start)
        return columns, list(values)

    def _getidx(
        self, function: Callable[..., int], name: Sequence[str]
    ) -> list[int]:
        n = len(listed("name", name))
        names = strings("name", name, n)
        found = (c_int * n)()
        self._call(function, n, names, found)
        return list(found)

    def _added(
        self,
        family: type[_Block],
        function: Callable[..., int],
        n: int,
        *arguments: object,
    ) -> _Block:
        pos = c_int()
        self._call(function, n, *arguments, byref(pos))
        return family(self, pos.value, pos.value + n, self._gen)

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
