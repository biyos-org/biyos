from concurrent.futures import ThreadPoolExecutor

import biyos


def build(size):
    with biyos.Env() as env, biyos.Model(env) as model:
        env.setopt("seed", size)
        x = model.addvar(size, ub=[1.0] * size)
        f = model.addfun(1)
        model.addlinterm([f[0]] * size, x, [1.0] * size)
        return model.getnvar(), model.getnrlinterm(f)


with ThreadPoolExecutor(max_workers=4) as pool:
    results = list(pool.map(build, [10, 20, 30, 40]))

print(results)
assert results == [(size, size) for size in (10, 20, 30, 40)]
