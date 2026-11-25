from collections.abc import Callable, Iterator
from typing import (
    TYPE_CHECKING,
    Any,
    ClassVar,
    Generic,
    SupportsIndex,
    TypeVar,
    overload,
)

from biyos._convert.integers import integer, whole
from biyos._convert.numbers import Index
from biyos._model.expression import Operand

if TYPE_CHECKING:
    from biyos._model.model import Model


def _current(mdl: "Model", gen: int, noun: str) -> None:
    if mdl._gen != gen:
        raise ValueError(f"the {noun} predates a deletion from its model")


def _target(value: "Model") -> "Model":
    from biyos._model.model import Model

    if not isinstance(value, Model):
        found = type(value).__name__
        raise TypeError(f"mdl must be a Model, not {found}")
    return value


class View:
    __slots__ = ("_gen", "_idx", "_model")
    _noun: ClassVar[str]

    def __init__(self, mdl: "Model", idx: Index, gen: int) -> None:
        self._model = mdl
        self._idx = idx
        self._gen = gen

    def __index__(self) -> Index:
        """Return the index of the item.

        Returns
        -------
        idx
            The index of the item in its model.

        Raises
        ------
        ValueError
            When the model has had a deletion since the view was made.
        """
        _current(self._model, self._gen, self._noun)
        return self._idx


Item = TypeVar("Item", bound=View)
Self = TypeVar("Self", bound="Block[Any]")


class Block(Generic[Item]):
    __slots__ = ("_gen", "_model", "_start", "_stop")
    _element: type[Item]
    _held: Callable[["Model"], Index]

    def __init__(
        self, mdl: "Model", start: Index, stop: Index, gen: int
    ) -> None:
        self._model = mdl
        self._start = start
        self._stop = stop
        self._gen = gen

    def __len__(self) -> Index:
        """Return the number of items in the block.

        Returns
        -------
        n
            The number of items.

        Raises
        ------
        ValueError
            When the model has had a deletion since the block was made.
        """
        self._check()
        return self._stop - self._start

    def __contains__(self, item: object) -> bool:
        """Return whether a view names one of the block's items.

        Parameters
        ----------
        item
            A view, such as a ``Variable``.

        Returns
        -------
        found
            Whether ``item`` is a view of the block's family and model whose
            index lies in the block. False for any other value.

        Raises
        ------
        ValueError
            When the model has had a deletion since the block or the view
            was made.
        """
        self._check()
        if (
            not isinstance(item, self._element)
            or item._model is not self._model
        ):
            return False
        _current(item._model, item._gen, item._noun)
        return self._start <= item._idx < self._stop

    def __iter__(self) -> Iterator[Item]:
        """Iterate over the items of the block, each as a view.

        Returns
        -------
        views
            The views of the items, in order.

        Raises
        ------
        ValueError
            When the model has had a deletion since the block was made.
        """
        self._check()
        return map(self._view, self._range())

    @overload
    def __getitem__(self, key: SupportsIndex) -> Item: ...

    @overload
    def __getitem__(self: Self, key: slice) -> Self: ...

    def __getitem__(self, key: SupportsIndex | slice) -> "Item | Block[Item]":
        """Return one item of the block, or a block for a slice.

        Parameters
        ----------
        key
            A position in the block, a negative one counting from the end,
            or a slice with step 1.

        Returns
        -------
        item
            The item at the position, such as a ``Variable`` of a
            ``Variables`` block, or the items of the slice, as a block of
            the same family.

        Raises
        ------
        TypeError
            When the key is not an int or a slice of ints.
        IndexError
            When the position lies outside the block.
        ValueError
            When the slice has another step, or when the model has had a
            deletion since the block was made.
        """
        self._check()
        if isinstance(key, slice):
            return self._slice(key)
        return self._view(self._start + self._position(key))

    def on(self: Self, mdl: "Model") -> Self:
        """Return the block's items in another model.

        The block keeps its first index and count, so it names the items
        under the same indices in ``mdl``, such as those of a copy.

        Parameters
        ----------
        mdl
            The model the new block belongs to.

        Returns
        -------
        block
            A block of the same family on ``mdl``.

        Raises
        ------
        TypeError
            When ``mdl`` is not a ``Model``.
        ValueError
            When ``mdl`` is closed or holds too few items of the family, or
            when the block's own model has had a deletion since the block was
            made.
        """
        self._check()
        mdl = _target(mdl)
        held = self._held(mdl)
        if held < self._stop:
            noun = self._element._noun
            needs = f"{held}, the block needs {self._stop}"
            raise ValueError(f"mdl holds too few {noun}s: {needs}")
        return type(self)(mdl, self._start, self._stop, mdl._gen)

    def _check(self) -> None:
        _current(self._model, self._gen, "block")

    def _range(self) -> range:
        return range(self._start, self._stop)

    def _view(self, idx: Index) -> Item:
        return self._element(self._model, idx, self._gen)

    def _position(self, key: SupportsIndex) -> Index:
        number = whole("index", key)
        n = self._stop - self._start
        position = number + n if number < 0 else number
        if not 0 <= position < n:
            raise IndexError(f"index {number} is outside a block of {n}")
        return position

    def _slice(self: Self, key: slice) -> Self:
        start, stop, step = key.indices(self._stop - self._start)
        if step != 1:
            raise ValueError(f"slice step must be 1, not {step}")
        first = self._start + start
        last = self._start + max(stop, start)
        return type(self)(self._model, first, last, self._gen)


