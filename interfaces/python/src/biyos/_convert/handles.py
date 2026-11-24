from ctypes import c_void_p

from biyos._capi import OK, lib


class Handles:
    def __init__(self) -> None:
        self.closed = False
        self._live: dict[int, c_void_p] = {}
        self._parked: list[c_void_p] = []
        self._kept: dict[int, object] = {}

    def add(self, handle: c_void_p) -> None:
        self._live[id(handle)] = handle

    def take(self, handle: c_void_p) -> bool:
        try:
            del self._live[id(handle)]
        except KeyError:
            return False
        return True

    def park(self, handle: c_void_p, kept: object) -> None:
        if not self.closed and id(handle) in self._live:
            self._parked.append(handle)
            self._kept[id(handle)] = kept

    def drain(self) -> None:
        while not self.closed:
            try:
                handle = self._parked.pop()
            except IndexError:
                return
            if self.take(handle) and lib.BIYOS_freemdl(handle) != OK:
                self.add(handle)
            else:
                self._kept.pop(id(handle), None)

    def release(self) -> None:
        self.closed = True
        self._parked.clear()
        while self._live:
            lib.BIYOS_freemdl(self._live.popitem()[1])
        self._kept.clear()
