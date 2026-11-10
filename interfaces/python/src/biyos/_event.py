from enum import IntEnum


class EventKind(IntEnum):
    MESSAGE = 1
    CHECK = 2
    PHASE = 3


class Level(IntEnum):
    DEBUG = 1
    INFO = 2
    WARNING = 3
    ERROR = 4
