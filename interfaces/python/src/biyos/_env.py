import weakref
from collections.abc import Callable, Iterator
from contextlib import contextmanager
from ctypes import byref, c_int, c_void_p
from typing import TYPE_CHECKING, NoReturn

from biyos import _capi, _values
from biyos._capi import lib
from biyos._error import Error, check
from biyos._event import Event, EventKind, EventLevel
from biyos._handles import Handles
from biyos._integers import integer
from biyos._relay import Relay
from biyos._hooks import Hooks
from biyos._text import encoded
from biyos._values import Value
from biyos._versions import Versions, version_text

if TYPE_CHECKING:
    from biyos._model import Model


def _name(name: str) -> bytes:
    if not isinstance(name, str):
        found = type(name).__name__
        raise TypeError(f"option name must be a str, not {found}")
    return encoded(name)


def _callable(
    function: Callable[[Event], object],
) -> Callable[[Event], object]:
    if not callable(function):
        found = type(function).__name__
        raise TypeError(f"hook must be callable, not {found}")
    return function


_SETTERS: dict[int, Callable[..., int]] = {
    _capi.KIND_BOOL: lib.BIYOS_setenvboolopt,
    _capi.KIND_INT: lib.BIYOS_setenvintopt,
    _capi.KIND_DOUBLE: lib.BIYOS_setenvdblopt,
    _capi.KIND_STRING: lib.BIYOS_setenvstropt,
    _capi.KIND_ENUM: lib.BIYOS_setenvintopt,
    _capi.KIND_FLAGS: lib.BIYOS_setenvflgopt,
}

_GETTERS: dict[int, Callable[..., int]] = {
    _capi.KIND_BOOL: lib.BIYOS_getenvboolopt,
    _capi.KIND_INT: lib.BIYOS_getenvintopt,
    _capi.KIND_DOUBLE: lib.BIYOS_getenvdblopt,
    _capi.KIND_STRING: lib.BIYOS_getenvstropt,
    _capi.KIND_ENUM: lib.BIYOS_getenvintopt,
    _capi.KIND_FLAGS: lib.BIYOS_getenvflgopt,
}


def _release(models: Handles, handle: c_void_p) -> int:
    models.release()
    status: int = lib.BIYOS_freeenv(handle)
    return status


