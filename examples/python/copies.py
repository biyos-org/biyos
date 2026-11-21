import biyos

with biyos.Env() as env, biyos.Env() as other:
    with biyos.Model(env) as model:
        x = model.addvars(2, ub=[1.0, 2.0])
        f = model.addfuns(1)
        model.addcons(f)

        copy = model.copy(other)
        assert copy.getvars() == model.getvars()

        copy.addvars(1)
        y = x.on(copy)
        copy.setvarbnd(y, ub=[3.0, 4.0])
        print(model.getvars()[2], copy.getvars()[2])
        assert model.getnvars() == 2 and copy.getnvars() == 3
        copy.close()
