import biyos

with biyos.Env() as env, biyos.Env() as other:
    with biyos.Model(env) as model:
        x = model.addvar(2, ub=[1.0, 2.0])
        f = model.addfun(1)
        model.addcon(f)

        copy = model.copy(other)
        assert copy.getvar() == model.getvar()

        copy.addvar(1)
        y = x.on(copy)
        copy.setvarbnd(y, ub=[3.0, 4.0])
        print(model.getvar()[2], copy.getvar()[2])
        assert model.getnvar() == 2 and copy.getnvar() == 3
        copy.close()
