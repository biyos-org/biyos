#include "reading.hpp"
#include "texts.hpp"

#include <gtest/gtest.h>

#include <string>

using biyos::test::bad_line;
using biyos::test::native_fault;
using biyos::test::text;

TEST(NativeTest, EmptyFile)
{
    EXPECT_EQ(native_fault(""), "bad file: file 't.bys': is empty");
    EXPECT_EQ(native_fault(text({"", " "})), bad_line(2, "the file does not start with BIYOS"));
}

TEST(NativeTest, NoMagic)
{
    EXPECT_EQ(native_fault(text({"NAME t", "END"})),
              bad_line(1, "the file does not start with BIYOS"));
}

TEST(NativeTest, Version)
{
    EXPECT_EQ(native_fault(text({"BIYOS 2", "END"})),
              "unsupported: file 't.bys' line 1: version 2 is newer than 1");
    EXPECT_EQ(native_fault(text({"BIYOS 2 x", "END"})),
              "unsupported: file 't.bys' line 1: version 2 is newer than 1");
    EXPECT_EQ(native_fault(text({"BIYOS 0", "END"})), bad_line(1, "version 0 is unknown"));
    EXPECT_EQ(native_fault(text({"BIYOS one", "END"})), bad_line(1, "'one' is not an integer"));
    EXPECT_EQ(native_fault(text({"BIYOS", "END"})), bad_line(1, "the line ends early"));
    EXPECT_EQ(native_fault(text({"BIYOS 1 2", "END"})),
              bad_line(1, "the line holds too many fields"));
}

TEST(NativeTest, Sections)
{
    EXPECT_EQ(native_fault(text({"BIYOS 1", "ROWS 1", "END"})),
              bad_line(2, "'ROWS' is not a section"));
    EXPECT_EQ(native_fault(text({"BIYOS 1", "NAMES row 0", "END"})),
              bad_line(2, "'row' is not a family"));
    EXPECT_EQ(native_fault(text({"BIYOS 1", "FUNCTIONS 0", "VARIABLES 0", "END"})),
              bad_line(3, "section VARIABLES is out of order"));
    EXPECT_EQ(native_fault(text({"BIYOS 1", "NAMES variable 0", "NAMES variable 0", "END"})),
              bad_line(3, "section NAMES variable is out of order"));
    EXPECT_EQ(native_fault(text({"BIYOS 1", "NAME a", "NAME b", "END"})),
              bad_line(3, "section NAME is out of order"));
    EXPECT_EQ(native_fault(text({"BIYOS 1", "END 1"})),
              bad_line(2, "the line holds too many fields"));
}

TEST(NativeTest, Ends)
{
    EXPECT_EQ(native_fault(text({"BIYOS 1", "VARIABLES 0"})),
              bad_line(2, "the file ends without END"));
    EXPECT_EQ(native_fault(text({"BIYOS 1", "VARIABLES 2", "1 0 1"})),
              bad_line(3, "the file ends inside VARIABLES"));
    EXPECT_EQ(native_fault(text({"BIYOS 1", "END", "", "END"})), bad_line(4, "a line follows END"));
}

TEST(NativeTest, Fields)
{
    EXPECT_EQ(native_fault(text({"BIYOS 1", "VARIABLES -1", "END"})),
              bad_line(2, "'-1' is not a count"));
    EXPECT_EQ(native_fault(text({"BIYOS 1", "VARIABLES 1", "1 0", "END"})),
              bad_line(3, "the line ends early"));
    EXPECT_EQ(native_fault(text({"BIYOS 1", "VARIABLES 1", "1 0 1 1", "END"})),
              bad_line(3, "the line holds too many fields"));
    EXPECT_EQ(native_fault(text({"BIYOS 1", "VARIABLES 1", "9 0 1", "END"})),
              bad_line(3, "variable kind 9 is unknown"));
    EXPECT_EQ(native_fault(text({"BIYOS 1", "VARIABLES 1", "1 0 1x", "END"})),
              bad_line(3, "'1x' is not a number"));
    EXPECT_EQ(native_fault(text({"BIYOS 1", "FUNCTIONS 1", "0", "CONSTRAINTS 1", "f 0 1", "END"})),
              bad_line(5, "'f' is not an index"));
    EXPECT_EQ(native_fault(text({"BIYOS 1", "OBJECTIVES 1", "0 3 0 1 0 0", "END"})),
              bad_line(3, "objective sense 3 is unknown"));
}

TEST(NativeTest, Entries)
{
    EXPECT_EQ(native_fault(text({"BIYOS 1", "PIECEWISE 1", "3 0 0 1 1", "END"})),
              bad_line(3, "the line holds fewer than 3 entries"));
    EXPECT_EQ(native_fault(text({"BIYOS 1", "SETS 1", "1 2 0 1", "END"})),
              bad_line(3, "the line holds fewer than 2 entries"));
    EXPECT_EQ(native_fault(text({"BIYOS 1", "EXPRESSIONS 1", "0 1", "1 0", "END"})),
              bad_line(4, "the line ends early"));
    EXPECT_EQ(native_fault(text({"BIYOS 1", "EXPRESSIONS 1", "0 2", "1 0 1"})),
              bad_line(4, "the file ends inside EXPRESSIONS"));
}

TEST(NativeTest, Attributes)
{
    EXPECT_EQ(native_fault(text({"BIYOS 1", "NAMES variable 2", "1 a", "0 b", "END"})),
              bad_line(4, "index 0 is out of order"));
    EXPECT_EQ(native_fault(text({"BIYOS 1", "NAMES variable 2", "0 a", "0 b", "END"})),
              bad_line(4, "index 0 is out of order"));
    EXPECT_EQ(native_fault(text({"BIYOS 1", "TOLERANCES constraint 1", "0 none none", "END"})),
              bad_line(3, "the line sets no tolerance"));
}

TEST(NativeTest, Escapes)
{
    EXPECT_EQ(native_fault(text({"BIYOS 1", "NAME a\\4", "END"})),
              bad_line(2, "'a\\4' holds a bad escape"));
    EXPECT_EQ(native_fault(text({"BIYOS 1", "NAME a\\zz", "END"})),
              bad_line(2, "'a\\zz' holds a bad escape"));
}

TEST(NativeTest, ModelFaults)
{
    EXPECT_EQ(native_fault(text({"BIYOS 1", "CONSTRAINTS 1", "0 0 1", "END"})),
              bad_line(2, "constraint 0: function 0 is unknown"));
    EXPECT_EQ(
        native_fault(text({"BIYOS 1", "VARIABLES 1", "1 0 1", "NAMES variable 1", "3 x", "END"})),
        bad_line(4, "variable 3: unknown"));
}
