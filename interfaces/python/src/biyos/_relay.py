from collections.abc import Callable, Mapping
from ctypes import c_void_p
from typing import TYPE_CHECKING

from biyos._capi import lib
from biyos._error import Error, check

if TYPE_CHECKING:
    from biyos._model import Model


class Relay:
    def __init__(self, handle: c_void_p, owners: Mapping[int, "Model"]) -> None:
        self._handle = handle
        self._owners = owners
        self._raised: BaseException | None = None
        self._depth = 0

    def run(self, function: Callable[..., object], *arguments: object) -> None:
        self._depth += 1
        try:
            function(*arguments)
        except KeyboardInterrupt as error:
            self._keep(error)
            lib.BIYOS_interrupt(self._handle)
        except BaseException as error:
            self._keep(error)
        finally:
            self._depth -= 1

    def idle(self) -> bool:
        return self._depth == 0

    def start(self) -> BaseException | None:
        outer = self._raised
        self._raised = None
        return outer

    def restore(self, outer: BaseException | None) -> None:
        self._raised = outer

    def finish(self, status: int, outer: BaseException | None) -> None:
        raised = self._raised
        self._raised = outer
        if raised is None:
            return
        try:
            check(self._handle, status, self._owners)
        except Error:
            raise raised
        else:
            raise raised
        finally:
            del raised

    def _keep(self, error: BaseException) -> None:
        if self._raised is None:
            self._raised = error
