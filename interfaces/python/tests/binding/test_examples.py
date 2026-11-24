import runpy

import pytest

NAMES = ["copies", "errors", "events", "model", "options", "start", "threads"]


@pytest.mark.parametrize("name", NAMES)
def test_example(project, name):
    path = project / "examples" / f"{name}.py"
    runpy.run_path(str(path), run_name="__main__")
