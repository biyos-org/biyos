from functools import partial

from biyos._capi import SUBSCRIBER
from biyos._event import deliver


def _deliver(functions, pointer, key):
    deliver(functions[key], pointer)


class Subscribers:
    def __init__(self):
        self._functions = {}
        self._keys = {}
        self._count = 0
        self.callback = SUBSCRIBER(partial(_deliver, self._functions))

    def add(self, function):
        self._count += 1
        self._functions[self._count] = function
        return self._count

    def settle(self, key, subscription):
        if subscription == 0:
            del self._functions[key]
        else:
            self._keys[subscription] = key

    def remove(self, subscription):
        del self._functions[self._keys.pop(subscription)]

    def clear(self):
        self._functions.clear()
        self._keys.clear()
