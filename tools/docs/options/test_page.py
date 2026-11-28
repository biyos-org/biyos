import unittest
from pathlib import Path

import page
from table import Option


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
    def test_targets(self):
        found = page.targets([option()], Path("reference"))
        self.assertEqual(
            found, {"maxtime": "reference/options.qmd#opt-maxtime"}
        )

    def test_double(self):
        text = page.entry(option())
        self.assertIn("## `maxtime` {#opt-maxtime .api .unnumbered}", text)
        self.assertIn(
            "| [`BIYOS_OPT_MAXTIME`]{.name} | [2]{.value} | double "
            "| [`∞`]{.unbroken} | [0, ∞] |",
            text,
        )
        self.assertIn("[Since 0.1.0]{.since}", text)

    def test_enum(self):
        values = [("BIYOS_LVL_LOW", "low"), ("BIYOS_LVL_HIGH", "high")]
        level = option(kind="enum", default="BIYOS_LVL_HIGH", values=values)
        self.assertEqual(page.shown(level), "`BIYOS_LVL_HIGH`")
        self.assertEqual(
            page.accepted(level),
            "[`BIYOS_LVL_LOW`]{.unbroken} (low), "
            "[`BIYOS_LVL_HIGH`]{.unbroken} (high)",
        )

    def test_flags(self):
        values = [
            ("BIYOS_STEPS_DUAL", "dual", 0, "on"),
            ("BIYOS_STEPS_PROBE", "probe", 1, "off"),
            ("BIYOS_STEPS_SCALE", "scale", 2, "automatic"),
        ]
        steps = option(kind="flags", default="", values=values)
        self.assertEqual(page.shown(steps), "`(0, 0)`")
        self.assertEqual(
            page.accepted(steps),
            "[`BIYOS_STEPS_DUAL`]{.unbroken} (dual, bit 0, on), "
            "[`BIYOS_STEPS_PROBE`]{.unbroken} (probe, bit 1, off), "
            "[`BIYOS_STEPS_SCALE`]{.unbroken} (scale, bit 2, automatic)",
        )

    def test_string(self):
        repro = option(kind="string", default='""')
        self.assertEqual(page.shown(repro), "empty")
        self.assertEqual(page.accepted(repro), "any string")

    def test_history(self):
        old = option(versions={"since": "0.1.0", "deprecated": "0.2.0"})
        self.assertIn("deprecated in 0.2.0", page.history(old))

    def test_numbers(self):
        self.assertEqual(page.number("1e-6"), "1e-6")
        self.assertEqual(page.number("2147483647"), "2147483647")
        self.assertEqual(page.number("-infinity"), "−∞")
