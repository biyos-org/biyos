from collections.abc import Callable
from ctypes import Array, byref, c_char, c_size_t, create_string_buffer
from enum import IntEnum

from biyos._capi import OK, lib
from biyos._error import Error


class EventKind(IntEnum):
    """The kinds of events, as ``BIYOS_EVT_`` numbers them.

    Attributes
    ----------
    MESSAGE
        A message for people.
    CHECK
        A failed check of an invariant.
    PHASE
        A phase beginning or ending, with the wall and CPU time.
    """

    MESSAGE = 1
    CHECK = 2
    PHASE = 3


class EventLevel(IntEnum):
    """The levels of events, from least to most severe, as ``BIYOS_LVL_``
    numbers them.

    Attributes
    ----------
    DEBUG
        Detail for whoever debugs biyos.
    INFO
        Progress and timings.
    WARNING
        Something the caller should look at.
    ERROR
        A failed call or a failed check.
    """

    DEBUG = 1
    INFO = 2
    WARNING = 3
    ERROR = 4


def _decoded(raw: bytes) -> str:
    return raw.decode(errors="replace")


class Event:
    """An event, as a hook receives it.

    An event is valid only during the call of the hook; reading it later
    raises ``ValueError``.
    """

    def __init__(self, pointer: int) -> None:
        self._pointer: int | None = pointer

    @property
    def kind(self) -> EventKind:
        """The kind of the event.

        See Also
        --------
        BIYOS_getevtknd
        """
        return EventKind(lib.BIYOS_getevtknd(self._live()))

    @property
    def level(self) -> EventLevel:
        """The level of the event.

        See Also
        --------
        BIYOS_getevtlvl
        """
        return EventLevel(lib.BIYOS_getevtlvl(self._live()))

    @property
    def module(self) -> str:
        """The name of the module that sent the event, such as ``"capi"``.

        See Also
        --------
        BIYOS_getevtmod
        """
        return _decoded(lib.BIYOS_getevtmod(self._live()))

    @property
    def text(self) -> str:
        """The text of the event.

        See Also
        --------
        BIYOS_getevttext
        """
        return _decoded(lib.BIYOS_getevttext(self._live()))

    @property
    def fields(self) -> tuple[tuple[str, str], ...]:
        """The fields of the event, as pairs of name and value.

        See Also
        --------
        BIYOS_getnevtflds, BIYOS_getevtfldname, BIYOS_getevtfldval
        """
        pointer = self._live()
        count = lib.BIYOS_getnevtflds(pointer)
        return tuple(_field(pointer, index) for index in range(count))

    def fmttext(self) -> str:
        """Return the event as one line of text.

        Returns
        -------
        buf
            The line, with no line break.

        See Also
        --------
        BIYOS_fmttext
        """
        return _formatted(lib.BIYOS_fmttext, self._live())

    def fmtjson(self) -> str:
        """Return the event as one JSON object.

        Returns
        -------
        buf
            The object, on one line.

        See Also
        --------
        BIYOS_fmtjson
        """
        return _formatted(lib.BIYOS_fmtjson, self._live())

    def _live(self) -> int:
        if self._pointer is None:
            raise ValueError("the event is no longer valid")
        return self._pointer


def _field(pointer: int, index: int) -> tuple[str, str]:
    name = lib.BIYOS_getevtfldname(pointer, index)
    value = lib.BIYOS_getevtfldval(pointer, index)
    return _decoded(name), _decoded(value)


def _formatted(function: Callable[..., int], pointer: int) -> str:
    needed = c_size_t()
    _format(function, pointer, None, 0, needed)
    buffer = create_string_buffer(needed.value + 1)
    _format(function, pointer, buffer, len(buffer), needed)
    return _decoded(buffer.value)


def _format(
    function: Callable[..., int],
    pointer: int,
    buffer: Array[c_char] | None,
    size: int,
    needed: c_size_t,
) -> None:
    status = function(pointer, size, buffer, byref(needed))
    if status != OK:
        raise Error("cannot format the event", status)


def deliver(function: Callable[[Event], object], pointer: int) -> None:
    event = Event(pointer)
    try:
        function(event)
    finally:
        event._pointer = None
