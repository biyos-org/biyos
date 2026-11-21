import unittest

import options_page
from option_table import Option


def option(**fields):
    base = dict(
        name="maxtime",
        macro="BIYOS_OPT_MAXTIME",
        id=2,
        kind="double",
        doc="Wall time limit in seconds.",
        default="infinity",
        lower="0.0",
        upper="infinity",
        versions={"since": "0.1.0"},
    )
    return Option(**{**base, **fields})


class OptionsPageTest(unittest.TestCase):
    def test_double(self):
        text = options_page.entry(option())
        self.assertIn("## `maxtime` {#opt-maxtime .api .unnumbered}", text)
        self.assertIn(
            "| [`BIYOS_OPT_MAXTIME`]{.name} | [2]{.value} | double "
            "| `∞` | [0, ∞] |",
            text,
        )
        self.assertIn("[Since 0.1.0]{.since}", text)

    def test_enum(self):
        level = option(
            kind="enum", default='"cheap"', values=["none", "cheap"]
        )
        self.assertEqual(options_page.shown(level), "`cheap`")
        self.assertEqual(options_page.accepted(level), "`none`, `cheap`")

    def test_string(self):
        repro = option(kind="string", default='""')
        self.assertEqual(options_page.shown(repro), "empty")
        self.assertEqual(options_page.accepted(repro), "any string")

    def test_history(self):
        old = option(versions={"since": "0.1.0", "deprecated": "0.2.0"})
        self.assertIn("deprecated in 0.2.0", options_page.history(old))

    def test_numbers(self):
        self.assertEqual(options_page.number("1e-6"), "1e-6")
        self.assertEqual(options_page.number("2147483647"), "2147483647")
        self.assertEqual(options_page.number("-infinity"), "−∞")
