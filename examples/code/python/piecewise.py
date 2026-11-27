import biyos
from biyos import Operator


def build(model):
    x = model.addvar(2, lb=[0.0, 0.0], ub=[4.0, 4.0])
    p = model.addpwl([3], [0.0, 1.0, 2.0], [0.0, 1.0, 3.0])
    model.addobj(biyos.piecewise(p[0], x[0]) + x[1])
    model.addcon(biyos.piecewise(p[0], x[0] + x[1]) >= 2)
    return x, p


with biyos.Env() as env, biyos.Model(env) as model:
    x, p = build(model)
    assert model.getpwl(p) == ([3], [0.0, 1.0, 2.0], [0.0, 1.0, 3.0])
    f = model.getobj()[0]
    fun, var, coef = model.getrlinterm(f)
    assert (fun.indices(), var.indices(), coef) == (f.indices(), [1], [1.0])
    assert model.getexpr(f)[1] == [Operator.VARIABLE, Operator.PIECEWISE_LINEAR]
    print(model.getnpwl(), model.getnpwlpt())
