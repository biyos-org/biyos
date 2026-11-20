import re
import sys
from pathlib import Path

INCLUDE = re.compile(r'^\s*#\s*include\s+"([^"]+)"')
SOURCES = {".c", ".h", ".cpp", ".hpp"}


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


def includes(path):
    for number, line in enumerate(path.read_text().splitlines(), 1):
        match = INCLUDE.match(line)
        if match:
            yield number, match.group(1)


def refuse(module, target, layers):
    used = target.split("/")[0]
    if used in layers and used not in layers[module] and used != module:
        return f"{module} may not use {used}"
    return None


def check_file(src, path, layers):
    name = path.relative_to(src.parent).as_posix()
    parts = path.relative_to(src).parts
    if len(parts) < 2 or parts[0] not in layers:
        return [f"{name}: not in a declared module"]
    problems = []
    for number, target in includes(path):
        refusal = refuse(parts[0], target, layers)
        if refusal:
            problems.append(f"{name}:{number}: {refusal}")
    return problems


def check_sources(src, layers):
    problems = []
    for path in sorted(src.rglob("*")):
        if path.suffix in SOURCES:
            problems += check_file(src, path, layers)
    return problems


def check(src, layers):
    return check_table(layers) + check_sources(src, layers)


def main():
    root = Path(__file__).resolve().parent.parent
    problems = check(root / "src", read_layers(root / "tools" / "layers.txt"))
    for problem in problems:
        print(problem)
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
