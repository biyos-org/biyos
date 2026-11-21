from functools import partial

from biyos._capi import HOOK
from biyos._event import deliver


def _deliver(relay, functions, pointer, key):
    relay.run(deliver, functions[key], pointer)


class Hooks:
    def __init__(self, relay):
        self._functions = {}
        self._keys = {}
        self._count = 0
        function = partial(_deliver, relay, self._functions)
        self.callback = HOOK(function)

    def add(self, function):
        self._count += 1
        self._functions[self._count] = function
        return self._count

    def settle(self, key, idx):
        if idx == 0:
            del self._functions[key]
        else:
            self._keys[idx] = key

    def remove(self, idx):
        del self._functions[self._keys.pop(idx)]

    def clear(self):
        self._functions.clear()
        self._keys.clear()
