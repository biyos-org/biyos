from biyos._capi import OK, lib


class Error(Exception):
    def __init__(self, msg, code):
        super().__init__(msg, code)
        self.code = code

    def __str__(self):
        return self.args[0]


def check(handle, status):
    if status != OK:
        message = lib.BIYOS_lasterror(handle).decode(errors="replace")
        raise Error(message, status)
