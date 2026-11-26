from inspect import signature
from operator import index

import pytest

import biyos
from biyos import ConeKind, ObjectiveSense, SpecialOrderedSetKind
from biyos import VariableKind

NAMED = ("var", "fun", "con", "obj", "ind", "sos", "cone", "ccon")
TOLERANCES = ("var", "con", "obj", "cone", "ccon")
WRITERS = [
    ("setvarbnd", "var", {"lb": 1.0, "ub": 1.0}),
    ("setvarknd", "var", {"knd": VariableKind.INTEGER}),
    ("setvartol", "var", {"atol": 0.5, "rtol": None}),
    ("setcst", "fun", {"cst": 1.5}),
    ("addcst", "fun", {"cst": 1.5}),
    ("attachexpr", "fun", {"node": 0}),
    ("setconbnd", "con", {"lb": -1.0, "ub": 1.0}),
    ("setcontol", "con", {"atol": 0.5, "rtol": 0.25}),
    ("setobjsense", "obj", {"sense": ObjectiveSense.MAXIMIZE}),
    ("setobjprio", "obj", {"prio": 3}),
    ("setobjcoef", "obj", {"coef": 2.0}),
    ("setobjtol", "obj", {"atol": 0.5, "rtol": 0.25}),
    ("setindbnd", "ind", {"lb": 0.0, "ub": 2.0}),
    ("setindvar", "ind", {"var": 0, "val": 0}),
    ("setconetol", "cone", {"atol": 0.5, "rtol": 0.25}),
    ("setcconbnd", "ccon", {"lb": -1.0, "ub": 1.0}),
    ("setccontol", "ccon", {"atol": 0.5, "rtol": 0.25}),
] + [(f"set{short}name", short, {"name": "z"}) for short in NAMED]
DELETERS = (
    [(f"del{short}", short) for short in NAMED]
    + [("clrfun", "fun")]
    + [(f"del{short}name", short) for short in NAMED]
    + [(f"del{short}tol", short) for short in TOLERANCES if short != "obj"]
)
READERS = [
    ("getnvar", None),
    ("getnfun", None),
    ("getvar", "var"),
    ("getvartol", "var"),
    ("getcst", "fun"),
    ("getrlinterm", None),
    ("getcon", "con"),
    ("getcontol", "con"),
    ("getobj", "obj"),
    ("getobjtol", "obj"),
    ("getind", "ind"),
    ("getsos", "sos"),
    ("getcone", "cone"),
    ("getconetol", "cone"),
    ("getccon", "ccon"),
    ("getccontol", "ccon"),
] + [(f"get{short}name", short) for short in NAMED]
SCALAR_READERS = [
    ("getvarbnd", "var"),
    ("getvarknd", "var"),
    ("getvartol", "var"),
    ("getvar", "var"),
    ("getcst", "fun"),
    ("getconbnd", "con"),
    ("getcontol", "con"),
    ("getcon", "con"),
    ("getobjsense", "obj"),
    ("getobjprio", "obj"),
    ("getobjcoef", "obj"),
    ("getobjtol", "obj"),
    ("getobj", "obj"),
    ("getindbnd", "ind"),
    ("getindvar", "ind"),
    ("getind", "ind"),
    ("getconetol", "cone"),
    ("getcconbnd", "ccon"),
    ("getccontol", "ccon"),
    ("getccon", "ccon"),
] + [(f"get{short}name", short) for short in NAMED]
DEFAULT_READERS = [
    (f"get{short}", short) for short in ("var", "con", "obj", "ind", "ccon")
]
LIST_READERS = {
    "getrlinterm",
    "getclinterm",
    "getquadterm",
    "getexpr",
    "getsos",
    "getpwl",
    "getcone",
    "getconeprm",
    "getgpat",
    "gethpat",
}
LIST_COUNTERS = (
    "getnrlinterm",
    "getnclinterm",
    "getnquadterm",
    "getnexpr",
    "getnsosvar",
    "getnpwlpt",
    "getnconefun",
    "getnconeprm",
    "getngpat",
    "getnhpat",
)
COUNTS = {
    "var": "getnvar",
    "fun": "getnfun",
    "con": "getncon",
    "obj": "getnobj",
    "ind": "getnind",
    "sos": "getnsos",
    "cone": "getncone",
    "ccon": "getnccon",
}


@pytest.fixture
def env():
    with biyos.Env() as env:
        yield env


@pytest.fixture
def model(env):
    with biyos.Model(env) as model:
        yield model


@pytest.fixture
def other(env):
    with biyos.Model(env) as other:
        yield other


def build(model):
    x = model.addvar(4, [VariableKind.BINARY] * 4)
    f = model.addfun(4)
    g = f[:3]
    sizes = [1, 1, 1]
    model.addlinterm([0, 1, 2], [0, 1, 3], [1.0, 2.0, 3.0])
    model.setcst(f, [0.5] * 4)
    model.addvarnode(x[0])
    blocks = {
        "var": x,
        "fun": f,
        "con": model.addcon(g),
        "obj": model.addobj(g),
        "ind": model.addind(x[:3], [1, 0, 1], g),
        "sos": model.addsos([SpecialOrderedSetKind.ONE] * 3, sizes, x[:3]),
        "cone": model.addcone([ConeKind.NONNEGATIVE] * 3, sizes, g),
        "ccon": model.addccon(g, [f[1], f[2], f[0]]),
    }
    for short in NAMED:
        count = len(blocks[short])
        names = [f"{short}{k}" for k in range(count)]
        getattr(model, f"set{short}name")(blocks[short], names)
    for short in TOLERANCES:
        count = len(blocks[short])
        getattr(model, f"set{short}tol")(blocks[short], [0.125] * count)
    return blocks


