import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
PAGE = ROOT / "docs" / "_book" / "references.html"
ICONS = {
    "Article": "journal-text",
    "Book": "book",
    "Chapter": "bookmark",
    "Report": "file-earmark-text",
    "Standard": "patch-check",
    "Paper": "file-earmark-text",
}
KIND = re.compile(
    r'(<div id="ref-[^"]+" class="csl-entry"[^>]*>\s*)'
    r'<div class="csl-block">([^<]+)</div>'
)


def dressed(match):
    kind = match.group(2)
    icon = ICONS.get(kind, ICONS["Paper"])
    return (
        f'{match.group(1)}<span class="ref-icon">'
        f'<i class="bi bi-{icon}" aria-hidden="true"></i></span>'
        f'<div class="ref-kind">{kind}</div>'
    )


def dress(html):
    return KIND.sub(dressed, html)


def main():
    if PAGE.exists():
        PAGE.write_text(dress(PAGE.read_text()))


if __name__ == "__main__":
    main()
