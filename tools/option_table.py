import re
from dataclasses import dataclass, field
from pathlib import Path

LIMITS = {
    "(std::numeric_limits<int>::min)()": "-2147483648",
    "(std::numeric_limits<int>::max)()": "2147483647",
    "-std::numeric_limits<double>::infinity()": "-infinity",
    "std::numeric_limits<double>::infinity()": "infinity",
}
RULE = re.compile(r"struct (\w+)Rule\s*\{(.*?)\};", re.S)
MEMBER = re.compile(r"\b(lower|upper) = ([^;]+);")
CONSTANT = re.compile(r"constexpr \w+ (k\w+) = ([^;]+);")
ARRAY = re.compile(r"(k\w+)\{((?:\s*(?:EnumValue|FlagPart)\{[^}]*\},?)+)\s*\}")
ITEM = re.compile(r"(?:EnumValue|FlagPart)\{([^}]*)\}")
ENUMERATOR = re.compile(r"(\w+) = (BIYOS_OPT_\w+),")
DEFINE = re.compile(r"^#define (BIYOS_OPT_\w+) (\d+)$", re.M)
BIT = re.compile(r"^#define (BIYOS_\w+) \(\(BIYOS_mask\)1 << (\d+)\)$", re.M)
FIELD = re.compile(r"\.(\w+) = ")
STRING = re.compile(r"\"([^\"]*)\"")
VERSION = re.compile(r"\.(since|deprecated|retired) = \{(\d+), (\d+), (\d+)\}")


@dataclass
class Option:
    name: str
    macro: str
    id: int
    kind: str
    doc: str
    default: str
    lower: str = ""
    upper: str = ""
    values: list = field(default_factory=list)
    versions: dict = field(default_factory=dict)
    replacement: str = ""


def blocks(text, opener):
    found = []
    start = text.find(opener)
    while start >= 0:
        depth = 0
        for end in range(start + len(opener) - 1, len(text)):
            depth += {"{": 1, "}": -1}.get(text[end], 0)
            if depth == 0:
                break
        found.append(text[start + len(opener) : end])
        start = text.find(opener, end)
    return found


def depth(text):
    return text.count("{") - text.count("}")


def fields(block):
    marks = [
        m for m in FIELD.finditer(block) if depth(block[: m.start()]) == 0
    ]
    ends = [mark.start() for mark in marks[1:]] + [len(block)]
    pairs = zip(marks, ends)
    return {
        m.group(1): block[m.end() : e].strip().rstrip(",") for m, e in pairs
    }


def item_of(block, bits):
    found = fields(block)
    name = STRING.findall(found["name"])[0]
    if "bit" not in found:
        return found["number"], name
    default = found["default_value"].split("::")[-1].lower()
    return found["bit"], name, bits[found["bit"]], default


def items_of(array, bits):
    return [item_of(block, bits) for block in ITEM.findall(array)]


def rule_limits(header):
    found = {}
    for kind, body in RULE.findall(header):
        found[kind] = {
            k: LIMITS.get(v.strip(), v.strip())
            for k, v in MEMBER.findall(body)
        }
    return found


def constant(text, constants):
    text = text.strip()
    if text in constants:
        return constants[text]
    return LIMITS.get(text, text)


def rule_of(text, limits, constants, arrays):
    kind, _, body = text.partition("Rule{")
    args = fields(body.rstrip("}"))
    values = arrays.get(args.get("values", args.get("parts", "")), [])
    bounds = {**limits.get(kind, {}), **args}
    default = constant(args.get("default_value", ""), constants)
    lower = constant(bounds.get("lower", ""), constants)
    upper = constant(bounds.get("upper", ""), constants)
    return kind.lower(), default, lower, upper, values


def versions_of(text):
    return {key: ".".join(parts) for key, *parts in VERSION.findall(text)}


def option_of(block, context):
    limits, constants, arrays, macros, ids = context
    found = fields(block)
    member = found["id"].split("::")[-1]
    kind, default, lower, upper, values = rule_of(
        found["rule"], limits, constants, arrays
    )
    return Option(
        name=STRING.findall(found["name"])[0],
        macro=macros[member],
        id=ids[macros[member]],
        kind=kind,
        doc="".join(STRING.findall(found["doc"])),
        default=default,
        lower=lower,
        upper=upper,
        values=values,
        versions=versions_of(found.get("versions", "")),
        replacement=found.get("replacement", "").split("::")[-1],
    )


def read(root):
    source = (root / "src" / "core" / "option_table.cpp").read_text()
    header = (root / "src" / "core" / "options.hpp").read_text()
    public = (root / "include" / "biyos" / "biyos.h").read_text()
    constants = {
        n: LIMITS.get(v.strip(), v.strip())
        for n, v in CONSTANT.findall(source)
    }
    bits = {name: int(bit) for name, bit in BIT.findall(public)}
    arrays = {n: items_of(v, bits) for n, v in ARRAY.findall(source)}
    macros = dict(ENUMERATOR.findall(header))
    ids = {name: int(value) for name, value in DEFINE.findall(public)}
    context = (rule_limits(header), constants, arrays, macros, ids)
    table = source[source.index("kTable{") :]
    return [
        option_of(block, context) for block in blocks(table, "OptionSpec{")
    ]
