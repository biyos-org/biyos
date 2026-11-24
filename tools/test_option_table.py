import tempfile
import unittest
from pathlib import Path

import option_table

TABLE = """namespace
{
constexpr std::array kLevels{
    EnumValue{.number = BIYOS_LEVEL_LOW, .name = "low"},
    EnumValue{.number = BIYOS_LEVEL_HIGH, .name = "high"},
};
constexpr std::array kSteps{
    FlagPart{
        .bit = BIYOS_STEPS_DUAL,
        .name = "dual",
        .default_value = PartDefault::On,
    },
    FlagPart{.bit = BIYOS_STEPS_SCALE, .name = "scale", .default_value = PartDefault::Automatic},
};

constexpr std::array kTable{
    OptionSpec{
        .id = Option::Seed,
        .name = "seed",
        .rule = IntRule{.default_value = 0, .lower = 0},
        .doc = "Seed of every "
               "random choice.",
        .versions = {.since = {0, 1, 0}},
    },
    OptionSpec{
        .id = Option::MaxTime,
        .name = "maxtime",
        .rule = DoubleRule{.default_value = kInfinity, .lower = 0.0},
        .doc = "Wall time limit.",
        .versions = {.since = {0, 1, 0}, .deprecated = {0, 2, 0}},
        .replacement = Option::Seed,
    },
    OptionSpec{
        .id = Option::Level,
        .name = "level",
        .rule = EnumRule{.default_value = BIYOS_LEVEL_LOW, .values = kLevels},
        .doc = "Level.",
        .versions = {.since = {0, 1, 0}},
    },
    OptionSpec{
        .id = Option::Steps,
        .name = "steps",
        .rule = FlagsRule{.parts = kSteps},
        .doc = "Steps.",
        .versions = {.since = {0, 1, 0}},
    },
};
}
"""

HEADER = """enum class Option
{
    Seed = BIYOS_OPT_SEED,
    MaxTime = BIYOS_OPT_MAXTIME,
    Level = BIYOS_OPT_LEVEL,
    Steps = BIYOS_OPT_STEPS,
};

struct IntRule
{
    int default_value;
    int lower = (std::numeric_limits<int>::min)();
    int upper = (std::numeric_limits<int>::max)();
};

struct DoubleRule
{
    double default_value;
    double lower = -kInfinity;
    double upper = kInfinity;
};
"""

PUBLIC = """#define BIYOS_OPT_SEED 1
#define BIYOS_OPT_MAXTIME 2
#define BIYOS_OPT_LEVEL 3
#define BIYOS_OPT_STEPS 4

#define BIYOS_STEPS_DUAL ((BIYOS_mask)1 << 0)
#define BIYOS_STEPS_SCALE ((BIYOS_mask)1 << 1)
"""


class OptionTableTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        root = Path(self.tmp.name)
        files = {
            "src/core/options/table.cpp": TABLE,
            "src/core/options/options.hpp": HEADER,
            "include/biyos/biyos.h": PUBLIC,
        }
        for name, text in files.items():
            (root / name).parent.mkdir(parents=True, exist_ok=True)
            (root / name).write_text(text)
        self.options = option_table.read(root)

    def tearDown(self):
        self.tmp.cleanup()

    def test_rows(self):
        names = [(o.name, o.macro, o.id, o.kind) for o in self.options]
        self.assertEqual(
            names,
            [
                ("seed", "BIYOS_OPT_SEED", 1, "int"),
                ("maxtime", "BIYOS_OPT_MAXTIME", 2, "double"),
                ("level", "BIYOS_OPT_LEVEL", 3, "enum"),
                ("steps", "BIYOS_OPT_STEPS", 4, "flags"),
            ],
        )

    def test_rules(self):
        seed, limit, level, steps = self.options
        self.assertEqual(
            (seed.default, seed.lower, seed.upper), ("0", "0", "2147483647")
        )
        self.assertEqual(
            (limit.default, limit.upper), ("infinity", "infinity")
        )
        self.assertEqual(level.default, "BIYOS_LEVEL_LOW")
        self.assertEqual(
            level.values,
            [("BIYOS_LEVEL_LOW", "low"), ("BIYOS_LEVEL_HIGH", "high")],
        )
        self.assertEqual(steps.default, "")
        self.assertEqual(
            steps.values,
            [
                ("BIYOS_STEPS_DUAL", "dual", 0, "on"),
                ("BIYOS_STEPS_SCALE", "scale", 1, "automatic"),
            ],
        )

    def test_texts(self):
        seed, limit, _, _ = self.options
        self.assertEqual(seed.doc, "Seed of every random choice.")
        self.assertEqual(
            limit.versions, {"since": "0.1.0", "deprecated": "0.2.0"}
        )
        self.assertEqual(limit.replacement, "Seed")
