from enum import IntEnum


class VariableKind(IntEnum):
    CONTINUOUS = 1
    BINARY = 2
    INTEGER = 3
    SEMICONTINUOUS = 4
    SEMIINTEGER = 5
