import inspect
from importlib.resources import files

import biyos

GENERATED = {"__annotate__"}


def shown(name):
    return not name.startswith("_") or name.endswith("__")


def written(function):
    module = function.__module__
    return module.startswith("biyos") and function.__name__ not in GENERATED


def methods(cls):
    for owner in cls.__mro__:
        for name, value in vars(owner).items():
            found = value.fget if isinstance(value, property) else value
            if shown(name) and inspect.isfunction(found) and written(found):
                yield found


def api():
    found = {}
    for name in biyos.__all__:
        value = getattr(biyos, name)
        if inspect.isfunction(value):
            found[value.__qualname__] = (value, False)
        elif inspect.isclass(value):
            found.update((f.__qualname__, (f, True)) for f in methods(value))
    return found


def missing(function, method):
    signature = inspect.signature(function)
    taken = list(signature.parameters.values())[1 if method else 0 :]
    names = [p.name for p in taken if p.annotation is p.empty]
    if signature.return_annotation is signature.empty:
        names.append("return")
    return names


def test_marker():
    assert (files("biyos") / "py.typed").is_file()


def test_annotated():
    found = {name: missing(*entry) for name, entry in api().items()}
    assert "Model.addvar" in found
    assert "Block.__getitem__" in found
    assert "Event.kind" in found
    assert {name: names for name, names in found.items() if names} == {}


def test_fields():
    fields = biyos.Versions._fields
    assert tuple(biyos.Versions.__annotations__) == fields
