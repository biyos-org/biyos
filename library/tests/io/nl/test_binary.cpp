#include "bytes.hpp"
#include "files.hpp"
#include "loading.hpp"

#include "core/numbers.hpp"
#include "model/compare/compare.hpp"
#include "model/model.hpp"

#include <gtest/gtest.h>

#include <array>
#include <bit>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

using biyos::difference;
using biyos::Index;
using biyos::Model;
using biyos::test::nl::arith_of;
using biyos::test::nl::bad;
using biyos::test::nl::bad_at;
using biyos::test::nl::binary_header;
using biyos::test::nl::Bytes;
using biyos::test::nl::fault_of;
using biyos::test::nl::infos_of;
using biyos::test::nl::kKindsNlFile;
using biyos::test::nl::kLinearNlFile;
using biyos::test::nl::kNonlinearNlFile;
using biyos::test::nl::kObjectivesNlFile;
using biyos::test::nl::kOrderNlFile;
using biyos::test::nl::kTinyHeader;
using biyos::test::nl::located;
using biyos::test::nl::read_sample;
using biyos::test::nl::refused;
using biyos::test::nl::tiny_with;

namespace
{

constexpr std::array kOrders{std::endian::little, std::endian::big};
constexpr Index kKindsVariables = 5;
constexpr Index kOrderVariables = 6;
constexpr Index kManyVariables = 1000;
constexpr std::string_view kOptional = "SkJGdx";

constexpr std::string_view kLeavesTail = R"(C0
n-3
O0 0
n-5
r
3
b
3
)";

constexpr std::string_view kPiecesTail = R"(C0
n0
O0 0
o64
3
n3
n1
n4
n2
n5
v0
r
3
b
3
)";

constexpr std::string_view kManyHeader = R"(b3 1 1 0
 1000 0 1 0 0
 0 0
 0 0
 0 0 0
 0 0 1 1
 0 0 0 0 0
 0 0
 0 0
 0 0 0 0 0
)";

struct Sample
{
    std::string_view text;
    Bytes body;
};

Bytes linear_body(std::endian order)
{
    Bytes bytes(order);
    bytes.segment('C').integer(0).byte('n').real(0);
    bytes.segment('C').integer(1).byte('n').real(0);
    bytes.segment('O').integer(0).integer(0).byte('n').real(1);
    bytes.segment('r').byte('2').real(1).byte('4').real(0);
    bytes.segment('b').byte('0').real(0).real(10).byte('2').real(0);
    bytes.segment('k').integer(1).integer(2);
    bytes.segment('J').integer(0).integer(2).integer(0).real(1).integer(1).real(1);
    bytes.segment('J').integer(1).integer(2).integer(0).real(1).integer(1).real(-1);
    bytes.segment('G').integer(0).integer(2).integer(0).real(2).integer(1).real(3);
    return bytes;
}

Bytes nonlinear_body(std::endian order)
{
    Bytes bytes(order);
    bytes.segment('V').integer(2).integer(1).integer(0).integer(0).real(2);
    bytes.byte('o').integer(41).byte('v').integer(0);
    bytes.segment('C').integer(0).byte('o').integer(2).byte('v').integer(2).byte('v').integer(1);
    bytes.segment('O').integer(0).integer(1).byte('o').integer(0).byte('v').integer(2);
    bytes.byte('o').integer(5).byte('v').integer(0).byte('n').real(2);
    bytes.segment('r').byte('1').real(4);
    bytes.segment('b').byte('3').byte('3');
    bytes.segment('k').integer(1).integer(1);
    bytes.segment('J').integer(0).integer(2).integer(0).real(0).integer(1).real(0);
    bytes.segment('G').integer(0).integer(2).integer(0).real(0).integer(1).real(0);
    return bytes;
}

Bytes box_body(std::endian order, Index variables)
{
    Bytes bytes(order);
    bytes.segment('O').integer(0).integer(0).byte('n').real(0);
    bytes.segment('b');
    for (Index k = 0; k < variables; ++k)
    {
        bytes.byte('0').real(0).real(1);
    }
    return bytes;
}