class Env:
    """An environment: the options and the hooks every model made on it shares.

    A model may also hold its own value of an option, which it reads before
    the environment's.

    The environment is closed by ``close``, on leaving a ``with`` block, or
    when it is collected, and closing it closes the models still open on it.

    Raises
    ------
    Error
        When the environment cannot be created.

    See Also
    --------
    BIYOS_newenv, BIYOS_freeenv
    """

    def __init__(self) -> None:
        handle = c_void_p()
        status = lib.BIYOS_newenv(byref(handle))
        if status != _capi.OK:
            raise Error("cannot create the environment", status)
        self._handle = handle
        self._models = Handles()
        self._owners: weakref.WeakValueDictionary[int, Model] = (
            weakref.WeakValueDictionary()
        )
        self._arm()
        self._relay = Relay(handle, self._owners)
        self._hooks = Hooks(self._relay, self._owners)
        self._calls = 0
        self._closing = False
        self._pending = False

    def __enter__(self) -> "Env":
        return self

    def __exit__(self, *exc_info: object) -> None:
        self.close()

    def __copy__(self) -> NoReturn:
        raise TypeError("an environment cannot be copied")

    def __deepcopy__(self, memo: dict[int, object]) -> NoReturn:
        self.__copy__()

    def close(self) -> None:
        """Free the environment and every model made on it.

        The models still open are closed first, so using one afterwards
        raises ``ValueError``. Closing a freed environment does nothing.
        Closed during a call on it or on one of its models, by a hook or
        a finalizer, it reads as closed at once and is freed when the
        outermost call returns. Such a deferred close never raises.

        Raises
        ------
        Error
            When the free is refused because a model is still allocated, as
            after an interrupted ``Model.close``. An exception raised by a
            hook during the refusal is raised instead. The environment
            stays allocated, and only a later ``close`` or its collection
            tries again; the collection cannot raise and fails silently.

        See Also
        --------
        BIYOS_freeenv
        """
        self._closing = True
        self._pending = True
        if self._calls == 0:
            self._run(self._dispose)

    def setopt(self, name: str, val: Value) -> None:
        """Set an option.

        Parameters
        ----------
        name
            The name of the option, such as ``"maxtime"``.
        val
            The new value, of the type the option's kind takes. An enum
            option takes a member of its class, such as
            ``CheckLevel.FULL``, or its number. A flags option takes a
            ``Masks`` or a pair of ``int``: the parts forced on, then the
            parts forced off.

        Raises
        ------
        TypeError
            When ``val`` does not match the option's kind.
        OverflowError
            When a mask of a flags option is negative or wider than 64 bits.
        Error
            When the name is unknown or the value is refused.

        See Also
        --------
        BIYOS_setenvboolopt, BIYOS_setenvintopt,
        BIYOS_setenvdblopt, BIYOS_setenvstropt,
        BIYOS_setenvflgopt
        """
        option, kind = self._lookup(name)
        self._call(_SETTERS[kind], option, *_values.arguments(name, kind, val))

    def getopt(self, name: str) -> Value:
        """Read an option.

        Parameters
        ----------
        name
            The name of the option.

        Returns
        -------
        val
            The value, of the type the option's kind gives. An enum
            option gives a member of its class, such as
            ``CheckLevel.CHEAP``, and a flags option its ``Masks`` as set.

        Raises
        ------
        Error
            When the name is unknown.

        See Also
        --------
        BIYOS_getenvboolopt, BIYOS_getenvintopt,
        BIYOS_getenvdblopt, BIYOS_getenvstropt,
        BIYOS_getenvflgopt
        """
        option, kind = self._lookup(name)
        found = _values.slots(kind)
        self._call(_GETTERS[kind], option, *map(byref, found))
        return _values.value(name, kind, found)

    def getoptver(self, name: str) -> Versions:
        """Read the versions that added, deprecated and retired an option.

        Parameters
        ----------
        name
            The name of the option.

        Returns
        -------
        ver
            The three versions, each a string such as ``"0.1.0"``, or
            ``None`` for never.

        Raises
        ------
        Error
            When the name is unknown.

        See Also
        --------
        BIYOS_getoptver
        """
        option = self._option(name)
        numbers = (c_int(), c_int(), c_int())
        pointers = (byref(number) for number in numbers)
        self._call(lib.BIYOS_getoptver, option, *pointers)
        return Versions(*(version_text(number.value) for number in numbers))

    def addhook(
        self,
        hk: Callable[[Event], object],
        knd: EventKind,
        lvl: EventLevel,
    ) -> int:
        """Add a hook on one kind of event at a lowest level.

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
            The id of the hook, for ``delhook``.

        Raises
        ------
        TypeError
            When ``hk`` is not callable.
        Error
            When the kind or the level is unknown.

        See Also
        --------
        BIYOS_addenvhook
        """
        hk = _callable(hk)
        kind = integer("knd", knd)
        level = integer("lvl", lvl)
        idx = c_int()
        with self._busy():
            self._addhook(hk, kind, level, idx)
        return idx.value

    def delhook(self, idx: int) -> None:
        """Remove a hook.

        Parameters
        ----------
        idx
            The id ``addhook`` gave.

        Raises
        ------
        Error
            When the environment has no hook of that id.

        See Also
        --------
        BIYOS_delenvhook
        """
        idx = integer("idx", idx)
        with self._busy():
            self._delhook(idx)

    def interrupt(self) -> None:
        """Ask the call running on the environment to stop.

        This is the one method another thread may call while a call runs on
        the environment or on one of its models. No thread may close the
        environment while it runs. A long call stops where it checks its
        time limits. The request clears when the next call starts, so one
        made between two calls stops neither. A ``KeyboardInterrupt`` raised
        in a hook makes the same request, and is raised once the call
        returns.

        Raises
        ------
        ValueError
            When the environment is closed.

        See Also
        --------
        BIYOS_interrupt
        """
        if not self._open():
            raise ValueError("the environment is closed")
        status = lib.BIYOS_interrupt(self._handle)
        check(self._handle, status, self._owners)

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
            self._call(lib.BIYOS_addenvhook, callback, key, knd, lvl, out)
        finally:
            self._hooks.settle(key, idx.value)

    def _delhook(self, idx: int) -> None:
        self._call(lib.BIYOS_delenvhook, idx)
        self._hooks.remove(idx)

    def _lookup(self, name: str) -> tuple[int, int]:
        option = self._option(name)
        kind = c_int()
        self._call(lib.BIYOS_getoptknd, option, byref(kind))
        return option, kind.value

    def _option(self, name: str) -> int:
        option = c_int()
        self._call(lib.BIYOS_getoptid, _name(name), byref(option))
        return option.value

    def _call(self, function: Callable[..., int], *arguments: object) -> None:
        with self._busy():
            if not self._open():
                raise ValueError("the environment is closed")
            self._run(function, self._handle, *arguments)

    @contextmanager
    def _busy(self) -> Iterator[None]:
        """Keep the environment allocated while the block runs.

        A close during the block reads as closed at once and frees when the
        last hold ends. Inside the block, before calling C, check ``_open()``
        and register the slot that will receive any handle the call creates;
        hold every other environment the call uses.
        """
        self._calls += 1
        try:
            yield
        finally:
            self._calls -= 1
            self._settle()

    def _run(self, function: Callable[..., int], *arguments: object) -> None:
        outer = self._relay.start()
        try:
            status = function(*arguments)
            self._settle()
        except BaseException:
            self._relay.restore(outer)
            raise
        self._relay.finish(status, outer)
        check(self._handle, status, self._owners)

    def _open(self) -> bool:
        return self._free.alive and not self._closing

    def _arm(self) -> None:
        arguments = (self._models, self._handle)
        self._free = weakref.finalize(self, _release, *arguments)

    def _settle(self) -> None:
        if self._relay.idle():
            self._drain()
        if self._calls == 0 and self._pending:
            self._dispose()

    def _drain(self) -> None:
        self._calls += 1
        try:
            self._models.drain()
        finally:
            self._calls -= 1

    def _dispose(self) -> int:
        self._pending = False
        if self._free.detach() is None:
            return _capi.OK
        status = _release(self._models, self._handle)
        if status == _capi.OK:
            self._hooks.clear()
        else:
            self._arm()
        return status
