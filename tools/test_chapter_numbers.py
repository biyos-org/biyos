import unittest

import chapter_numbers

NUMBER = '<span class="chapter-number">6</span>&nbsp; '
NAME = '<span class="chapter-title">Errors</span>'
SIDEBAR = f'<nav id="quarto-sidebar"><a>{NUMBER}{NAME}</a></nav>'
PAGE = (
    "<title>6&nbsp; Errors – biyos</title>"
    f'<li class="breadcrumb-item">{NUMBER}{NAME}</li>'
    f"{SIDEBAR}"
    f'<h1 class="title">{NUMBER}{NAME}</h1>'
)


class ChapterNumberTest(unittest.TestCase):
    def test_title(self):
        html = chapter_numbers.strip(PAGE)
        self.assertIn("<title>Errors – biyos</title>", html)
        self.assertIn(f'<h1 class="title">{NAME}</h1>', html)

    def test_breadcrumb(self):
        html = chapter_numbers.strip(PAGE)
        self.assertIn(f'<li class="breadcrumb-item">{NAME}</li>', html)

    def test_sidebar(self):
        html = chapter_numbers.strip(PAGE)
        self.assertIn(SIDEBAR, html)
        self.assertEqual(html.count(NUMBER), 1)

    def test_twice(self):
        once = chapter_numbers.strip(PAGE)
        self.assertEqual(chapter_numbers.strip(once), once)


if __name__ == "__main__":
    unittest.main()
