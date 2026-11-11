from biyos._integers import integer, whole


def _current(model, version, noun):
    if model._version != version:
        raise ValueError(f"the {noun} predates a deletion from its model")


class Variable:
    """One variable of a model, as indexing a ``Variables`` block gives.

    A view holds its model and its index, and passes as that index wherever
    its model takes an index of variables. Any other model refuses it as an
    index. A view made before a deletion from its model raises ``ValueError``
    when used, as its index may name another variable.
    """

    __slots__ = ("_model", "_index", "_version")

    def __init__(self, model, index, version):
        self._model = model
        self._index = index
        self._version = version

    def __index__(self):
        """Return the index of the variable.

        Returns
        -------
        int
            The index of the variable in its model.

        Raises
        ------
        ValueError
            When the model has had a deletion since the view was made.
        """
        _current(self._model, self._version, "variable")
        return self._index


class Variables:
    """Consecutive variables of a model, as ``Model.add_variables`` returns.

    A block holds its model, its first index and its count, so it costs the
    same at any size. ``len`` gives the count, and iterating gives each
    variable in order as a ``Variable``, so a block passes wherever its model
    takes indices of variables. Any other model refuses it as indices. A block
    made before a deletion from its model raises ``ValueError`` when used, as
    its indices may name other variables.
    """

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
        """Return one variable of the block, or a block for a slice.

        Parameters
        ----------
        key : int or slice
            A position in the block, a negative one counting from the end,
            or a slice with step 1.

        Returns
        -------
        Variable or Variables
            The variable at the position, or the variables of the slice.

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

    def _indices(self, model, name):
        if self._model is not model:
            raise ValueError(f"{name} is a block of another model")
        return self._range()

    def _check(self):
        _current(self._model, self._version, "block")

    def _range(self):
        return range(self._first, self._first + self._count)

    def _view(self, index):
        return Variable(self._model, index, self._version)

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
        return Variables(self._model, first, count, self._version)


def variable_index(model, name, value):
    if isinstance(value, Variable) and value._model is not model:
        raise ValueError(f"{name} is a variable of another model")
    return integer(name, value)