class Variable(View, Operand):
    """One variable of a model, as indexing a ``Variables`` block gives.

    A view holds its model and its index, and passes as that index wherever
    its model takes an index of variables. Any other model refuses it as an
    index. A view made before a deletion from its model raises ``ValueError``
    when used, as its index may name another variable. Arithmetic on views
    builds an ``Expression``.
    """

    __slots__ = ()
    _noun = "variable"

    def __repr__(self) -> str:
        """Return the variable as an expression writes it.

        Returns
        -------
        text
            ``x[i]``, where ``i`` is the index of the variable.
        """
        return f"x[{self._idx}]"

    def __hash__(self) -> int:
        """Return a hash of the model and the index.

        Two views of one variable hash alike, and ``==`` between them
        builds a ``Comparison`` that is true.

        Returns
        -------
        hash
            The hash of the view's model and index.
        """
        return hash(self._key())

    def _key(self) -> tuple[object, Index]:
        return (self._model, self._idx)


class Variables(Block[Variable]):
    """Consecutive variables of a model, as ``Model.addvar`` returns.

    A block holds its model, its first index and its count, so it costs the
    same at any size. ``len`` gives the count, and iterating gives each
    variable in order as a ``Variable``, so a block passes wherever its model
    takes indices of variables. Any other model refuses it as indices, and
    ``on`` gives the block of the same indices in another model, such as a
    copy. A block made before a deletion from its model raises ``ValueError``
    when used, as its indices may name other variables.
    """

    __slots__ = ()
    _element = Variable

    def _held(self, model: "Model") -> Index:
        return model.getnvar()


class Function(View):
    """One function of a model, as indexing a ``Functions`` block gives.

    A view holds its model and its index, and passes as that index wherever
    its model takes an index of functions. Any other model refuses it as an
    index. A view made before a deletion from its model raises ``ValueError``
    when used, as its index may name another function.
    """

    __slots__ = ()
    _noun = "function"


class Functions(Block[Function]):
    """Consecutive functions of a model, as ``Model.addfun`` returns.

    A block holds its model, its first index and its count, so it costs the
    same at any size. ``len`` gives the count, and iterating gives each
    function in order as a ``Function``, so a block passes wherever its model
    takes indices of functions. Any other model refuses it as indices, and
    ``on`` gives the block of the same indices in another model, such as a
    copy. A block made before a deletion from its model raises ``ValueError``
    when used, as its indices may name other functions.
    """

    __slots__ = ()
    _element = Function

    def _held(self, model: "Model") -> Index:
        return model.getnfun()


