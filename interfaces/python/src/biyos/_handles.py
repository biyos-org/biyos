from ctypes import c_void_p

from biyos._capi import OK, lib


class Handles:
    def __init__(self) -> None:
        self.closed = False
        self._live: dict[int, c_void_p] = {}
        self._parked: list[c_void_p] = []

    def add(self, handle: c_void_p) -> None:
        self._live[id(handle)] = handle

    def take(self, handle: c_void_p) -> bool:
        try:
            del self._live[id(handle)]
        except KeyError:
            return False
        return True

    def park(self, handle: c_void_p) -> None:
        if not self.closed and id(handle) in self._live:
            self._parked.append(handle)

    def drain(self) -> None:
        while not self.closed:
            try:
                handle = self._parked.pop()
            except IndexError:
                return
            if self.take(handle) and lib.BIYOS_freemdl(handle) != OK:
                self.add(handle)

    def release(self) -> None:
        self.closed = True
        self._parked.clear()
        while self._live:
            lib.BIYOS_freemdl(self._live.popitem()[1])
