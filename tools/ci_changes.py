import sys
from fnmatch import fnmatchcase

CPP = (
    "library/src/*",
    "library/include/*",
    "library/tests/*",
    "examples/c/*",
    "examples/CMakeLists.txt",
    "library/cmake/*",
    "library/CMakeLists.txt",
    "library/.clang-tidy",
)
AREAS = {
    "cpp": CPP,
    "format": ("*.c", "*.h", "*.cpp", "*.hpp", "library/.clang-format"),
    "python": ("interfaces/python/*", *CPP),
    "types": (
        "interfaces/python/src/*",
        "interfaces/python/tests/binding/forms.py",
    ),
    "docs": (
        "docs/*",
        "tools/*",
        "library/tools/*",
        "library/include/*",
        "examples/*",
        "interfaces/python/*",
        "library/src/core/options/*",
    ),
}
EVERYTHING = (".github/*",)


def touches(paths, patterns):
    return any(fnmatchcase(path, p) for path in paths for p in patterns)


def flags(paths, everything=False):
    everything = everything or touches(paths, EVERYTHING)
    return {
        area: everything or touches(paths, patterns)
        for area, patterns in AREAS.items()
    }


def main():
    paths = sys.stdin.read().splitlines()
    for area, on in flags(paths, "--all" in sys.argv[1:]).items():
        print(f"{area}={str(on).lower()}")


if __name__ == "__main__":
    main()
