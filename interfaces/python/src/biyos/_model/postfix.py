from collections.abc import Sequence
from math import isfinite
from typing import TYPE_CHECKING, NamedTuple, TypeVar

from biyos._convert.numbers import Offset, Real
from biyos._model.blocks import (
    Functions,
    PiecewiseLinears,
    Variable,
    Variables,
    index_of,
)
from biyos._model.expression import (
    Comparison,
    Condition,
    Expression,
    Logical,
    Operand,
    Operation,
    Piecewise,
)
from biyos._model.operators import Operator

if TYPE_CHECKING:
    from biyos._model.model import Model

Postfix = tuple[list[Offset], list[Operator], list[Offset], list[Real]]
Given = TypeVar("Given")


class _Close(NamedTuple):
    op: Operator
    arg: Offset
    values: tuple[Real, ...]
    key: int | None


def _inner(node: Operand) -> bool:
    return isinstance(node, Expression) and bool(node._args)


def _shared(trees: Sequence[Operand]) -> set[int]:
    seen: set[int] = set()
    shared: set[int] = set()
    stack = list(trees)
    while stack:
        node = stack.pop()
        if not isinstance(node, Expression) or not _inner(node):
            continue
        key = id(node)
        if key in seen:
            shared.add(key)
            continue
        seen.add(key)
        stack.extend(node._args)
    return shared


def _merges(
    node: Operand, weight: Real, constant: Real, shared: set[int]
) -> bool:
    if not isinstance(node, Expression) or node._op != Operator.SUM:
        return False
    if id(node) in shared:
        return False
    if not isfinite(constant + weight * node._values[0]):
        return False
    return all(isfinite(weight * value) for value in node._values)


def _flat(
    node: Expression, shared: set[int]
) -> tuple[list[Operand], tuple[Real, ...]]:
    constant = node._values[0]
    children: list[Operand] = []
    weights: list[Real] = []
    stack = list(zip(reversed(node._args), reversed(node._values[1:])))
    while stack:
        child, weight = stack.pop()
        if not _merges(child, weight, constant, shared):
            children.append(child)
            weights.append(weight)
            continue
        assert isinstance(child, Expression)
        constant += weight * child._values[0]
        scaled = [weight * value for value in child._values[1:]]
        stack.extend(zip(reversed(child._args), reversed(scaled)))
    return children, (constant, *weights)


class _Writer:
    def __init__(self, model: "Model", trees: Sequence[Operand]) -> None:
        self._model = model
        self._shared = _shared(trees)
        self._kept: dict[int, Offset] = {}
        self.cnt: list[Offset] = []
        self.code: list[Operator] = []
        self.arg: list[Offset] = []
        self.val: list[Real] = []
        for tree in trees:
            before = len(self.code)
            self._write(tree)
            self.cnt.append(len(self.code) - before)

    def _write(self, tree: Operand) -> None:
        stack: list[Operand | _Close] = [tree]
        while stack:
            item = stack.pop()
            if isinstance(item, _Close):
                self._close(item)
            elif isinstance(item, Variable):
                self._variable(item)
            elif id(item) in self._kept:
                self._entry(Operator.REFERENCE, self._kept[id(item)], ())
            else:
                assert isinstance(item, Expression)
                stack.extend(self._open(item))

    def _open(self, node: Expression) -> list[Operand | _Close]:
        children: Sequence[Operand] = node._args
        values = node._values
        if node._op == Operator.SUM:
            children, values = _flat(node, self._shared)
        key = id(node) if id(node) in self._shared else None
        close = _Close(node._op, self._arg(node, children), values, key)
        return [close, *reversed(children)]

    def _arg(self, node: Expression, children: Sequence[Operand]) -> Offset:
        if isinstance(node, Piecewise):
            pwl = node._pwl
            return index_of(self._model, PiecewiseLinears, "expr", pwl)
        return len(children)

    def _close(self, close: _Close) -> None:
        self._entry(close.op, close.arg, close.values)
        if close.key is not None:
            self._kept[close.key] = len(self.code)
            self._entry(Operator.KEEP, 1, ())

    def _variable(self, view: Variable) -> None:
        idx = index_of(self._model, Variables, "expr", view)
        self._entry(Operator.VARIABLE, idx, ())

    def _entry(
        self, op: Operator, arg: Offset, values: tuple[Real, ...]
    ) -> None:
        self.code.append(op)
        self.arg.append(arg)
        self.val.extend(values)


def postfix(model: "Model", trees: Sequence[Operand]) -> Postfix:
    for tree in trees:
        if not isinstance(tree, Operand):
            found = type(tree).__name__
            raise TypeError(f"expr must be an expression, not {found}")
    writer = _Writer(model, trees)
    return writer.cnt, writer.code, writer.arg, writer.val


def attached(model: "Model", trees: Sequence[Operand]) -> Functions:
    nodes = model.addexpr(*postfix(model, trees))
    funs = model.addfun(len(trees))
    model.attachexpr(funs, nodes)
    return funs


def expression_of(top: Condition) -> Expression:
    made: dict[int, Expression] = {}
    stack: list[tuple[Logical, bool]] = [(top, False)]
    while stack:
        item, ready = stack.pop()
        if id(item) in made:
            continue
        if isinstance(item, Comparison):
            made[id(item)] = item._node()
            continue
        assert isinstance(item, Condition)
        if ready:
            args = tuple(made[id(arg)] for arg in item._args)
            made[id(item)] = Operation(item._op, args, ())
        else:
            stack.append((item, True))
            stack.extend((arg, False) for arg in item._args)
    return made[id(top)]


def given(value: object, kind: type[Given], noun: str) -> list[Given] | None:
    if isinstance(value, kind):
        return [value]
    if not isinstance(value, (list, tuple)) or not value:
        return None
    if not isinstance(value[0], kind):
        return None
    for k, item in enumerate(value):
        if not isinstance(item, kind):
            found = type(item).__name__
            raise TypeError(f"fun[{k}] is {found}, not {noun}")
    return list(value)
