from collections.abc import Mapping
from ctypes import byref, c_void_p, sizeof
from typing import TYPE_CHECKING

from biyos._capi import OK, ErrorRecord, lib

if TYPE_CHECKING:
    from biyos._model import Model


class Error(Exception):
    """A call of the C library failed.

    ``str`` of the error gives the message the environment kept.

    See Also
    --------
    BIYOS_lasterror
    """

    def __init__(
        self, msg: str, code: int, model: "Model | None" = None
    ) -> None:
        super().__init__(msg, code)
        self.code: int = code
        """The status the call returned, a negative ``BIYOS_ERR_`` value."""
        self.model: Model | None = model
        """The model of the call that failed, or ``None`` for a call on the
        environment or for an error that was pickled."""

    def __reduce__(
        self,
    ) -> tuple[type["Error"], tuple[str, int], dict[str, object]]:
        state = {k: v for k, v in vars(self).items() if k != "model"}
        return type(self), (self.args[0], self.code), state

    def __str__(self) -> str:
        msg: str = self.args[0]
        return msg


def last_error(handle: c_void_p) -> ErrorRecord:
    record = ErrorRecord(size=sizeof(ErrorRecord))
    lib.BIYOS_lasterror(handle, byref(record))
    return record


def check(
    handle: c_void_p, status: int, owners: Mapping[int, "Model"]
) -> None:
    if status != OK:
        record = last_error(handle)
        text = (record.msg or b"").decode(errors="replace")
        model = None if record.mdl is None else owners.get(record.mdl)
        raise Error(text, status, model)
