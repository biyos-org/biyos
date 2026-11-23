import weakref
from collections.abc import Callable, Mapping
from functools import partial
from typing import TYPE_CHECKING

from biyos._capi import HOOK
from biyos._event import Event, deliver
from biyos._relay import Relay

if TYPE_CHECKING:
    from biyos._model import Model


def _deliver(
    relay: Relay,
    owners: Mapping[int, "Model"],
    hooks: "weakref.ref[Hooks]",
    pointer: int,
    key: int,
) -> None:
    found = hooks()
    if found is not None:
        relay.run(deliver, found._functions[key], pointer, owners)


def checked_hook(
    function: Callable[[Event], object],
) -> Callable[[Event], object]:
    if not callable(function):
        found = type(function).__name__
        raise TypeError(f"hook must be callable, not {found}")
    return function


class Hooks:
    def __init__(
        self, relay: Relay, owners: Mapping[int, "Model"]
    ) -> None:
        self._functions: dict[int, Callable[[Event], object]] = {}
        self._keys: dict[int, int] = {}
        self._count = 0
        function = partial(_deliver, relay, owners, weakref.ref(self))
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
