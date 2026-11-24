import os
import re
from pathlib import Path

NUMBER = re.compile(r'<span class="chapter-number">[^<]*</span>&nbsp; ')
TITLE = re.compile(r"(<title>)[0-9]+&nbsp; ")
SIDEBAR = re.compile(r'<nav id="quarto-sidebar".*?</nav>', re.DOTALL)


def bare(text):
    return TITLE.sub(r"\1", NUMBER.sub("", text))


def strip(html):
    sidebar = SIDEBAR.search(html)
    if sidebar is None:
        return bare(html)
    before = bare(html[: sidebar.start()])
    after = bare(html[sidebar.end() :])
    return before + sidebar[0] + after


def main():
    book = Path(os.environ["QUARTO_PROJECT_OUTPUT_DIR"])
    for page in book.rglob("*.html"):
        page.write_text(strip(page.read_text()))


if __name__ == "__main__":
    main()
