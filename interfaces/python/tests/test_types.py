from importlib.resources import files


def test_marker():
    assert (files("biyos") / "py.typed").is_file()
