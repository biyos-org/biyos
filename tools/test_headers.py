import tempfile
import unittest
from pathlib import Path

import headers

LONG = (
    "BIYOS_API BIYOS_status BIYOS_fmttext(const BIYOS_event *evt, "
    "size_t size, char *buf, size_t *len);\n"
)


def declaration(size):
    head = "BIYOS_status BIYOS_edge(BIYOS_env *env, BIYOS_int *"
    return "BIYOS_API " + head + "v" * (size - len(head) - 2) + ");\n"


HEADER = """#pragma once

#define BIYOS_OK 0
#define BIYOS_ERR_NOMEMORY (-400)

typedef int BIYOS_status;
typedef void (*BIYOS_hook)(const BIYOS_event *evt, void *data);
typedef struct BIYOS_error
{
    size_t size;
    const char *msg;
} BIYOS_error;

BIYOS_API BIYOS_status BIYOS_addvar(BIYOS_model *mdl,
                                     BIYOS_index n,
                                     BIYOS_index *beg);
""" + LONG


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

    def test_struct(self):
        shown = self.declared["BIYOS_error"]
        self.assertEqual(
            shown.split("\n"),
            [
                "typedef struct BIYOS_error",
                "{",
                "    size_t size;",
                "    const char *msg;",
                "} BIYOS_error;",
            ],
        )

    def test_function(self):
        shown = self.declared["BIYOS_addvar"]
        self.assertEqual(
            shown.split("\n"),
            [
                "BIYOS_status BIYOS_addvar(BIYOS_model *mdl,",
                "                           BIYOS_index n,",
                "                           BIYOS_index *beg);",
            ],
        )

    def test_long(self):
        shown = self.declared["BIYOS_fmttext"]
        self.assertEqual(
            shown.split("\n"),
            [
                "BIYOS_status BIYOS_fmttext(const BIYOS_event *evt,",
                "                           size_t size,",
                "                           char *buf,",
                "                           size_t *len);",
            ],
        )

    def test_width(self):
        for size, count in ((76, 1), (77, 2)):
            path = Path(self.tmp.name, "biyos.h")
            path.write_text(declaration(size))
            shown = headers.read(self.tmp.name)[1]["BIYOS_edge"]
            self.assertEqual(len(shown.split("\n")), count)
