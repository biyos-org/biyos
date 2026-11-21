import biyos

print("biyos", biyos.version())

with biyos.Env() as env:
    print("maxtime", env.getopt("maxtime"))
