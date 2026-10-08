/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The land-script writer (LHScriptX/ScriptWriter.h) against the original's format: each command's text from the
// command table and the position text of a MapCoords (docs/bw1-notes/land-script-save.md).

#include <algorithm>

#include <gtest/gtest.h>

#include "LHScriptX/FeatureScriptCommands.h"
#include "LHScriptX/ScriptWriter.h"

using namespace openblack;
using namespace openblack::lhscriptx;
using openblack::map_coords::MapCoords;

namespace
{
// 1000 m and 5 m: exact in float products, so the text is the same on any C runtime
constexpr MapCoords k_At {0x10000 * 100, 0x8000, 0.0f};
constexpr MapCoords k_Home {0x10000 * 101, 0x10000 * 2, 0.0f};

std::string FormatOf(std::string_view name)
{
	const auto* command = FindCommandFormat(name);
	return command != nullptr ? CommandAsText(*command) : std::string("(none)");
}
} // namespace

TEST(ScriptWriter, TableAsTheExe)
{
	EXPECT_EQ(k_CommandFormats[0].name, "CREATE_MIST");
	EXPECT_EQ(k_CommandFormats[16].name, "CREATE_TOWN_VILLAGER");
	EXPECT_EQ(k_CommandFormats[16].types, "NAAN");
	EXPECT_EQ(k_CommandFormats[101].name, "EDIT_LEVEL");
	EXPECT_EQ(k_CommandFormats[104].name, "SET_LOST_TOWN_SCALE");
	EXPECT_EQ(FormatOf("EDIT_LEVEL"), "EDIT_LEVEL()\n"); // nothing added: no ", " to cut
}

TEST(ScriptWriter, CommandAsTextOfTheSevenWriters)
{
	EXPECT_EQ(FormatOf("CREATE_ABODE"), "CREATE_ABODE(%d, %s, \"%s\", %d, %d, %d, %d)\n");
	EXPECT_EQ(FormatOf("CREATE_TOWN_VILLAGER"), "CREATE_TOWN_VILLAGER(%d, %s, %s, %d)\n");
	EXPECT_EQ(FormatOf("CREATE_SPECIAL_TOWN_VILLAGER"), "CREATE_SPECIAL_TOWN_VILLAGER(%d, %s, %d, %d)\n");
	EXPECT_EQ(FormatOf("CREATE_VILLAGER_POS"), "CREATE_VILLAGER_POS(%s, %s, \"%s\", %d)\n");
	EXPECT_EQ(FormatOf("CREATE_NEW_TREE"), "CREATE_NEW_TREE(%d, %s, %d, %d, %f, %f, %f)\n");
	EXPECT_EQ(FormatOf("CREATE_POT"), "CREATE_POT(%s, %d, %d, %d)\n");
	EXPECT_EQ(FormatOf("CREATE_MOBILE_STATIC"), "CREATE_MOBILE_STATIC(%s, %d, %f, %f, %f, %f, %f)\n");
}

TEST(ScriptWriter, PositionText)
{
	EXPECT_EQ(PositionText(k_At), "\"1000.00,5.00\"");
	EXPECT_EQ(PositionText(k_Home), "\"1010.00,20.00\"");
	// a decimal tie: 0x1000 -> 0.625 exactly; the original's C runtime rounds the digits half up ("0.63"), the UCRT
	// would give 0.62
	EXPECT_EQ(PositionText(MapCoords {0x1000, 0x10000 * 3 + 0x1000, 0.0f}), "\"0.63,30.63\"");
}

TEST(ScriptWriter, FloatTieRoundsHalfUp)
{
	// %f of 2^-7 = 0.0078125: "0.007813" as the original's C runtime (half to even would give 0.007812)
	EXPECT_EQ(WriteCommand("CREATE_AREA", {k_At, 0.0078125f}), "CREATE_AREA(\"1000.00,5.00\", 0.007813)\n");
	EXPECT_EQ(WriteCommand("VERSION", {-1.5f}), "VERSION(-1.500000)\n");
}

