from ctypes import byref, c_size_t, create_string_buffer
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


def _decoded(raw):
    return raw.decode(errors="replace")


class Event:
    """An event, as a hook receives it.

    An event is valid only during the call of the hook; reading it later
    raises ``ValueError``.
    """

    def __init__(self, pointer):
        self._pointer = pointer

    @property
    def kind(self):
        """The kind of the event, an ``EventKind``.

        See Also
        --------
        BIYOS_getevtknd
        """
        return EventKind(lib.BIYOS_getevtknd(self._live()))

    @property
    def level(self):
        """The level of the event, an ``EventLevel``.

        See Also
        --------
        BIYOS_getevtlvl
        """
        return EventLevel(lib.BIYOS_getevtlvl(self._live()))

    @property
    def module(self):
        """The name of the module that sent the event, such as ``"capi"``.

        See Also
        --------
        BIYOS_getevtmod
        """
        return _decoded(lib.BIYOS_getevtmod(self._live()))

    @property
    def text(self):
        """The text of the event.

        See Also
        --------
        BIYOS_getevttext
        """
        return _decoded(lib.BIYOS_getevttext(self._live()))

    @property
    def fields(self):
        """The fields of the event, as pairs of name and value.

        See Also
        --------
        BIYOS_getnevtflds, BIYOS_getevtfldname, BIYOS_getevtfldval
        """
        pointer = self._live()
        count = lib.BIYOS_getnevtflds(pointer)
        return tuple(_field(pointer, index) for index in range(count))

    def fmttext(self):
        """Return the event as one line of text.

        Returns
        -------
        str
            The line, with no line break.

        See Also
        --------
        BIYOS_fmttext
        """
        return _formatted(lib.BIYOS_fmttext, self._live())

    def fmtjson(self):
        """Return the event as one JSON object.

        Returns
        -------
        str
            The object, on one line.

        See Also
        --------
        BIYOS_fmtjson
        """
        return _formatted(lib.BIYOS_fmtjson, self._live())

    def _live(self):
        if self._pointer is None:
            raise ValueError("the event is no longer valid")
        return self._pointer


def _field(pointer, index):
    name = lib.BIYOS_getevtfldname(pointer, index)
    value = lib.BIYOS_getevtfldval(pointer, index)
    return _decoded(name), _decoded(value)


def _formatted(function, pointer):
    needed = c_size_t()
    _format(function, pointer, None, 0, needed)
    buffer = create_string_buffer(needed.value + 1)
    _format(function, pointer, buffer, len(buffer), needed)
    return _decoded(buffer.value)


def _format(function, pointer, buffer, size, needed):
    status = function(pointer, size, buffer, byref(needed))
    if status != OK:
        raise Error("cannot format the event", status)


def deliver(function, pointer):
    event = Event(pointer)
    try:
        function(event)
    finally:
        event._pointer = None
