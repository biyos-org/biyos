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
