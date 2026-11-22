import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import spec

DOC = "docs/reference/capi.txt"
MASKED = re.compile(
    r'"(?:\\.|[^"\\\n])*"'
    r"|'(?:\\.|[^'\\\n])*'"
    r"|/\*.*?\*/|//[^\n]*",
    re.S,
)
PUBLIC = re.compile(r"\b(?i:biyos)_\w+")
STATEMENT = re.compile(r"[^;{}]+")
TOKEN = re.compile(r"\w+|[^\w\s]")


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


def arguments(words):
    close = len(words) - 1 - words[::-1].index(")")
    pieces = [[]]
    for word in words[words.index("(") + 1 : close]:
        if word == ",":
            pieces.append([])
        else:
            pieces[-1].append(word)
    return [piece[-1] for piece in pieces if piece and piece != ["void"]]


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
    functions = {}
    unnamed = []
    for tokens in statements("\n".join(code)):
        words = [word for word, _ in tokens]
        if "BIYOS_API" not in words:
            continue
        name = named(tokens)
        if name is None:
            unnamed.append(tokens[words.index("BIYOS_API")][1])
            continue
        names.setdefault(name[0], name[1])
        words.remove("BIYOS_API")
        functions[name[0]] = (name[1], words)
    return names, functions, unnamed


def documented(path):
    names = {}
    params = {}
    for group in spec.read(path):
        for entry in group.entries:
            if entry.kind != "table":
                named_params = [name for name, _, _ in entry.params]
                for name in entry.names:
                    names.setdefault(name, entry.line)
                    params[name] = (entry.line, named_params, entry.names)
            for name, _, line in entry.values:
                names.setdefault(name, line)
    return names, params


def mismatched(params, calls):
    found = []
    for name, (header, number, words) in calls.items():
        if name not in params:
            continue
        line, named, pair = params[name]
        partners = [calls[other][2] for other in pair if other in calls]
        if spec.differs(named, words, partners):
            text = f"{name} parameters differ from {header}:{number}"
            found.append((line, text))
    return found


def check(root):
    names, params = documented(root / DOC)
    problems = []
    doc_problems = []
    declared = set()
    calls = {}
    for path in sorted((root / "include" / "biyos").rglob("*.h")):
        header = path.relative_to(root).as_posix()
        found_names, functions, unnamed = scan(path)
        found = [(n, "BIYOS_API declares no function") for n in unnamed]
        for name, number in found_names.items():
            if name not in names and name not in declared:
                found.append((number, f"{name} has no entry in {DOC}"))
            declared.add(name)
        for name, (number, words) in functions.items():
            calls[name] = (header, number, arguments(words))
        problems += [f"{header}:{n}: {text}" for n, text in sorted(found)]
    doc_problems += mismatched(params, calls)
    for name, number in names.items():
        if name not in declared:
            text = f"{name} has an entry but no header declares it"
            doc_problems.append((number, text))
    problems += [f"{DOC}:{n}: {text}" for n, text in sorted(doc_problems)]
    return problems


def main():
    problems = check(Path(__file__).resolve().parent.parent)
    for problem in problems:
        print(problem)
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
