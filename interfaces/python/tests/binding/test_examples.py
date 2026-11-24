import runpy
from operator import attrgetter
from pathlib import Path

import pytest

EXAMPLES = Path(__file__).resolve().parents[4] / "examples" / "python"
PATHS = sorted(EXAMPLES.glob("*.py"))


@pytest.mark.parametrize("path", PATHS, ids=attrgetter("stem"))
def test_example(path):
    runpy.run_path(str(path), run_name="__main__")
