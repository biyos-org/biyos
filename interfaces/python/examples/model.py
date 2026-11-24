import biyos
from biyos import ObjectiveSense, VariableKind


def build(model):
    knd = [
        VariableKind.CONTINUOUS,
        VariableKind.INTEGER,
        VariableKind.BINARY,
    ]
    x = model.addvar(3, knd, [0.0, 0.0, 0.0], [4.0, 3.0, 1.0])
    f = model.addfun(3)
    model.setcst(f, [0.0, 0.0, 1.0])
    model.addlinterm(
        [f[0], f[0], f[0], f[1], f[1], f[2]],
        [x[0], x[1], x[2], x[0], x[1], x[0]],
        [1.0, 1.0, 1.0, 1.0, -2.0, -2.0],
    )
    model.addquadterm([f[2], f[2]], [x[0], x[1]], [x[0], x[1]], [1.0, 1.0])
    model.addcon([f[0], f[1]], lb=[1.0, 0.0], ub=[5.0, 0.0])
    o = model.addobj(
        [f[2], f[0]],
        sense=[ObjectiveSense.MINIMIZE, ObjectiveSense.MAXIMIZE],
        prio=[0, 1],
    )
    model.setobjtol([o[0]], atol=[0.5])
    return x, f


with biyos.Env() as env, biyos.Model(env) as model:
    x, f = build(model)
    funidx, varidx1, varidx2, coef = model.getquadterm(f[2:])
    print(f"function 2 has {model.getnquadterm(f[2:])} quadratic terms")
    assert (varidx1, varidx2) == ([0, 1], [0, 1])
    assert len(x) == model.getnvar() == 3
