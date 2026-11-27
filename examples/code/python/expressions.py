import biyos
from biyos import Operator


def build(model):
    x = model.addvar(4)
    model.addobj(x[0] + x[1] + x[2] ** 2 + biyos.log(x[3]))
    s = x[0] * x[1]
    model.addcon(
        [
            biyos.exp(s) + biyos.sin(s) <= 4,
            1 <= x[2] + biyos.max(x[3], 0),
            x[0] - x[1] == x[2],
            biyos.ifelse(x[3] <= 1, x[3], x[3] ** 2) <= 4,
        ]
    )
    return x


with biyos.Env() as env, biyos.Model(env) as model:
    build(model)
    f = model.getobj()[0]
    fun, var, coef = model.getrlinterm(f)
    assert (fun.indices(), var.indices(), coef) == ([0, 0], [0, 1], [1.0, 1.0])
    fun, var1, var2, coef = model.getquadterm(f)
    assert (var1.indices(), var2.indices(), coef) == ([2], [2], [1.0])
    assert model.getexpr(f)[1] == [Operator.VARIABLE, Operator.LOG]
    code = model.getexpr(model.getcon()[0][:1])[1]
    assert code.count(Operator.KEEP) == code.count(Operator.REFERENCE) == 1
    print(model.getcon()[1:])
