import pickle

import pytest

import biyos


def test_pickle():
    with biyos.Environment() as env, pytest.raises(biyos.Error) as caught:
        env.get_option("seeds")
    copy = pickle.loads(pickle.dumps(caught.value))
    assert type(copy) is biyos.Error
    assert str(copy) == str(caught.value)
    assert copy.code == caught.value.code
