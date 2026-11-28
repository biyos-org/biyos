import unittest

import references

ENTRY = (
    '<div id="ref-boyd2004" class="csl-entry" role="listitem">\n'
    '<div class="csl-block">{kind}</div><div class="csl-block">'
    '<a href="https://doi.org/10.1017/CBO9780511804441">Convex '
    "Optimization</a></div>\n</div>"
)


class ReferencesPageTest(unittest.TestCase):
    def test_book(self):
        html = references.dress(ENTRY.format(kind="Book"))
        self.assertIn('<i class="bi bi-book" aria-hidden="true"></i>', html)
        self.assertIn('<div class="ref-kind">Book</div>', html)
        self.assertNotIn('<div class="csl-block">Book</div>', html)

    def test_unknown(self):
        html = references.dress(ENTRY.format(kind="Thesis"))
        self.assertIn("bi-file-earmark-text", html)
        self.assertIn('<div class="ref-kind">Thesis</div>', html)

    def test_twice(self):
        once = references.dress(ENTRY.format(kind="Book"))
        self.assertEqual(references.dress(once), once)


if __name__ == "__main__":
    unittest.main()
