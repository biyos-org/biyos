import ctypes


class Sized:
    def __init__(self, kind, most):
        self.kind = kind
        self.most = most

    def __call__(self, *arguments):
        return self.kind(*arguments)

    def __mul__(self, length):
        assert length <= self.most, f"an array of {length} entries"
        return self.kind * length


def limit(monkeypatch, module, most):
    for name in ("c_int", "c_double", "c_int64"):
        if hasattr(module, name):
            sized = Sized(getattr(ctypes, name), most)
            monkeypatch.setattr(module, name, sized)


class Room:
    def __init__(self, function, lengths):
        self.function = function
        self.lengths = lengths

    def __call__(self, *arguments):
        for array, length in self.lengths(*arguments):
            assert array is None or len(array) >= length, "an array too short"
        return self.function(*arguments)
