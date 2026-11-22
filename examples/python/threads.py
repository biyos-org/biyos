from concurrent.futures import ThreadPoolExecutor

import biyos


def build(size):
    with biyos.Env() as env, biyos.Model(env) as model:
        env.setopt("seed", size)
        x = model.addvar(size, ub=[1.0] * size)
        model.addfuns(1, None, [0, size], list(x), [1.0] * size)
        return model.getnvar(), model.getfuns()[1]


with ThreadPoolExecutor(max_workers=4) as pool:
    results = list(pool.map(build, [10, 20, 30, 40]))

print(results)
assert results == [(size, [0, size]) for size in (10, 20, 30, 40)]
