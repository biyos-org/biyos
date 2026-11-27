from collections.abc import Callable, Sequence
from typing import NamedTuple, Protocol, TypeGuard

from biyos._convert.numbers import Real
from biyos._model.operators import Operator


class _Node(Protocol):
    _op: Operator
    _args: tuple[object, ...]
    _values: tuple[Real, ...]


Piece = str | tuple[object, int]
Form = Callable[[_Node, "_Row"], list[Piece]]

_SUM, _PRODUCT, _UNARY, _POWER, _ATOM = range(1, 6)
_PLUS = " + "
_MINUS = " - "
_SIGN = "-"
_ONE = "1"


class _Row(NamedTuple):
    form: Form
    text: str = ""
    level: int = _ATOM
    left: int = 0
    right: int = 0


def printed(tree: object) -> str:
    out: list[str] = []
    stack: list[Piece] = [(tree, 0)]
    while stack:
        item = stack.pop()
        if isinstance(item, str):
            out.append(item)
        else:
            stack.extend(reversed(_placed(*item)))
    return "".join(out)


def _placed(item: object, need: int) -> list[Piece]:
    node = _bare(item)
    if not _node(node):
        return [repr(node)]
    row = _ROWS[node._op]
    pieces = row.form(node, row)
    if _level(node) < need:
        return ["(", *pieces, ")"]
    return pieces


def _node(value: object) -> TypeGuard[_Node]:
    return isinstance(getattr(value, "_op", None), Operator)


def _bare(node: object) -> object:
    while _node(node) and _plain(node):
        node = node._args[0]
    return node


def _plain(node: _Node) -> bool:
    if len(node._args) != 1:
        return False
    if node._op == Operator.KEEP:
        return True
    return node._op == Operator.SUM and node._values == (0.0, 1.0)


def _level(node: _Node) -> int:
    if node._op == Operator.CONSTANT:
        return _sign_level(_text(node._values[0]))
    if node._op == Operator.SUM:
        return _sum_level(node)
    return _ROWS[node._op].level


def _sign_level(text: str) -> int:
    return _UNARY if text.startswith(_SIGN) else _ATOM


def _sum_level(node: _Node) -> int:
    constant, *weights = node._values
    if len(weights) + (constant != 0) > 1:
        return _SUM
    if not weights:
        return _sign_level(_text(constant))
    return _PRODUCT


def _text(value: Real) -> str:
    text = repr(value)
    return text[:-2] if text.endswith(".0") else text


def _constant(node: _Node, row: _Row) -> list[Piece]:
    return [_text(node._values[0])]


def _sum(node: _Node, row: _Row) -> list[Piece]:
    constant, *weights = node._values
    if _negated(constant, weights):
        return [_text(constant), _MINUS, (node._args[0], row.right)]
    pieces: list[Piece] = []
    for child, weight in zip(node._args, weights):
        if pieces:
            pieces.extend(_term(child, _text(weight), row))
        else:
            pieces.extend(_lead(child, _text(weight), row))
    if constant != 0 or not pieces:
        pieces.extend(_shift(_text(constant), not pieces))
    return pieces


def _negated(constant: Real, weights: list[Real]) -> bool:
    if len(weights) != 1 or constant == 0:
        return False
    return _text(weights[0]) == _SIGN + _ONE


def _lead(child: object, weight: str, row: _Row) -> list[Piece]:
    if weight == _ONE:
        return [(child, row.left)]
    return _scaled(child, weight)


def _term(child: object, weight: str, row: _Row) -> list[Piece]:
    sign = _sign(weight)
    weight = weight.removeprefix(_SIGN)
    if weight == _ONE:
        return [sign, (child, row.right)]
    return [sign, *_scaled(child, weight)]


def _scaled(child: object, weight: str) -> list[Piece]:
    product = _ROWS[Operator.PRODUCT]
    return [weight, product.text, (child, product.right)]


def _shift(constant: str, first: bool) -> list[str]:
    if first:
        return [constant]
    return [_sign(constant), constant.removeprefix(_SIGN)]


def _sign(text: str) -> str:
    return _MINUS if text.startswith(_SIGN) else _PLUS


def _product(node: _Node, row: _Row) -> list[Piece]:
    coefficient = _text(node._values[0])
    lead: list[Piece] = [] if coefficient == _ONE else [coefficient]
    return _joined(node._args, row, lead)


