from importlib.metadata import version

import biyos


def test_version():
    assert biyos.version() == version("biyos")
