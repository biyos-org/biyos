import re

from griffe import (
    Alias,
    DocstringSectionAttributes,
    DocstringSectionParameters,
    DocstringSectionRaises,
    DocstringSectionReturns,
    DocstringSectionText,
)
from plum import dispatch
from quartodoc import MdRenderer, layout
from quartodoc import ast as qast

ENUMS = {"Enum", "IntEnum", "Flag", "IntFlag"}
RECORDS = {"NamedTuple"}
SPLATS = {"var_positional": "*", "var_keyword": "**"}
HANDLED = (
    DocstringSectionText,
    DocstringSectionAttributes,
    DocstringSectionParameters,
    DocstringSectionReturns,
    DocstringSectionRaises,
)


def cell(text):
    return re.sub(r"(?<!\\)\|", r"\\|", " ".join(str(text or "").split()))


WEIGHTS = {
    "Parameter": 10,
    "Type": 5,
    "Default": 2,
    "Meaning": 7,
    "Returns": 3,
    "Raises": 3,
    "Condition": 9,
    "Member": 4,
    "Attribute": 3,
    "Value": 1,
    "Method": 3,
    "Summary": 9,
}


COLUMNS = 80


def table(header, rows):
    kept = [k for k in range(len(header)) if k == 0 or any(r[k] for r in rows)]
    names = [header[k] for k in kept]
    unit = COLUMNS // sum(WEIGHTS[name] for name in names) + 1
    rules = ["-" * unit * WEIGHTS[name] for name in names]
    lines = ["| " + " | ".join(map(str.ljust, names, map(len, rules))) + " |"]
    lines.append("| " + " | ".join(rules) + " |")
    for row in rows:
        lines.append("| " + " | ".join(cell(row[k]) for k in kept) + " |")
    return "\n".join(lines)


def code(text):
    return f"`{text}`" if text else ""


def quiet(text, classes=".type"):
    shown = str(text).replace("...", "\\.\\.\\.")
    return f"[{shown}]{{{classes}}}" if text else ""


def target(obj):
    return obj.final_target if isinstance(obj, Alias) else obj


def sections(obj):
    docstring = target(obj).docstring
    return qast.transform(docstring.parsed) if docstring else []


def text_of(found):
    texts = [s.value for s in found if isinstance(s, DocstringSectionText)]
    summary, _, rest = "\n\n".join(texts).strip().partition("\n\n")
    return summary, rest


def summary_of(obj):
    return text_of(sections(obj))[0]


def described(found, kind):
    return [item for s in found if isinstance(s, kind) for item in s.value]


def above(kind, meaning):
    below = f"[{meaning}]{{.under}}" if meaning else ""
    return " ".join(part for part in (kind, below) if part)


def stack(header, rows):
    shown = table((header,), [(above(*row),) for row in rows])
    return f"::: {{.stack}}\n{shown}\n:::"


def headed(name, kind, default):
    given = f"[=]{{.type}} {code(default)}" if default else ""
    return " ".join(part for part in (name, kind, given) if part)


def merged(rows):
    out = []
    for row in rows:
        if out and row[1:] == out[-1][1:] and row[3]:
            out[-1] = (out[-1][0] + ", " + row[0], *row[1:])
        else:
            out.append(row)
    return out


def see_names(section):
    value = getattr(section, "value", "")
    text = getattr(value, "description", value)
    return ", ".join(f"`{n}`" for n in text.replace(",", " ").split())


def see_also(section):
    names = see_names(section)
    return f"::: {{.meta}}\n[See also]{{.see}} [{names}]{{.names}}\n:::"


def see_inline(obj):
    found = [s for s in sections(obj) if is_see_also(s)]
    lines = [f"[See also]{{.see}} [{see_names(s)}]{{.names}}" for s in found]
    return "".join(f" [{line}]{{.also}}" for line in lines)


def is_see_also(section):
    return (section.title or "").lower() == "see also" or isinstance(
        section, qast.DocstringSectionSeeAlso
    )


def public(cls):
    found = sections(cls)
    documented = described(found, DocstringSectionParameters)
    taken = getattr(target(cls), "parameters", [])
    return bool(documented) or not [p for p in taken if p.name != "self"]


def returned(given, count):
    if not getattr(given, "is_tuple", False):
        return [given] * count
    elements = getattr(given.slice, "elements", [given.slice])
    return elements if len(elements) == count else [given] * count


def is_enum(obj):
    bases = getattr(target(obj), "bases", [])
    return any(str(base).split(".")[-1] in ENUMS for base in bases)


def is_record(obj):
    bases = getattr(target(obj), "bases", [])
    return any(str(base).split(".")[-1] in RECORDS for base in bases)


