from enum import IntEnum

from biyos._capi import lib


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
        return EventKind(lib.biyos_event_kind(self._live()))

    @property
    def level(self):
        return Level(lib.biyos_event_level(self._live()))

    @property
    def module(self):
        return _decoded(lib.biyos_event_module(self._live()))

    @property
    def text(self):
        return _decoded(lib.biyos_event_text(self._live()))

    @property
    def fields(self):
        pointer = self._live()
        count = lib.biyos_event_field_count(pointer)
        return tuple(_field(pointer, index) for index in range(count))

    def _live(self):
        if self._pointer is None:
            raise ValueError("the event is no longer valid")
        return self._pointer


def _field(pointer, index):
    name = lib.biyos_event_field_name(pointer, index)
    value = lib.biyos_event_field_value(pointer, index)
    return _decoded(name), _decoded(value)


def deliver(function, pointer):
    event = Event(pointer)
    try:
        function(event)
    finally:
        event._pointer = None