Bytes objectives_body(std::endian order)
{
    Bytes bytes(order);
    bytes.segment('O').integer(0).integer(0).byte('n').real(1);
    bytes.segment('O').integer(1).integer(1).byte('n').real(0);
    bytes.segment('b').byte('1').real(5);
    bytes.segment('G').integer(1).integer(1).integer(0).real(2);
    return bytes;
}

Bytes tiny_body(std::endian order)
{
    Bytes bytes(order);
    bytes.segment('C').integer(0).byte('n').real(0);
    bytes.segment('O').integer(0).integer(0).byte('n').real(0);
    return bytes;
}

Bytes &free_ends(Bytes &bytes)
{
    bytes.segment('r').byte('3');
    return bytes.segment('b').byte('3');
}

Bytes leaves_body(std::endian order)
{
    Bytes bytes(order);
    bytes.segment('C').integer(0).byte('s').small(-3);
    bytes.segment('O').integer(0).integer(0).byte('l').integer(-5);
    return free_ends(bytes);
}

Bytes guesses_body(std::endian order)
{
    Bytes bytes = tiny_body(order);
    bytes.segment('d').integer(1).integer(0).real(1);
    bytes.segment('x').integer(1).integer(0).real(2);
    return free_ends(bytes);
}

std::vector<Sample> samples_of(std::endian order)
{
    return {
        {.text = kLinearNlFile, .body = linear_body(order)},
        {.text = kNonlinearNlFile, .body = nonlinear_body(order)},
        {.text = kKindsNlFile, .body = box_body(order, kKindsVariables)},
        {.text = kOrderNlFile, .body = box_body(order, kOrderVariables)},
        {.text = kObjectivesNlFile, .body = objectives_body(order)},
    };
}

std::string binary_of(const Sample &sample, std::endian order)
{
    return binary_header(sample.text, arith_of(order)) + sample.body.text();
}

std::string tiny_binary(const Bytes &body)
{
    return binary_header(kTinyHeader, arith_of(std::endian::little)) + body.text();
}

bool may_end(const Bytes &body, std::size_t cut)
{
    const std::vector<std::size_t> &starts = body.starts();
    for (std::size_t k = 0; k < starts.size(); ++k)
    {
        if (starts[k] == cut)
        {
            return body.keys().find_first_not_of(kOptional, k) == std::string::npos;
        }
    }
    return cut == body.text().size();
}

}

TEST(NlTest, BinarySamples)
{
    for (const std::endian order : kOrders)
    {
        for (const Sample &sample : samples_of(order))
        {
            const Model text = read_sample(sample.text);
            EXPECT_EQ(difference(read_sample(binary_of(sample, order)), text), "");
        }
    }
}

TEST(NlTest, BinaryCuts)
{
    for (const Sample &sample : samples_of(std::endian::little))
    {
        const std::string file = binary_of(sample, std::endian::little);
        const std::size_t header = file.size() - sample.body.text().size();
        for (std::size_t cut = 0; cut < file.size(); ++cut)
        {
            const std::string fault = fault_of(file.substr(0, cut));
            if (cut > header && may_end(sample.body, cut - header) && fault.empty())
            {
                continue;
            }
            EXPECT_TRUE(fault.starts_with("bad file: ")) << cut << ": " << fault;
        }
    }
}

TEST(NlTest, BinaryLeaves)
{
    const std::string file = binary_header(kTinyHeader, arith_of(std::endian::big)) +
                             leaves_body(std::endian::big).text();
    EXPECT_EQ(difference(read_sample(file), read_sample(tiny_with(kLeavesTail))), "");
}

TEST(NlTest, BinaryPieces)
{
    Bytes body(std::endian::little);
    body.segment('C').integer(0).byte('n').real(0);
    body.segment('O').integer(0).integer(0).byte('o').integer(64).integer(3);
    body.byte('s').small(3).byte('l').integer(1).byte('n').real(4);
    body.byte('s').small(2).byte('n').real(5).byte('v').integer(0);
    const Model wanted = read_sample(tiny_with(kPiecesTail));
    EXPECT_EQ(difference(read_sample(tiny_binary(free_ends(body))), wanted), "");
}