def _infix(node: _Node, row: _Row) -> list[Piece]:
    return _joined(node._args, row, [])


def _joined(
    args: Sequence[object], row: _Row, pieces: list[Piece]
) -> list[Piece]:
    for arg in args:
        need = row.right if pieces else row.left
        if pieces:
            pieces.append(row.text)
        pieces.append((arg, need))
    return pieces


def _prefix(node: _Node, row: _Row) -> list[Piece]:
    return [row.text, (node._args[0], row.right)]


def _raised(node: _Node, row: _Row) -> list[Piece]:
    power = _text(node._values[0])
    return [(node._args[0], row.left), row.text, power]


def _piecewise(node: _Node, row: _Row) -> list[Piece]:
    idx = getattr(node, "_idx", None)
    lead: list[Piece] = [str(idx)] if isinstance(idx, int) else []
    return _written(node, row, lead)


def _written(
    node: _Node, row: _Row, lead: Sequence[Piece] = ()
) -> list[Piece]:
    operands: list[Piece] = [(arg, 0) for arg in node._args]
    values = [_text(value) for value in node._values]
    pieces: list[Piece] = [row.text, "("]
    for k, item in enumerate([*lead, *operands, *values]):
        if k:
            pieces.append(", ")
        pieces.append(item)
    pieces.append(")")
    return pieces


_ROWS = {
    Operator.CONSTANT: _Row(_constant),
    Operator.VARIABLE: _Row(_written, "var"),
    Operator.SUM: _Row(_sum, left=_SUM, right=_PRODUCT),
    Operator.PRODUCT: _Row(_product, "*", _PRODUCT, _PRODUCT, _UNARY),
    Operator.NEGATION: _Row(_prefix, "-", _UNARY, right=_POWER),
    Operator.DIVISION: _Row(_infix, "/", _PRODUCT, _PRODUCT, _UNARY),
    Operator.CONSTANT_POWER: _Row(_raised, "**", _POWER, _ATOM),
    Operator.POWER: _Row(_infix, "**", _POWER, _ATOM, _UNARY),
    Operator.EXP: _Row(_written, "exp"),
    Operator.LOG: _Row(_written, "log"),
    Operator.SQRT: _Row(_written, "sqrt"),
    Operator.SIN: _Row(_written, "sin"),
    Operator.COS: _Row(_written, "cos"),
    Operator.TAN: _Row(_written, "tan"),
    Operator.ABS: _Row(_written, "abs"),
    Operator.MAX: _Row(_written, "max"),
    Operator.MIN: _Row(_written, "min"),
    Operator.ASIN: _Row(_written, "asin"),
    Operator.ACOS: _Row(_written, "acos"),
    Operator.ATAN: _Row(_written, "atan"),
    Operator.SINH: _Row(_written, "sinh"),
    Operator.COSH: _Row(_written, "cosh"),
    Operator.TANH: _Row(_written, "tanh"),
    Operator.ASINH: _Row(_written, "asinh"),
    Operator.ACOSH: _Row(_written, "acosh"),
    Operator.ATANH: _Row(_written, "atanh"),
    Operator.ENTROPY: _Row(_written, "entropy"),
    Operator.SIGN_POWER: _Row(_written, "signpow"),
    Operator.ERF: _Row(_written, "erf"),
    Operator.NODE: _Row(_written, "node"),
    Operator.PIECEWISE_LINEAR: _Row(_piecewise, "piecewise"),
    Operator.KEEP: _Row(_written, "keep"),
    Operator.REFERENCE: _Row(_written, "ref"),
    Operator.ATAN2: _Row(_written, "atan2"),
    Operator.LESS_EQUAL: _Row(_written, "le"),
    Operator.GREATER_EQUAL: _Row(_written, "ge"),
    Operator.EQUAL: _Row(_written, "eq"),
    Operator.LESS: _Row(_written, "lt"),
    Operator.GREATER: _Row(_written, "gt"),
    Operator.NOT_EQUAL: _Row(_written, "ne"),
    Operator.AND: _Row(_written, "and"),
    Operator.OR: _Row(_written, "or"),
    Operator.NOT: _Row(_written, "not"),
    Operator.IF: _Row(_written, "if"),
}
