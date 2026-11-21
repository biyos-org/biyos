import biyos
from biyos import VariableKind

GROUPS = ["the caller", "the model", "user code", "resources", "biyos"]


def at_fault(code):
    return GROUPS[min(-code // 100, 5) - 1]


with biyos.Env() as env, biyos.Model(env) as model:
    try:
        model.addvars(1, [VariableKind.BINARY], ub=[2.0])
    except biyos.Error as error:
        print(error.code, at_fault(error.code), error)
        assert error.code == -100

    try:
        env.setopt("seed", 1.5)
    except TypeError as error:
        print(error)
    assert env.getopt("seed") == 0
