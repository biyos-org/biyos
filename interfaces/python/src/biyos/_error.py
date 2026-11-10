from biyos._capi import OK, lib


class Error(Exception):
    def __init__(self, message, code):
        super().__init__(message, code)
        self.code = code

    def __str__(self):
        return self.args[0]


def check(handle, status):
    if status != OK:
        message = lib.biyos_last_error_message(handle).decode()
        raise Error(message, status)
