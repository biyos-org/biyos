from collections.abc import Callable
from ctypes import c_void_p

from biyos._capi import lib
from biyos._error import Error, check


class Relay:
    def __init__(self, handle: c_void_p) -> None:
        self._handle = handle
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

    def start(self) -> None:
        if self._depth == 0:
            self._raised = None

    def finish(self, status: int) -> None:
        raised = self._raised
        if self._depth > 0 or raised is None:
            return
        self._raised = None
        try:
            check(self._handle, status)
        except Error:
            raise raised
        else:
            raise raised
        finally:
            del raised

    def _keep(self, error: BaseException) -> None:
        if self._raised is None:
            self._raised = error
