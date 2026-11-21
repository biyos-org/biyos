from ctypes import c_void_p

from biyos._capi import OK, lib


class Error(Exception):
    """A call of the C library failed.

    ``str`` of the error gives the message the environment kept.

    See Also
    --------
    BIYOS_lasterror
    """

    def __init__(self, msg: str, code: int) -> None:
        super().__init__(msg, code)
        self.code: int = code
        """The status the call returned, a negative ``BIYOS_ERR_`` value."""

    def __str__(self) -> str:
        msg: str = self.args[0]
        return msg


def check(handle: c_void_p, status: int) -> None:
    if status != OK:
        message = lib.BIYOS_lasterror(handle).decode(errors="replace")
        raise Error(message, status)
