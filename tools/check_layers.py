import re
import sys
from pathlib import Path

INCLUDE = re.compile(r'^\s*#\s*include\s+"([^"/]+)/')
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


def check_sources(src, layers):
    problems = []
    for path in sorted(src.rglob("*")):
        if path.suffix not in SOURCES:
            continue
        name = path.relative_to(src.parent).as_posix()
        parts = path.relative_to(src).parts
        if len(parts) < 2 or parts[0] not in layers:
            problems.append(f"{name}: not in a declared module")
            continue
        module = parts[0]
        allowed = set(layers[module]) | {module}
        lines = path.read_text().splitlines()
        for number, line in enumerate(lines, 1):
            match = INCLUDE.match(line)
            if not match:
                continue
            target = match.group(1)
            if target in layers and target not in allowed:
                problems.append(
                    f"{name}:{number}: {module} may not use {target}"
                )
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
