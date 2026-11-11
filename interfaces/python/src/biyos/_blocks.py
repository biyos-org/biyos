from biyos._integers import integer, whole


def _current(model, version, noun):
    if model._version != version:
        raise ValueError(f"the {noun} predates a deletion from its model")


def _target(value):
    from biyos._model import Model

    if not isinstance(value, Model):
        found = type(value).__name__
        raise TypeError(f"model must be a Model, not {found}")
    return value


class View:
    __slots__ = ("_model", "_index", "_version")

    def __init__(self, model, index, version):
        self._model = model
        self._index = index
        self._version = version

    def __index__(self):
        """Return the index of the item.

        Returns
        -------
        int
            The index of the item in its model.

        Raises
        ------
        ValueError
            When the model has had a deletion since the view was made.
        """
        _current(self._model, self._version, self._noun)
        return self._index


class Block:
    __slots__ = ("_model", "_first", "_count", "_version")

    def __init__(self, model, first, count, version):
        self._model = model
        self._first = first
        self._count = count
        self._version = version

    def __len__(self):
        self._check()
        return self._count

    def __iter__(self):
        self._check()
        return map(self._view, self._range())

    def __getitem__(self, key):
        """Return one item of the block, or a block for a slice.

        Parameters
        ----------
        key : int or slice
            A position in the block, a negative one counting from the end,
            or a slice with step 1.

        Returns
        -------
        view or block
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
        return self._view(self._first + self._position(key))

    def on(self, model):
        """Return the block's items in another model.

        The block keeps its first index and count, so it names the items
        under the same indices in ``model``, such as those of a copy.

        Parameters
        ----------
        model : Model
            The model the new block belongs to.

        Returns
        -------
        block
            A block of the same family on ``model``.

        Raises
        ------
        TypeError
            When ``model`` is not a ``Model``.
        ValueError
            When ``model`` is closed or holds too few items of the family, or
            when the block's own model has had a deletion since the block was
            made.
        """
        self._check()
        model = _target(model)
        held = self._held(model)
        end = self._first + self._count
        if held < end:
            noun = self._element._noun
            needs = f"{held}, the block needs {end}"
            raise ValueError(f"model holds too few {noun}s: {needs}")
        return type(self)(model, self._first, self._count, model._version)

    def _check(self):
        _current(self._model, self._version, "block")

    def _range(self):
        return range(self._first, self._first + self._count)

    def _view(self, index):
        return self._element(self._model, index, self._version)

    def _position(self, key):
        number = whole("index", key)
        position = number + self._count if number < 0 else number
        if not 0 <= position < self._count:
            count = self._count
            raise IndexError(f"index {number} is outside a block of {count}")
        return position

    def _slice(self, key):
        start, stop, step = key.indices(self._count)
        if step != 1:
            raise ValueError(f"slice step must be 1, not {step}")
        first = self._first + start
        count = max(stop - start, 0)
        return type(self)(self._model, first, count, self._version)


class Variable(View):
    """One variable of a model, as indexing a ``Variables`` block gives.

    A view holds its model and its index, and passes as that index wherever
    its model takes an index of variables. Any other model refuses it as an
    index. A view made before a deletion from its model raises ``ValueError``
    when used, as its index may name another variable.
    """

    __slots__ = ()
    _noun = "variable"


class Variables(Block):
    """Consecutive variables of a model, as ``Model.add_variables`` returns.

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

    def _held(self, model):
        return model.variable_count()


class Function(View):
    """One function of a model, as indexing a ``Functions`` block gives.

    A view holds its model and its index, and passes as that index wherever
    its model takes an index of functions. Any other model refuses it as an
    index. A view made before a deletion from its model raises ``ValueError``
    when used, as its index may name another function.
    """

    __slots__ = ()
    _noun = "function"


class Functions(Block):
    """Consecutive functions of a model, as ``Model.add_functions`` returns.

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

    def _held(self, model):
        return model.function_count()


class Constraint(View):
    """One constraint of a model, as indexing a ``Constraints`` block gives.

    A view holds its model and its index, and passes as that index wherever
    its model takes an index of constraints. Any other model refuses it as an
    index. A view made before a deletion from its model raises ``ValueError``
    when used, as its index may name another constraint.
    """

    __slots__ = ()
    _noun = "constraint"


class Constraints(Block):
    """Consecutive constraints, as ``Model.add_constraints`` returns.

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

    def _held(self, model):
        return model.constraint_count()


class Objective(View):
    """One objective of a model, as indexing an ``Objectives`` block gives.

    A view holds its model and its index, and passes as that index wherever
    its model takes an index of objectives. Any other model refuses it as an
    index. A view made before a deletion from its model raises ``ValueError``
    when used, as its index may name another objective.
    """

    __slots__ = ()
    _noun = "objective"


class Objectives(Block):
    """Consecutive objectives, as ``Model.add_objectives`` returns.

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

    def _held(self, model):
        return model.objective_count()


def _indefinite(noun):
    article = "an" if noun[0] in "aeiou" else "a"
    return f"{article} {noun}"


def _member(model, family, name, view):
    noun = _indefinite(family._element._noun)
    if not isinstance(view, family._element):
        found = _indefinite(view._noun)
        raise ValueError(f"{name} is {found}, not {noun}")
    if view._model is not model:
        raise ValueError(f"{name} is {noun} of another model")


def index_of(model, family, name, value):
    if isinstance(value, View):
        _member(model, family, name, value)
    return integer(name, value)


def range_of(model, family, name, block):
    if not isinstance(block, family):
        found = block._element._noun
        noun = family._element._noun
        raise ValueError(f"{name} is a block of {found}s, not {noun}s")
    if block._model is not model:
        raise ValueError(f"{name} is a block of another model")
    return block._range()
