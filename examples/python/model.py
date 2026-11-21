import biyos
from biyos import Sense, VariableKind


def build(model):
    knd = [
        VariableKind.CONTINUOUS,
        VariableKind.INTEGER,
        VariableKind.BINARY,
    ]
    x = model.addvars(3, knd, [0.0, 0.0, 0.0], [4.0, 3.0, 1.0])
    f = model.addfuns(
        3,
        cst=[0.0, 0.0, 1.0],
        off=[0, 3, 5, 6],
        varidx=[x[0], x[1], x[2], x[0], x[1], x[0]],
        coef=[1.0, 1.0, 1.0, 1.0, -2.0, -2.0],
    )
    model.addquadterms(
        [f[2]],
        off=[0, 2],
        varidx1=[x[0], x[1]],
        varidx2=[x[0], x[1]],
        coef=[1.0, 1.0],
    )
    model.addcons([f[0], f[1]], lb=[1.0, 0.0], ub=[5.0, 0.0])
    model.addobjs(
        [f[2], f[0]],
        sense=[Sense.MINIMIZE, Sense.MAXIMIZE],
        prio=[0, 1],
        atol=[0.5, 0.0],
    )
    return x, f


with biyos.Env() as env, biyos.Model(env) as model:
    x, f = build(model)
    off, varidx1, varidx2, coef = model.getquadterms(f[2], 1)
    print(f"function 2 has {off[1]} quadratic terms")
    assert (varidx1, varidx2) == ([0, 1], [0, 1])
    assert len(x) == model.getnvars() == 3
