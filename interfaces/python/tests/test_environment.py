import gc

import pytest

import biyos


def test_context():
    with biyos.Environment() as env:
        assert env.get_option("seed") == 0
    with pytest.raises(ValueError, match="the environment is closed"):
        env.get_option("seed")


def test_close():
    env = biyos.Environment()
    env.close()
    env.close()
    with pytest.raises(ValueError, match="the environment is closed"):
        env.get_option("seed")


def test_collected():
    env = biyos.Environment()
    free = env._free
    del env
    gc.collect()
    assert not free.alive
