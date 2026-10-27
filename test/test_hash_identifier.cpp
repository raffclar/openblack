/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <limits>
#include <string>

#include <entt/core/hashed_string.hpp>
#include <gtest/gtest.h>

#include "Resources/ResourceManager.h"

namespace
{
enum class Clip : uint32_t
{
	Walk = 42,
};

[[nodiscard]] entt::id_type HashOfText(const std::string& text)
{
	return entt::hashed_string(text.c_str()).value();
}
} // namespace

// A number's id is the hash of its decimal digits, as the resources are registered by name
TEST(HashIdentifier, NumberIsHashOfItsDigits)
{
	using openblack::resources::HashIdentifier;
	EXPECT_EQ(HashIdentifier(0U), HashOfText("0"));
	EXPECT_EQ(HashIdentifier(7U), HashOfText("7"));
	EXPECT_EQ(HashIdentifier(10U), HashOfText("10"));
	EXPECT_EQ(HashIdentifier(1234567U), HashOfText("1234567"));
	EXPECT_EQ(HashIdentifier(std::numeric_limits<uint32_t>::max()), HashOfText("4294967295"));
	EXPECT_EQ(HashIdentifier(Clip::Walk), HashOfText("42"));
}

// A negative number is written as the unsigned number of the same bits
TEST(HashIdentifier, NegativeNumberUsesItsUnsignedValue)
{
	using openblack::resources::HashIdentifier;
	EXPECT_EQ(HashIdentifier(-1), HashOfText("4294967295"));
}

// The text ids are not changed
TEST(HashIdentifier, TextIsHashedAsItIs)
{
	using openblack::resources::HashIdentifier;
	EXPECT_EQ(HashIdentifier(std::string("raw/envmap")), HashOfText("raw/envmap"));
}
