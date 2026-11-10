import tempfile
import unittest
from pathlib import Path

from check_docs import check

HEADER = """#pragma once

#if defined(BIYOS_STATIC)
#define BIYOS_API
#endif

#define BIYOS_OK 0

BIYOS_API int biyos_run(int *steps);
"""

DOC = """# C API

| Macro | Use |
| --- | --- |
| `BIYOS_API` | export |
| `BIYOS_STATIC` | static |
| `BIYOS_OK` | success |

### `biyos_run`
"""


def missing(where, symbol):
    return [f"include/biyos/{where}: {symbol} has no entry in docs/capi.md"]


class DocTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name)
        self.header("biyos.h", HEADER)
        self.doc(DOC)

    def tearDown(self):
        self.tmp.cleanup()

    def header(self, name, text):
        path = self.root / "include" / "biyos" / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text)

    def doc(self, text):
        path = self.root / "docs" / "capi.md"
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text)

    def problems(self):
        return check(self.root)

    def test_documented(self):
        self.assertEqual(self.problems(), [])

    def test_function(self):
        self.header("biyos.h", HEADER + "BIYOS_API void biyos_stop(void);\n")
        self.assertEqual(self.problems(), missing("biyos.h:10", "biyos_stop"))

    def test_wrapped(self):
        text = HEADER + "BIYOS_API int\nbiyos_stop(int *steps,\n int n);\n"
        self.header("biyos.h", text)
        self.assertEqual(self.problems(), missing("biyos.h:11", "biyos_stop"))

    def test_unnamed(self):
        self.header("biyos.h", HEADER + "BIYOS_API int count;\n")
        problem = "include/biyos/biyos.h:10: BIYOS_API declares no function"
        self.assertEqual(self.problems(), [problem])

    def test_typedef(self):
        text = "typedef struct biyos_event biyos_event;\n"
        text += "typedef void (*biyos_subscriber)(const biyos_event *event);\n"
        self.header("biyos.h", HEADER + text)
        expected = missing("biyos.h:10", "biyos_event")
        expected += missing("biyos.h:11", "biyos_subscriber")
        self.assertEqual(self.problems(), expected)

    def test_macro(self):
        self.header("events.h", "#pragma once\n\n#define BIYOS_DEBUG 1\n")
        self.assertEqual(self.problems(), missing("events.h:3", "BIYOS_DEBUG"))

    def test_condition(self):
        self.header("biyos.h", "#ifdef BIYOS_SHARED\n#endif\n")
        self.assertEqual(self.problems(), missing("biyos.h:1", "BIYOS_SHARED"))

    def test_enum(self):
        self.header("biyos.h", HEADER + "enum\n{\n    BIYOS_RED = 1,\n};\n")
        self.assertEqual(self.problems(), missing("biyos.h:12", "BIYOS_RED"))

    def test_lowercase(self):
        text = "#define biyos_max(a, b) ((a) > (b) ? (a) : (b))\n"
        self.header("biyos.h", HEADER + text)
        self.assertEqual(self.problems(), missing("biyos.h:10", "biyos_max"))

    def test_mixed(self):
        self.header("biyos.h", HEADER + "#define Biyos_Limit 3\n")
        self.assertEqual(self.problems(), missing("biyos.h:10", "Biyos_Limit"))

    def test_continued(self):
        text = "#define BIYOS_EXPORT(name) \\\n    BIYOS_API void name(void)\n"
        self.header("biyos.h", HEADER + text)
        self.doc(DOC + "| `BIYOS_EXPORT` | export |\n")
        self.assertEqual(self.problems(), [])

    def test_comment(self):
        text = "/* BIYOS_API void biyos_stop(void);\n   BIYOS_OLD */\n"
        text += "// BIYOS_GONE\n"
        self.header("biyos.h", HEADER + text)
        self.assertEqual(self.problems(), [])

    def test_lines(self):
        text = "/* one\n   two\n   end */\nBIYOS_API void biyos_stop(void);\n"
        self.header("biyos.h", HEADER + text)
        self.assertEqual(self.problems(), missing("biyos.h:13", "biyos_stop"))

    def test_literal(self):
        text = '#define BIYOS_GLOB "lib/*"\n'
        text += "BIYOS_API void biyos_stop(void);\n"
        text += '#define BIYOS_NOTE "BIYOS_FAKE */ // biyos_fake"\n'
        self.header("biyos.h", HEADER + text)
        self.doc(DOC + "| `BIYOS_GLOB` | glob |\n| `BIYOS_NOTE` | note |\n")
        self.assertEqual(self.problems(), missing("biyos.h:11", "biyos_stop"))

    def test_once(self):
        text = "#ifdef BIYOS_NEW\n#endif\n#ifdef BIYOS_NEW\n#endif\n"
        self.header("biyos.h", HEADER + text)
        self.header("extra.h", "#ifdef BIYOS_NEW\n#endif\n")
        self.assertEqual(self.problems(), missing("biyos.h:10", "BIYOS_NEW"))

    def test_nested(self):
        self.header("detail/extra.h", "#define BIYOS_DEEP 1\n")
        self.assertEqual(
            self.problems(), missing("detail/extra.h:1", "BIYOS_DEEP")
        )

    def test_levels(self):
        self.header("biyos.h", HEADER + "#define BIYOS_NEW 1\n")
        self.doc(DOC.replace("### ", "## ") + "#### `BIYOS_NEW`\n")
        self.assertEqual(self.problems(), [])

    def test_indented(self):
        self.doc(DOC.replace("| `BIYOS_OK` |", "  | `BIYOS_OK` |"))
        self.assertEqual(self.problems(), [])

    def test_mention(self):
        prose = "Call `biyos_run` first.\n"
        code = "```c\n# `biyos_run`\n```\n"
        cell = "| Step | Call |\n| 1 | `biyos_run` |\n"
        self.doc(DOC.replace("### `biyos_run`\n", prose + code + cell))
        self.assertEqual(self.problems(), missing("biyos.h:9", "biyos_run"))


if __name__ == "__main__":
    unittest.main()
