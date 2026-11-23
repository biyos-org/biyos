import json
from pathlib import Path

import option_table

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "docs" / "reference" / "_options"
INFINITE = {"infinity": "∞", "-infinity": "−∞"}
HEADER = ("Macro", "Id", "Kind", "Default", "Accepts")
RULE = ("-" * 24, "-" * 4 + ":", "-" * 9, "-" * 24, "-" * 30)


def number(raw):
    if raw in INFINITE:
        return INFINITE[raw]
    value = float(raw)
    return str(int(value)) if value.is_integer() else raw


def shown(option):
    if option.kind in ("int", "double"):
        return f"`{number(option.default)}`"
    if option.kind == "flags":
        return "`(0, 0)`"
    text = option.default.strip('"')
    return f"`{text}`" if text else "empty"


def part(item):
    macro, name, bit, default = item
    return f"`{macro}` ({name}, bit {bit}, {default})"


def accepted(option):
    if option.kind in ("int", "double"):
        return f"[{number(option.lower)}, {number(option.upper)}]"
    if option.kind == "enum":
        values = option.values
        return ", ".join(f"`{macro}` ({name})" for macro, name in values)
    if option.kind == "flags":
        return ", ".join(map(part, option.values))
    return {"bool": "`true`, `false`", "string": "any string"}[option.kind]


def history(option):
    found = [f"[Since {option.versions['since']}]{{.since}}"]
    if "deprecated" in option.versions:
        found.append(f"deprecated in {option.versions['deprecated']}")
    if "retired" in option.versions:
        found.append(f"retired in {option.versions['retired']}")
    return found


def facts(option):
    cells = (
        f"[`{option.macro}`]{{.name}}",
        f"[{option.id}]{{.value}}",
        option.kind,
        f"[{shown(option)}]{{.unbroken}}",
        accepted(option),
    )
    return "\n".join(
        [
            "| " + " | ".join(map(str.ljust, HEADER, map(len, RULE))) + " |",
            "| " + " | ".join(RULE) + " |",
            "| " + " | ".join(cells) + " |",
        ]
    )


def entry(option):
    title = f"## `{option.name}` {{#opt-{option.name} .api .unnumbered}}"
    meta = " · ".join(history(option))
    body = [option.doc, facts(option)]
    body.append("::: {.meta}\n" + meta + "\n:::")
    return title + "\n\n::: {.entry}\n\n" + "\n\n".join(body) + "\n\n:::"


def main():
    options = option_table.read(ROOT)
    OUT.mkdir(exist_ok=True)
    text = "\n\n".join(entry(option) for option in options) + "\n"
    (OUT / "options.md").write_text(text)
    names = {o.name: f"reference/options.qmd#opt-{o.name}" for o in options}
    (OUT / "names.json").write_text(json.dumps(names, indent=1) + "\n")


if __name__ == "__main__":
    main()