def last(model, short):
    return getattr(model, COUNTS[short])() - 1


def read(model):
    found = []
    for reader, short in READERS:
        call = getattr(model, reader)
        if short is None:
            found.append(call())
        else:
            found.append(call(range(last(model, short) + 1)))
    return repr(found)


def listed(args):
    return {
        name: value if value is None else [value]
        for name, value in args.items()
    }


def apply(model, method, idx, args):
    return getattr(model, method)(idx, **args)


def check_alike(model, other, method, short, args):
    build(model)
    build(other)
    before = read(model)
    k = last(model, short)
    apply(model, method, k, args)
    apply(other, method, [k], listed(args))
    assert read(model) == read(other)
    assert read(model) != before


@pytest.mark.parametrize("method, short, args", WRITERS)
def test_writer_scalar(model, other, method, short, args):
    check_alike(model, other, method, short, args)


@pytest.mark.parametrize("method, short, args", WRITERS)
def test_writer_view(model, other, method, short, args):
    blocks = build(model)
    build(other)
    view = blocks[short][-1]
    apply(model, method, view, args)
    apply(other, method, index(view), args)
    assert read(model) == read(other)


@pytest.mark.parametrize("method, short, args", WRITERS)
def test_writer_list_value(model, method, short, args):
    build(model)
    k = last(model, short)
    for name, value in args.items():
        if value is not None:
            mixed = {**args, name: [value]}
            with pytest.raises(TypeError, match=f"^{name} must be one"):
                apply(model, method, k, mixed)


@pytest.mark.parametrize("method, short, args", WRITERS)
def test_writer_scalar_value(model, method, short, args):
    build(model)
    k = last(model, short)
    for name, value in args.items():
        if value is not None:
            mixed = {**listed(args), name: value}
            with pytest.raises(TypeError, match=f"^{name} must be a"):
                apply(model, method, [k], mixed)


@pytest.mark.parametrize("method, short", DELETERS)
def test_deleter_scalar(model, other, method, short):
    build(model)
    build(other)
    before = read(model)
    k = last(model, short)
    found = getattr(model, method)(k)
    assert found == getattr(other, method)([k])
    assert read(model) == read(other)
    assert read(model) != before


@pytest.mark.parametrize("method, short", DELETERS)
def test_deleter_view(model, other, method, short):
    blocks = build(model)
    build(other)
    view = blocks[short][-1]
    k = index(view)
    found = getattr(model, method)(view)
    assert found == getattr(other, method)(k)
    assert read(model) == read(other)


def test_delvar_map(model):
    x = build(model)["var"]
    assert model.delvar(x[3]) == [0, 1, 2, -1]


def test_indvar_view(model):
    blocks = build(model)
    x = blocks["var"]
    model.setindvar(blocks["ind"][1], x[2], 1)
    assert model.getindvar([1]) == ([2], [1])


def test_delcb_scalar(model):
    model.addfun(3)
    model.addcb([0])
    model.addcb([1, 2])
    assert model.delcb(0) == [-1, 0]
    assert model.getcbfun(0) == [1, 2]


def test_coverage():
    tested = {call[0] for call in WRITERS + DELETERS} | {"delcb", "delhook"}
    for name in dir(biyos.Model):
        if name.startswith(("set", "del")):
            call = getattr(biyos.Model, name)
            assert "idx" not in signature(call).parameters or name in tested


def unwrapped(found):
    if isinstance(found, tuple):
        return tuple(values[0] for values in found)
    return found[0]


@pytest.mark.parametrize("method, short", SCALAR_READERS)
def test_reader_scalar(model, method, short):
    build(model)
    k = last(model, short)
    call = getattr(model, method)
    assert repr(call(k)) == repr(unwrapped(call([k])))


@pytest.mark.parametrize("method, short", SCALAR_READERS)
def test_reader_view(model, method, short):
    view = build(model)[short][-1]
    call = getattr(model, method)
    assert repr(call(view)) == repr(call(index(view)))


@pytest.mark.parametrize("method, short", DEFAULT_READERS)
def test_reader_every(model, method, short):
    build(model)
    call = getattr(model, method)
    assert call() == call(range(last(model, short) + 1))


@pytest.mark.parametrize("method", sorted(LIST_READERS) + list(LIST_COUNTERS))
def test_reader_list(model, method):
    with pytest.raises(TypeError, match="idx must be a sequence, not int"):
        getattr(model, method)(0)


def test_reader_coverage():
    tested = {call[0] for call in SCALAR_READERS} | LIST_READERS
    for name in dir(biyos.Model):
        if name.startswith("get") and not name.startswith("getn"):
            call = getattr(biyos.Model, name)
            assert "idx" not in signature(call).parameters or name in tested
