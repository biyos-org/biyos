import argparse
import os
import re
from pathlib import Path

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


def arguments():
    parser = argparse.ArgumentParser()
    parser.add_argument("--page", required=True)
    return parser.parse_args()


def main():
    book = Path(os.environ["QUARTO_PROJECT_OUTPUT_DIR"])
    page = book / arguments().page
    if page.exists():
        page.write_text(dress(page.read_text()))


if __name__ == "__main__":
    main()
