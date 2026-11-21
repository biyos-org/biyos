from collections.abc import Callable
from functools import partial

from biyos._capi import HOOK
from biyos._event import Event, deliver
from biyos._relay import Relay


def _deliver(
    relay: Relay,
    functions: dict[int, Callable[[Event], object]],
    pointer: int,
    key: int,
) -> None:
    relay.run(deliver, functions[key], pointer)


class Hooks:
    def __init__(self, relay: Relay) -> None:
        self._functions: dict[int, Callable[[Event], object]] = {}
        self._keys: dict[int, int] = {}
        self._count = 0
        function = partial(_deliver, relay, self._functions)
        self.callback = HOOK(function)

    def add(self, function: Callable[[Event], object]) -> int:
        self._count += 1
        self._functions[self._count] = function
        return self._count

    def settle(self, key: int, idx: int) -> None:
        if idx == 0:
            del self._functions[key]
        else:
            self._keys[idx] = key

    def remove(self, idx: int) -> None:
        del self._functions[self._keys.pop(idx)]

    def clear(self) -> None:
        self._functions.clear()
        self._keys.clear()
