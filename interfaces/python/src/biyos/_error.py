from biyos._capi import OK, lib


class Error(Exception):
    """A call of the C library failed.

    ``str`` of the error gives the message the environment kept.

    See Also
    --------
    BIYOS_lasterror
    """

    def __init__(self, msg, code):
        super().__init__(msg, code)
        self.code = code
        """The status the call returned, a negative ``BIYOS_ERR_`` value."""

    def __str__(self):
        return self.args[0]


def check(handle, status):
    if status != OK:
        message = lib.BIYOS_lasterror(handle).decode(errors="replace")
        raise Error(message, status)
