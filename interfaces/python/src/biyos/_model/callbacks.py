from collections.abc import Callable
from ctypes import POINTER, byref, cast, py_object
from typing import Any

from biyos._capi import CALLBACK, OK, RequestRecord, lib
from biyos._convert.integers import whole
from biyos._convert.numbers import Index, Offset, Real, c_index, c_real
from biyos._error import Error


def _bytes(address: int | None, n: Offset) -> memoryview:
    if address is None or n == 0:
        return memoryview(bytearray())
    array = (c_real * n).from_address(address)
    return memoryview(array).cast("B")


def _output(address: int | None, n: Offset) -> "memoryview[Real]":
    return _bytes(address, n).cast("d")


def _input(address: int | None, n: Index) -> "memoryview[Real]":
    return _bytes(address, n).toreadonly().cast("d")


def _optional(address: int | None, n: Index) -> "memoryview[Real] | None":
    return None if address is None else _input(address, n)


def _nvar(model: int | None) -> Index:
    n = c_index()
    status = lib.BIYOS_getnvar(model, byref(n))
    if status != OK:
        raise Error("cannot read the variables of the model", status)
    return n.value


def data_at(address: int | None) -> object:
    if address is None:
        return None
    return cast(address, POINTER(py_object))[0]


def _status(result: Any) -> int:
    if result is None:
        return 0
    number = whole("the result of a callback", result)
    return (number > 0) - (number < 0)


class CallbackRequest:
    """A request to a callback, valid only during the call.

    Its arrays, and the one the callback fills, are views of memory biyos
    owns. They are released when the callback returns, and reading one
    afterwards raises ``ValueError``. Nothing derived from them, such as a
    NumPy array or another ``memoryview``, may outlive the call: it would
    still point at that memory.

    See Also
    --------
    BIYOS_cbreq
    """

    def __init__(
        self, record: RequestRecord, count: Index, nvar: Index
    ) -> None:
        self._callback: Index = record.cb
        self._thread: Index = record.thread
        self._x = _input(record.x, nvar)
        self._w = _optional(record.w, count)
        self._v = _optional(record.v, nvar)

    @property
    def callback(self) -> Index:
        """The index of the callback."""
        return self._callback

    @property
    def thread(self) -> Index:
        """The thread of the call, counted from 0 among the threads of a
        solve."""
        return self._thread

    @property
    def x(self) -> "memoryview[Real]":
        """The value of every variable, read-only."""
        return self._x

    @property
    def w(self) -> "memoryview[Real] | None":
        """A weight for each covered function, read-only, in a call of the
        Hessian or of the Hessian-vector product, and ``None`` otherwise."""
        return self._w

    @property
    def v(self) -> "memoryview[Real] | None":
        """A value for every variable, read-only, the vector the Hessian
        multiplies in a call of the Hessian-vector product, and ``None``
        otherwise."""
        return self._v

    def _release(self) -> None:
        for view in (self._x, self._w, self._v):
            if view is not None:
                view.release()


class Caller:
    def __init__(
        self,
        function: Callable[..., Any],
        count: Index,
        size: Offset | None,
        raised: dict[int | None, BaseException],
    ) -> None:
        self._function = function
        self._count = count
        self._size = size
        self._raised = raised
        self.pointer = CALLBACK(self)

    def __call__(
        self,
        record: Any,
        out: int | None,
        data: int | None,
    ) -> int:
        try:
            return self._answer(record.contents, out, data)
        except BaseException as error:
            self._raised[record.contents.mdl] = error
            return -1

    def _answer(
        self, record: RequestRecord, out: int | None, data: int | None
    ) -> int:
        nvar = _nvar(record.mdl)
        request = CallbackRequest(record, self._count, nvar)
        values = _output(out, nvar if self._size is None else self._size)
        try:
            result = self._function(request, values, data_at(data))
        finally:
            request._release()
            values.release()
        return _status(result)


def checked(name: str, function: object) -> Callable[..., Any]:
    if not callable(function):
        found = type(function).__name__
        raise TypeError(f"{name} must be callable, not {found}")
    return function


class Callbacks:
    def __init__(self) -> None:
        self._counts: list[Index] = []
        self._kept: list[object] = []
        self._raised: dict[int | None, BaseException] = {}

    def copy(self) -> "Callbacks":
        copied = Callbacks()
        copied._counts = list(self._counts)
        copied._kept = list(self._kept)
        copied._raised = self._raised
        return copied

    def known(self, cb: Index) -> bool:
        return 0 <= cb < len(self._counts)

    def count(self, cb: Index) -> Index:
        return self._counts[cb] if self.known(cb) else 0

    def total(self) -> Index:
        return len(self._counts)

    def added(self, n: Index) -> None:
        self._counts.append(n)

    def deleted(self, renumbered: list[Index]) -> None:
        pairs = zip(self._counts, renumbered)
        self._counts = [count for count, new in pairs if new != -1]

    def keep(self, held: object) -> None:
        self._kept.append(held)

    def caller(
        self, function: Callable[..., Any], count: Index, size: Offset | None
    ) -> Caller:
        caller = Caller(function, count, size, self._raised)
        self._kept.append(caller)
        return caller

    def taken(self, mdl: int | None) -> BaseException | None:
        return self._raised.pop(mdl, None)
