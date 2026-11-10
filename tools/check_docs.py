import re
import sys
from pathlib import Path

DOC = "docs/capi.md"
MASKED = re.compile(
    r'"(?:\\.|[^"\\\n])*"'
    r"|'(?:\\.|[^'\\\n])*'"
    r"|/\*.*?\*/|//[^\n]*",
    re.S,
)
PUBLIC = re.compile(r"\b(?i:biyos)_\w+")
STATEMENT = re.compile(r"[^;{}]+")
TOKEN = re.compile(r"\w+|[^\w\s]")
HEADING = re.compile(r"#{1,6}\s")
NAME = re.compile(r"`(\w+)`")


def mask(match):
    text = match.group()
    if text[0] in "\"'":
        return text[0] * 2
    return " " + "\n" * text.count("\n")


def statements(text):
    for statement in STATEMENT.finditer(text):
        tokens = []
        for token in TOKEN.finditer(statement.group()):
            line = text.count("\n", 0, statement.start() + token.start()) + 1
            tokens.append((token.group(), line))
        if tokens:
            yield tokens


def named(tokens):
    words = [word for word, _ in tokens]
    if "(" not in words:
        return None
    at = words.index("(")
    if at == 0 or not words[at - 1].isidentifier():
        return None
    return tokens[at - 1]


def scan(path):
    lines = MASKED.sub(mask, path.read_text()).split("\n")
    names = {}
    code = []
    continued = False
    for number, line in enumerate(lines, 1):
        directive = continued or line.lstrip().startswith("#")
        continued = directive and line.rstrip().endswith("\\")
        code.append("" if directive else line)
        for name in PUBLIC.findall(line):
            names.setdefault(name, number)
    unnamed = []
    for tokens in statements("\n".join(code)):
        words = [word for word, _ in tokens]
        if "BIYOS_API" not in words:
            continue
        name = named(tokens)
        if name is None:
            unnamed.append(tokens[words.index("BIYOS_API")][1])
        else:
            names.setdefault(name[0], name[1])
    return names, unnamed


def entries(path):
    names = set()
    fenced = False
    for line in path.read_text().splitlines():
        line = line.strip()
        if line.startswith("```"):
            fenced = not fenced
        elif fenced:
            continue
        elif HEADING.match(line):
            names.update(NAME.findall(line))
        elif line.startswith("|"):
            names.update(NAME.findall(line.split("|")[1]))
    return names


def check(root):
    documented = entries(root / DOC)
    problems = []
    seen = set()
    for path in sorted((root / "include" / "biyos").rglob("*.h")):
        header = path.relative_to(root).as_posix()
        names, unnamed = scan(path)
        found = [(n, "BIYOS_API declares no function") for n in unnamed]
        for name, number in names.items():
            if name in documented or name in seen:
                continue
            seen.add(name)
            found.append((number, f"{name} has no entry in {DOC}"))
        problems += [f"{header}:{n}: {text}" for n, text in sorted(found)]
    return problems


def main():
    problems = check(Path(__file__).resolve().parent.parent)
    for problem in problems:
        print(problem)
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