class Constraint(View):
    """One constraint of a model, as indexing a ``Constraints`` block gives.

    A view holds its model and its index, and passes as that index wherever
    its model takes an index of constraints. Any other model refuses it as an
    index. A view made before a deletion from its model raises ``ValueError``
    when used, as its index may name another constraint.
    """

    __slots__ = ()
    _noun = "constraint"


class Constraints(Block[Constraint]):
    """Consecutive constraints, as ``Model.addcon`` returns.

    A block holds its model, its first index and its count, so it costs the
    same at any size. ``len`` gives the count, and iterating gives each
    constraint in order as a ``Constraint``, so a block passes wherever its
    model takes indices of constraints. Any other model refuses it as
    indices, and ``on`` gives the block of the same indices in another model,
    such as a copy. A block made before a deletion from its model raises
    ``ValueError`` when used, as its indices may name other constraints.
    """

    __slots__ = ()
    _element = Constraint

    def _held(self, model: "Model") -> Index:
        return model.getncon()


class Objective(View):
    """One objective of a model, as indexing an ``Objectives`` block gives.

    A view holds its model and its index, and passes as that index wherever
    its model takes an index of objectives. Any other model refuses it as an
    index. A view made before a deletion from its model raises ``ValueError``
    when used, as its index may name another objective.
    """

    __slots__ = ()
    _noun = "objective"


class Objectives(Block[Objective]):
    """Consecutive objectives, as ``Model.addobj`` returns.

    A block holds its model, its first index and its count, so it costs the
    same at any size. ``len`` gives the count, and iterating gives each
    objective in order as an ``Objective``, so a block passes wherever its
    model takes indices of objectives. Any other model refuses it as indices,
    and ``on`` gives the block of the same indices in another model, such as
    a copy. A block made before a deletion from its model raises
    ``ValueError`` when used, as its indices may name other objectives.
    """

    __slots__ = ()
    _element = Objective

    def _held(self, model: "Model") -> Index:
        return model.getnobj()


class Indicator(View):
    """One indicator of a model, as indexing an ``Indicators`` block gives.

    A view holds its model and its index, and passes as that index wherever
    its model takes an index of indicators. Any other model refuses it as an
    index. A view made before a deletion from its model raises ``ValueError``
    when used, as its index may name another indicator.
    """

    __slots__ = ()
    _noun = "indicator"


class Indicators(Block[Indicator]):
    """Consecutive indicators, as ``Model.addind`` returns.

    A block holds its model, its first index and its count, so it costs the
    same at any size. ``len`` gives the count, and iterating gives each
    indicator in order as an ``Indicator``, so a block passes wherever its
    model takes indices of indicators. Any other model refuses it as indices,
    and ``on`` gives the block of the same indices in another model, such as
    a copy. A block made before a deletion from its model raises
    ``ValueError`` when used, as its indices may name other indicators.
    """

    __slots__ = ()
    _element = Indicator

    def _held(self, model: "Model") -> Index:
        return model.getnind()


class SpecialOrderedSet(View):
    """One special ordered set of a model, as indexing a
    ``SpecialOrderedSets`` block gives.

    A view holds its model and its index, and passes as that index wherever
    its model takes an index of sets. Any other model refuses it as an
    index. A view made before a deletion from its model raises
    ``ValueError`` when used, as its index may name another set.
    """

    __slots__ = ()
    _noun = "set"


class SpecialOrderedSets(Block[SpecialOrderedSet]):
    """Consecutive special ordered sets, as ``Model.addsos`` returns.

    A block holds its model, its first index and its count, so it costs the
    same at any size. ``len`` gives the count, and iterating gives each set
    in order as a ``SpecialOrderedSet``, so a block passes wherever its
    model takes indices of sets. Any other model refuses it as indices, and
    ``on`` gives the block of the same indices in another model, such as a
    copy. A block made before a deletion from its model raises
    ``ValueError`` when used, as its indices may name other sets.
    """

    __slots__ = ()
    _element = SpecialOrderedSet

    def _held(self, model: "Model") -> Index:
        return model.getnsos()


