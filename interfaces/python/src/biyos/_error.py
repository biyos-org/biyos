from ctypes import byref, c_void_p, sizeof

from biyos._capi import OK, ErrorRecord, lib


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


def last_error(handle: c_void_p) -> ErrorRecord:
    record = ErrorRecord(size=sizeof(ErrorRecord))
    lib.BIYOS_lasterror(handle, byref(record))
    return record


def check(handle: c_void_p, status: int) -> None:
    if status != OK:
        text = last_error(handle).msg or b""
        raise Error(text.decode(errors="replace"), status)
