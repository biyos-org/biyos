from functools import partial

import pytest

import biyos
from biyos import EventKind, EventLevel
from refusal import LOCKED, refusal

ETE = "été \U0001f600"
FAMILIES = ("var", "fun", "con", "obj", "ind")
NOUNS = {
    "var": "variable",
    "fun": "function",
    "con": "constraint",
    "obj": "objective",
    "ind": "indicator",
}


@pytest.fixture
def env():
    with biyos.Env() as env:
        yield env


@pytest.fixture
def model(env):
    with biyos.Model(env) as model:
        add_items(model, 3)
        yield model


def add_items(model, count):
    x = model.addvar(count, [biyos.VariableKind.BINARY] * count)
    model.addfun(count)
    model.addcon([0] * count)
    model.addobj([0] * count)
    model.addind(x, [1] * count, [0] * count)


def calls(model, family):
    return (
        getattr(model, f"set{family}name"),
        getattr(model, f"get{family}name"),
        getattr(model, f"get{family}idx"),
        getattr(model, f"del{family}name"),
    )


def names_of(model, family):
    count = getattr(model, f"getn{family}")()
    return calls(model, family)[1](range(count))


def test_mdlname(model):
    assert model.getmdlname() is None
    model.setmdlname("plant")
    assert model.getmdlname() == "plant"
    model.setmdlname(ETE)
    assert model.getmdlname() == ETE
    model.delmdlname()
    assert model.getmdlname() is None
    model.delmdlname()
    assert model.getmdlname() is None


@pytest.mark.parametrize("family", FAMILIES)
def test_family(model, family):
    set_names, get_name, get_idx, _ = calls(model, family)
    assert names_of(model, family) == [None, None, None]
    set_names([2, 0], ["z", ETE])
    assert names_of(model, family) == [ETE, None, "z"]
    assert get_idx(["z"]) == [2]
    assert get_idx([ETE, "z", ETE]) == [0, 2, 0]
    assert type(get_idx(["z"])[0]) is int
    assert get_name([2]) == ["z"]
    assert get_name([2, 1, 2]) == ["z", None, "z"]
    assert get_name([]) == []
    assert get_idx([]) == []
    for other in FAMILIES:
        if other != family:
            assert names_of(model, other) == [None, None, None]


def test_shared(model):
    for k, family in enumerate(FAMILIES):
        calls(model, family)[0]([k % 3], ["x"])
    for k, family in enumerate(FAMILIES):
        assert calls(model, family)[2](["x"]) == [k % 3]


def test_views(env, model):
    x = model.addvar(2)
    c = model.addcon([0])
    f = model.addfun(1)
    o = model.addobj([0])
    model.setvarname(x, ["a", "b"])
    model.setconname([c[0]], ["c"])
    model.setfunname(f, ["f"])
    model.setobjname(o, ["o"])
    assert model.getvarname([x[1]]) == ["b"]
    assert model.getvarname(x) == ["a", "b"]
    assert model.getconname(c) == ["c"]
    assert model.getfunname([f[0]]) == ["f"]
    assert model.getobjname(o) == ["o"]
    assert names_of(model, "var")[3:] == ["a", "b"]
    other = biyos.Model(env)
    y = other.addvar(1)
    with pytest.raises(ValueError, match="idx is a variable of another"):
        model.getvarname([y[0]])
    with pytest.raises(ValueError, match="idx is a block of another"):
        model.setvarname(y, ["a"])
    with pytest.raises(ValueError, match="is a variable, not a constraint"):
        model.setconname([x[0]], ["d"])
    with pytest.raises(ValueError, match="idx is a constraint, not a"):
        model.getvarname([c[0]])
    with pytest.raises(ValueError, match="of functions, not objectives"):
        model.setobjname(f, ["g"])
    assert names_of(model, "con") == [None, None, None, "c"]


@pytest.mark.parametrize("family", FAMILIES)
def test_repeat(model, family):
    set_names, get_name, get_idx, del_names = calls(model, family)
    noun = NOUNS[family]
    set_names([0, 1, 2], ["c", "c", "c"])
    assert names_of(model, family) == ["c", "c", "c"]
    message = refusal(get_idx, ["c"])
    assert message == f"invalid argument: {noun} 'c': names 3 {noun}s"
    set_names([0], ["d"])
    message = refusal(get_idx, ["d", "c"])
    assert message == f"invalid argument: {noun} 'c': names 2 {noun}s"
    del_names([1])
    assert get_idx(["c", "d"]) == [2, 0]


@pytest.mark.parametrize("family", FAMILIES)
def test_delete(model, family):
    set_names, _, get_idx, del_names = calls(model, family)
    noun = NOUNS[family]
    set_names([0, 1, 2], ["a", "b", "c"])
    del_names([1, 0, 1])
    assert names_of(model, family) == [None, None, "c"]
    del_names([])
    message = refusal(del_names, [2, 3])
    assert message == f"invalid argument: {noun} 3: unknown"
    assert names_of(model, family) == [None, None, "c"]
    message = refusal(get_idx, ["c", "a"])
    assert message == f"invalid argument: {noun} 'a': unknown"
    set_names([1], ["a"])
    assert get_idx(["a"]) == [1]


