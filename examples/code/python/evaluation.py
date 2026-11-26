import biyos


def evaluate(model):
    x = model.addvar(2)
    model.addobj(x[0] * x[1] + biyos.exp(x[0]))
    point = [0.0, 2.0]
    f = model.evalf(point)
    cnt, var = model.getgpat()
    g = model.evalg(point)
    h = model.evalh(point, [1.0])
    return f, (cnt, var, g), (*model.gethpat(), h)


with biyos.Env() as env, biyos.Model(env) as model:
    f, jacobian, hessian = evaluate(model)
    assert f == [1.0]
    assert jacobian == ([2], [0, 1], [3.0, 0.0])
    assert hessian == ([2, 0], [0, 1], [1.0, 1.0])
    print(f, jacobian, hessian)
