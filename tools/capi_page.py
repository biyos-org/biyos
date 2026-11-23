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


def shown_arguments(entry, shown):
    if entry.kind == "type":
        return ARGUMENT.findall(shown.rsplit("(", 1)[-1])
    return ARGUMENT.findall(shown.split("(", 1)[-1])


def checked(entry, declarations):
    shown = [declarations[name] for name in entry.names]
    found = [shown_arguments(entry, text) for text in shown]
    named = [name for name, _, _ in entry.params]
    for arguments in found:
        if named and spec.differs(named, arguments, found):
            warn(entry, f"parameters {named} against {arguments}")


def signature(entry, declarations):
    if not all(name in declarations for name in entry.names):
        warn(entry, "no declaration")
        return []
    checked(entry, declarations)
    return [
        "```{.c .signature}\n" + declarations[name] + "\n```"
        for name in entry.names
    ]


def name(text, anchor=""):
    label = f"#{anchor} " if anchor else ""
    return f"[`{text}`]{{{label}.name}}"


def direction(key):
    return f"[{DIRECTIONS[key]}]{{.dir}}"


def above(head, text):
    return f"{head} [{text}]{{.under}}"


def stack(header, rows):
    shown = table((header,), [(above(*row),) for row in rows])
    return f"::: {{.stack}}\n{shown}\n:::"


def parameters(entry):
    if not entry.params:
        return []
    rows = [(f"{name(n)} {direction(d)}", t) for n, d, t in entry.params]
    return [stack("Parameter", rows)]


def errors(entry):
    if not entry.errors:
        return []
    return [stack("Error", [(f"`{c}`", t) for c, t in entry.errors])]


def dated(entry):
    return [f"[since {entry.since}]{{.since}}"] if entry.since else []


def seen(entry):
    if not entry.see:
        return []
    names = ", ".join(f"`{name}`" for name in entry.see)
    return [f"[See also]{{.see}} [{names}]{{.names}}"]


def meta(parts):
    return ["::: {.meta}\n" + "\n".join(parts) + "\n:::"] if parts else []


def title(entry):
    names = ", ".join(f"`{name}`" for name in entry.names)
    shown = " ".join([names, *dated(entry)])
    return f"### {shown} {{#{entry.name} .api .unnumbered .unlisted}}"


def anchors(entry):
    return [f"[]{{#{name}}}" for name in entry.names[1:]]


def declared(entry, declarations):
    body = [*anchors(entry), entry.summary, *signature(entry, declarations)]
    body += parameters(entry)
    if entry.returns:
        body.append(f"**Returns** {entry.returns}")
    body += errors(entry) + entry.notes + meta(seen(entry))
    return [title(entry), "::: {.entry}\n\n" + "\n\n".join(body) + "\n\n:::"]


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
    caption = f"\n\n: {entry.summary or entry.name}"
    shown = table(header, rows, weights, aligns) + caption
    return [shown, *meta(seen(entry) + dated(entry))]


def stacked(names):
    first, *rest = names
    return " ".join([f"`{first}`", *(f"[`{n}`]{{.under}}" for n in rest)])


def index(group):
    rows = [
        (stacked(e.names), e.summary)
        for e in group.entries
        if e.kind != "table"
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
