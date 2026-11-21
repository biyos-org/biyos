import biyos

with biyos.Env() as env:
    env.setopt("maxtime", 60.0)
    env.setopt("presolve", False)
    env.setopt("check_lvl", "full")
    for name in ("maxtime", "presolve", "check_lvl"):
        print(name, env.getopt(name))
    assert env.getopt("presolve") is False

    versions = env.getoptver("check_lvl")
    print(versions)
    assert versions == biyos.Versions("0.1.0", None, None)