class Renderer(MdRenderer):
    style = "biyos"

    def heading(self, doc, text, classes=""):
        level = "#" * self.crnt_header_level
        return f"{level} {text} {{#{doc.obj.path} .api{classes}}}"

    def shown(self, doc):
        return self.signature(doc).replace(
            "```python", "```{.python .signature}"
        )

    def parameters(self, obj, found):
        notes = {
            p.name: p for p in described(found, DocstringSectionParameters)
        }
        if not hasattr(target(obj), "parameters"):
            return []
        rows = []
        for par in self._fetch_method_parameters(target(obj)):
            note = notes.get(par.name)
            splat = SPLATS.get(par.kind.value, "")
            default = None if splat else par.default
            anno = par.annotation or (note.annotation if note else None)
            rows.append(
                (
                    f"[`{splat}{par.name}`]{{.name}}",
                    quiet(self.render_annotation(anno) if anno else ""),
                    default,
                    note.description if note else "",
                )
            )
        shown = [(headed(*row[:3]), row[3]) for row in merged(rows)]
        return [stack("Parameter", shown)] if rows else []

    def returns(self, obj, found):
        given = getattr(target(obj), "returns", None)
        notes = described(found, DocstringSectionReturns)
        rows = [
            (
                quiet(self.render_annotation(part or r.annotation)),
                r.description,
            )
            for part, r in zip(returned(given, len(notes)), notes)
        ]
        if given and not rows and str(given) != "None":
            rows = [(quiet(self.render_annotation(given)), "")]
        return [stack("Returns", rows)] if rows else []

    def raises(self, found):
        rows = [
            (code(self.render_annotation(r.annotation)), r.description)
            for r in described(found, DocstringSectionRaises)
        ]
        return [stack("Raises", rows)] if rows else []

    def others(self, found):
        parts = []
        for section in found:
            if is_see_also(section):
                continue
            if not isinstance(section, HANDLED):
                title = section.title or section.kind.value.title()
                parts.append(f"**{title}**\n\n{super().render(section)}")
        return parts

    def entry(self, doc, signature, members=()):
        found = sections(doc.obj)
        summary, rest = text_of(found)
        body = [summary, *signature, rest]
        if signature:
            body += self.parameters(doc.obj, found)
        body += self.returns(doc.obj, found) + self.raises(found)
        body += self.others(found) + list(members)
        body += [see_also(s) for s in found if is_see_also(s)]
        kept = "\n\n".join(part for part in body if part)
        return "::: {.entry}\n\n" + kept + "\n\n:::"

    def attributes(self, owner, docs, enum):
        notes = {
            a.name: a.description
            for a in described(sections(owner), DocstringSectionAttributes)
        }
        rows = [
            (
                f"[`{d.obj.name}`]{{#{d.obj.path} .name}}",
                quiet(self.render_annotation(target(d.obj).annotation)),
                f"[{target(d.obj).value}]{{.value}}" if enum else "",
                (notes.get(d.obj.name) or summary_of(d.obj))
                + see_inline(d.obj),
            )
            for d in docs
        ]
        if not rows:
            return []
        if not enum:
            shown = [(headed(*row[:2], ""), row[3]) for row in rows]
            return [stack("Attribute", shown)]
        return [table(("Member", "Type", "Value", "Meaning"), rows)]

    def index(self, docs):
        rows = [
            (f"[[`{d.obj.name}`](#{d.obj.path})]{{.name}}", summary_of(d.obj))
            for d in docs
        ]
        return [table(("Method", "Summary"), rows)] if rows else []

    @dispatch
    def render(self, el: layout.Section):
        slug = "-".join(el.title.lower().split())
        title = (
            f"{'#' * self.crnt_header_level} {el.title} {{#sec-python-{slug}}}"
        )
        with self._increment_header():
            body = [self.render(doc) for doc in el.contents]
        return "\n\n".join([title, el.desc or "", *body])

    @dispatch
    def render(self, el: layout.DocClass):
        enum = is_enum(el.obj)
        members = [m for m in el.members if isinstance(m, layout.Doc)]
        attrs = [m for m in members if m.obj.is_attribute]
        methods = [m for m in members if m.obj.is_function]
        visible = public(el.obj) and not enum and not is_record(el.obj)
        signature = [self.shown(el)] if visible else []
        listed = self.attributes(el.obj, attrs, enum) + self.index(methods)
        parts = [self.heading(el, f"`{el.name}`", " .class")]
        parts.append(self.entry(el, signature, listed))
        with self._increment_header():
            parts += [self.render(m) for m in methods]
        return "\n\n".join(parts)

    @dispatch
    def render(self, el: layout.DocFunction):
        owner, _, name = el.obj.path.removeprefix("biyos.").rpartition(".")
        text = f"[`{owner}.`]{{.owner}}`{name}`" if owner else f"`{name}`"
        return (
            self.heading(el, text) + "\n\n" + self.entry(el, [self.shown(el)])
        )

    @dispatch
    def render(self, el: layout.DocAttribute):
        value = target(el.obj).value
        shown = f"```{{.python .signature}}\n{el.name} = {value}\n```"
        signature = [shown] if value else []
        heading = self.heading(el, f"`{el.name}`")
        return heading + "\n\n" + self.entry(el, signature)
