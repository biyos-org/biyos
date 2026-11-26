import sys
from fnmatch import fnmatchcase

CPP = (
    "src/*",
    "include/*",
    "tests/*",
    "examples/c/*",
    "examples/CMakeLists.txt",
    "cmake/*",
    "CMakeLists.txt",
    ".clang-tidy",
)
AREAS = {
    "cpp": CPP,
    "format": ("*.c", "*.h", "*.cpp", "*.hpp", ".clang-format"),
    "python": ("interfaces/python/*", *CPP),
    "types": ("interfaces/python/src/*",),
    "docs": (
        "docs/*",
        "tools/*",
        "include/*",
        "examples/*",
        "interfaces/python/*",
        "src/core/options/*",
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
