import subprocess
import sys

import pytest

NAMES = [
    "copies",
    "errors",
    "evaluation",
    "events",
    "expressions",
    "model",
    "options",
    "piecewise",
    "start",
    "threads",
]


@pytest.fixture
def examples(request):
    return request.config.rootpath / "code" / "python"


def test_listed(examples):
    assert sorted(path.stem for path in examples.glob("*.py")) == NAMES


@pytest.mark.parametrize("name", NAMES)
def test_example(examples, name):
    path = examples / f"{name}.py"
    command = [sys.executable, str(path)]
    ran = subprocess.run(command, check=False, capture_output=True, text=True)
    assert ran.returncode == 0, ran.stderr
