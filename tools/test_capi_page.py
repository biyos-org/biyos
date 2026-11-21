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
            "```c\nBIYOS_status BIYOS_freeenv(BIYOS_env *env);\n```", text
        )
        self.assertIn("| `env` | in | The environment. |", text)
        self.assertIn("**Returns** `BIYOS_OK`.", text)
        self.assertIn(
            "| `BIYOS_ERR_BADARG` | It delivers an event. |", text
        )
        self.assertIn("[See also]{.see} [`BIYOS_newenv`]{.names}", text)

    def test_table(self):
        text = self.render(table())
        self.assertIn(
            "| [`BIYOS_ERR_X`]{#BIYOS_ERR_X} | -100 | Bad \\| worse. |", text
        )
        self.assertIn(": Codes", text)

    def test_missing(self):
        entry = function()
        entry.name = "BIYOS_gone"
        with redirect_stderr(io.StringIO()) as err:
            text = self.render(entry)
        self.assertNotIn("```c", text)
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
