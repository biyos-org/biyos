import unittest

from ci_changes import AREAS, flags


def on(paths, everything=False):
    return {area for area, value in flags(paths, everything).items() if value}


class ChangeTest(unittest.TestCase):
    def test_docs(self):
        self.assertEqual(on(["docs/index.qmd"]), {"docs"})

    def test_source(self):
        self.assertEqual(
            on(["src/expr/graph/node.cpp"]), {"cpp", "format", "python"}
        )

    def test_examples(self):
        self.assertEqual(
            on(["examples/CMakeLists.txt"]), {"cpp", "python", "docs"}
        )

    def test_options(self):
        self.assertIn("docs", on(["src/core/options/table.cpp"]))

    def test_header(self):
        self.assertEqual(
            on(["include/biyos/biyos.h"]),
            {"cpp", "format", "python", "docs"},
        )

    def test_package(self):
        self.assertEqual(
            on(["interfaces/python/src/biyos/_capi.py"]),
            {"python", "types", "docs"},
        )

    def test_python_tests(self):
        self.assertEqual(
            on(["interfaces/python/tests/env/test_env.py"]),
            {"python", "docs"},
        )

    def test_probe(self):
        self.assertEqual(
            on(["interfaces/python/tests/binding/forms.py"]),
            {"python", "types", "docs"},
        )

    def test_workflow(self):
        self.assertEqual(on([".github/workflows/ci.yml"]), set(AREAS))

    def test_forced(self):
        self.assertEqual(on([], everything=True), set(AREAS))

    def test_readme(self):
        self.assertEqual(on(["README.md"]), set())


if __name__ == "__main__":
    unittest.main()
