import tempfile
import unittest
from pathlib import Path

from check_layers import check, read_layers

LAYERS = """core:
linalg: core
model: core
ipm: core linalg
"""

ROLES = """linalg/factor: linalg core
ipm/barrier: ipm core linalg
ipm/kkt: ipm core linalg
"""

LAYOUT = {
    "linalg/sparse.hpp": ["core/error.hpp"],
    "linalg/factor/factor.hpp": ["linalg/sparse.hpp"],
    "linalg/factor/ldlt.hpp": ["linalg/factor/factor.hpp"],
    "linalg/factor/ldlt.cpp": ["linalg/factor/ldlt.hpp", "core/error.hpp"],
    "ipm/form/blocks.hpp": ["linalg/sparse.hpp"],
    "ipm/barrier/barrier.hpp": ["ipm/form/blocks.hpp"],
    "ipm/barrier/mehrotra.hpp": ["ipm/barrier/barrier.hpp"],
    "ipm/barrier/mehrotra.cpp": [
        "ipm/barrier/mehrotra.hpp",
        "ipm/form/blocks.hpp",
    ],
    "ipm/kkt/kkt.hpp": ["ipm/form/blocks.hpp", "linalg/factor/factor.hpp"],
    "ipm/kkt/augmented.cpp": ["ipm/kkt/kkt.hpp", "linalg/factor/factor.hpp"],
    "ipm/engine.hpp": ["ipm/barrier/barrier.hpp", "ipm/kkt/kkt.hpp"],
    "ipm/engine.cpp": ["ipm/engine.hpp", "linalg/factor/factor.hpp"],
}


def lines(targets):
    return "".join(f'#include "{target}"\n' for target in targets)


class RoleTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name)
        self.layers = self.table("layers.txt", LAYERS)
        self.roles = self.table("roles.txt", ROLES)

    def tearDown(self):
        self.tmp.cleanup()

    def table(self, name, text):
        path = self.root / name
        path.write_text(text)
        return read_layers(path)

    def source(self, name, *targets):
        path = self.root / "src" / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(lines(targets))

    def problems(self):
        return check(self.root / "src", self.layers, self.roles)

    def test_layout(self):
        for name, targets in LAYOUT.items():
            self.source(name, *targets)
        self.assertEqual(self.problems(), [])

    def test_narrow(self):
        self.roles = self.table("roles.txt", "ipm/barrier: core\n")
        self.source(
            "ipm/barrier/mehrotra.cpp",
            "ipm/barrier/barrier.hpp",
            "linalg/sparse.hpp",
        )
        self.assertEqual(
            self.problems(),
            ["src/ipm/barrier/mehrotra.cpp:2: ipm/barrier may not use linalg"],
        )

    def test_wide(self):
        self.roles = self.table("roles.txt", "ipm/barrier: ipm model\n")
        self.assertEqual(
            self.problems(),
            ["roles: ipm/barrier uses model, which ipm may not use"],
        )

    def test_unknown(self):
        self.roles = self.table("roles.txt", "lp/pricing: lp core\n")
        self.assertEqual(
            self.problems(), ["roles: lp/pricing is in unknown lp"]
        )


if __name__ == "__main__":
    unittest.main()
