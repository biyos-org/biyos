import contextlib
import io
import tempfile
import unittest
from pathlib import Path
from unittest import mock

from layers import check, main, read_layers


class LayerTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name)
        self.sources = self.root / "src"
        self.table_path = self.root / "layers.txt"
        self.table("core:\ntree: core\n")

    def tearDown(self):
        self.tmp.cleanup()

    def table(self, text):
        self.table_path.write_text(text)
        self.layers = read_layers(self.table_path)

    def source(self, name, text):
        path = self.sources / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text)

    def problems(self):
        found = check(self.sources, self.layers, {})
        return [problem.replace(f"{self.root}/", "") for problem in found]

    def test_allowed(self):
        self.source("tree/node.hpp", '#include "core/error.hpp"\n')
        self.source("core/error.cpp", '#include "core/error.hpp"\n')
        self.assertEqual(self.problems(), [])

    def test_forbidden(self):
        self.source("core/error.cpp", '#include "tree/node.hpp"\n')
        self.assertEqual(
            self.problems(), ["src/core/error.cpp:1: core may not use tree"]
        )

    def test_relative(self):
        self.source("tree/node.hpp", "")
        self.source("core/error.cpp", '#include "../tree/node.hpp"\n')
        self.assertEqual(
            self.problems(), ["src/core/error.cpp:1: core may not use tree"]
        )

    def test_external(self):
        text = '#include "biyos/biyos.h"\n#include <map>\n'
        self.source("core/error.cpp", text)
        self.assertEqual(self.problems(), [])

    def test_c_name(self):
        self.source("core/error.hpp", "BIYOS_status status_of();\n")
        self.assertEqual(
            self.problems(),
            ["src/core/error.hpp:1: core may not use BIYOS_status"],
        )

    def test_c_constant(self):
        self.source("core/error.cpp", "int code = BIYOS_ERR_BADARG;\n")
        self.assertEqual(self.problems(), [])

    def test_c_name_in_capi(self):
        self.table("core:\ncapi: core\n")
        self.source("capi/env.cpp", "BIYOS_status BIYOS_newenv();\n")
        self.assertEqual(self.problems(), [])

    def test_undeclared(self):
        self.source("cones/soc.cpp", "")
        self.assertEqual(
            self.problems(), ["src/cones/soc.cpp: not in a declared module"]
        )

    def test_order(self):
        self.table("core: tree\ntree:\n")
        self.assertEqual(
            self.problems(), ["layers: core uses tree, declared after it"]
        )

    def test_unknown(self):
        self.table("core: graph\n")
        self.assertEqual(self.problems(), ["layers: core uses unknown graph"])

    def test_nested(self):
        self.sources = self.root / "library" / "src"
        self.source("core/error.cpp", '#include "tree/node.hpp"\n')
        self.assertEqual(
            self.problems(),
            ["library/src/core/error.cpp:1: core may not use tree"],
        )

    def test_arguments(self):
        self.source("core/error.cpp", '#include "tree/node.hpp"\n')
        roles = self.root / "roles.txt"
        roles.write_text("")
        argv = [
            "layers.py",
            "--sources",
            str(self.sources),
            "--layers",
            str(self.table_path),
            "--roles",
            str(roles),
        ]
        output = io.StringIO()
        with mock.patch("sys.argv", argv), contextlib.redirect_stdout(output):
            self.assertEqual(main(), 1)
        problem = f"{self.sources}/core/error.cpp:1: core may not use tree\n"
        self.assertEqual(output.getvalue(), problem)


if __name__ == "__main__":
    unittest.main()
