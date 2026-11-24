import pytest

import biyos

INVALID_ARGUMENT = -100
LOCKED = "invalid argument: model: is read-only while delivering an event"


def refusal(call, *arguments):
    with pytest.raises(biyos.Error) as caught:
        call(*arguments)
    assert caught.value.code == INVALID_ARGUMENT
    return str(caught.value)