def test_rename(model):
    model.setvarname([0, 1], ["a", "b"])
    model.setvarname([0], ["c"])
    model.setvarname([2], ["a"])
    assert names_of(model, "var") == ["c", "b", "a"]
    model.setvarname([0, 1], ["b", "c"])
    assert names_of(model, "var") == ["b", "c", "a"]
    model.setvarname([2, 2], ["d", "e"])
    assert names_of(model, "var") == ["b", "c", "e"]
    assert model.getvaridx(["b"]) == [0]
    message = refusal(model.getvaridx, ["d"])
    assert message == "invalid argument: variable 'd': unknown"


@pytest.mark.parametrize("family", FAMILIES)
def test_missing(model, family):
    set_names, _, get_idx, _ = calls(model, family)
    noun = NOUNS[family]
    set_names([0], ["x"])
    message = refusal(get_idx, ["w"])
    assert message == f"invalid argument: {noun} 'w': unknown"
    message = refusal(get_idx, [""])
    assert message == f"invalid argument: {noun} '': unknown"


def test_refused(model):
    model.setmdlname("plant")
    message = refusal(model.setmdlname, "")
    assert message == "invalid argument: model: name is empty"
    message = refusal(model.setvarname, [0], [""])
    assert message == "invalid argument: variable 0: name is empty"
    message = refusal(model.setvarname, [3], ["x"])
    assert message == "invalid argument: variable 3: unknown"
    message = refusal(model.setvarname, [0], None)
    assert message == "invalid argument: argument 'name': is null"
    message = refusal(model.setconname, [3], ["c"])
    assert message == "invalid argument: constraint 3: unknown"
    message = refusal(model.getfunname, [3])
    assert message == "invalid argument: function 3: unknown"
    message = refusal(model.getobjname, [-1])
    assert message == "invalid argument: objective -1: unknown"
    model.setvarname([], [])
    model.setvarname([], None)
    assert model.getmdlname() == "plant"
    assert names_of(model, "var") == [None, None, None]


def test_types(model):
    with pytest.raises(TypeError, match="name must be a str, not int"):
        model.setmdlname(3)
    with pytest.raises(TypeError, match="name must be a str, not bytes"):
        model.setmdlname(b"plant")
    with pytest.raises(TypeError, match="an entry of name must be a str"):
        model.setvarname([0], [b"x"])
    with pytest.raises(TypeError, match="name must be a sequence of str"):
        model.getvaridx("x")
    with pytest.raises(TypeError, match="sequence of str, not NoneType"):
        model.getvaridx(None)
    with pytest.raises(TypeError, match="name must be a sequence of str"):
        model.setvarname([0], "x")
    with pytest.raises(TypeError, match="an entry of name must be a str"):
        model.getvaridx([None])
    with pytest.raises(TypeError, match="idx must be an int, not str"):
        model.getvarname("x")
    with pytest.raises(ValueError, match="embedded null character"):
        model.setmdlname("a\0b")
    with pytest.raises(ValueError, match="embedded null character"):
        model.setconname([0], ["c\0"])
    with pytest.raises(ValueError, match="embedded null character"):
        model.getfunidx(["x\0"])
    with pytest.raises(UnicodeEncodeError):
        model.setobjname([0], ["\udc80"])
    with pytest.raises(ValueError, match="name has length 1, not 2"):
        model.setvarname([0, 1], ["x"])
    assert model.getmdlname() is None
    for family in FAMILIES:
        assert names_of(model, family) == [None, None, None]


def test_copy(env, model):
    model.setmdlname("plant")
    for family in FAMILIES:
        calls(model, family)[0]([0, 2], ["x", "z"])
    copy = model.copy()
    assert copy.getmdlname() == "plant"
    for family in FAMILIES:
        assert names_of(copy, family) == ["x", None, "z"]
        calls(copy, family)[0]([0, 1], ["y", "x"])
        assert names_of(model, family) == ["x", None, "z"]
    model.close()
    for family in FAMILIES:
        assert calls(copy, family)[2](["x"]) == [1]


def test_locked(env, model):
    seen = []

    def change(event):
        changes = [partial(model.setmdlname, "w"), model.delmdlname]
        for family in FAMILIES:
            set_names, _, _, del_names = calls(model, family)
            changes.append(partial(set_names, [0], ["w"]))
            changes.append(partial(del_names, [0]))
        for call in changes:
            try:
                call()
            except biyos.Error as error:
                seen.append(str(error))
        seen.append(model.getmdlname())
        for family in FAMILIES:
            _, get_name, get_idx, _ = calls(model, family)
            seen.append(get_name([0]))
            seen.append(get_idx(["x"]))

    model.setmdlname("plant")
    for family in FAMILIES:
        calls(model, family)[0]([0], ["x"])
    env.addhook(change, EventKind.MESSAGE, EventLevel.ERROR)
    with pytest.raises(biyos.Error):
        env.setopt("seed", -1)
    assert seen == [LOCKED] * 12 + ["plant"] + [["x"], [0]] * 5
    assert model.getmdlname() == "plant"


def test_closed(env):
    model = biyos.Model(env)
    model.close()
    each = [
        partial(model.setmdlname, "plant"),
        model.getmdlname,
        model.delmdlname,
    ]
    for family in FAMILIES:
        set_names, get_name, get_idx, del_names = calls(model, family)
        each.append(partial(set_names, [], []))
        each.append(partial(get_name, [0]))
        each.append(partial(get_idx, ["x"]))
        each.append(partial(del_names, []))
    for call in each:
        with pytest.raises(ValueError, match="the model is closed"):
            call()
