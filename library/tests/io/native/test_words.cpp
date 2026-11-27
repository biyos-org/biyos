#include "biyos/biyos.h"
#include "io/native/utils/words.hpp"
#include "model/model.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>

using biyos::native::kConeWords;
using biyos::native::kOperatorWords;
using biyos::native::kSenseWords;
using biyos::native::kSetWords;
using biyos::native::kVariableWords;
using biyos::native::Word;
using biyos::native::word_of;

namespace
{

template <typename Value, std::size_t N>
void expect_distinct(const std::array<Word<Value>, N> &words)
{
    for (const Word<Value> &word : words)
    {
        SCOPED_TRACE(word.text);
        EXPECT_EQ(std::ranges::find(words, word.text, &Word<Value>::text)->value, word.value);
    }
}

template <typename Value, std::size_t N, std::size_t M>
void expect_words(const std::array<Value, N> &values, const std::array<Word<Value>, M> &words)
{
    EXPECT_EQ(N, M);
    for (const Value value : values)
    {
        EXPECT_NE(word_of(value, words), "");
    }
    expect_distinct(words);
}

}

TEST(WordTest, Kinds)
{
    expect_words(biyos::kVariableKinds, kVariableWords);
    expect_words(biyos::kSenses, kSenseWords);
    expect_words(biyos::kSpecialOrderedSetKinds, kSetWords);
    expect_words(biyos::kConeKinds, kConeWords);
}

TEST(WordTest, Operators)
{
    EXPECT_EQ(kOperatorWords.size(), std::size_t{BIYOS_OP_REF});
    for (int code = BIYOS_OP_CONST; code <= BIYOS_OP_REF; ++code)
    {
        EXPECT_NE(word_of(code, kOperatorWords), "");
    }
    expect_distinct(kOperatorWords);
}