class PiecewiseLinear(View):
    """One piecewise linear function of a model, as indexing a
    ``PiecewiseLinears`` block gives.

    A view holds its model and its index, and passes as that index wherever
    its model takes an index of pwls. Any other model refuses it as an
    index. A view made before a deletion from its model raises
    ``ValueError`` when used, as do the views of every family.
    """

    __slots__ = ()
    _noun = "pwl"


class PiecewiseLinears(Block[PiecewiseLinear]):
    """Consecutive piecewise linear functions, as ``Model.addpwl`` returns.

    A block holds its model, its first index and its count, so it costs the
    same at any size. ``len`` gives the count, and iterating gives each pwl
    in order as a ``PiecewiseLinear``, so a block passes wherever its model
    takes indices of pwls. Any other model refuses it as indices, and ``on``
    gives the block of the same indices in another model, such as a copy. A
    block made before a deletion from its model raises ``ValueError`` when
    used, as do the blocks of every family.
    """

    __slots__ = ()
    _element = PiecewiseLinear

    def _held(self, model: "Model") -> Index:
        return model.getnpwl()


class Cone(View):
    """One cone of a model, as indexing a ``Cones`` block gives.

    A view holds its model and its index, and passes as that index wherever
    its model takes an index of cones. Any other model refuses it as an
    index. A view made before a deletion from its model raises ``ValueError``
    when used, as its index may name another cone.
    """

    __slots__ = ()
    _noun = "cone"


class Cones(Block[Cone]):
    """Consecutive cones, as ``Model.addcone`` returns.

    A block holds its model, its first index and its count, so it costs the
    same at any size. ``len`` gives the count, and iterating gives each cone
    in order as a ``Cone``, so a block passes wherever its model takes
    indices of cones. Any other model refuses it as indices, and ``on`` gives
    the block of the same indices in another model, such as a copy. A block
    made before a deletion from its model raises ``ValueError`` when used, as
    its indices may name other cones.
    """

    __slots__ = ()
    _element = Cone

    def _held(self, model: "Model") -> Index:
        return model.getncone()


class Complement(View):
    """One complement of a model, as indexing a ``Complements`` block gives.

    A view holds its model and its index, and passes as that index wherever
    its model takes an index of complements. Any other model refuses it as an
    index. A view made before a deletion from its model raises ``ValueError``
    when used, as its index may name another complement.
    """

    __slots__ = ()
    _noun = "complement"


class Complements(Block[Complement]):
    """Consecutive complements, as ``Model.addccon`` returns.

    A block holds its model, its first index and its count, so it costs the
    same at any size. ``len`` gives the count, and iterating gives each
    complement in order as a ``Complement``, so a block passes wherever its
    model takes indices of complements. Any other model refuses it as
    indices, and ``on`` gives the block of the same indices in another model,
    such as a copy. A block made before a deletion from its model raises
    ``ValueError`` when used, as its indices may name other complements.
    """

    __slots__ = ()
    _element = Complement

    def _held(self, model: "Model") -> Index:
        return model.getnccon()


def _indefinite(noun: str) -> str:
    article = "an" if noun[0] in "aeiou" else "a"
    return f"{article} {noun}"


def _member(
    model: "Model", family: type[Block[Any]], name: str, view: View
) -> None:
    noun = _indefinite(family._element._noun)
    if not isinstance(view, family._element):
        found = _indefinite(view._noun)
        raise TypeError(f"{name} is {found}, not {noun}")
    if view._model is not model:
        raise ValueError(f"{name} is {noun} of another model")


def index_of(
    model: "Model", family: type[Block[Any]], name: str, value: SupportsIndex
) -> Index:
    if isinstance(value, View):
        _member(model, family, name, value)
    return integer(name, value)


def range_of(
    model: "Model", family: type[Block[Any]], name: str, block: Block[Any]
) -> range:
    if not isinstance(block, family):
        found = block._element._noun
        noun = family._element._noun
        raise TypeError(f"{name} is a block of {found}s, not {noun}s")
    if block._model is not model:
        raise ValueError(f"{name} is a block of another model")
    block._check()
    return block._range()
