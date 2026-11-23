import io
import unittest
from contextlib import redirect_stderr

import capi_page
from spec import Entry, Group


def function():
    entry = Entry("function", "BIYOS_freeenv", 1, summary="Free it.")
    entry.params = [("env", "in", "The environment.")]
    entry.returns = "`BIYOS_OK`."
    entry.errors = [("BIYOS_ERR_BADARG", "It delivers an event.")]
    entry.see = ["BIYOS_newenv"]
    entry.since = "0.1.0"
    return entry


def table():
    entry = Entry("table", "Codes", 9, since="0.1.0")
    entry.values = [
        ("BIYOS_OK", "Done.", 10),
        ("BIYOS_ERR_X", "Bad | worse.", 11),
    ]
    return entry


DECLARED = {"BIYOS_freeenv": "BIYOS_status BIYOS_freeenv(BIYOS_env *env);"}
VALUES = {"BIYOS_OK": 0, "BIYOS_ERR_X": -100}


class PageTest(unittest.TestCase):
    def render(self, *entries):
        group = Group("env", "Environment", ["Text."], list(entries))
        return capi_page.render(group, VALUES, DECLARED)

    def test_function(self):
        text = self.render(function())
        self.assertIn("## Environment {#sec-capi-env}", text)
        self.assertIn("### `BIYOS_freeenv` {#BIYOS_freeenv .api", text)
        self.assertIn(
            "```{.c .signature}\n"
            "BIYOS_status BIYOS_freeenv(BIYOS_env *env);\n```",
            text,
        )
        self.assertIn(
            "| [`env`]{.name} [in]{.dir .dir-in} "
            "[The environment.]{.under} |",
            text,
        )
        self.assertIn("**Returns** `BIYOS_OK`.", text)
        self.assertIn(
            "| `BIYOS_ERR_BADARG` [It delivers an event.]{.under} |", text
        )
        self.assertIn("::: {.stack}\n| Error", text)
        self.assertIn("[See also]{.see} [`BIYOS_newenv`]{.names}", text)

    def test_pair(self):
        entry = function()
        entry.pair = "BIYOS_freeenvrg"
        declared = dict(DECLARED)
        declared["BIYOS_freeenvrg"] = "BIYOS_status BIYOS_freeenvrg(int at);"
        entry.params.append(("at", "in", "The first."))
        group = Group("env", "Environment", [], [entry, table()])
        with redirect_stderr(io.StringIO()) as err:
            text = capi_page.render(group, VALUES, declared)
        self.assertEqual(err.getvalue(), "")
        heading = "### `BIYOS_freeenv`, `BIYOS_freeenvrg` {#BIYOS_freeenv "
        self.assertIn(heading, text)
        self.assertIn("[]{#BIYOS_freeenvrg}", text)
        self.assertIn("BIYOS_freeenvrg(int at);\n```", text)

    def test_index(self):
        entry = function()
        entry.pair = "BIYOS_freeenvrg"
        declared = dict(DECLARED)
        declared["BIYOS_freeenvrg"] = "BIYOS_status BIYOS_freeenvrg(int env);"
        group = Group("env", "Environment", [], [entry, function()])
        with redirect_stderr(io.StringIO()):
            text = capi_page.render(group, VALUES, declared)
        row = "| `BIYOS_freeenv` [`BIYOS_freeenvrg`]{.under} | Free it. |"
        self.assertIn(row, text)

    def test_table(self):
        text = self.render(table())
        self.assertIn(
            "| [`BIYOS_ERR_X`]{#BIYOS_ERR_X .name} | [-100]{.value} "
            "| Bad \\| worse. |",
            text,
        )
        self.assertRegex(text, r"\| -{20,} \| -+: \|")
        self.assertIn(": Codes", text)

    def test_widths(self):
        text = self.render(function(), table())
        rules = [line for line in text.split("\n") if line.startswith("| -")]
        self.assertEqual(len(rules), 3)
        for line in rules:
            self.assertGreater(len(line), 80)

    def test_missing(self):
        entry = function()
        entry.name = "BIYOS_gone"
        with redirect_stderr(io.StringIO()) as err:
            text = self.render(entry)
        self.assertNotIn("```{.c", text)
        self.assertIn("BIYOS_gone: no declaration", err.getvalue())

    def test_parameters(self):
        entry = function()
        entry.params = [("environment", "in", "The environment.")]
        with redirect_stderr(io.StringIO()) as err:
            self.render(entry)
        self.assertIn("['environment'] against ['env']", err.getvalue())

    def test_targets(self):
        groups = [
            Group("codes", "Codes", [], [table()]),
            Group("env", "E", [], [function()]),
        ]
        found = capi_page.targets(groups)
        self.assertEqual(found["BIYOS_OK"], "reference/codes.qmd#BIYOS_OK")
        self.assertEqual(
            found["BIYOS_freeenv"], "reference/capi.qmd#BIYOS_freeenv"
        )
