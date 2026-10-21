/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The inspector writes a script value as both the float and the integer that its bits hold

#include <cstdint>

#include <bit>

#include <LHVMTypes.h>
#include <gtest/gtest.h>

#include "Inspector/ComponentJsonCodec.h"

TEST(ScriptValueJson, AScriptValueIsWrittenAsItsFloatAndItsInteger)
{
	const auto json = openblack::ecs::reflection::Encode(openblack::lhvm::VMValue(2480.0f));
	ASSERT_TRUE(json.has_value());
	EXPECT_EQ((*json)["float"].get<double>(), 2480.0);
	EXPECT_EQ((*json)["int"].get<int32_t>(), std::bit_cast<int32_t>(2480.0f));
}

TEST(ScriptValueJson, AnIntegerScriptValueKeepsItsNumber)
{
	const auto json = openblack::ecs::reflection::Encode(openblack::lhvm::VMValue(int32_t {42}));
	ASSERT_TRUE(json.has_value());
	EXPECT_EQ((*json)["int"].get<int32_t>(), 42);
}
