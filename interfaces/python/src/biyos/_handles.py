from biyos._capi import OK, lib


class Handles:
    def __init__(self):
        self.closed = False
        self._live = set()
        self._parked = []

    def add(self, handle):
        self._live.add(handle)

    def take(self, handle):
        try:
            self._live.remove(handle)
        except KeyError:
            return False
        return True

    def park(self, handle):
        if not self.closed and handle in self._live:
            self._parked.append(handle)

    def drain(self):
        while self._parked and not self.closed:
            handle = self._parked.pop()
            if self.take(handle) and lib.biyos_model_free(handle) != OK:
                self._live.add(handle)

    def release(self):
        self.closed = True
        self._parked.clear()
        while self._live:
            lib.biyos_model_free(self._live.pop())
