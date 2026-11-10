import tempfile
import unittest
from pathlib import Path

from check_layers import check, read_layers


class LayerTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name)
        self.table("core:\ntree: core\n")

    def tearDown(self):
        self.tmp.cleanup()

    def table(self, text):
        path = self.root / "layers.txt"
        path.write_text(text)
        self.layers = read_layers(path)

    def source(self, name, text):
        path = self.root / "src" / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text)

    def problems(self):
        return check(self.root / "src", self.layers)

    def test_allowed(self):
        self.source("tree/node.hpp", '#include "core/error.hpp"\n')
        self.source("core/error.cpp", '#include "core/error.hpp"\n')
        self.assertEqual(self.problems(), [])

    def test_forbidden(self):
        self.source("core/error.cpp", '#include "tree/node.hpp"\n')
        self.assertEqual(
            self.problems(), ["src/core/error.cpp:1: core may not use tree"]
        )

    def test_external(self):
        text = '#include "biyos/biyos.h"\n#include <map>\n'
        self.source("core/error.cpp", text)
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


if __name__ == "__main__":
    unittest.main()
