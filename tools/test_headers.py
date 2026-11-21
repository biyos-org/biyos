import tempfile
import unittest
from pathlib import Path

import headers

HEADER = """#pragma once

#define BIYOS_OK 0
#define BIYOS_ERR_NOMEMORY (-400)

typedef int BIYOS_status;
typedef void (*BIYOS_hook)(const BIYOS_event *evt, void *data);

BIYOS_API BIYOS_status BIYOS_addvars(BIYOS_model *mdl,
                                     BIYOS_index n,
                                     BIYOS_index *beg);
"""


class HeaderTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        Path(self.tmp.name, "biyos.h").write_text(HEADER)
        self.values, self.declared = headers.read(self.tmp.name)

    def tearDown(self):
        self.tmp.cleanup()

    def test_values(self):
        self.assertEqual(
            self.values, {"BIYOS_OK": 0, "BIYOS_ERR_NOMEMORY": -400}
        )

    def test_typedefs(self):
        self.assertEqual(
            self.declared["BIYOS_status"], "typedef int BIYOS_status;"
        )
        self.assertIn("(*BIYOS_hook)", self.declared["BIYOS_hook"])

    def test_function(self):
        shown = self.declared["BIYOS_addvars"]
        self.assertEqual(
            shown.split("\n"),
            [
                "BIYOS_status BIYOS_addvars(BIYOS_model *mdl,",
                "                           BIYOS_index n,",
                "                           BIYOS_index *beg);",
            ],
        )
