import json
import re
import sys
from pathlib import Path

import headers
import spec

ROOT = Path(__file__).resolve().parent.parent
SPEC = ROOT / "docs" / "reference" / "capi.txt"
INCLUDE = ROOT / "include" / "biyos"
OUT = ROOT / "docs" / "reference" / "_capi"
PAGES = {"codes": "codes", "sent": "codes"}
DIRECTIONS = {"in": "in", "out": "out", "inout": "in/out"}
ARGUMENT = re.compile(r"(\w+)\s*[,)]")


def page_of(group):
    return PAGES.get(group.key, "capi")


def cell(text):
    return text.replace("|", "\\|")


COLUMNS = 80


def rule(dashes, align):
    return f" {dashes[:-1]}: |" if align == "r" else f" {dashes} |"


def table(header, rows, weights=None, aligns=None):
    weights = weights or [1] * len(header)
    aligns = aligns or "l" * len(header)
    unit = COLUMNS // sum(weights) + 1
    dashes = ["-" * unit * weight for weight in weights]
    cells = [name.ljust(len(rule)) for name, rule in zip(header, dashes)]
    lines = ["| " + " | ".join(cells) + " |"]
    lines.append("|" + "".join(map(rule, dashes, aligns)))
    lines += ["| " + " | ".join(map(cell, row)) + " |" for row in rows]
    return "\n".join(lines)


def warn(entry, text):
    print(f"WARNING: {entry.name}: {text}", file=sys.stderr)


def checked(entry, shown):
    arguments = ARGUMENT.findall(shown.split("(", 1)[-1])
    if entry.kind == "type":
        arguments = ARGUMENT.findall(shown.rsplit("(", 1)[-1])
    named = [name for name, _, _ in entry.params]
    if named and named != arguments:
        warn(entry, f"parameters {named} against {arguments}")


def signature(entry, declarations):
    shown = declarations.get(entry.name)
    if shown is None:
        warn(entry, "no declaration")
        return []
    checked(entry, shown)
    return ["```c\n" + shown + "\n```"]


def name(text, anchor=""):
    label = f"#{anchor} " if anchor else ""
    return f"[`{text}`]{{{label}.name}}"


def direction(key):
    return f"[{DIRECTIONS[key]}]{{.dir .dir-{key}}}"


def parameters(entry):
    if not entry.params:
        return []
    rows = [(name(n), direction(d), t) for n, d, t in entry.params]
    return [table(("Parameter", "", "Meaning"), rows, (2, 1, 9))]


def errors(entry):
    if not entry.errors:
        return []
    rows = [(f"`{code}`", text) for code, text in entry.errors]
    return [table(("Error", "When"), rows, (4, 8))]


def meta(entry):
    parts = [f"[Since {entry.since}]{{.since}}"] if entry.since else []
    if entry.see:
        names = ", ".join(f"`{name}`" for name in entry.see)
        parts.append(f"[See also]{{.see}} [{names}]{{.names}}")
    return ["::: {.meta}\n" + "\n".join(parts) + "\n:::"] if parts else []


def declared(entry, declarations):
    title = f"### `{entry.name}` {{#{entry.name} .api .unnumbered .unlisted}}"
    body = [entry.summary, *signature(entry, declarations)]
    body += parameters(entry)
    if entry.returns:
        body.append(f"**Returns** {entry.returns}")
    body += errors(entry) + entry.notes + meta(entry)
    return [title, "::: {.entry}\n\n" + "\n\n".join(body) + "\n\n:::"]


def listed(entry, values):
    known = all(name in values for name, *_ in entry.values)
    header = ("Name", "Value", "Meaning") if known else ("Name", "Meaning")
    weights = (4, 1, 8) if known else (3, 8)
    aligns = "lrl" if known else "ll"
    rows = []
    for key, text, _ in entry.values:
        label = name(key, key)
        shown = f"[{values.get(key)}]{{.value}}"
        rows.append((label, shown, text) if known else (label, text))
    since = f"\n\n[Since {entry.since}]{{.since}}" if entry.since else ""
    caption = f"\n\n: {entry.summary or entry.name}"
    return [table(header, rows, weights, aligns) + caption + since]


def index(group):
    rows = [
        (f"`{e.name}`", e.summary) for e in group.entries if e.kind != "table"
    ]
    return [table(("Name", "Summary"), rows, (1, 2))] if len(rows) > 1 else []


def render(group, values, declarations):
    parts = [f"## {group.title} {{#sec-capi-{group.key}}}", *group.texts]
    parts += index(group)
    for entry in group.entries:
        if entry.kind == "table":
            parts += listed(entry, values)
        else:
            parts += declared(entry, declarations)
    return "\n\n".join(parts) + "\n"


def targets(groups):
    found = spec.names(groups)
    return {
        name: f"reference/{page_of(group)}.qmd#{name}"
        for name, (group, _) in found.items()
    }


def main():
    groups = spec.read(SPEC)
    values, declarations = headers.read(INCLUDE)
    OUT.mkdir(exist_ok=True)
    for group in groups:
        (OUT / f"{group.key}.md").write_text(
            render(group, values, declarations)
        )
    (OUT / "names.json").write_text(
        json.dumps(targets(groups), indent=1) + "\n"
    )


if __name__ == "__main__":
    main()