TEST(ScriptWriter, TheSevenLines)
{
	EXPECT_EQ(WriteCommand("CREATE_ABODE", {int32_t {0}, k_At, std::string("CELTIC_ABODE_A"), int32_t {30}, int32_t {1000},
	                                        int32_t {50}, int32_t {25}}),
	          "CREATE_ABODE(0, \"1000.00,5.00\", \"CELTIC_ABODE_A\", 30, 1000, 50, 25)\n");
	// the villager's type in an A: a bare name, as the shipped Land2.txt writes it
	EXPECT_EQ(WriteCommand("CREATE_TOWN_VILLAGER", {int32_t {1}, k_At, std::string("CELTIC_FISHERMAN"), int32_t {26}}),
	          "CREATE_TOWN_VILLAGER(1, \"1000.00,5.00\", CELTIC_FISHERMAN, 26)\n");
	EXPECT_EQ(WriteCommand("CREATE_SPECIAL_TOWN_VILLAGER", {int32_t {1}, k_At, int32_t {3}, int32_t {40}}),
	          "CREATE_SPECIAL_TOWN_VILLAGER(1, \"1000.00,5.00\", 3, 40)\n");
	EXPECT_EQ(WriteCommand("CREATE_VILLAGER_POS", {k_Home, k_At, std::string("CELTIC_HOUSEWIFE"), int32_t {30}}),
	          "CREATE_VILLAGER_POS(\"1010.00,20.00\", \"1000.00,5.00\", \"CELTIC_HOUSEWIFE\", 30)\n");
	EXPECT_EQ(WriteCommand("CREATE_NEW_TREE", {int32_t {0}, k_At, int32_t {4}, int32_t {0}, 0.5f, 1.0f, 2.25f}),
	          "CREATE_NEW_TREE(0, \"1000.00,5.00\", 4, 0, 0.500000, 1.000000, 2.250000)\n");
	EXPECT_EQ(WriteCommand("CREATE_POT", {k_At, int32_t {2}, int32_t {0}, int32_t {100}}),
	          "CREATE_POT(\"1000.00,5.00\", 2, 0, 100)\n");
	EXPECT_EQ(WriteCommand("CREATE_MOBILE_STATIC", {k_At, int32_t {12}, 0.0f, 0.0f, 1.5f, 0.0f, 1.0f}),
	          "CREATE_MOBILE_STATIC(\"1000.00,5.00\", 12, 0.000000, 0.000000, 1.500000, 0.000000, 1.000000)\n");
}

TEST(ScriptWriter, RefusesWhatTheFormatCannotTake)
{
	EXPECT_FALSE(WriteCommand("NOT_A_COMMAND", {}).has_value());
	EXPECT_FALSE(WriteCommand("CREATE_POT", {k_At, int32_t {2}, int32_t {0}}).has_value()); // too few
	EXPECT_FALSE(WriteCommand("CREATE_POT", {k_At, int32_t {2}, int32_t {0}, int32_t {1}, int32_t {2}}).has_value());
	EXPECT_FALSE(WriteCommand("CREATE_POT", {k_At, 2.0f, int32_t {0}, int32_t {100}}).has_value());    // F for an N
	EXPECT_FALSE(WriteCommand("CREATE_VILLAGER_POS", {k_Home, k_At, k_At, int32_t {30}}).has_value()); // MapCoords for an L
	EXPECT_EQ(WriteCommand("EDIT_LEVEL", {}), "EDIT_LEVEL()\n");
}

TEST(ScriptWriter, TheReaderTakesEveryWrittenCommand)
{
	// every command the original can write has a reader with the same parameters (N a Number, F a Float, L a String, A a
	// Vector or a bare name String)
	for (const auto& command : k_CommandFormats)
	{
		const auto signature =
		    std::find_if(FeatureScriptCommands::k_Signatures.begin(), FeatureScriptCommands::k_Signatures.end(),
		                 [&](const auto& s) { return std::string_view(s.name.data()) == command.name; });
		ASSERT_NE(signature, FeatureScriptCommands::k_Signatures.end()) << command.name;
		size_t i = 0;
		for (const char letter : command.types)
		{
			ASSERT_LT(i, signature->parameters.size()) << command.name;
			const auto p = signature->parameters[i++];
			switch (letter)
			{
			case 'N':
				EXPECT_EQ(p, ParameterType::Number) << command.name << " " << i;
				break;
			case 'F':
				EXPECT_EQ(p, ParameterType::Float) << command.name << " " << i;
				break;
			case 'L':
				EXPECT_EQ(p, ParameterType::String) << command.name << " " << i;
				break;
			default: // 'A'
				EXPECT_TRUE(p == ParameterType::Vector || p == ParameterType::String) << command.name << " " << i;
				break;
			}
		}
		EXPECT_TRUE(i == signature->parameters.size() || signature->parameters[i] == ParameterType::None) << command.name;
	}
}
