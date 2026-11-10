from importlib.metadata import distribution
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]


def test_expression():
    metadata = distribution("biyos").metadata
    assert metadata["License-Expression"] == "Apache-2.0"
    assert metadata.get_all("License-File") == ["LICENSE"]


def test_file():
    shipped = distribution("biyos").read_text("licenses/LICENSE")
    assert shipped == (ROOT / "LICENSE").read_text()
