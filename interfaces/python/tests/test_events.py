import biyos
from header import macros


def test_constants():
    kinds = {f"BIYOS_EVENT_{kind.name}": kind for kind in biyos.EventKind}
    levels = {f"BIYOS_LEVEL_{level.name}": level for level in biyos.Level}
    assert macros("events.h") == kinds | levels
