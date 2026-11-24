from importlib.metadata import distribution


def test_expression():
    metadata = distribution("biyos").metadata
    assert metadata["License-Expression"] == "Apache-2.0"
    assert metadata.get_all("License-File") == ["LICENSE"]


def test_file(project):
    shipped = distribution("biyos").read_text("licenses/LICENSE")
    assert shipped == (project / "LICENSE").read_text()
