import contextlib
import io
import tempfile
import unittest
from pathlib import Path
from unittest import mock

from check_docs import check, main

HEADER = """#pragma once

#if defined(BIYOS_STATIC)
#define BIYOS_API
#endif

#define BIYOS_OK 0

BIYOS_API int BIYOS_run(int *steps);
"""

SPEC = """group main Main

table Linking
value BIYOS_API Export.
value BIYOS_STATIC Static.
value BIYOS_OK Success.

function BIYOS_run
summary Run.
param steps out The steps.
"""

DOC = "docs/reference/capi.txt"
PAGE = "docs/reference/capi.qmd"


def missing(where, symbol):
    return [f"include/biyos/{where}: {symbol} has no entry in {DOC}"]


class DocTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name)
        self.headers = self.root / "include" / "biyos"
        self.header("biyos.h", HEADER)
        self.doc(SPEC)
        self.page('::: {source="reference/_capi/main.md"}\n:::\n')

    def tearDown(self):
        self.tmp.cleanup()

    def header(self, name, text):
        path = self.headers / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text)

    def doc(self, text):
        path = self.root / DOC
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text)

    def page(self, text):
        (self.root / PAGE).write_text(text)

    def problems(self):
        found = check(self.headers, self.root / DOC)
        return [problem.replace(f"{self.root}/", "") for problem in found]

    def test_documented(self):
        self.assertEqual(self.problems(), [])

    def test_unplaced(self):
        self.page("")
        problem = f"{PAGE}: includes no reference/_capi/main.md"
        self.assertEqual(self.problems(), [problem])

    def test_function(self):
        self.header("biyos.h", HEADER + "BIYOS_API void BIYOS_stop(void);\n")
        self.assertEqual(self.problems(), missing("biyos.h:10", "BIYOS_stop"))

    def test_wrapped(self):
        text = HEADER + "BIYOS_API int\nBIYOS_stop(int *steps,\n int n);\n"
        self.header("biyos.h", text)
        self.assertEqual(self.problems(), missing("biyos.h:11", "BIYOS_stop"))

    def test_unnamed(self):
        self.header("biyos.h", HEADER + "BIYOS_API int count;\n")
        problem = "include/biyos/biyos.h:10: BIYOS_API declares no function"
        self.assertEqual(self.problems(), [problem])

    def test_typedef(self):
        text = "typedef struct BIYOS_event BIYOS_event;\n"
        text += "typedef void (*BIYOS_hook)(const BIYOS_event *event);\n"
        self.header("biyos.h", HEADER + text)
        expected = missing("biyos.h:10", "BIYOS_event")
        expected += missing("biyos.h:11", "BIYOS_hook")
        self.assertEqual(self.problems(), expected)

    def test_macro(self):
        self.header("events.h", "#pragma once\n\n#define BIYOS_DEBUG 1\n")
        self.assertEqual(self.problems(), missing("events.h:3", "BIYOS_DEBUG"))

    def test_condition(self):
        self.header("biyos.h", HEADER + "#ifdef BIYOS_DLL\n#endif\n")
        self.assertEqual(self.problems(), missing("biyos.h:10", "BIYOS_DLL"))

    def test_enum(self):
        self.header("biyos.h", HEADER + "enum\n{\n    BIYOS_RED = 1,\n};\n")
        self.assertEqual(self.problems(), missing("biyos.h:12", "BIYOS_RED"))

    def test_lowercase(self):
        text = "#define BIYOS_max(a, b) ((a) > (b) ? (a) : (b))\n"
        self.header("biyos.h", HEADER + text)
        self.assertEqual(self.problems(), missing("biyos.h:10", "BIYOS_max"))

    def test_mixed(self):
        self.header("biyos.h", HEADER + "#define Biyos_Limit 3\n")
        self.assertEqual(self.problems(), missing("biyos.h:10", "Biyos_Limit"))

    def test_case(self):
        text = "#define BIYOS_RUN 1\nBIYOS_API void BIYOS_ok(void);\n"
        self.header("biyos.h", HEADER + text)
        expected = missing("biyos.h:10", "BIYOS_RUN")
        expected += missing("biyos.h:11", "BIYOS_ok")
        self.assertEqual(self.problems(), expected)

    def test_continued(self):
        text = "#define BIYOS_EXPORT(name) \\\n    BIYOS_API void name(void)\n"
        self.header("biyos.h", HEADER + text)
        self.doc(
            SPEC.replace(
                "value BIYOS_OK", "value BIYOS_EXPORT Export.\nvalue BIYOS_OK"
            )
        )
        self.assertEqual(self.problems(), [])

    def test_comment(self):
        text = "/* BIYOS_API void BIYOS_stop(void);\n   BIYOS_OLD */\n"
        text += "// BIYOS_GONE\n"
        self.header("biyos.h", HEADER + text)
        self.assertEqual(self.problems(), [])

    def test_lines(self):
        text = "/* one\n   two\n   end */\nBIYOS_API void BIYOS_stop(void);\n"
        self.header("biyos.h", HEADER + text)
        self.assertEqual(self.problems(), missing("biyos.h:13", "BIYOS_stop"))

    def test_literal(self):
        text = '#define BIYOS_GLOB "lib/*"\n'
        text += "BIYOS_API void BIYOS_stop(void);\n"
        text += '#define BIYOS_NOTE "BIYOS_FAKE */ // BIYOS_fake"\n'
        self.header("biyos.h", HEADER + text)
        values = (
            "value BIYOS_GLOB Glob.\nvalue BIYOS_NOTE Note.\nvalue BIYOS_OK"
        )
        self.doc(SPEC.replace("value BIYOS_OK", values))
        self.assertEqual(self.problems(), missing("biyos.h:11", "BIYOS_stop"))

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

    def test_stale(self):
        text = "\ntable Gone\nvalue BIYOS_GONE Gone.\n"
        text += "\nfunction BIYOS_old\nsummary Old.\n"
        self.doc(SPEC + text)
        stale = "has an entry but no header declares it"
        expected = [
            f"{DOC}:13: BIYOS_GONE {stale}",
            f"{DOC}:15: BIYOS_old {stale}",
        ]
        self.assertEqual(self.problems(), expected)

    def test_parameters(self):
        self.doc(SPEC.replace("param steps out", "param count out"))
        where = "include/biyos/biyos.h:9"
        problem = f"{DOC}:8: BIYOS_run parameters differ from {where}"
        self.assertEqual(self.problems(), [problem])

    def test_pair(self):
        self.pair("param at in The first.\nparam steps out The steps.\n")
        self.assertEqual(self.problems(), [])

    def test_pair_order(self):
        self.pair("param steps out The steps.\nparam at in The first.\n")
        where = "include/biyos/biyos.h:10"
        problem = f"{DOC}:8: BIYOS_runrg parameters differ from {where}"
        self.assertEqual(self.problems(), [problem])

    def test_pair_extra(self):
        self.pair(
            "param at in The first.\n"
            "param steps out The steps.\n"
            "param to in Nowhere.\n"
        )
        expected = [
            f"{DOC}:8: BIYOS_run parameters differ from "
            "include/biyos/biyos.h:9",
            f"{DOC}:8: BIYOS_runrg parameters differ from "
            "include/biyos/biyos.h:10",
        ]
        self.assertEqual(self.problems(), expected)

    def pair(self, params):
        text = HEADER + "BIYOS_API int BIYOS_runrg(int at, int *steps);\n"
        self.header("biyos.h", text)
        single = SPEC[SPEC.index("function") :]
        entry = "function BIYOS_run BIYOS_runrg\nsummary Run.\n" + params
        self.doc(SPEC.replace(single, entry))

    def test_bare(self):
        self.doc(SPEC.replace("param steps out The steps.\n", ""))
        where = "include/biyos/biyos.h:9"
        problem = f"{DOC}:8: BIYOS_run parameters differ from {where}"
        self.assertEqual(self.problems(), [problem])

    def test_void(self):
        text = HEADER + "BIYOS_API void BIYOS_stop(void);\n"
        self.header("biyos.h", text)
        self.doc(SPEC + "\nfunction BIYOS_stop\nsummary Stop.\n")
        self.assertEqual(self.problems(), [])

    def test_spacing(self):
        text = HEADER.replace(
            "BIYOS_run(int *steps);", "BIYOS_run( int * steps );"
        )
        self.header("biyos.h", text)
        self.assertEqual(self.problems(), [])

    def test_mention(self):
        self.doc(SPEC.replace("function BIYOS_run\n", "function BIYOS_walk\n"))
        stale = "has an entry but no header declares it"
        expected = missing("biyos.h:9", "BIYOS_run")
        expected.append(f"{DOC}:8: BIYOS_walk {stale}")
        self.assertEqual(self.problems(), expected)

    def test_arguments(self):
        self.page("")
        argv = [
            "check_docs.py",
            "--headers",
            str(self.headers),
            "--doc",
            str(self.root / DOC),
        ]
        output = io.StringIO()
        with mock.patch("sys.argv", argv), contextlib.redirect_stdout(output):
            self.assertEqual(main(), 1)
        page = self.root / PAGE
        problem = f"{page}: includes no reference/_capi/main.md\n"
        self.assertEqual(output.getvalue(), problem)
