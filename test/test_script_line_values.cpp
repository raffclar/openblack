/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <string>

#include <gtest/gtest.h>

#include "LHScriptX/ScriptLineValues.h"

using openblack::lhscriptx::ScriptCommandParameter;
using openblack::lhscriptx::ScriptCommandParameters;
using openblack::lhscriptx::ScriptLineValues;

TEST(ScriptLineValues, ALineFillsThePlacesOfItsArguments)
{
	ScriptLineValues values;
	values.Keep(ScriptCommandParameters {ScriptCommandParameter(int32_t {4}), ScriptCommandParameter(std::string("Abodes")),
	                                     ScriptCommandParameter(-0.75f)});
	EXPECT_EQ(values.number[0], 4);
	EXPECT_EQ(values.text[1], "Abodes");
	EXPECT_EQ(values.real[2], -0.75f);
}

TEST(ScriptLineValues, AShorterLineKeepsTheLaterPlacesOfEarlierLines)
{
	// A town desire boost line, then a firefly line with only a position: the firefly line changes only the first place
	ScriptLineValues values;
	values.Keep(ScriptCommandParameters {ScriptCommandParameter(int32_t {4}), ScriptCommandParameter(std::string("Abodes")),
	                                     ScriptCommandParameter(-0.75f)});
	values.Keep(ScriptCommandParameters {ScriptCommandParameter(1000.0f, 0.0f, 2000.0f)});
	EXPECT_EQ(values.number[0], 4);
	EXPECT_EQ(values.text[0], "");
	EXPECT_EQ(values.text[1], "Abodes");
	EXPECT_EQ(values.real[2], -0.75f);
}

TEST(ScriptLineValues, EachKindOfArgumentFillsOnlyItsOwnValue)
{
	ScriptLineValues values;
	values.Keep(ScriptCommandParameters {ScriptCommandParameter(int32_t {7}), ScriptCommandParameter(std::string("Food")),
	                                     ScriptCommandParameter(0.5f)});
	// A float in the first place and a number in the third: the number of the first and the float of the third stay
	values.Keep(ScriptCommandParameters {ScriptCommandParameter(2.0f), ScriptCommandParameter(int32_t {9}),
	                                     ScriptCommandParameter(int32_t {3})});
	EXPECT_EQ(values.number[0], 7);
	EXPECT_EQ(values.real[0], 2.0f);
	EXPECT_EQ(values.number[1], 9);
	// A number has no name in its text
	EXPECT_EQ(values.text[1], "");
	EXPECT_EQ(values.real[2], 0.5f);
	EXPECT_EQ(values.number[2], 3);
}
