import posixpath
import re
import sys
from pathlib import Path, PurePosixPath

INCLUDE = re.compile(r'^\s*#\s*include\s+"([^"]+)"')
SOURCES = {".c", ".h", ".cpp", ".hpp"}
ENGINE = "engine"


def read_layers(path):
    layers = {}
    for number, line in enumerate(path.read_text().splitlines(), 1):
        if not line.strip():
            continue
        module, colon, deps = line.partition(":")
        if not colon:
            raise ValueError(f"{path}:{number}: expected 'module: deps'")
        layers[module.strip()] = deps.split()
    return layers


def check_table(layers):
    problems = []
    seen = set()
    for module, deps in layers.items():
        for dep in deps:
            if dep not in layers:
                problems.append(f"layers: {module} uses unknown {dep}")
            elif dep not in seen:
                problems.append(
                    f"layers: {module} uses {dep}, declared after it"
                )
        seen.add(module)
    return problems


def module_of(path):
    return path.split("/")[0]


def check_roles(roles, layers):
    problems = []
    for role, uses in roles.items():
        method = module_of(role)
        if method not in layers:
            problems.append(f"roles: {role} is in unknown {method}")
            continue
        for use in uses:
            if use != method and use not in layers[method]:
                problems.append(
                    f"roles: {role} uses {use}, which {method} may not use"
                )
    return problems


def resolve(src, path, target):
    folder = path.parent.relative_to(src).as_posix()
    local = posixpath.normpath(posixpath.join(folder, target))
    return local if (src / local).is_file() else target


def includes(src, path):
    for number, line in enumerate(path.read_text().splitlines(), 1):
        match = INCLUDE.match(line)
        if match:
            yield number, resolve(src, path, match.group(1))


def role_of(path, roles):
    parts = PurePosixPath(path).parts
    role = "/".join(parts[:2])
    return role if len(parts) > 2 and role in roles else None


def stem(path):
    return PurePosixPath(path).with_suffix("")


def is_interface(path, role):
    return stem(path) == PurePosixPath(role, PurePosixPath(role).name)


def is_brick(path, roles):
    role = role_of(path, roles)
    return role is not None and not is_interface(path, role)


def is_engine(path):
    file = PurePosixPath(path)
    return len(file.parts) == 2 and file.stem == ENGINE


def refuse_sibling(role, source, target):
    if is_interface(target, role) or stem(target) == stem(source):
        return None
    return f"{role} may not use sibling {target}"


def refuse_role(role, source, target, roles):
    if role_of(target, roles) == role:
        return refuse_sibling(role, source, target)
    if is_brick(target, roles):
        return f"{role} may not use brick {target}"
    used = module_of(target)
    if used not in roles[role]:
        return f"{role} may not use {used}"
    return None


def refuse_module(module, target, layers):
    used = module_of(target)
    if used != module and used not in layers[module]:
        return f"{module} may not use {used}"
    return None


def refuse(source, target, layers, roles):
    if module_of(target) not in layers:
        return None
    role = role_of(source, roles)
    if role:
        return refuse_role(role, source, target, roles)
    if is_engine(source) and is_brick(target, roles):
        return f"engine may not use brick {target}"
    return refuse_module(module_of(source), target, layers)


def check_file(src, path, layers, roles):
    name = path.relative_to(src.parent).as_posix()
    parts = path.relative_to(src).parts
    if len(parts) < 2 or parts[0] not in layers:
        return [f"{name}: not in a declared module"]
    source = "/".join(parts)
    problems = []
    for number, target in includes(src, path):
        refusal = refuse(source, target, layers, roles)
        if refusal:
            problems.append(f"{name}:{number}: {refusal}")
    return problems


def check_sources(src, layers, roles):
    problems = []
    for path in sorted(src.rglob("*")):
        if path.suffix in SOURCES:
            problems += check_file(src, path, layers, roles)
    return problems


def check(src, layers, roles):
    tables = check_table(layers) + check_roles(roles, layers)
    return tables + check_sources(src, layers, roles)


def main():
    tools = Path(__file__).resolve().parent
    layers = read_layers(tools / "layers.txt")
    roles = read_layers(tools / "roles.txt")
    problems = check(tools.parent / "src", layers, roles)
    for problem in problems:
        print(problem)
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
