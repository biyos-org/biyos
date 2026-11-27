import weakref
from collections.abc import Callable, Iterator, Sequence, Sized
from contextlib import contextmanager
from ctypes import (
    Array,
    addressof,
    byref,
    c_char_p,
    c_void_p,
    py_object,
    sizeof,
)
from enum import IntEnum
from functools import partial
from math import isnan
from os import PathLike, fspath
from typing import (
    Any,
    NoReturn,
    SupportsIndex,
    TypeVar,
    cast,
    overload,
)

from biyos import _capi
from biyos._capi import lib
from biyos._convert.arrays import (
    counted,
    counts,
    doubles,
    entries_of,
    index_array,
    index_view,
    indices_of,
    int64s,
    ints,
    listed,
    matched,
    nested,
    nested_doubles,
    nested_indices,
    picked,
    sequence,
    span_of,
    strings,
)
from biyos._convert.integers import integer, offset
from biyos._convert.numbers import (
    Index,
    Offset,
    Real,
    c_index,
    c_integer,
    c_offset,
    c_real,
)
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
    Many,
    Objective,
    Objectives,
    One,
    PiecewiseLinear,
    PiecewiseLinears,
    Selection,
    SpecialOrderedSet,
    SpecialOrderedSets,
    Variable,
    Variables,
    View,
    idx_of,
    index_of,
)
from biyos._model.callbacks import Callbacks, checked, data_at
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
    FIXED_MPS
        Fixed MPS, for linear and mixed-integer models. Read only.
    NL
        Text NL, for nonlinear models. Read only.
    """

    MPS = 1
    FIXED_MPS = 2
    NL = 3


_INT_MAX = 2 ** (8 * sizeof(c_index) - 1) - 1
_Block = TypeVar("_Block", bound=Block[View])
_Argument = tuple[type[Block[View]], str, Any]
_Column = type[c_integer | c_real] | type[Block[View]]

_COUNTS: dict[type[Block[View]], str] = {
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


def _kept(found: list[str], event: Event) -> None:
    if event.level == EventLevel.INFO:
        found.append(event.text)


def _expanded(
    counts: Array[c_offset], records: Sequence[Index]
) -> list[Index]:
    pairs = zip(records, counts)
    return [record for record, count in pairs for _ in range(count)]


def _or_none(value: Real) -> Real | None:
    return None if isnan(value) else value


def _each(convert: Callable[[Any], object], found: Any) -> Any:
    if isinstance(found, list):
        return [convert(value) for value in found]
    return convert(found)


def _renumbered(total: Index, gone: set[Index]) -> list[Index]:
    found: list[Index] = []
    kept = 0
    for k in range(total):
        if k in gone:
            found.append(-1)
        else:
            found.append(kept)
            kept += 1
    return found


def _alike(name: str, entries: object, many: bool) -> None:
    if many:
        sequence(name, entries)
    elif isinstance(entries, Sized):
        found = type(entries).__name__
        raise TypeError(f"{name} must be a scalar, not {found}")


def _lifted(entries: object, many: bool) -> list[object]:
    return [entries] if many else [[entries]]


def _single(
    others: Sequence[_Argument], coef: object
) -> tuple[list[_Argument], list[object]]:
    many = isinstance(others[0][2], Sized)
    for _, name, entries in others[1:]:
        _alike(name, entries, many)
    _alike("coef", coef, many)
    lifted = [
        (family, name, _lifted(entries, many))
        for family, name, entries in others
    ]
    return lifted, _lifted(coef, many)


def _output(column: _Column, n: Index) -> "Array[Any] | memoryview[int]":
    if issubclass(column, Block):
        return index_view(n)
    return (column * n)()


def _argument(found: "Array[Any] | memoryview[int]") -> Array[Any]:
    if isinstance(found, memoryview):
        return index_array(found)
    return found


def _total(counts: Array[c_offset] | None) -> Offset:
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

    def compare(self, other: "Model") -> str | None:
        """Compare the model with another.

        Two models are the same when they hold the same name and the same
        variables, functions, constraints, objectives, indicators, special
        ordered sets, cones, complements, piecewise linear functions and
        callbacks, under the same indices and names. Numbers match bit for
        bit, so ``0.0`` and ``-0.0`` differ. Options and hooks are not
        compared. The other model may belong to any environment.

        The difference is sent as an info message to the hooks of the model
        and of its environment, never to those of ``other``. The call reads
        it through a hook of its own, added and removed on the model, which
        uses up one hook id.

        Parameters
        ----------
        other
            The model to compare with.

        Returns
        -------
        difference
            ``None`` when the models are the same, otherwise the first
            difference found, as the info message the call sends.

        Raises
        ------
        TypeError
            When ``other`` is not a ``Model``.
        Error
            While the environment of the model delivers an event.
        ValueError
            When either model is closed.

        See Also
        --------
        BIYOS_cmpmdl
        """
        other = self._peer("other", other)
        found: list[str] = []
        idx = c_index()
        same = c_integer()
        kind = EventKind.MESSAGE
        level = EventLevel.INFO
        with self._env._busy(), other._env._busy(), other._entered():
            self._addhook(partial(_kept, found), kind, level, idx)
            try:
                self._run(lib.BIYOS_cmpmdl, other._handle, byref(same))
            finally:
                if self._open():
                    self._delhook(idx.value)
        return None if same.value else found[0]

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
            When ``fmt`` is not a format or is read only, the file cannot
            be written or the format cannot hold a part of the model.

        See Also
        --------
        BIYOS_writemdl
        """
        name = text("path", fspath(path))
        number = integer("fmt", fmt)
        self._call(lib.BIYOS_writemdl, name, number)

    def setname(self, name: str) -> None:
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

    def getname(self) -> str | None:
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

    def delname(self) -> None:
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
        src = c_integer()
        self._call(lib.BIYOS_getmdloptsrc, option, byref(src))
        return OptionSource(src.value)

    def addhook(
        self,
        hk: Callable[[Event], object],
        knd: EventKind,
        lvl: EventLevel,
    ) -> Index:
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
        idx = c_index()
        with self._env._busy():
            self._addhook(hk, kind, level, idx)
        return idx.value

    def delhook(self, idx: Index) -> None:
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
        n: Index,
        knd: Sequence[VariableKind] | None = None,
        lb: Sequence[Real] | None = None,
        ub: Sequence[Real] | None = None,
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

    @overload
    def setvarbnd(
        self,
        idx: One[Variable],
        lb: Real | None = None,
        ub: Real | None = None,
    ) -> None: ...

    @overload
    def setvarbnd(
        self,
        idx: Many[Variable, Variables],
        lb: Sequence[Real] | None = None,
        ub: Sequence[Real] | None = None,
    ) -> None: ...

    def setvarbnd(
        self,
        idx: Selection[Variable, Variables],
        lb: Real | Sequence[Real] | None = None,
        ub: Real | Sequence[Real] | None = None,
    ) -> None:
        """Set the bounds of variables.

        A variable may appear more than once; its entries apply in order.

        Parameters
        ----------
        idx
            The variables.
            An index or a view stands for one variable, and each other argument
            is then a scalar.
        lb, ub
            The new bounds of each; ``None`` leaves that side as it is.

        See Also
        --------
        BIYOS_setvarbndsp
        BIYOS_setvarbndrg
        """
        one, n, chosen = self._selected("BIYOS_setvarbnd", Variables, idx)
        arrays = (
            doubles("lb", lb, n, one),
            doubles("ub", ub, n, one),
        )
        self._call(*chosen, *arrays)

    @overload
    def getvarbnd(self, idx: One[Variable]) -> tuple[Real, Real]: ...

    @overload
    def getvarbnd(
        self, idx: Many[Variable, Variables]
    ) -> tuple[list[Real], list[Real]]: ...

    def getvarbnd(
        self, idx: Selection[Variable, Variables]
    ) -> tuple[Real, Real] | tuple[list[Real], list[Real]]:
        """Read the bounds of variables.

        Parameters
        ----------
        idx
            The variables, in any order, any of them more than once.
            An index or a view stands for one variable, and each result is then
            a scalar.

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
        bounds = (c_real, c_real)
        lb, ub = self._getlist("BIYOS_getvarbnd", Variables, idx, *bounds)
        return lb, ub

    @overload
    def setvarknd(
        self, idx: One[Variable], knd: VariableKind | None
    ) -> None: ...

    @overload
    def setvarknd(
        self,
        idx: Many[Variable, Variables],
        knd: Sequence[VariableKind] | None,
    ) -> None: ...

    def setvarknd(
        self,
        idx: Selection[Variable, Variables],
        knd: VariableKind | Sequence[VariableKind] | None,
    ) -> None:
        """Set the kinds of variables.

        Parameters
        ----------
        idx
            The variables.
            An index or a view stands for one variable, and each other argument
            is then a scalar.
        knd
            The new kind of each.

        See Also
        --------
        BIYOS_setvarkndsp
        BIYOS_setvarkndrg
        """
        one, n, chosen = self._selected("BIYOS_setvarknd", Variables, idx)
        arrays = (ints("knd", knd, n, one),)
        self._call(*chosen, *arrays)

    @overload
    def getvarknd(self, idx: One[Variable]) -> VariableKind: ...

    @overload
    def getvarknd(
        self, idx: Many[Variable, Variables]
    ) -> list[VariableKind]: ...

    def getvarknd(
        self, idx: Selection[Variable, Variables]
    ) -> VariableKind | list[VariableKind]:
        """Read the kinds of variables.

        Parameters
        ----------
        idx
            The variables, in any order, any of them more than once.
            An index or a view stands for one variable, and the result is then a
            scalar.

        Returns
        -------
        knd
            The kind of each.

        See Also
        --------
        BIYOS_getvarkndsp
        BIYOS_getvarkndrg
        """
        (found,) = self._getlist("BIYOS_getvarknd", Variables, idx, c_integer)
        knd: VariableKind | list[VariableKind] = _each(VariableKind, found)
        return knd

    @overload
    def setvartol(
        self,
        idx: One[Variable],
        atol: Real | None = None,
        rtol: Real | None = None,
    ) -> None: ...

    @overload
    def setvartol(
        self,
        idx: Many[Variable, Variables],
        atol: Sequence[Real] | None = None,
        rtol: Sequence[Real] | None = None,
    ) -> None: ...

    def setvartol(
        self,
        idx: Selection[Variable, Variables],
        atol: Real | Sequence[Real] | None = None,
        rtol: Real | Sequence[Real] | None = None,
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
            An index or a view stands for one variable, and each other argument
            is then a scalar.
        atol, rtol
            The new absolute and relative tolerance of each; ``None``
            leaves that tolerance as it is.

        See Also
        --------
        BIYOS_setvartolsp
        BIYOS_setvartolrg
        """
        one, n, chosen = self._selected("BIYOS_setvartol", Variables, idx)
        arrays = (
            doubles("atol", atol, n, one),
            doubles("rtol", rtol, n, one),
        )
        self._call(*chosen, *arrays)

    @overload
    def getvartol(
        self, idx: One[Variable]
    ) -> tuple[Real | None, Real | None]: ...

    @overload
    def getvartol(
        self, idx: Many[Variable, Variables]
    ) -> tuple[list[Real | None], list[Real | None]]: ...

    def getvartol(
        self, idx: Selection[Variable, Variables]
    ) -> (
        tuple[Real | None, Real | None]
        | tuple[list[Real | None], list[Real | None]]
    ):
        """Read the tolerances of variables.

        Parameters
        ----------
        idx
            The variables, in any order, any of them more than once.
            An index or a view stands for one variable, and each result is then
            a scalar.

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
        pair = (c_real, c_real)
        atol, rtol = self._getlist("BIYOS_getvartol", Variables, idx, *pair)
        return _each(_or_none, atol), _each(_or_none, rtol)

    def delvartol(self, idx: Selection[Variable, Variables]) -> None:
        """Return variables to ``feastol_abs`` and ``feastol_rel``.

        Parameters
        ----------
        idx
            The variables that lose their tolerances.
            An index or a view stands for one variable.

        See Also
        --------
        BIYOS_delvartolsp
        BIYOS_delvartolrg
        """
        rows = picked(self, Variables, idx)
        self._call(*self._chosen("BIYOS_delvartol", Variables, rows))

    def getnvar(self) -> Index:
        """Return the number of variables.

        Returns
        -------
        n
            The number of variables.

        See Also
        --------
        BIYOS_getnvar
        """
        count = c_index()
        self._call(lib.BIYOS_getnvar, byref(count))
        return count.value

    @overload
    def getvar(
        self, idx: One[Variable]
    ) -> tuple[VariableKind, Real, Real]: ...

    @overload
    def getvar(
        self, idx: Many[Variable, Variables] | None = None
    ) -> tuple[list[VariableKind], list[Real], list[Real]]: ...

    def getvar(
        self, idx: Selection[Variable, Variables] | None = None
    ) -> (
        tuple[VariableKind, Real, Real]
        | tuple[list[VariableKind], list[Real], list[Real]]
    ):
        """Read the kinds and bounds of variables.

        Parameters
        ----------
        idx
            The variables, in any order, any of them more than once; every
            variable by default.
            An index or a view stands for one variable, and each result is then
            a scalar.

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
        types = (c_integer, c_real, c_real)
        found = self._getlist("BIYOS_getvar", Variables, every, *types)
        kinds, lower, upper = found
        return _each(VariableKind, kinds), lower, upper

    @overload
    def setvarname(self, idx: One[Variable], name: str) -> None: ...

    @overload
    def setvarname(
        self, idx: Many[Variable, Variables], name: Sequence[str]
    ) -> None: ...

    def setvarname(
        self, idx: Selection[Variable, Variables], name: str | Sequence[str]
    ) -> None:
        """Name variables of the model.

        Variable ``idx[j]`` takes the name ``name[j]``, under the rules of
        ``setname``. A variable listed more than once takes the last of
        its names. Names may repeat: several variables may hold one name,
        and ``getvaridx`` then refuses to choose among them. Every other
        family is named the same way, each apart.

        Parameters
        ----------
        idx
            The variables to name.
            An index or a view stands for one variable, and each other argument
            is then a scalar.
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

    @overload
    def getvarname(self, idx: One[Variable]) -> str | None: ...

    @overload
    def getvarname(
        self, idx: Many[Variable, Variables]
    ) -> list[str | None]: ...

    def getvarname(
        self, idx: Selection[Variable, Variables]
    ) -> str | None | list[str | None]:
        """Return the names of variables.

        Parameters
        ----------
        idx
            The variables to read, in any order, any of them more than once.
            An index or a view stands for one variable, and the result is then a
            scalar.

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

    def getvaridx(self, name: Sequence[str]) -> Variables:
        """Return the variables with names.

        Parameters
        ----------
        name
            The names, each matched exactly.

        Returns
        -------
        idx
            The variable that alone holds each name, as a ``Variables``
            block, which raises ``ValueError`` once the model has had a
            deletion. ``indices`` gives the indices as a list.

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
        return self._getidx(lib.BIYOS_getvaridx, Variables, name)

    def delvarname(self, idx: Selection[Variable, Variables]) -> None:
        """Remove the names of variables, which then have none.

        Parameters
        ----------
        idx
            The variables that lose their names. One without a name, or
            listed more than once, is left without one.
            An index or a view stands for one variable.

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

    def delvar(self, idx: Selection[Variable, Variables]) -> list[Index]:
        """Delete variables, with their terms in every function.

        The variables left keep their order and are numbered from 0 again, and
        each term, set, indicator, node and callback refers to them by their
        new indices. A deleted variable's kind, bounds, tolerances, name and
        terms go with it, and a function left with no term keeps its constant.
        Once any variable is deleted, the model refuses every block and view
        it gave before.

        Parameters
        ----------
        idx
            The variables to delete, in any order, none of them twice.
            An index or a view stands for one variable.

        Returns
        -------
        map
            For each variable the model held, its new index, or -1 when it is
            deleted.

        Raises
        ------
        Error
            When a variable to delete has a node, is a member of a set, is
            the binary of an indicator or is listed by a callback, or when a
            callback reads every variable and a variable is to be deleted.
            The model is then left as it was.

        See Also
        --------
        BIYOS_delvarsp
        BIYOS_delvarrg
        """
        return self._delete("BIYOS_delvar", Variables, idx)

    def addfun(self, n: Index) -> Functions:
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

    def getnfun(self) -> Index:
        """Return the number of functions.

        Returns
        -------
        n
            The number of functions.

        See Also
        --------
        BIYOS_getnfun
        """
        count = c_index()
        self._call(lib.BIYOS_getnfun, byref(count))
        return count.value

    @overload
    def addcst(self, idx: One[Function], cst: Real | None) -> None: ...

    @overload
    def addcst(
        self, idx: Many[Function, Functions], cst: Sequence[Real] | None
    ) -> None: ...

    def addcst(
        self,
        idx: Selection[Function, Functions],
        cst: Real | Sequence[Real] | None,
    ) -> None:
        """Add to the constants of functions.

        A function may appear more than once; its entries add up, in order.

        Parameters
        ----------
        idx
            The functions.
            An index or a view stands for one function, and each other argument
            is then a scalar.
        cst
            The value each entry adds to its function's constant, finite;
            ``None`` changes nothing. A constant that would leave the finite
            range refuses the call.

        See Also
        --------
        BIYOS_addcstsp
        BIYOS_addcstrg
        """
        one, n, chosen = self._selected("BIYOS_addcst", Functions, idx)
        self._call(*chosen, doubles("cst", cst, n, one))

    @overload
    def setcst(self, idx: One[Function], cst: Real | None) -> None: ...

    @overload
    def setcst(
        self, idx: Many[Function, Functions], cst: Sequence[Real] | None
    ) -> None: ...

    def setcst(
        self,
        idx: Selection[Function, Functions],
        cst: Real | Sequence[Real] | None,
    ) -> None:
        """Set the constants of functions.

        A function may appear more than once; its entries apply in order.

        Parameters
        ----------
        idx
            The functions.
            An index or a view stands for one function, and each other argument
            is then a scalar.
        cst
            The new constant of each, finite; ``None`` changes nothing.

        See Also
        --------
        BIYOS_setcstsp
        BIYOS_setcstrg
        """
        one, n, chosen = self._selected("BIYOS_setcst", Functions, idx)
        self._call(*chosen, doubles("cst", cst, n, one))

    @overload
    def getcst(self, idx: One[Function]) -> Real: ...

    @overload
    def getcst(self, idx: Many[Function, Functions]) -> list[Real]: ...

    def getcst(self, idx: Selection[Function, Functions]) -> Real | list[Real]:
        """Read the constants of functions.

        Parameters
        ----------
        idx
            The functions, in any order, any of them more than once.
            An index or a view stands for one function, and the result is then a
            scalar.

        Returns
        -------
        cst
            The constant of each.

        See Also
        --------
        BIYOS_getcstsp
        BIYOS_getcstrg
        """
        cst: Real | list[Real]
        (cst,) = self._getlist("BIYOS_getcst", Functions, idx, c_real)
        return cst

    @overload
    def setfunname(self, idx: One[Function], name: str) -> None: ...

    @overload
    def setfunname(
        self, idx: Many[Function, Functions], name: Sequence[str]
    ) -> None: ...

    def setfunname(
        self, idx: Selection[Function, Functions], name: str | Sequence[str]
    ) -> None:
        """Name functions of the model, as ``setvarname`` names variables.

        Parameters
        ----------
        idx
            The functions to name.
            An index or a view stands for one function, and each other argument
            is then a scalar.
        name
            The new name of each, as many as ``idx``.

        See Also
        --------
        BIYOS_setfunnamesp
        BIYOS_setfunnamerg
        """
        self._setname("BIYOS_setfunname", Functions, idx, name)

    @overload
    def getfunname(self, idx: One[Function]) -> str | None: ...

    @overload
    def getfunname(
        self, idx: Many[Function, Functions]
    ) -> list[str | None]: ...

    def getfunname(
        self, idx: Selection[Function, Functions]
    ) -> str | None | list[str | None]:
        """Return the names of functions, as ``getvarname`` does.

        Parameters
        ----------
        idx
            The functions to read.
            An index or a view stands for one function, and the result is then a
            scalar.

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

    def getfunidx(self, name: Sequence[str]) -> Functions:
        """Return the named functions, as ``getvaridx`` does.

        Parameters
        ----------
        name
            The names, each matched exactly.

        Returns
        -------
        idx
            The function that alone holds each name, as a ``Functions`` block.

        See Also
        --------
        BIYOS_getfunidx
        """
        return self._getidx(lib.BIYOS_getfunidx, Functions, name)

    def delfunname(self, idx: Selection[Function, Functions]) -> None:
        """Remove the names of functions, as ``delvarname`` does.

        Parameters
        ----------
        idx
            The functions that lose their names.
            An index or a view stands for one function.

        See Also
        --------
        BIYOS_delfunnamesp
        BIYOS_delfunnamerg
        """
        self._delname("BIYOS_delfunname", Functions, idx)

    def delfun(self, idx: Selection[Function, Functions]) -> list[Index]:
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
            An index or a view stands for one function.

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

    def clrfun(self, idx: Selection[Function, Functions]) -> None:
        """Empty functions of the model, so they can be built again.

        Each function given has the constant 0, no term and no expression
        once the call returns. Its expression's nodes stay in the graph.

        Parameters
        ----------
        idx
            The functions, any of them more than once.
            An index or a view stands for one function.

        See Also
        --------
        BIYOS_clrfunsp
        BIYOS_clrfunrg
        """
        rows = picked(self, Functions, idx)
        self._call(*self._chosen("BIYOS_clrfun", Functions, rows))

    @overload
    def addlinterm(
        self, fun: One[Function], var: One[Variable], coef: Real
    ) -> None: ...

    @overload
    def addlinterm(
        self,
        fun: One[Function],
        var: Many[Variable, Variables],
        coef: Sequence[Real],
    ) -> None: ...

    @overload
    def addlinterm(
        self,
        fun: Many[Function, Functions],
        var: Many[Variable, Variables],
        coef: Sequence[Real],
    ) -> None: ...

    @overload
    def addlinterm(
        self,
        fun: Many[Function, Functions],
        var: Sequence[Many[Variable, Variables]],
        coef: Sequence[Sequence[Real]],
    ) -> None: ...

    def addlinterm(
        self,
        fun: Selection[Function, Functions],
        var: (
            Selection[Variable, Variables]
            | Sequence[Many[Variable, Variables]]
        ),
        coef: Real | Sequence[Real] | Sequence[Sequence[Real]],
    ) -> None:
        """Add linear terms to functions, as coordinates or by function.

        As coordinates, term ``k`` adds ``coef[k]`` times variable
        ``var[k]`` to function ``fun[k]``, in any order. By function, each
        entry of ``var`` is a sequence, and function ``fun[j]`` gains
        ``coef[j][k]`` times variable ``var[j][k]`` for each ``k``; a
        function listed twice gains both groups. A scalar ``fun`` is one
        function, ``var`` and ``coef`` its terms, or its one term when both
        are scalars. A variable the function holds already adds its
        coefficient to the one held.

        Parameters
        ----------
        fun
            The function of each term; or the functions, in any order, a
            block or a range of them; or one function.
        var
            The variable of each term, or a sequence of variables for each
            function, each variable once for its function.
        coef
            The coefficient of each term, nested as ``var`` is.

        Raises
        ------
        TypeError
            When ``var`` or ``coef`` is a scalar while ``fun`` is not,
            ``coef`` is a sequence where a scalar ``fun`` and ``var`` want
            a scalar, or ``coef`` is not nested as ``var`` is.
        ValueError
            When an entry of ``coef`` is not as long as that of ``var``.

        See Also
        --------
        BIYOS_addlinterm
        BIYOS_addrlintermsp
        BIYOS_addrlintermrg
        """
        lead = (Functions, "fun", fun)
        others = [(Variables, "var", var)]
        self._addterms(
            lead,
            others,
            coef,
            coordinates="BIYOS_addlinterm",
            lists="BIYOS_addrlinterm",
        )

    def getnrlinterm(
        self, idx: Sequence[Index | Function] | Functions | None = None
    ) -> Offset:
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
        self, idx: Sequence[Index | Function] | Functions | None = None
    ) -> tuple[list[Index], list[Index], list[Real]]:
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

    @overload
    def addclinterm(
        self, var: One[Variable], fun: One[Function], coef: Real
    ) -> None: ...

    @overload
    def addclinterm(
        self,
        var: One[Variable],
        fun: Many[Function, Functions],
        coef: Sequence[Real],
    ) -> None: ...

    @overload
    def addclinterm(
        self,
        var: Many[Variable, Variables],
        fun: Sequence[Many[Function, Functions]],
        coef: Sequence[Sequence[Real]],
    ) -> None: ...

    def addclinterm(
        self,
        var: Selection[Variable, Variables],
        fun: (
            Selection[Function, Functions]
            | Sequence[Many[Function, Functions]]
        ),
        coef: Real | Sequence[Real] | Sequence[Sequence[Real]],
    ) -> None:
        """Add linear terms to functions, by variable.

        Each entry of ``fun`` is a sequence, and variable ``var[j]`` joins
        function ``fun[j][k]`` with the coefficient ``coef[j][k]`` for each
        ``k``; a variable listed twice joins both groups. A scalar ``var``
        is one variable, ``fun`` and ``coef`` its terms, or its one term
        when both are scalars. A variable the function holds already adds
        its coefficient to the one held.

        Parameters
        ----------
        var
            The variables, in any order, a block or a range of them; or one
            variable.
        fun
            A sequence of functions for each variable, each function once
            for its variable.
        coef
            The coefficient of each term, nested as ``fun`` is.

        Raises
        ------
        TypeError
            When ``fun`` or ``coef`` is not nested as ``var`` asks, or
            ``coef`` is a sequence where a scalar ``var`` and ``fun`` want a
            scalar.
        ValueError
            When an entry of ``coef`` is not as long as that of ``fun``.

        See Also
        --------
        BIYOS_addclintermsp
        BIYOS_addclintermrg
        """
        lead = (Variables, "var", var)
        others = [(Functions, "fun", fun)]
        self._addlists("BIYOS_addclinterm", lead, others, coef)

    def getnclinterm(
        self, idx: Sequence[Index | Variable] | Variables | None = None
    ) -> Offset:
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
        self, idx: Sequence[Index | Variable] | Variables | None = None
    ) -> tuple[list[Index], list[Index], list[Real]]:
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

    @overload
    def addquadterm(
        self,
        fun: One[Function],
        var1: One[Variable],
        var2: One[Variable],
        coef: Real,
    ) -> None: ...

    @overload
    def addquadterm(
        self,
        fun: One[Function],
        var1: Many[Variable, Variables],
        var2: Many[Variable, Variables],
        coef: Sequence[Real],
    ) -> None: ...

    @overload
    def addquadterm(
        self,
        fun: Many[Function, Functions],
        var1: Many[Variable, Variables],
        var2: Many[Variable, Variables],
        coef: Sequence[Real],
    ) -> None: ...

    @overload
    def addquadterm(
        self,
        fun: Many[Function, Functions],
        var1: Sequence[Many[Variable, Variables]],
        var2: Sequence[Many[Variable, Variables]],
        coef: Sequence[Sequence[Real]],
    ) -> None: ...

    def addquadterm(
        self,
        fun: Selection[Function, Functions],
        var1: (
            Selection[Variable, Variables]
            | Sequence[Many[Variable, Variables]]
        ),
        var2: (
            Selection[Variable, Variables]
            | Sequence[Many[Variable, Variables]]
        ),
        coef: Real | Sequence[Real] | Sequence[Sequence[Real]],
    ) -> None:
        """Add quadratic terms to functions, as coordinates or by function.

        As coordinates, term ``k`` adds ``coef[k]`` times the product of
        variables ``var1[k]`` and ``var2[k]`` to function ``fun[k]``, in any
        order. By function, each entry of ``var1`` is a sequence, and
        function ``fun[j]`` gains ``coef[j][k]`` times the product of
        ``var1[j][k]`` and ``var2[j][k]`` for each ``k``; a function listed
        twice gains both groups. A scalar ``fun`` is one function,
        ``var1``, ``var2`` and ``coef`` its terms, or its one term when all
        three are scalars. A product carries no factor 1/2, and the two
        variables of a pair may come in either order. A pair the function
        holds already adds its coefficient to the one held.

        Parameters
        ----------
        fun
            The function of each term; or the functions, in any order, a
            block or a range of them; or one function.
        var1, var2
            The two variables of each term; by function, a sequence of
            first and a sequence of second variables for each function. Each
            pair comes once for its function.
        coef
            The coefficient of each term, nested as ``var1`` is.

        Raises
        ------
        TypeError
            When ``var1``, ``var2`` or ``coef`` is a scalar while ``fun`` is
            not, ``var2`` or ``coef`` is a sequence where a scalar ``fun``
            and ``var1`` want scalars, or either is not nested as ``var1``
            is.
        ValueError
            When an entry of ``var2`` or ``coef`` is not as long as that of
            ``var1``.

        See Also
        --------
        BIYOS_addquadterm
        BIYOS_addquadtermsp
        BIYOS_addquadtermrg
        """
        lead = (Functions, "fun", fun)
        others = [(Variables, "var1", var1), (Variables, "var2", var2)]
        self._addterms(
            lead,
            others,
            coef,
            coordinates="BIYOS_addquadterm",
            lists="BIYOS_addquadterm",
        )

    def getnquadterm(
        self, idx: Sequence[Index | Function] | Functions | None = None
    ) -> Offset:
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
        self, idx: Sequence[Index | Function] | Functions | None = None
    ) -> tuple[list[Index], list[Index], list[Index], list[Real]]:
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

    def addvarnode(self, var: Index | Variable) -> Offset:
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
        node = c_offset()
        self._call(lib.BIYOS_addvarnode, var, byref(node))
        return node.value

    def addnode(
        self,
        op: Operator,
        arg: Sequence[Offset] | None = None,
        val: Sequence[Real] | None = None,
    ) -> Offset:
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
        node = c_offset()
        self._call(lib.BIYOS_addnode, code, n, *arrays, byref(node))
        return node.value

    def addexpr(
        self,
        cnt: Sequence[Offset],
        code: Sequence[Operator],
        arg: Sequence[Offset],
        val: Sequence[Real] | None = None,
    ) -> list[Offset]:
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
        nodes = (c_offset * n)()
        self._call(lib.BIYOS_addexpr, n, sizes, codes, args, values, nodes)
        return list(nodes)

    @overload
    def attachexpr(self, idx: One[Function], node: Offset) -> None: ...

    @overload
    def attachexpr(
        self, idx: Many[Function, Functions], node: Sequence[Offset]
    ) -> None: ...

    def attachexpr(
        self,
        idx: Selection[Function, Functions],
        node: Offset | Sequence[Offset],
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
            An index or a view stands for one function, and each other argument
            is then a scalar.
        node
            The node each function gains, by id.

        See Also
        --------
        BIYOS_attachexprsp
        BIYOS_attachexprrg
        """
        one, n, chosen = self._selected("BIYOS_attachexpr", Functions, idx)
        self._call(*chosen, int64s("node", node, n, one))

    def getnexpr(
        self, idx: Sequence[Index | Function] | Functions | None = None
    ) -> tuple[Offset, Offset]:
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
        nnz = c_offset()
        nval = c_offset()
        chosen = self._chosen("BIYOS_getnexpr", Functions, every)
        self._call(*chosen, byref(nnz), byref(nval))
        return nnz.value, nval.value

    def getexpr(
        self, idx: Sequence[Index | Function] | Functions | None = None
    ) -> tuple[list[Offset], list[Operator], list[Offset], list[Real]]:
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
        counts = (c_offset * len(every))()
        codes = (c_integer * nnz)()
        args = (c_offset * nnz)()
        values = (c_real * nval)()
        self._call(*chosen, nnz, nval, counts, codes, args, values)
        operators = [Operator(code) for code in codes]
        return list(counts), operators, list(args), list(values)

    def addcb(
        self,
        fun: Sequence[Index | Function] | Functions,
        var: Sequence[Index | Variable] | Variables | None = None,
        data: object = None,
    ) -> Index:
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
        var
            The variables the callback reads, each listed once, or None for
            every variable of the model, the default. Their order is the
            order of the callback's ``x`` and ``v``, and of the values it
            writes for ``hv``. Its patterns hold only these variables, and
            name them by their model index.
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
        and a copy shares them. ``evalf``, ``evalg``, ``evalh`` and
        ``evalhv`` call them.

        See Also
        --------
        BIYOS_addcb
        """
        m = len(fun)
        functions = indices_of(self, Functions, "fun", fun, m)
        n = _capi.DENSE if var is None else len(var)
        variables = indices_of(self, Variables, "var", var, n)
        cell = py_object(data)
        cb = c_index()
        self._callbacks.keep(cell)
        self._call(
            lib.BIYOS_addcb,
            m,
            functions,
            n,
            variables,
            addressof(cell),
            byref(cb),
        )
        self._callbacks.added(m)
        return cb.value

    def setcbf(self, cb: Index, f: Callable[..., object]) -> None:
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
        caller = self._callbacks.caller(checked("f", f), count, count)
        self._call(lib.BIYOS_setcbf, cb, caller.pointer)

    def setcbg(
        self,
        cb: Index,
        fun: Sequence[Index | Function] | Functions,
        var: Sequence[Index | Variable] | Variables,
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
        count = self._callbacks.count(cb)
        caller = self._callbacks.caller(checked("g", g), count, nnz)
        pattern = (nnz, functions, variables, caller.pointer)
        self._call(lib.BIYOS_setcbg, cb, *pattern)

    def setcbh(
        self,
        cb: Index,
        var1: Sequence[Index | Variable] | Variables,
        var2: Sequence[Index | Variable] | Variables,
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
        count = self._callbacks.count(cb)
        caller = self._callbacks.caller(checked("h", h), count, nnz)
        pairs = (nnz, first, second, caller.pointer)
        self._call(lib.BIYOS_setcbh, cb, *pairs)

    def setcbhv(self, cb: Index, hv: Callable[..., object]) -> None:
        """Give a callback the function that multiplies its Hessian by a
        vector.

        Parameters
        ----------
        cb
            The callback.
        hv
            The function, as ``addcb`` describes. It writes, for each
            variable the callback reads, in its order, the entry of the
            product of ``req.v`` with the sum of the Hessians of the
            callback's parts, each weighted by ``req.w``, at the point.

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
        count = self._callbacks.count(cb)
        caller = self._callbacks.caller(function, count, None)
        self._call(lib.BIYOS_setcbhv, cb, caller.pointer)

    def setcbdata(self, cb: Index, data: object) -> None:
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

    def getcbdata(self, cb: Index) -> object:
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

    def getncbfun(self, cb: Index) -> Index:
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
        count = c_index()
        self._call(lib.BIYOS_getncbfun, integer("cb", cb), byref(count))
        return count.value

    def getcbfun(self, cb: Index) -> Functions:
        """Read the functions a callback covers, as ``addcb`` takes them.

        Parameters
        ----------
        cb
            The callback.

        Returns
        -------
        fun
            The functions, in the order given to ``addcb``, as a
            ``Functions`` block, which raises ``ValueError`` once the model
            has had a deletion. ``indices`` gives the indices as a list.

        See Also
        --------
        BIYOS_getcbfun
        """
        n = self.getncbfun(cb)
        found = index_view(n)
        self._call(lib.BIYOS_getcbfun, integer("cb", cb), n, index_array(found))
        return Functions(self, found, self._gen)

    def getcbvar(self, cb: Index) -> Variables | None:
        """Read the variables a callback reads, as ``addcb`` takes them.

        Parameters
        ----------
        cb
            The callback.

        Returns
        -------
        var
            The variables, in the order given to ``addcb``, as a
            ``Variables`` block, or None when the callback reads every
            variable of the model. The block raises ``ValueError`` once the
            model has had a deletion. ``indices`` gives the indices as a
            list.

        See Also
        --------
        BIYOS_getncbvar
        BIYOS_getcbvar
        """
        count = c_index()
        self._call(lib.BIYOS_getncbvar, integer("cb", cb), byref(count))
        if count.value == _capi.DENSE:
            return None
        n = count.value
        found = index_view(n)
        self._call(lib.BIYOS_getcbvar, integer("cb", cb), n, index_array(found))
        return Variables(self, found, self._gen)

    def delcb(self, idx: Index | Sequence[Index]) -> list[Index]:
        """Delete callbacks.

        The callbacks left keep their order, their functions and their
        ``data``, and are numbered from 0 again. A deleted callback no longer
        covers its functions, so a function no other callback covers may then
        be deleted. The model keeps the deleted callback's functions and
        ``data`` alive until it is freed, since a copy may share them.

        Parameters
        ----------
        idx
            The callbacks to delete, in any order, none of them twice.
            An index stands for one callback.

        Returns
        -------
        map
            For each callback the model held, its new index, or -1 when it is
            deleted.

        See Also
        --------
        BIYOS_delcbsp
        BIYOS_delcbrg
        """
        one, n = counted(idx)
        found = (c_index * self._callbacks.total())()
        self._call(lib.BIYOS_delcbsp, n, ints("idx", idx, n, one), found)
        renumbered = list(found)
        self._callbacks.deleted(renumbered)
        return renumbered

    def addcon(
        self,
        fun: Sequence[Index | Function]
        | Functions
        | Comparison
        | Sequence[Comparison],
        lb: Sequence[Real] | None = None,
        ub: Sequence[Real] | None = None,
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
        funs = cast("Sequence[Index | Function] | Functions", fun)
        n = len(funs)
        arrays = (
            indices_of(self, Functions, "fun", funs, n),
            doubles("lb", lb, n),
            doubles("ub", ub, n),
        )
        return self._added(Constraints, lib.BIYOS_addcon, n, *arrays)

    @overload
    def setconbnd(
        self,
        idx: One[Constraint],
        lb: Real | None = None,
        ub: Real | None = None,
    ) -> None: ...

    @overload
    def setconbnd(
        self,
        idx: Many[Constraint, Constraints],
        lb: Sequence[Real] | None = None,
        ub: Sequence[Real] | None = None,
    ) -> None: ...

    def setconbnd(
        self,
        idx: Selection[Constraint, Constraints],
        lb: Real | Sequence[Real] | None = None,
        ub: Real | Sequence[Real] | None = None,
    ) -> None:
        """Set the bounds of constraints and keep their functions.

        Parameters
        ----------
        idx
            The constraints.
            An index or a view stands for one constraint, and each other
            argument is then a scalar.
        lb, ub
            The new bounds of each; ``None`` leaves that side as it is.

        See Also
        --------
        BIYOS_setconbndsp
        BIYOS_setconbndrg
        """
        one, n, chosen = self._selected("BIYOS_setconbnd", Constraints, idx)
        arrays = (
            doubles("lb", lb, n, one),
            doubles("ub", ub, n, one),
        )
        self._call(*chosen, *arrays)

    @overload
    def getconbnd(self, idx: One[Constraint]) -> tuple[Real, Real]: ...

    @overload
    def getconbnd(
        self, idx: Many[Constraint, Constraints]
    ) -> tuple[list[Real], list[Real]]: ...

    def getconbnd(
        self, idx: Selection[Constraint, Constraints]
    ) -> tuple[Real, Real] | tuple[list[Real], list[Real]]:
        """Read the bounds of constraints.

        Parameters
        ----------
        idx
            The constraints, in any order, any of them more than once.
            An index or a view stands for one constraint, and each result is
            then a scalar.

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
        bounds = (c_real, c_real)
        lb, ub = self._getlist("BIYOS_getconbnd", Constraints, idx, *bounds)
        return lb, ub

    @overload
    def setcontol(
        self,
        idx: One[Constraint],
        atol: Real | None = None,
        rtol: Real | None = None,
    ) -> None: ...

    @overload
    def setcontol(
        self,
        idx: Many[Constraint, Constraints],
        atol: Sequence[Real] | None = None,
        rtol: Sequence[Real] | None = None,
    ) -> None: ...

    def setcontol(
        self,
        idx: Selection[Constraint, Constraints],
        atol: Real | Sequence[Real] | None = None,
        rtol: Real | Sequence[Real] | None = None,
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
            An index or a view stands for one constraint, and each other
            argument is then a scalar.
        atol, rtol
            The new absolute and relative tolerance of each; ``None``
            leaves that tolerance as it is.

        See Also
        --------
        BIYOS_setcontolsp
        BIYOS_setcontolrg
        """
        one, n, chosen = self._selected("BIYOS_setcontol", Constraints, idx)
        arrays = (
            doubles("atol", atol, n, one),
            doubles("rtol", rtol, n, one),
        )
        self._call(*chosen, *arrays)

    @overload
    def getcontol(
        self, idx: One[Constraint]
    ) -> tuple[Real | None, Real | None]: ...

    @overload
    def getcontol(
        self, idx: Many[Constraint, Constraints]
    ) -> tuple[list[Real | None], list[Real | None]]: ...

    def getcontol(
        self, idx: Selection[Constraint, Constraints]
    ) -> (
        tuple[Real | None, Real | None]
        | tuple[list[Real | None], list[Real | None]]
    ):
        """Read the tolerances of constraints.

        Parameters
        ----------
        idx
            The constraints, in any order, any of them more than once.
            An index or a view stands for one constraint, and each result is
            then a scalar.

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
        pair = (c_real, c_real)
        atol, rtol = self._getlist("BIYOS_getcontol", Constraints, idx, *pair)
        return _each(_or_none, atol), _each(_or_none, rtol)

    def delcontol(self, idx: Selection[Constraint, Constraints]) -> None:
        """Return constraints to ``feastol_abs`` and ``feastol_rel``.

        Parameters
        ----------
        idx
            The constraints that lose their tolerances.
            An index or a view stands for one constraint.

        See Also
        --------
        BIYOS_delcontolsp
        BIYOS_delcontolrg
        """
        rows = picked(self, Constraints, idx)
        self._call(*self._chosen("BIYOS_delcontol", Constraints, rows))

    def getncon(self) -> Index:
        """Return the number of constraints.

        Returns
        -------
        n
            The number of constraints.

        See Also
        --------
        BIYOS_getncon
        """
        count = c_index()
        self._call(lib.BIYOS_getncon, byref(count))
        return count.value

    @overload
    def getcon(self, idx: One[Constraint]) -> tuple[Function, Real, Real]: ...

    @overload
    def getcon(
        self, idx: Many[Constraint, Constraints] | None = None
    ) -> tuple[Functions, list[Real], list[Real]]: ...

    def getcon(
        self, idx: Selection[Constraint, Constraints] | None = None
    ) -> tuple[Function, Real, Real] | tuple[Functions, list[Real], list[Real]]:
        """Read the functions and bounds of constraints.

        The functions come as a block, which raises ``ValueError`` once the
        model has had a deletion. ``indices`` gives them as a list.

        Parameters
        ----------
        idx
            The constraints, in any order, any of them more than once; every
            constraint by default.
            An index or a view stands for one constraint, and each result is
            then a scalar.

        Returns
        -------
        fun
            The function each constraint bounds, as a ``Functions`` block,
            or a ``Function`` for one constraint.
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
        types = (Functions, c_real, c_real)
        found = self._getlist("BIYOS_getcon", Constraints, every, *types)
        functions, lower, upper = found
        return functions, lower, upper

    @overload
    def setconname(self, idx: One[Constraint], name: str) -> None: ...

    @overload
    def setconname(
        self, idx: Many[Constraint, Constraints], name: Sequence[str]
    ) -> None: ...

    def setconname(
        self, idx: Selection[Constraint, Constraints], name: str | Sequence[str]
    ) -> None:
        """Name constraints of the model, as ``setvarname`` names variables.

        Parameters
        ----------
        idx
            The constraints to name.
            An index or a view stands for one constraint, and each other
            argument is then a scalar.
        name
            The new name of each, as many as ``idx``.

        See Also
        --------
        BIYOS_setconnamesp
        BIYOS_setconnamerg
        """
        self._setname("BIYOS_setconname", Constraints, idx, name)

    @overload
    def getconname(self, idx: One[Constraint]) -> str | None: ...

    @overload
    def getconname(
        self, idx: Many[Constraint, Constraints]
    ) -> list[str | None]: ...

    def getconname(
        self, idx: Selection[Constraint, Constraints]
    ) -> str | None | list[str | None]:
        """Return the names of constraints, as ``getvarname`` does.

        Parameters
        ----------
        idx
            The constraints to read.
            An index or a view stands for one constraint, and the result is then
            a scalar.

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

    def getconidx(self, name: Sequence[str]) -> Constraints:
        """Return the named constraints, as ``getvaridx`` does.

        Parameters
        ----------
        name
            The names, each matched exactly.

        Returns
        -------
        idx
            The constraint that alone holds each name, as a ``Constraints``
            block.

        See Also
        --------
        BIYOS_getconidx
        """
        return self._getidx(lib.BIYOS_getconidx, Constraints, name)

    def delconname(self, idx: Selection[Constraint, Constraints]) -> None:
        """Remove the names of constraints, as ``delvarname`` does.

        Parameters
        ----------
        idx
            The constraints that lose their names.
            An index or a view stands for one constraint.

        See Also
        --------
        BIYOS_delconnamesp
        BIYOS_delconnamerg
        """
        self._delname("BIYOS_delconname", Constraints, idx)

    def delcon(self, idx: Selection[Constraint, Constraints]) -> list[Index]:
        """Delete constraints.

        The constraints left keep their order and are numbered from 0 again. A
        deleted constraint's tolerances and name go with it, and its function
        stays. Once any constraint is deleted, the model refuses every block
        and view it gave before.

        Parameters
        ----------
        idx
            The constraints to delete, in any order, none of them twice.
            An index or a view stands for one constraint.

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
        fun: Sequence[Index | Function]
        | Functions
        | Expression
        | Variable
        | Sequence[Expression | Variable],
        sense: Sequence[ObjectiveSense] | None = None,
        prio: Sequence[int] | None = None,
        coef: Sequence[Real] | None = None,
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
        funs = cast("Sequence[Index | Function] | Functions", fun)
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

    @overload
    def setobjsense(
        self, idx: One[Objective], sense: ObjectiveSense | None
    ) -> None: ...

    @overload
    def setobjsense(
        self,
        idx: Many[Objective, Objectives],
        sense: Sequence[ObjectiveSense] | None,
    ) -> None: ...

    def setobjsense(
        self,
        idx: Selection[Objective, Objectives],
        sense: ObjectiveSense | Sequence[ObjectiveSense] | None,
    ) -> None:
        """Set the senses of objectives.

        An objective may appear more than once; its entries apply in order.

        Parameters
        ----------
        idx
            The objectives.
            An index or a view stands for one objective, and each other argument
            is then a scalar.
        sense
            The new sense of each; ``None`` changes nothing.

        See Also
        --------
        BIYOS_setobjsensesp
        BIYOS_setobjsenserg
        """
        one, n, chosen = self._selected("BIYOS_setobjsense", Objectives, idx)
        arrays = (ints("sense", sense, n, one),)
        self._call(*chosen, *arrays)

    @overload
    def getobjsense(self, idx: One[Objective]) -> ObjectiveSense: ...

    @overload
    def getobjsense(
        self, idx: Many[Objective, Objectives]
    ) -> list[ObjectiveSense]: ...

    def getobjsense(
        self, idx: Selection[Objective, Objectives]
    ) -> ObjectiveSense | list[ObjectiveSense]:
        """Read the senses of objectives.

        Parameters
        ----------
        idx
            The objectives, in any order, any of them more than once.
            An index or a view stands for one objective, and the result is then
            a scalar.

        Returns
        -------
        sense
            The sense of each.

        See Also
        --------
        BIYOS_getobjsensesp
        BIYOS_getobjsenserg
        """
        (found,) = self._getlist(
            "BIYOS_getobjsense", Objectives, idx, c_integer
        )
        sense: ObjectiveSense | list[ObjectiveSense]
        sense = _each(ObjectiveSense, found)
        return sense

    @overload
    def setobjprio(self, idx: One[Objective], prio: int | None) -> None: ...

    @overload
    def setobjprio(
        self, idx: Many[Objective, Objectives], prio: Sequence[int] | None
    ) -> None: ...

    def setobjprio(
        self,
        idx: Selection[Objective, Objectives],
        prio: int | Sequence[int] | None,
    ) -> None:
        """Set the priorities of objectives.

        An objective may appear more than once; its entries apply in order.

        Parameters
        ----------
        idx
            The objectives.
            An index or a view stands for one objective, and each other argument
            is then a scalar.
        prio
            The new priority of each; ``None`` changes nothing.

        See Also
        --------
        BIYOS_setobjpriosp
        BIYOS_setobjpriorg
        """
        one, n, chosen = self._selected("BIYOS_setobjprio", Objectives, idx)
        arrays = (ints("prio", prio, n, one),)
        self._call(*chosen, *arrays)

    @overload
    def getobjprio(self, idx: One[Objective]) -> int: ...

    @overload
    def getobjprio(self, idx: Many[Objective, Objectives]) -> list[int]: ...

    def getobjprio(
        self, idx: Selection[Objective, Objectives]
    ) -> int | list[int]:
        """Read the priorities of objectives.

        Parameters
        ----------
        idx
            The objectives, in any order, any of them more than once.
            An index or a view stands for one objective, and the result is then
            a scalar.

        Returns
        -------
        prio
            The priority of each.

        See Also
        --------
        BIYOS_getobjpriosp
        BIYOS_getobjpriorg
        """
        prio: int | list[int]
        (prio,) = self._getlist("BIYOS_getobjprio", Objectives, idx, c_integer)
        return prio

    @overload
    def setobjcoef(self, idx: One[Objective], coef: Real | None) -> None: ...

    @overload
    def setobjcoef(
        self, idx: Many[Objective, Objectives], coef: Sequence[Real] | None
    ) -> None: ...

    def setobjcoef(
        self,
        idx: Selection[Objective, Objectives],
        coef: Real | Sequence[Real] | None,
    ) -> None:
        """Set the weights of objectives.

        An objective may appear more than once; its entries apply in order.

        Parameters
        ----------
        idx
            The objectives.
            An index or a view stands for one objective, and each other argument
            is then a scalar.
        coef
            The new weight of each, finite and positive; ``None`` changes
            nothing.

        See Also
        --------
        BIYOS_setobjcoefsp
        BIYOS_setobjcoefrg
        """
        one, n, chosen = self._selected("BIYOS_setobjcoef", Objectives, idx)
        arrays = (doubles("coef", coef, n, one),)
        self._call(*chosen, *arrays)

    @overload
    def getobjcoef(self, idx: One[Objective]) -> Real: ...

    @overload
    def getobjcoef(self, idx: Many[Objective, Objectives]) -> list[Real]: ...

    def getobjcoef(
        self, idx: Selection[Objective, Objectives]
    ) -> Real | list[Real]:
        """Read the weights of objectives.

        Parameters
        ----------
        idx
            The objectives, in any order, any of them more than once.
            An index or a view stands for one objective, and the result is then
            a scalar.

        Returns
        -------
        coef
            The weight of each.

        See Also
        --------
        BIYOS_getobjcoefsp
        BIYOS_getobjcoefrg
        """
        coef: Real | list[Real]
        (coef,) = self._getlist("BIYOS_getobjcoef", Objectives, idx, c_real)
        return coef

    @overload
    def setobjtol(
        self,
        idx: One[Objective],
        atol: Real | None = None,
        rtol: Real | None = None,
    ) -> None: ...

    @overload
    def setobjtol(
        self,
        idx: Many[Objective, Objectives],
        atol: Sequence[Real] | None = None,
        rtol: Sequence[Real] | None = None,
    ) -> None: ...

    def setobjtol(
        self,
        idx: Selection[Objective, Objectives],
        atol: Real | Sequence[Real] | None = None,
        rtol: Real | Sequence[Real] | None = None,
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
            An index or a view stands for one objective, and each other argument
            is then a scalar.
        atol, rtol
            The new absolute and relative tolerance of each; ``None``
            leaves that tolerance as it is.

        See Also
        --------
        BIYOS_setobjtolsp
        BIYOS_setobjtolrg
        """
        one, n, chosen = self._selected("BIYOS_setobjtol", Objectives, idx)
        arrays = (
            doubles("atol", atol, n, one),
            doubles("rtol", rtol, n, one),
        )
        self._call(*chosen, *arrays)

    @overload
    def getobjtol(self, idx: One[Objective]) -> tuple[Real, Real]: ...

    @overload
    def getobjtol(
        self, idx: Many[Objective, Objectives]
    ) -> tuple[list[Real], list[Real]]: ...

    def getobjtol(
        self, idx: Selection[Objective, Objectives]
    ) -> tuple[Real, Real] | tuple[list[Real], list[Real]]:
        """Read the tolerances of objectives.

        Parameters
        ----------
        idx
            The objectives, in any order, any of them more than once.
            An index or a view stands for one objective, and each result is then
            a scalar.

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
        pair = (c_real, c_real)
        atol, rtol = self._getlist("BIYOS_getobjtol", Objectives, idx, *pair)
        return atol, rtol

    def getnobj(self) -> Index:
        """Return the number of objectives.

        Returns
        -------
        n
            The number of objectives.

        See Also
        --------
        BIYOS_getnobj
        """
        count = c_index()
        self._call(lib.BIYOS_getnobj, byref(count))
        return count.value

    @overload
    def getobj(
        self, idx: One[Objective]
    ) -> tuple[Function, ObjectiveSense, int, Real]: ...

    @overload
    def getobj(
        self, idx: Many[Objective, Objectives] | None = None
    ) -> tuple[Functions, list[ObjectiveSense], list[int], list[Real]]: ...

    def getobj(
        self, idx: Selection[Objective, Objectives] | None = None
    ) -> (
        tuple[Function, ObjectiveSense, int, Real]
        | tuple[Functions, list[ObjectiveSense], list[int], list[Real]]
    ):
        """Read objectives.

        The functions come as a block, which raises ``ValueError`` once the
        model has had a deletion. ``indices`` gives them as a list.

        Parameters
        ----------
        idx
            The objectives, in any order, any of them more than once; every
            objective by default.
            An index or a view stands for one objective, and each result is then
            a scalar.

        Returns
        -------
        fun
            The function each objective optimizes, as a ``Functions`` block,
            or a ``Function`` for one objective.
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
        types = (Functions, c_integer, c_integer, c_real)
        found = self._getlist("BIYOS_getobj", Objectives, every, *types)
        functions, codes, priorities, weights = found
        senses = _each(ObjectiveSense, codes)
        return functions, senses, priorities, weights

    @overload
    def setobjname(self, idx: One[Objective], name: str) -> None: ...

    @overload
    def setobjname(
        self, idx: Many[Objective, Objectives], name: Sequence[str]
    ) -> None: ...

    def setobjname(
        self, idx: Selection[Objective, Objectives], name: str | Sequence[str]
    ) -> None:
        """Name objectives of the model, as ``setvarname`` names variables.

        Parameters
        ----------
        idx
            The objectives to name.
            An index or a view stands for one objective, and each other argument
            is then a scalar.
        name
            The new name of each, as many as ``idx``.

        See Also
        --------
        BIYOS_setobjnamesp
        BIYOS_setobjnamerg
        """
        self._setname("BIYOS_setobjname", Objectives, idx, name)

    @overload
    def getobjname(self, idx: One[Objective]) -> str | None: ...

    @overload
    def getobjname(
        self, idx: Many[Objective, Objectives]
    ) -> list[str | None]: ...

    def getobjname(
        self, idx: Selection[Objective, Objectives]
    ) -> str | None | list[str | None]:
        """Return the names of objectives, as ``getvarname`` does.

        Parameters
        ----------
        idx
            The objectives to read.
            An index or a view stands for one objective, and the result is then
            a scalar.

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

    def getobjidx(self, name: Sequence[str]) -> Objectives:
        """Return the named objectives, as ``getvaridx`` does.

        Parameters
        ----------
        name
            The names, each matched exactly.

        Returns
        -------
        idx
            The objective that alone holds each name, as a ``Objectives`` block.

        See Also
        --------
        BIYOS_getobjidx
        """
        return self._getidx(lib.BIYOS_getobjidx, Objectives, name)

    def delobjname(self, idx: Selection[Objective, Objectives]) -> None:
        """Remove the names of objectives, as ``delvarname`` does.

        Parameters
        ----------
        idx
            The objectives that lose their names.
            An index or a view stands for one objective.

        See Also
        --------
        BIYOS_delobjnamesp
        BIYOS_delobjnamerg
        """
        self._delname("BIYOS_delobjname", Objectives, idx)

    def delobj(self, idx: Selection[Objective, Objectives]) -> list[Index]:
        """Delete objectives.

        The objectives left keep their order and are numbered from 0 again. A
        deleted objective's name goes with it, and its function stays. Once any
        objective is deleted, the model refuses every block and view it gave
        before.

        Parameters
        ----------
        idx
            The objectives to delete, in any order, none of them twice.
            An index or a view stands for one objective.

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
        var: Sequence[Index | Variable] | Variables,
        val: Sequence[int],
        fun: Sequence[Index | Function] | Functions,
        lb: Sequence[Real] | None = None,
        ub: Sequence[Real] | None = None,
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

    @overload
    def setindbnd(
        self,
        idx: One[Indicator],
        lb: Real | None = None,
        ub: Real | None = None,
    ) -> None: ...

    @overload
    def setindbnd(
        self,
        idx: Many[Indicator, Indicators],
        lb: Sequence[Real] | None = None,
        ub: Sequence[Real] | None = None,
    ) -> None: ...

    def setindbnd(
        self,
        idx: Selection[Indicator, Indicators],
        lb: Real | Sequence[Real] | None = None,
        ub: Real | Sequence[Real] | None = None,
    ) -> None:
        """Set the bounds of indicators and keep the rest.

        Parameters
        ----------
        idx
            The indicators.
            An index or a view stands for one indicator, and each other argument
            is then a scalar.
        lb, ub
            The new bounds of each; ``None`` leaves that side as it is.

        See Also
        --------
        BIYOS_setindbndsp
        BIYOS_setindbndrg
        """
        one, n, chosen = self._selected("BIYOS_setindbnd", Indicators, idx)
        arrays = (
            doubles("lb", lb, n, one),
            doubles("ub", ub, n, one),
        )
        self._call(*chosen, *arrays)

    @overload
    def getindbnd(self, idx: One[Indicator]) -> tuple[Real, Real]: ...

    @overload
    def getindbnd(
        self, idx: Many[Indicator, Indicators]
    ) -> tuple[list[Real], list[Real]]: ...

    def getindbnd(
        self, idx: Selection[Indicator, Indicators]
    ) -> tuple[Real, Real] | tuple[list[Real], list[Real]]:
        """Read the bounds of indicators.

        Parameters
        ----------
        idx
            The indicators, in any order, any of them more than once.
            An index or a view stands for one indicator, and each result is then
            a scalar.

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
        bounds = (c_real, c_real)
        lb, ub = self._getlist("BIYOS_getindbnd", Indicators, idx, *bounds)
        return lb, ub

    @overload
    def setindvar(
        self,
        idx: One[Indicator],
        var: One[Variable] | None = None,
        val: int | None = None,
    ) -> None: ...

    @overload
    def setindvar(
        self,
        idx: Many[Indicator, Indicators],
        var: Many[Variable, Variables] | None = None,
        val: Sequence[int] | None = None,
    ) -> None: ...

    def setindvar(
        self,
        idx: Selection[Indicator, Indicators],
        var: Selection[Variable, Variables] | None = None,
        val: int | Sequence[int] | None = None,
    ) -> None:
        """Set the variables that switch indicators on, and their values.

        Parameters
        ----------
        idx
            The indicators.
            An index or a view stands for one indicator, and each other argument
            is then a scalar.
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
        one, n, chosen = self._selected("BIYOS_setindvar", Indicators, idx)
        arrays = (
            indices_of(self, Variables, "var", var, n, one),
            ints("val", val, n, one),
        )
        self._call(*chosen, *arrays)

    @overload
    def getindvar(self, idx: One[Indicator]) -> tuple[Variable, int]: ...

    @overload
    def getindvar(
        self, idx: Many[Indicator, Indicators]
    ) -> tuple[Variables, list[int]]: ...

    def getindvar(
        self, idx: Selection[Indicator, Indicators]
    ) -> tuple[Variable, int] | tuple[Variables, list[int]]:
        """Read the variables that switch indicators on, and their values.

        The variables come as a block, which raises ``ValueError`` once the
        model has had a deletion. ``indices`` gives them as a list.

        Parameters
        ----------
        idx
            The indicators, in any order, any of them more than once.
            An index or a view stands for one indicator, and each result is then
            a scalar.

        Returns
        -------
        var
            The variable of each, as a ``Variables`` block, or a ``Variable``
            for one indicator.
        val
            The value of each.

        See Also
        --------
        BIYOS_getindvarsp
        BIYOS_getindvarrg
        """
        pair = (Variables, c_integer)
        found = self._getlist("BIYOS_getindvar", Indicators, idx, *pair)
        variables, values = found
        return variables, values

    def getnind(self) -> Index:
        """Return the number of indicators.

        Returns
        -------
        n
            The number of indicators.

        See Also
        --------
        BIYOS_getnind
        """
        count = c_index()
        self._call(lib.BIYOS_getnind, byref(count))
        return count.value

    @overload
    def getind(
        self, idx: One[Indicator]
    ) -> tuple[Variable, int, Function, Real, Real]: ...

    @overload
    def getind(
        self, idx: Many[Indicator, Indicators] | None = None
    ) -> tuple[Variables, list[int], Functions, list[Real], list[Real]]: ...

    def getind(
        self, idx: Selection[Indicator, Indicators] | None = None
    ) -> (
        tuple[Variable, int, Function, Real, Real]
        | tuple[Variables, list[int], Functions, list[Real], list[Real]]
    ):
        """Read indicators.

        The variables and the functions come as blocks, which raise
        ``ValueError`` once the model has had a deletion. ``indices`` gives
        them as lists.

        Parameters
        ----------
        idx
            The indicators, in any order, any of them more than once; every
            indicator by default.
            An index or a view stands for one indicator, and each result is then
            a scalar.

        Returns
        -------
        var
            The variable that switches each indicator on, as a ``Variables``
            block, or a ``Variable`` for one indicator.
        val
            The value of each variable that switches its indicator on.
        fun
            The function each indicator bounds, as a ``Functions`` block, or
            a ``Function`` for one indicator.
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
        types = (Variables, c_integer, Functions, c_real, c_real)
        found = self._getlist("BIYOS_getind", Indicators, every, *types)
        variables, values, functions, lower, upper = found
        return variables, values, functions, lower, upper

    @overload
    def setindname(self, idx: One[Indicator], name: str) -> None: ...

    @overload
    def setindname(
        self, idx: Many[Indicator, Indicators], name: Sequence[str]
    ) -> None: ...

    def setindname(
        self, idx: Selection[Indicator, Indicators], name: str | Sequence[str]
    ) -> None:
        """Name indicators of the model, as ``setvarname`` names variables.

        Parameters
        ----------
        idx
            The indicators to name.
            An index or a view stands for one indicator, and each other argument
            is then a scalar.
        name
            The new name of each, as many as ``idx``.

        See Also
        --------
        BIYOS_setindnamesp
        BIYOS_setindnamerg
        """
        self._setname("BIYOS_setindname", Indicators, idx, name)

    @overload
    def getindname(self, idx: One[Indicator]) -> str | None: ...

    @overload
    def getindname(
        self, idx: Many[Indicator, Indicators]
    ) -> list[str | None]: ...

    def getindname(
        self, idx: Selection[Indicator, Indicators]
    ) -> str | None | list[str | None]:
        """Return the names of indicators, as ``getvarname`` does.

        Parameters
        ----------
        idx
            The indicators to read.
            An index or a view stands for one indicator, and the result is then
            a scalar.

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

    def getindidx(self, name: Sequence[str]) -> Indicators:
        """Return the named indicators, as ``getvaridx`` does.

        Parameters
        ----------
        name
            The names, each matched exactly.

        Returns
        -------
        idx
            The indicator that alone holds each name, as a ``Indicators`` block.

        See Also
        --------
        BIYOS_getindidx
        """
        return self._getidx(lib.BIYOS_getindidx, Indicators, name)

    def delindname(self, idx: Selection[Indicator, Indicators]) -> None:
        """Remove the names of indicators, as ``delvarname`` does.

        Parameters
        ----------
        idx
            The indicators that lose their names.
            An index or a view stands for one indicator.

        See Also
        --------
        BIYOS_delindnamesp
        BIYOS_delindnamerg
        """
        self._delname("BIYOS_delindname", Indicators, idx)

    def delind(self, idx: Selection[Indicator, Indicators]) -> list[Index]:
        """Delete indicators.

        The indicators left keep their order and are numbered from 0 again. A
        deleted indicator's name goes with it, and its variable and function
        stay. Once any indicator is deleted, the model refuses every block and
        view it gave before.

        Parameters
        ----------
        idx
            The indicators to delete, in any order, none of them twice.
            An index or a view stands for one indicator.

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
        cnt: Sequence[Offset],
        var: Sequence[Index | Variable] | Variables,
        coef: Sequence[Real] | None = None,
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

    def getnsos(self) -> Index:
        """Return the number of special ordered sets.

        Returns
        -------
        n
            The number of sets.

        See Also
        --------
        BIYOS_getnsos
        """
        count = c_index()
        self._call(lib.BIYOS_getnsos, byref(count))
        return count.value

    def getnsosvar(
        self,
        idx: Sequence[Index | SpecialOrderedSet]
        | SpecialOrderedSets
        | None = None,
    ) -> Offset:
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
        idx: Sequence[Index | SpecialOrderedSet]
        | SpecialOrderedSets
        | None = None,
    ) -> tuple[
        list[SpecialOrderedSetKind], list[Offset], list[Index], list[Real]
    ]:
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
        kinds = (c_integer * n)()
        counts = (c_offset * n)()
        members = (c_index * size)()
        weights = (c_real * size)()
        chosen = self._chosen("BIYOS_getsos", SpecialOrderedSets, every)
        self._call(*chosen, size, kinds, counts, members, weights)
        found = [SpecialOrderedSetKind(code) for code in kinds]
        return found, list(counts), list(members), list(weights)

    @overload
    def setsosname(self, idx: One[SpecialOrderedSet], name: str) -> None: ...

    @overload
    def setsosname(
        self,
        idx: Many[SpecialOrderedSet, SpecialOrderedSets],
        name: Sequence[str],
    ) -> None: ...

    def setsosname(
        self,
        idx: Selection[SpecialOrderedSet, SpecialOrderedSets],
        name: str | Sequence[str],
    ) -> None:
        """Name special ordered sets, as ``setvarname`` names variables.

        Parameters
        ----------
        idx
            The sets to name.
            An index or a view stands for one set, and each other argument is
            then a scalar.
        name
            The new name of each, as many as ``idx``.

        See Also
        --------
        BIYOS_setsosnamesp
        BIYOS_setsosnamerg
        """
        self._setname("BIYOS_setsosname", SpecialOrderedSets, idx, name)

    @overload
    def getsosname(self, idx: One[SpecialOrderedSet]) -> str | None: ...

    @overload
    def getsosname(
        self, idx: Many[SpecialOrderedSet, SpecialOrderedSets]
    ) -> list[str | None]: ...

    def getsosname(
        self, idx: Selection[SpecialOrderedSet, SpecialOrderedSets]
    ) -> str | None | list[str | None]:
        """Return the names of special ordered sets, as ``getvarname`` does.

        Parameters
        ----------
        idx
            The sets to read.
            An index or a view stands for one set, and the result is then a
            scalar.

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

    def getsosidx(self, name: Sequence[str]) -> SpecialOrderedSets:
        """Return the named special ordered sets, as ``getvaridx`` does.

        Parameters
        ----------
        name
            The names, each matched exactly.

        Returns
        -------
        idx
            The set that alone holds each name, as a ``SpecialOrderedSets``
            block.

        See Also
        --------
        BIYOS_getsosidx
        """
        return self._getidx(lib.BIYOS_getsosidx, SpecialOrderedSets, name)

    def delsosname(
        self, idx: Selection[SpecialOrderedSet, SpecialOrderedSets]
    ) -> None:
        """Remove the names of special ordered sets, as ``delvarname``
        does.

        Parameters
        ----------
        idx
            The sets that lose their names.
            An index or a view stands for one set.

        See Also
        --------
        BIYOS_delsosnamesp
        BIYOS_delsosnamerg
        """
        self._delname("BIYOS_delsosname", SpecialOrderedSets, idx)

    def delsos(
        self, idx: Selection[SpecialOrderedSet, SpecialOrderedSets]
    ) -> list[Index]:
        """Delete special ordered sets.

        The sets left keep their order and are numbered from 0 again. A deleted
        set's members and name go with it, and its variables stay. Once any set
        is deleted, the model refuses every block and view it gave before.

        Parameters
        ----------
        idx
            The sets to delete, in any order, none of them twice.
            An index or a view stands for one set.

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
        cnt: Sequence[Offset],
        x: Sequence[Real],
        y: Sequence[Real],
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

    def getnpwl(self) -> Index:
        """Return the number of piecewise linear functions.

        Returns
        -------
        n
            The number of pwls.

        See Also
        --------
        BIYOS_getnpwl
        """
        count = c_index()
        self._call(lib.BIYOS_getnpwl, byref(count))
        return count.value

    def getnpwlpt(
        self,
        idx: Sequence[Index | PiecewiseLinear] | PiecewiseLinears | None = None,
    ) -> Offset:
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
        idx: Sequence[Index | PiecewiseLinear] | PiecewiseLinears | None = None,
    ) -> tuple[list[Offset], list[Real], list[Real]]:
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
        sizes = (c_offset * len(every))()
        xs = (c_real * size)()
        ys = (c_real * size)()
        chosen = self._chosen("BIYOS_getpwl", PiecewiseLinears, every)
        self._call(*chosen, size, sizes, xs, ys)
        return list(sizes), list(xs), list(ys)

    def addpwlnode(self, pwl: Index | PiecewiseLinear, arg: Offset) -> Offset:
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
        node = c_offset()
        self._call(lib.BIYOS_addpwlnode, pwl, offset("arg", arg), byref(node))
        return node.value

    def addcone(
        self,
        knd: Sequence[ConeKind],
        cnt: Sequence[Offset],
        fun: Sequence[Index | Function] | Functions,
        prmcnt: Sequence[Offset] | None = None,
        prm: Sequence[Real] | None = None,
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

    @overload
    def setconetol(
        self,
        idx: One[Cone],
        atol: Real | None = None,
        rtol: Real | None = None,
    ) -> None: ...

    @overload
    def setconetol(
        self,
        idx: Many[Cone, Cones],
        atol: Sequence[Real] | None = None,
        rtol: Sequence[Real] | None = None,
    ) -> None: ...

    def setconetol(
        self,
        idx: Selection[Cone, Cones],
        atol: Real | Sequence[Real] | None = None,
        rtol: Real | Sequence[Real] | None = None,
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
            An index or a view stands for one cone, and each other argument is
            then a scalar.
        atol, rtol
            The new absolute and relative tolerance of each; ``None``
            leaves that tolerance as it is.

        See Also
        --------
        BIYOS_setconetolsp
        BIYOS_setconetolrg
        """
        one, n, chosen = self._selected("BIYOS_setconetol", Cones, idx)
        arrays = (
            doubles("atol", atol, n, one),
            doubles("rtol", rtol, n, one),
        )
        self._call(*chosen, *arrays)

    @overload
    def getconetol(
        self, idx: One[Cone]
    ) -> tuple[Real | None, Real | None]: ...

    @overload
    def getconetol(
        self, idx: Many[Cone, Cones]
    ) -> tuple[list[Real | None], list[Real | None]]: ...

    def getconetol(
        self, idx: Selection[Cone, Cones]
    ) -> (
        tuple[Real | None, Real | None]
        | tuple[list[Real | None], list[Real | None]]
    ):
        """Read the tolerances of cones.

        Parameters
        ----------
        idx
            The cones, in any order, any of them more than once.
            An index or a view stands for one cone, and each result is then a
            scalar.

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
        pair = (c_real, c_real)
        atol, rtol = self._getlist("BIYOS_getconetol", Cones, idx, *pair)
        return _each(_or_none, atol), _each(_or_none, rtol)

    def delconetol(self, idx: Selection[Cone, Cones]) -> None:
        """Return cones to ``feastol_abs`` and ``feastol_rel``.

        Parameters
        ----------
        idx
            The cones that lose their tolerances.
            An index or a view stands for one cone.

        See Also
        --------
        BIYOS_delconetolsp
        BIYOS_delconetolrg
        """
        rows = picked(self, Cones, idx)
        self._call(*self._chosen("BIYOS_delconetol", Cones, rows))

    def getncone(self) -> Index:
        """Return the number of cones.

        Returns
        -------
        n
            The number of cones.

        See Also
        --------
        BIYOS_getncone
        """
        count = c_index()
        self._call(lib.BIYOS_getncone, byref(count))
        return count.value

    def getnconefun(
        self, idx: Sequence[Index | Cone] | Cones | None = None
    ) -> Offset:
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
        self, idx: Sequence[Index | Cone] | Cones | None = None
    ) -> tuple[list[ConeKind], list[Offset], list[Index]]:
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
        kinds = (c_integer * len(every))()
        counts = (c_offset * len(every))()
        members = (c_index * size)()
        self._call(*chosen, size, kinds, counts, members)
        found = [ConeKind(kind) for kind in kinds]
        return found, list(counts), list(members)

    def getnconeprm(
        self, idx: Sequence[Index | Cone] | Cones | None = None
    ) -> Offset:
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
        self, idx: Sequence[Index | Cone] | Cones | None = None
    ) -> tuple[list[Offset], list[Real]]:
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
        counts = (c_offset * len(every))()
        values = (c_real * size)()
        self._call(*chosen, size, counts, values)
        return list(counts), list(values)

    @overload
    def setconename(self, idx: One[Cone], name: str) -> None: ...

    @overload
    def setconename(
        self, idx: Many[Cone, Cones], name: Sequence[str]
    ) -> None: ...

    def setconename(
        self, idx: Selection[Cone, Cones], name: str | Sequence[str]
    ) -> None:
        """Name cones of the model, as ``setvarname`` names variables.

        Parameters
        ----------
        idx
            The cones to name.
            An index or a view stands for one cone, and each other argument is
            then a scalar.
        name
            The new name of each, as many as ``idx``.

        See Also
        --------
        BIYOS_setconenamesp
        BIYOS_setconenamerg
        """
        self._setname("BIYOS_setconename", Cones, idx, name)

    @overload
    def getconename(self, idx: One[Cone]) -> str | None: ...

    @overload
    def getconename(self, idx: Many[Cone, Cones]) -> list[str | None]: ...

    def getconename(
        self, idx: Selection[Cone, Cones]
    ) -> str | None | list[str | None]:
        """Return the names of cones, as ``getvarname`` does.

        Parameters
        ----------
        idx
            The cones to read.
            An index or a view stands for one cone, and the result is then a
            scalar.

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

    def getconeidx(self, name: Sequence[str]) -> Cones:
        """Return the named cones, as ``getvaridx`` does.

        Parameters
        ----------
        name
            The names, each matched exactly.

        Returns
        -------
        idx
            The cone that alone holds each name, as a ``Cones`` block.

        See Also
        --------
        BIYOS_getconeidx
        """
        return self._getidx(lib.BIYOS_getconeidx, Cones, name)

    def delconename(self, idx: Selection[Cone, Cones]) -> None:
        """Remove the names of cones, as ``delvarname`` does.

        Parameters
        ----------
        idx
            The cones that lose their names.
            An index or a view stands for one cone.

        See Also
        --------
        BIYOS_delconenamesp
        BIYOS_delconenamerg
        """
        self._delname("BIYOS_delconename", Cones, idx)

    def delcone(self, idx: Selection[Cone, Cones]) -> list[Index]:
        """Delete cones.

        The cones left keep their order and are numbered from 0 again. A
        deleted cone's members, parameters, tolerances and name go with it, and
        its functions stay. Once any cone is deleted, the model refuses every
        block and view it gave before.

        Parameters
        ----------
        idx
            The cones to delete, in any order, none of them twice.
            An index or a view stands for one cone.

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
        fun1: Sequence[Index | Function] | Functions,
        fun2: Sequence[Index | Function] | Functions,
        lb: Sequence[Real] | None = None,
        ub: Sequence[Real] | None = None,
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

    @overload
    def setcconbnd(
        self,
        idx: One[Complement],
        lb: Real | None = None,
        ub: Real | None = None,
    ) -> None: ...

    @overload
    def setcconbnd(
        self,
        idx: Many[Complement, Complements],
        lb: Sequence[Real] | None = None,
        ub: Sequence[Real] | None = None,
    ) -> None: ...

    def setcconbnd(
        self,
        idx: Selection[Complement, Complements],
        lb: Real | Sequence[Real] | None = None,
        ub: Real | Sequence[Real] | None = None,
    ) -> None:
        """Set the bounds of complements and keep the rest.

        Parameters
        ----------
        idx
            The complements.
            An index or a view stands for one complement, and each other
            argument is then a scalar.
        lb, ub
            The new bounds of each; ``None`` leaves that side as it is.

        See Also
        --------
        BIYOS_setcconbndsp
        BIYOS_setcconbndrg
        """
        one, n, chosen = self._selected("BIYOS_setcconbnd", Complements, idx)
        arrays = (
            doubles("lb", lb, n, one),
            doubles("ub", ub, n, one),
        )
        self._call(*chosen, *arrays)

    @overload
    def getcconbnd(self, idx: One[Complement]) -> tuple[Real, Real]: ...

    @overload
    def getcconbnd(
        self, idx: Many[Complement, Complements]
    ) -> tuple[list[Real], list[Real]]: ...

    def getcconbnd(
        self, idx: Selection[Complement, Complements]
    ) -> tuple[Real, Real] | tuple[list[Real], list[Real]]:
        """Read the bounds of complements.

        Parameters
        ----------
        idx
            The complements, in any order, any of them more than once.
            An index or a view stands for one complement, and each result is
            then a scalar.

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
        bounds = (c_real, c_real)
        lb, ub = self._getlist("BIYOS_getcconbnd", Complements, idx, *bounds)
        return lb, ub

    @overload
    def setccontol(
        self,
        idx: One[Complement],
        atol: Real | None = None,
        rtol: Real | None = None,
    ) -> None: ...

    @overload
    def setccontol(
        self,
        idx: Many[Complement, Complements],
        atol: Sequence[Real] | None = None,
        rtol: Sequence[Real] | None = None,
    ) -> None: ...

    def setccontol(
        self,
        idx: Selection[Complement, Complements],
        atol: Real | Sequence[Real] | None = None,
        rtol: Real | Sequence[Real] | None = None,
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
            An index or a view stands for one complement, and each other
            argument is then a scalar.
        atol, rtol
            The new absolute and relative tolerance of each; ``None``
            leaves that tolerance as it is.

        See Also
        --------
        BIYOS_setccontolsp
        BIYOS_setccontolrg
        """
        one, n, chosen = self._selected("BIYOS_setccontol", Complements, idx)
        arrays = (
            doubles("atol", atol, n, one),
            doubles("rtol", rtol, n, one),
        )
        self._call(*chosen, *arrays)

    @overload
    def getccontol(
        self, idx: One[Complement]
    ) -> tuple[Real | None, Real | None]: ...

    @overload
    def getccontol(
        self, idx: Many[Complement, Complements]
    ) -> tuple[list[Real | None], list[Real | None]]: ...

    def getccontol(
        self, idx: Selection[Complement, Complements]
    ) -> (
        tuple[Real | None, Real | None]
        | tuple[list[Real | None], list[Real | None]]
    ):
        """Read the tolerances of complements.

        Parameters
        ----------
        idx
            The complements, in any order, any of them more than once.
            An index or a view stands for one complement, and each result is
            then a scalar.

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
        pair = (c_real, c_real)
        atol, rtol = self._getlist("BIYOS_getccontol", Complements, idx, *pair)
        return _each(_or_none, atol), _each(_or_none, rtol)

    def delccontol(self, idx: Selection[Complement, Complements]) -> None:
        """Return complements to ``feastol_abs`` and ``feastol_rel``.

        Parameters
        ----------
        idx
            The complements that lose their tolerances.
            An index or a view stands for one complement.

        See Also
        --------
        BIYOS_delccontolsp
        BIYOS_delccontolrg
        """
        rows = picked(self, Complements, idx)
        self._call(*self._chosen("BIYOS_delccontol", Complements, rows))

    def getnccon(self) -> Index:
        """Return the number of complements.

        Returns
        -------
        n
            The number of complements.

        See Also
        --------
        BIYOS_getnccon
        """
        count = c_index()
        self._call(lib.BIYOS_getnccon, byref(count))
        return count.value

    @overload
    def getccon(
        self, idx: One[Complement]
    ) -> tuple[Function, Function, Real, Real]: ...

    @overload
    def getccon(
        self, idx: Many[Complement, Complements] | None = None
    ) -> tuple[Functions, Functions, list[Real], list[Real]]: ...

    def getccon(
        self, idx: Selection[Complement, Complements] | None = None
    ) -> (
        tuple[Function, Function, Real, Real]
        | tuple[Functions, Functions, list[Real], list[Real]]
    ):
        """Read the functions and bounds of complements.

        The functions come as blocks, which raise ``ValueError`` once the
        model has had a deletion. ``indices`` gives them as lists.

        Parameters
        ----------
        idx
            The complements, in any order, any of them more than once; every
            complement by default.
            An index or a view stands for one complement, and each result is
            then a scalar.

        Returns
        -------
        fun1
            The bounded function of each complement, as a ``Functions``
            block, or a ``Function`` for one complement.
        fun2
            The function complementary to it, in the same form.
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
        types = (Functions, Functions, c_real, c_real)
        found = self._getlist("BIYOS_getccon", Complements, every, *types)
        bounded, partners, lower, upper = found
        return bounded, partners, lower, upper

    @overload
    def setcconname(self, idx: One[Complement], name: str) -> None: ...

    @overload
    def setcconname(
        self, idx: Many[Complement, Complements], name: Sequence[str]
    ) -> None: ...

    def setcconname(
        self, idx: Selection[Complement, Complements], name: str | Sequence[str]
    ) -> None:
        """Name complements of the model, as ``setvarname`` names variables.

        Parameters
        ----------
        idx
            The complements to name.
            An index or a view stands for one complement, and each other
            argument is then a scalar.
        name
            The new name of each, as many as ``idx``.

        See Also
        --------
        BIYOS_setcconnamesp
        BIYOS_setcconnamerg
        """
        self._setname("BIYOS_setcconname", Complements, idx, name)

    @overload
    def getcconname(self, idx: One[Complement]) -> str | None: ...

    @overload
    def getcconname(
        self, idx: Many[Complement, Complements]
    ) -> list[str | None]: ...

    def getcconname(
        self, idx: Selection[Complement, Complements]
    ) -> str | None | list[str | None]:
        """Return the names of complements, as ``getvarname`` does.

        Parameters
        ----------
        idx
            The complements to read.
            An index or a view stands for one complement, and the result is then
            a scalar.

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

    def getcconidx(self, name: Sequence[str]) -> Complements:
        """Return the named complements, as ``getvaridx`` does.

        Parameters
        ----------
        name
            The names, each matched exactly.

        Returns
        -------
        idx
            The complement that alone holds each name, as a ``Complements``
            block.

        See Also
        --------
        BIYOS_getcconidx
        """
        return self._getidx(lib.BIYOS_getcconidx, Complements, name)

    def delcconname(self, idx: Selection[Complement, Complements]) -> None:
        """Remove the names of complements, as ``delvarname`` does.

        Parameters
        ----------
        idx
            The complements that lose their names.
            An index or a view stands for one complement.

        See Also
        --------
        BIYOS_delcconnamesp
        BIYOS_delcconnamerg
        """
        self._delname("BIYOS_delcconname", Complements, idx)

    def delccon(self, idx: Selection[Complement, Complements]) -> list[Index]:
        """Delete complements.

        The complements left keep their order and are numbered from 0 again. A
        deleted complement's tolerances and name go with it, and its functions
        stay. Once any complement is deleted, the model refuses every block and
        view it gave before.

        Parameters
        ----------
        idx
            The complements to delete, in any order, none of them twice.
            An index or a view stands for one complement.

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

    def evalf(self, x: Sequence[Real]) -> list[Real]:
        """Evaluate the functions at a point.

        Parameters
        ----------
        x
            The point, a value for each variable.

        Returns
        -------
        f
            The value of each function, in the order of the functions.

        Raises
        ------
        Error
            With ``code`` ``BIYOS_ERR_CALLBACK`` when a callback returns a
            negative int, raises an ``Exception``, which is then the
            error's ``__cause__``, or writes a value that is not finite
            while ``checking`` is on. With ``BIYOS_ERR_UNSUPPORTED`` when a
            callback has no function of its values.

        Notes
        -----
        The call runs the callbacks that cover the functions. One that
        returns a positive int leaves NaN in the values it computes, and the
        call still returns. An exception that is not an ``Exception``, such
        as ``KeyboardInterrupt`` or ``SystemExit``, comes out of the call
        unchanged.

        See Also
        --------
        BIYOS_evalf
        """
        f = (c_real * self.getnfun())()
        self._evaluate(lib.BIYOS_evalf, self._point(x), f)
        return list(f)

    def evalg(self, x: Sequence[Real]) -> list[Real]:
        """Evaluate the Jacobian of the functions at a point.

        Parameters
        ----------
        x
            The point, a value for each variable.

        Returns
        -------
        g
            The value of each entry of the Jacobian pattern, in the order
            ``getgpat()`` gives.

        Raises
        ------
        Error
            With ``BIYOS_ERR_CALLBACK`` as ``evalf`` gives it, and with
            ``BIYOS_ERR_UNSUPPORTED`` when a callback has no Jacobian.

        See Also
        --------
        BIYOS_evalg
        """
        point = self._point(x)
        g = (c_real * self.getngpat())()
        self._evaluate(lib.BIYOS_evalg, point, g)
        return list(g)

    def evalh(self, x: Sequence[Real], w: Sequence[Real]) -> list[Real]:
        """Evaluate the Hessian of the Lagrangian at a point.

        The Lagrangian is the sum of the functions, each weighted by its
        entry of ``w``.

        Parameters
        ----------
        x
            The point, a value for each variable.
        w
            The weight of each function.

        Returns
        -------
        h
            The value of each entry of the lower Hessian pattern, in the
            order ``gethpat()`` gives.

        Raises
        ------
        Error
            With ``BIYOS_ERR_CALLBACK`` as ``evalf`` gives it, and with
            ``BIYOS_ERR_UNSUPPORTED`` when a callback has no Hessian.

        See Also
        --------
        BIYOS_evalh
        """
        arrays = (self._point(x), doubles("w", w, self.getnfun()))
        h = (c_real * self.getnhpat())()
        self._evaluate(lib.BIYOS_evalh, *arrays, h)
        return list(h)

    def evalhv(
        self, x: Sequence[Real], w: Sequence[Real], v: Sequence[Real]
    ) -> list[Real]:
        """Multiply the Hessian of the Lagrangian at a point by a vector.

        Parameters
        ----------
        x
            The point, a value for each variable.
        w
            The weight of each function.
        v
            The vector, a value for each variable.

        Returns
        -------
        hv
            The product, a value for each variable.

        Raises
        ------
        Error
            With ``BIYOS_ERR_CALLBACK`` as ``evalf`` gives it, and with
            ``BIYOS_ERR_UNSUPPORTED`` when a callback has neither a product
            nor a Hessian.

        Notes
        -----
        A callback without a product of its own gives its part through its
        Hessian.

        See Also
        --------
        BIYOS_evalhv
        """
        n = self.getnvar()
        weights = doubles("w", w, self.getnfun())
        arrays = (self._point(x), weights, doubles("v", v, n))
        hv = (c_real * n)()
        self._evaluate(lib.BIYOS_evalhv, *arrays, hv)
        return list(hv)

    def getngpat(
        self, idx: Sequence[Index | Function] | Functions | None = None
    ) -> Offset:
        """Count the entries of the Jacobian pattern of functions.

        Parameters
        ----------
        idx
            The functions, in any order, any of them more than once; every
            function by default.

        Returns
        -------
        nnz
            The number of variables in their patterns, a function counted
            each time it appears.

        See Also
        --------
        BIYOS_getngpatsp
        BIYOS_getngpatrg
        """
        every = range(self.getnfun()) if idx is None else idx
        return self._count("BIYOS_getngpat", Functions, every)

    def getgpat(
        self, idx: Sequence[Index | Function] | Functions | None = None
    ) -> tuple[list[Offset], list[Index]]:
        """Read the Jacobian pattern of functions.

        Parameters
        ----------
        idx
            The functions, in any order, any of them more than once; every
            function by default.

        Returns
        -------
        cnt
            The number of variables in the pattern of each function.
        var
            The variables of each function's pattern, sorted, those of
            ``idx[0]`` first.

        See Also
        --------
        BIYOS_getgpatsp
        BIYOS_getgpatrg
        """
        every = range(self.getnfun()) if idx is None else idx
        return self._pattern("gpat", Functions, every)

    def getnhpat(
        self, idx: Sequence[Index | Variable] | Variables | None = None
    ) -> Offset:
        """Count the entries of the Hessian pattern in columns of its lower
        triangle.

        Parameters
        ----------
        idx
            The variables whose columns to count, in any order, any of them
            more than once; every variable by default.

        Returns
        -------
        nnz
            The number of rows in those columns, a column counted each time
            it appears.

        See Also
        --------
        BIYOS_getnhpatsp
        BIYOS_getnhpatrg
        """
        every = range(self.getnvar()) if idx is None else idx
        return self._count("BIYOS_getnhpat", Variables, every)

    def gethpat(
        self, idx: Sequence[Index | Variable] | Variables | None = None
    ) -> tuple[list[Offset], list[Index]]:
        """Read the Hessian pattern in columns of its lower triangle.

        Parameters
        ----------
        idx
            The variables whose columns to read, in any order, any of them
            more than once; every variable by default.

        Returns
        -------
        cnt
            The number of rows in each column.
        var
            The rows of each column, sorted, each at least the column's
            variable, those of ``idx[0]`` first.

        See Also
        --------
        BIYOS_gethpatsp
        BIYOS_gethpatrg
        """
        every = range(self.getnvar()) if idx is None else idx
        return self._pattern("hpat", Variables, every)

    def _compared(
        self,
        cons: list[Comparison],
        lb: Sequence[Real] | None,
        ub: Sequence[Real] | None,
    ) -> Constraints:
        if lb is not None or ub is not None:
            raise TypeError("lb and ub come from the comparisons")
        funs = attached(self, [con._body for con in cons])
        lower = [con._lb for con in cons]
        upper = [con._ub for con in cons]
        return self.addcon(funs, lower, upper)

    def _peer(self, name: str, value: object) -> "Model":
        if not isinstance(value, Model):
            found = type(value).__name__
            raise TypeError(f"{name} must be a Model, not {found}")
        return value

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
        idx: c_index,
    ) -> None:
        callback = self._hooks.callback
        out = byref(idx)
        key = self._hooks.add(hk)
        try:
            self._call(lib.BIYOS_addmdlhook, callback, key, knd, lvl, out)
        finally:
            self._hooks.settle(key, idx.value)

    def _delhook(self, idx: Index) -> None:
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
        family: type[Block[View]],
        idx: Sequence[SupportsIndex] | Block[View],
        outputs: int = 0,
        name: str = "idx",
    ) -> tuple[Callable[..., int], Index, object]:
        span = span_of(self, family, name, sequence(name, idx))
        if span is None:
            n = len(idx)
            indices = indices_of(self, family, name, idx, n)
            return getattr(lib, f"{call}sp"), n, indices
        start = integer(f"the start of {name}", span.start)
        stop = integer(f"the stop of {name}", min(start + len(span), _INT_MAX))
        function = getattr(lib, f"{call}rg")
        if outputs > 0 and not self._inside(family, start, stop):
            self._call(function, start, stop, *([None] * outputs))
        return function, start, stop

    def _selected(
        self,
        call: str,
        family: type[Block[View]],
        idx: SupportsIndex | Sequence[SupportsIndex] | Block[View],
        outputs: int = 0,
    ) -> tuple[bool, Index, tuple[Callable[..., int], Index, object]]:
        one, n = counted(idx)
        rows = picked(self, family, idx)
        return one, n, self._chosen(call, family, rows, outputs)

    def _inside(
        self, family: type[Block[View]], start: Index, stop: Index
    ) -> bool:
        total: Index = getattr(self, _COUNTS[family])()
        return 0 <= start <= stop <= total

    def _setname(
        self,
        call: str,
        family: type[Block[View]],
        idx: SupportsIndex | Sequence[SupportsIndex] | Block[View],
        name: str | Sequence[str],
    ) -> None:
        one, n, chosen = self._selected(call, family, idx)
        self._call(*chosen, strings("name", name, n, one))

    def _delname(
        self,
        call: str,
        family: type[Block[View]],
        idx: SupportsIndex | Sequence[SupportsIndex] | Block[View],
    ) -> None:
        idx = picked(self, family, idx)
        self._call(*self._chosen(call, family, idx))

    def _delete(
        self,
        call: str,
        family: type[Block[View]],
        idx: SupportsIndex | Sequence[SupportsIndex] | Block[View],
    ) -> list[Index]:
        idx = picked(self, family, idx)
        chosen = self._chosen(call, family, idx)
        gone = self._gone(family, idx)
        total: Index = getattr(self, _COUNTS[family])()
        self._call(*chosen, None)
        if len(idx) > 0:
            self._gen += 1
        return _renumbered(total, gone)

    def _gone(
        self,
        family: type[Block[View]],
        idx: Sequence[SupportsIndex] | Block[View],
    ) -> set[Index]:
        if isinstance(idx, Block):
            return set(idx_of(self, family, "idx", idx))
        span = span_of(self, family, "idx", idx)
        if span is not None:
            return set(span)
        return {index_of(self, family, "idx", value) for value in idx}

    def _getname(
        self,
        call: str,
        family: type[Block[View]],
        idx: SupportsIndex | Sequence[SupportsIndex] | Block[View],
    ) -> str | None | list[str | None]:
        one, n, chosen = self._selected(call, family, idx, 1)
        found = (c_char_p * n)()
        self._call(*chosen, found)
        names = [decoded(value) for value in found]
        return names[0] if one else names

    def _getlist(
        self,
        call: str,
        family: type[Block[View]],
        idx: SupportsIndex | Sequence[SupportsIndex] | Block[View],
        *types: _Column,
    ) -> list[Any]:
        one, n, chosen = self._selected(call, family, idx, len(types))
        found = [_output(column, n) for column in types]
        self._call(*chosen, *map(_argument, found))
        columns = zip(types, found)
        return [self._column(column, values, one) for column, values in columns]

    def _column(self, column: _Column, values: Any, one: bool) -> Any:
        if issubclass(column, Block):
            block = column(self, values, self._gen)
            return block[0] if one else block
        return values[0] if one else list(values)

    def _count(
        self,
        call: str,
        family: type[Block[View]],
        idx: Sequence[SupportsIndex] | Block[View],
    ) -> Offset:
        nnz = c_offset()
        self._call(*self._chosen(call, family, idx), byref(nnz))
        return nnz.value

    def _addterms(
        self,
        lead: _Argument,
        others: Sequence[_Argument],
        coef: object,
        *,
        coordinates: str,
        lists: str,
    ) -> None:
        _, name, entries = others[0]
        if isinstance(lead[2], Sized) and not nested(name, entries):
            self._addcoordinates(coordinates, [lead, *others], coef)
        else:
            self._addlists(lists, lead, others, coef)

    def _addcoordinates(
        self, call: str, arguments: Sequence[_Argument], coef: object
    ) -> None:
        for _, name, entries in arguments[1:]:
            sequence(name, entries)
        coef = sequence("coef", coef)
        size = len(arguments[0][2])
        arrays = [
            indices_of(self, family, name, entries, size)
            for family, name, entries in arguments
        ]
        coefs = doubles("coef", coef, size)
        self._call(getattr(lib, call), size, *arrays, coefs)

    def _addlists(
        self,
        call: str,
        lead: _Argument,
        others: Sequence[_Argument],
        coef: object,
    ) -> None:
        family, name, entries = lead
        if not isinstance(entries, Sized):
            entries = [index_of(self, family, name, entries)]
            others, coef = _single(others, coef)
        chosen = self._chosen(call, family, entries, name=name)
        cnt, arrays, coefs = self._lists(others, coef, len(entries))
        self._call(*chosen, cnt, *arrays, coefs)

    def _lists(
        self, others: Sequence[_Argument], coef: object, count: int
    ) -> tuple[Array[c_offset], list[Array[c_index]], Array[c_real]]:
        family, name, entries = others[0]
        first = entries_of(name, entries, count)
        sizes = [len(entry) for _, entry in first]
        arrays = [nested_indices(self, family, first)]
        for family, name, entries in others[1:]:
            parts = matched(name, entries, sizes)
            arrays.append(nested_indices(self, family, parts))
        coefs = nested_doubles(matched("coef", coef, sizes))
        cnt = (c_offset * count)(*sizes)
        return cnt, arrays, coefs

    def _terms(
        self,
        name: str,
        family: type[Block[View]],
        idx: Sequence[SupportsIndex] | Block[View],
        place: int,
        width: int = 2,
    ) -> tuple[list[list[Index]], list[Real]]:
        size = self._count(f"BIYOS_getn{name}", family, idx)
        span = span_of(self, family, "idx", idx)
        chosen = self._chosen(f"BIYOS_get{name}", family, idx)
        records = (
            list(cast("Array[c_index]", chosen[2])) if span is None else span
        )
        found = [(c_index * size)() for _ in range(width)]
        values = (c_real * size)()
        counts = (c_offset * len(records))()
        others = found[:place] + found[place + 1 :]
        self._call(*chosen, size, counts, *others, values)
        columns = [list(column) for column in found]
        columns[place] = _expanded(counts, records)
        return columns, list(values)

    def _pattern(
        self,
        name: str,
        family: type[Block[View]],
        idx: Sequence[SupportsIndex] | Block[View],
    ) -> tuple[list[Offset], list[Index]]:
        nnz = self._count(f"BIYOS_getn{name}", family, idx)
        cnt = (c_offset * len(idx))()
        var = (c_index * nnz)()
        chosen = self._chosen(f"BIYOS_get{name}", family, idx)
        self._call(*chosen, nnz, cnt, var)
        return list(cnt), list(var)

    def _point(self, x: Sequence[Real]) -> Array[c_real] | None:
        return doubles("x", x, self.getnvar())

    def _evaluate(
        self, function: Callable[..., int], *arguments: object
    ) -> None:
        mdl = self._handle.value
        try:
            self._call(function, *arguments)
        except Error as error:
            cause = self._callbacks.taken(mdl)
            if cause is None:
                raise
            if not isinstance(cause, Exception):
                raise cause
            raise error from cause

    def _getidx(
        self,
        function: Callable[..., int],
        family: type[_Block],
        name: Sequence[str],
    ) -> _Block:
        n = len(listed("name", name))
        names = strings("name", name, n)
        found = index_view(n)
        self._call(function, n, names, index_array(found))
        return family(self, found, self._gen)

    def _added(
        self,
        family: type[_Block],
        function: Callable[..., int],
        n: Index,
        *arguments: object,
    ) -> _Block:
        pos = c_index()
        self._call(function, n, *arguments, byref(pos))
        return family(self, range(pos.value, pos.value + n), self._gen)

    def _call(self, function: Callable[..., int], *arguments: object) -> None:
        with self._env._busy():
            self._run(function, *arguments)

    def _run(self, function: Callable[..., int], *arguments: object) -> None:
        with self._entered():
            self._env._run(function, self._handle, *arguments)

    @contextmanager
    def _entered(self) -> Iterator[None]:
        self._calls += 1
        try:
            if not self._open():
                raise ValueError("the model is closed")
            yield
        finally:
            self._calls -= 1
            self._settle()

    def _settle(self) -> None:
        if self._calls == 0 and self._closing:
            self._free()
