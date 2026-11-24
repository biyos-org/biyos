import pytest


@pytest.fixture
def project(request):
    return request.config.rootpath
