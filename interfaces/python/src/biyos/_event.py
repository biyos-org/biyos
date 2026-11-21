from ctypes import byref, c_size_t, create_string_buffer
from enum import IntEnum

from biyos._capi import OK, lib
from biyos._error import Error


class EventKind(IntEnum):
    MESSAGE = 1
    CHECK = 2
    PHASE = 3


class Level(IntEnum):
    DEBUG = 1
    INFO = 2
    WARNING = 3
    ERROR = 4


def _decoded(raw):
    return raw.decode(errors="replace")


class Event:
    def __init__(self, pointer):
        self._pointer = pointer

    @property
    def kind(self):
        return EventKind(lib.BIYOS_getevtkind(self._live()))

    @property
    def lvl(self):
        return Level(lib.BIYOS_getevtlvl(self._live()))

    @property
    def module(self):
        return _decoded(lib.BIYOS_getevtmodule(self._live()))

    @property
    def text(self):
        return _decoded(lib.BIYOS_getevttext(self._live()))

    @property
    def fields(self):
        pointer = self._live()
        count = lib.BIYOS_getnevtfields(pointer)
        return tuple(_field(pointer, index) for index in range(count))

    def fmttext(self):
        return _formatted(lib.BIYOS_fmttext, self._live())

    def fmtjson(self):
        return _formatted(lib.BIYOS_fmtjson, self._live())

    def _live(self):
        if self._pointer is None:
            raise ValueError("the event is no longer valid")
        return self._pointer


def _field(pointer, index):
    name = lib.BIYOS_getevtfieldname(pointer, index)
    value = lib.BIYOS_getevtfieldvalue(pointer, index)
    return _decoded(name), _decoded(value)


def _formatted(function, pointer):
    needed = c_size_t()
    _format(function, pointer, None, 0, needed)
    buffer = create_string_buffer(needed.value + 1)
    _format(function, pointer, buffer, len(buffer), needed)
    return _decoded(buffer.value)


def _format(function, pointer, buffer, size, needed):
    status = function(pointer, buffer, size, byref(needed))
    if status != OK:
        raise Error("cannot format the event", status)


def deliver(function, pointer):
    event = Event(pointer)
    try:
        function(event)
    finally:
        event._pointer = None
