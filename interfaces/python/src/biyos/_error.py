from biyos._capi import OK, lib


class Error(Exception):
    def __init__(self, message, code):
        super().__init__(message)
        self.code = code


def check(handle, status):
    if status != OK:
        message = lib.biyos_last_error_message(handle).decode()
        raise Error(message, status)
