import tempfile
import unittest
from pathlib import Path

import spec

SPEC = """group env Environment
text An environment holds the options.

type BIYOS_env
summary An environment.
since 0.1.0

function BIYOS_newenv
summary Create an environment.
param env out Receives the environment.
return `BIYOS_OK`.
error BIYOS_ERR_BADARG `env` is null.
note It keeps no message.
see BIYOS_freeenv BIYOS_env
since 0.1.0

table Codes
value BIYOS_OK The call succeeded.
since 0.1.0
"""


class SpecTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.path = Path(self.tmp.name) / "capi.txt"

    def tearDown(self):
        self.tmp.cleanup()

    def read(self, text):
        self.path.write_text(text)
        return spec.read(self.path)

    def test_groups(self):
        (group,) = self.read(SPEC)
        self.assertEqual((group.key, group.title), ("env", "Environment"))
        self.assertEqual(group.texts, ["An environment holds the options."])
        kinds = [(entry.kind, entry.name) for entry in group.entries]
        self.assertEqual(
            kinds,
            [
                ("type", "BIYOS_env"),
                ("function", "BIYOS_newenv"),
                ("table", "Codes"),
            ],
        )

    def test_fields(self):
        entry = self.read(SPEC)[0].entries[1]
        self.assertEqual(
            entry.params, [("env", "out", "Receives the environment.")]
        )
        self.assertEqual(entry.returns, "`BIYOS_OK`.")
        self.assertEqual(
            entry.errors, [("BIYOS_ERR_BADARG", "`env` is null.")]
        )
        self.assertEqual(entry.notes, ["It keeps no message."])
        self.assertEqual(entry.see, ["BIYOS_freeenv", "BIYOS_env"])
        self.assertEqual((entry.line, entry.since), (8, "0.1.0"))

    def test_values(self):
        entry = self.read(SPEC)[0].entries[2]
        self.assertEqual(
            entry.values, [("BIYOS_OK", "The call succeeded.", 18)]
        )

    def test_takes(self):
        text = SPEC.replace("succeeded.\n", "succeeded.\ntakes $c$\n")
        entry = self.read(text)[0].entries[2]
        self.assertEqual(entry.takes, {"BIYOS_OK": "$c$"})

    def test_untaken(self):
        text = SPEC.replace("value BIYOS_OK The call succeeded.", "takes $c$")
        with self.assertRaisesRegex(ValueError, "capi.txt:18: takes comes"):
            self.read(text)

    def test_names(self):
        found = spec.names(self.read(SPEC))
        self.assertEqual(
            sorted(found), ["BIYOS_OK", "BIYOS_env", "BIYOS_newenv"]
        )

    def test_pair(self):
        pair = "function BIYOS_a BIYOS_b"
        text = SPEC.replace("function BIYOS_newenv", pair)
        entry = self.read(text)[0].entries[1]
        self.assertEqual(entry.names, ["BIYOS_a", "BIYOS_b"])
        self.assertIn("BIYOS_b", spec.names(self.read(text)))

    def test_differs(self):
        named = ["mdl", "pos", "n", "idx"]
        partners = [["mdl", "n", "idx"], ["mdl", "pos", "n"]]
        self.assertFalse(spec.differs(named, partners[0], partners))
        self.assertFalse(spec.differs(named, partners[1], partners))
        self.assertTrue(spec.differs(named, ["mdl", "idx", "n"], partners))
        self.assertTrue(spec.differs(named, partners[0], partners[:1]))

    def test_direction(self):
        text = SPEC.replace("param env out", "param env sideways")
        with self.assertRaisesRegex(ValueError, "capi.txt:10: direction"):
            self.read(text)

    def test_keyword(self):
        with self.assertRaisesRegex(ValueError, "capi.txt:2: returns is not"):
            self.read("group env Environment\nreturns nothing\n")

    def test_orphan(self):
        with self.assertRaisesRegex(
            ValueError, "capi.txt:1: text comes before"
        ):
            self.read("text alone\n")