TEST(NlTest, BinarySuffix)
{
    Bytes body(std::endian::little);
    body.segment('S').integer(0).integer(1).name("zork").integer(0).integer(2);
    body.segment('S').integer(5).integer(1).name("scale").integer(0).real(0.5);
    const std::size_t second = body.starts().back();
    Bytes rest = tiny_body(std::endian::little);
    const std::string file = tiny_binary(body) + free_ends(rest).text();
    const std::size_t header = file.size() - body.text().size() - rest.text().size();
    const std::vector<std::string> wanted{
        located("byte", header, "suffix 'zork' is skipped"),
        located("byte", header + second, "suffix 'scale' is skipped"),
    };
    EXPECT_EQ(infos_of(file), wanted);
}

TEST(NlTest, BinaryGuesses)
{
    const Bytes body = guesses_body(std::endian::little);
    const std::string file = tiny_binary(body);
    const std::size_t header = file.size() - body.text().size();
    const std::vector<std::size_t> &starts = body.starts();
    const std::vector<std::string> wanted{
        located("byte", header + starts[2], "the file's dual guesses are skipped"),
        located("byte", header + starts[3], "the file's starting point is skipped"),
    };
    EXPECT_EQ(infos_of(file), wanted);
    Bytes plain = tiny_body(std::endian::little);
    EXPECT_EQ(difference(read_sample(file), read_sample(tiny_binary(free_ends(plain)))), "");
}

TEST(NlTest, BinaryPlaces)
{
    const std::size_t header = tiny_binary(Bytes(std::endian::little)).size();
    Bytes unknown(std::endian::little);
    unknown.segment('\0');
    EXPECT_EQ(fault_of(tiny_binary(unknown)), bad_at(header, "segment '\\x00' is unknown"));
    Bytes ends(std::endian::little);
    ends.segment('C').integer(0);
    EXPECT_EQ(fault_of(tiny_binary(ends)),
              bad_at(header + 5, "the file ends inside an expression"));
    Bytes code = tiny_body(std::endian::little);
    code.segment('r').byte('9');
    const std::size_t range = header + code.starts().back() + 1;
    EXPECT_EQ(fault_of(tiny_binary(code)), bad_at(range, "bound code '9' is unknown"));
}

TEST(NlTest, BinaryNames)
{
    const std::size_t header = tiny_binary(Bytes(std::endian::little)).size();
    Bytes longer(std::endian::little);
    longer.segment('S').integer(0).integer(1).integer(kManyVariables).byte('z');
    EXPECT_EQ(fault_of(tiny_binary(longer)), bad_at(header, "the file ends inside a segment"));
    Bytes negative(std::endian::little);
    negative.segment('S').integer(0).integer(1).integer(-1);
    EXPECT_EQ(fault_of(tiny_binary(negative)), bad_at(header, "'-1' is not a count"));
}

TEST(NlTest, Arithmetic)
{
    for (const std::string_view arith : {"0", "3", "7"})
    {
        const std::string file = binary_header(kTinyHeader, arith);
        EXPECT_EQ(fault_of(file), refused(10, "arithmetic kind " + std::string(arith)));
    }
}

TEST(NlTest, BinaryRoom)
{
    Bytes body(std::endian::little);
    body.segment('O').integer(0).integer(0).byte('n').real(0);
    const std::string head = std::string(kManyHeader) + body.text();
    EXPECT_EQ(fault_of(head), bad(10, "the header counts more records than the file holds"));
    body.segment('b');
    for (Index k = 0; k < kManyVariables; ++k)
    {
        body.byte('3');
    }
    const Model model = read_sample(std::string(kManyHeader) + body.text());
    EXPECT_EQ(model.n_variables(), kManyVariables);
}
