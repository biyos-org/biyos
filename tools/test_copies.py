import tempfile
import unittest
from pathlib import Path

from check_copies import check

LICENCE = "Apache License\nVersion 2.0\n"
LIBRARY = "project(biyos VERSION 0.1.0 LANGUAGES C CXX)\n"
EXAMPLES = "find_package(biyos 0.1.0 EXACT CONFIG REQUIRED)\n"
PYTHON = '[project]\nname = "biyos"\nversion = "0.1.0"\n'
FORMAT = (
    "BasedOnStyle: LLVM\n"
    "IndentWidth: 4\n"
    "ColumnLimit: 100\n"
    "StatementAttributeLikeMacros: [BIYOS_API]\n"
)


class CopyTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        root = Path(self.tmp.name)
        self.licence = root / "LICENSE"
        self.copy = root / "python" / "LICENSE"
        self.format = root / ".clang-format"
        self.format_copy = root / "examples" / ".clang-format"
        self.library = root / "CMakeLists.txt"
        self.examples = root / "examples" / "CMakeLists.txt"
        self.python = root / "python" / "pyproject.toml"
        self.write(self.licence, LICENCE)
        self.write(self.copy, LICENCE)
        self.write(self.format, FORMAT)
        self.write(self.format_copy, FORMAT)
        self.write(self.library, LIBRARY)
        self.write(self.examples, EXAMPLES)
        self.write(self.python, PYTHON)

    def tearDown(self):
        self.tmp.cleanup()

    def write(self, path, text):
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text)

    def problems(self):
        return check(
            self.licence,
            [self.licence, self.copy],
            self.format,
            [self.format_copy],
            self.library,
            self.examples,
            self.python,
        )

    def test_agreeing(self):
        self.assertEqual(self.problems(), [])

    def test_edited_copy(self):
        self.write(self.copy, LICENCE + " ")
        self.assertEqual(
            self.problems(), [f"{self.copy}: differs from {self.licence}"]
        )

    def test_missing_copy(self):
        self.copy.unlink()
        self.assertEqual(self.problems(), [f"{self.copy}: missing"])

    def test_column_limit(self):
        self.write(self.format_copy, FORMAT.replace("100", "76"))
        self.assertEqual(self.problems(), [])

    def test_format_value(self):
        self.write(self.format_copy, FORMAT.replace("Width: 4", "Width: 2"))
        self.assertEqual(
            self.problems(),
            [f"{self.format_copy}: IndentWidth 2, {self.format} has 4"],
        )

    def test_format_missing_key(self):
        self.write(self.format_copy, FORMAT.replace("IndentWidth: 4\n", ""))
        self.assertEqual(
            self.problems(),
            [f"{self.format_copy}: IndentWidth missing, {self.format} has 4"],
        )

    def test_format_extra_key(self):
        self.write(self.format_copy, FORMAT + "InsertBraces: true\n")
        self.assertEqual(
            self.problems(),
            [f"{self.format_copy}: InsertBraces true, {self.format} has none"],
        )

    def test_missing_format_copy(self):
        self.format_copy.unlink()
        self.assertEqual(self.problems(), [f"{self.format_copy}: missing"])

    def test_examples_version(self):
        self.write(self.examples, EXAMPLES.replace("0.1.0", "0.2"))
        self.assertEqual(
            self.problems(),
            [f"{self.examples}: version 0.2, {self.library} has 0.1.0"],
        )

    def test_python_version(self):
        self.write(self.python, PYTHON.replace("0.1.0", "0.1.1"))
        self.assertEqual(
            self.problems(),
            [f"{self.python}: version 0.1.1, {self.library} has 0.1.0"],
        )

    def test_missing_version(self):
        self.write(self.python, '[project]\nname = "biyos"\n')
        self.assertEqual(self.problems(), [f"{self.python}: names no version"])

    def test_library_version(self):
        self.write(self.library, LIBRARY.replace("0.1.0", "0.3.0"))
        self.assertEqual(
            self.problems(),
            [
                f"{self.examples}: version 0.1.0, {self.library} has 0.3.0",
                f"{self.python}: version 0.1.0, {self.library} has 0.3.0",
            ],
        )

    def test_library_unversioned(self):
        self.write(self.library, "project(biyos LANGUAGES C CXX)\n")
        self.assertEqual(
            self.problems(), [f"{self.library}: names no version"]
        )


if __name__ == "__main__":
    unittest.main()
