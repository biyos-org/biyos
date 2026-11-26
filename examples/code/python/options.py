import biyos

with biyos.Env() as env:
    env.setopt("maxtime", 60.0)
    env.setopt("presolve", False)
    env.setopt("checking", biyos.Checking.FULL)
    for name in ("maxtime", "presolve", "checking"):
        print(name, env.getopt(name))
    assert env.getopt("presolve") is False
    assert env.getopt("checking") is biyos.Checking.FULL

    versions = env.getoptver("checking")
    print(versions)
    assert versions == biyos.Versions("0.1.0", None, None)

    with biyos.Model(env) as model:
        model.setopt("maxtime", 5.0)
        own = model.getopt("maxtime")
        model.delopt("maxtime")
        shared = model.getopt("maxtime")
        source = model.getoptsrc("maxtime")
        print("own", own, "shared", shared, "source", source.name)
        assert source is biyos.OptionSource.ENV
