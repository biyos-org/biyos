from biyos._capi import OK, lib


class Handles:
    def __init__(self) -> None:
        self.closed = False
        self._live: set[int | None] = set()
        self._parked: list[int | None] = []

    def add(self, handle: int | None) -> None:
        self._live.add(handle)

    def take(self, handle: int | None) -> bool:
        try:
            self._live.remove(handle)
        except KeyError:
            return False
        return True

    def park(self, handle: int | None) -> None:
        if not self.closed and handle in self._live:
            self._parked.append(handle)

    def drain(self) -> None:
        while not self.closed:
            try:
                handle = self._parked.pop()
            except IndexError:
                return
            if self.take(handle) and lib.BIYOS_freemdl(handle) != OK:
                self._live.add(handle)

    def release(self) -> None:
        self.closed = True
        self._parked.clear()
        while self._live:
            lib.BIYOS_freemdl(self._live.pop())
