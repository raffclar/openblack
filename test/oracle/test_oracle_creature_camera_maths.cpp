/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <bit>
#include <functional>
#include <initializer_list>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Camera/ScriptCameraModel.h"
#include "Common/X87Angle.h"
#include "Creature/CreatureArena.h"
#include "Creature/CreaturePlayerAttention.h"
#include "OracleData.h"

using namespace openblack;

namespace
{

/// One row of a pinned table, read by column name
class Row
{
public:
	Row(const oracle::Table& table, size_t row)
	    : _table(table)
	    , _row(row)
	{
	}

	[[nodiscard]] float Float(std::string_view name) const { return _table.Float(_row, *_table.Column(name)); }
	[[nodiscard]] uint32_t Bits(std::string_view name) const { return _table.Bits(_row, *_table.Column(name)); }
	[[nodiscard]] glm::vec3 Vec3(std::string_view x, std::string_view y, std::string_view z) const
	{
		return {Float(x), Float(y), Float(z)};
	}

private:
	const oracle::Table& _table;
	size_t _row;
};

/// Compares our results with the game's columns, bit for bit, and gives the count of different rows
size_t CountDifferent(std::string_view file, std::initializer_list<std::string_view> inputs,
                      std::initializer_list<std::string_view> outputs,
                      const std::function<std::vector<float>(const Row&)>& ours)
{
	const auto table = oracle::Table::Load(file);
	EXPECT_TRUE(table.has_value()) << file;
	if (!table)
	{
		return 1;
	}
	for (const auto name : inputs)
	{
		EXPECT_TRUE(table->Column(name).has_value()) << name;
	}
	for (const auto name : outputs)
	{
		EXPECT_TRUE(table->Column(name).has_value()) << name;
	}
	size_t different = 0;
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		const Row read(*table, row);
		const auto results = ours(read);
		size_t column = 0;
		bool same = true;
		std::string text;
		for (const auto name : outputs)
		{
			const float result = results.at(column++);
			same = same && std::bit_cast<uint32_t>(result) == read.Bits(name);
			text += std::string(" ") + std::string(name) + " game " + oracle::Hex(read.Float(name)) + " ours " +
			        oracle::Hex(result);
		}
		if (!same)
		{
			++different;
			if (different <= 20)
			{
				std::string in;
				for (const auto name : inputs)
				{
					in += std::string(" ") + std::string(name) + " " + oracle::Hex(read.Float(name));
				}
				ADD_FAILURE() << file << " row " << row << ":" << in << ":" << text;
			}
		}
	}
	return different;
}

} // namespace

TEST(OracleCreatureCameraMaths, TheGamesAngleOfAVector)
{
	const auto different = CountDifferent("game_atan2.txt", {"x", "y"}, {"angle"}, [](const Row& row) {
		return std::vector {x87::RoundToFloat(x87::GameAtan2(row.Float("y"), row.Float("x")))};
	});
	EXPECT_EQ(different, 0U);
}

TEST(OracleCreatureCameraMaths, HeadingAndPitchFromAnOriginToAFocus)
{
	const auto different =
	    CountDifferent("heading_and_pitch.txt", {"origin_x", "origin_y", "origin_z", "focus_x", "focus_y", "focus_z"},
	                   {"heading", "pitch"}, [](const Row& row) {
		                   const auto result = script_camera::HeadingAndPitchFromPoints(
		                       row.Vec3("origin_x", "origin_y", "origin_z"), row.Vec3("focus_x", "focus_y", "focus_z"));
		                   return std::vector {result.heading, result.pitch};
	                   });
	EXPECT_EQ(different, 0U);
}

TEST(OracleCreatureCameraMaths, APointAtADistanceHeadingAndPitch)
{
	const auto different = CountDifferent(
	    "point_from_heading.txt", {"point_x", "point_y", "point_z", "distance", "heading", "pitch"}, {"x", "y", "z"},
	    [](const Row& row) {
		    const auto point = script_camera::PointFromDistanceHeadingAndPitch(
		        row.Vec3("point_x", "point_y", "point_z"), row.Float("distance"), row.Float("heading"), row.Float("pitch"));
		    return std::vector {point.x, point.y, point.z};
	    });
	EXPECT_EQ(different, 0U);
}

TEST(OracleCreatureCameraMaths, TheViewWidthAtTheFocus)
{
	const auto different = CountDifferent(
	    "view_width.txt", {"origin_x", "origin_y", "origin_z", "focus_x", "focus_y", "focus_z"}, {"width"}, [](const Row& row) {
		    return std::vector {creature_player_attention::ViewWidthAtFocus(row.Vec3("origin_x", "origin_y", "origin_z"),
		                                                                    row.Vec3("focus_x", "focus_y", "focus_z"))};
	    });
	EXPECT_EQ(different, 0U);
}

TEST(OracleCreatureCameraMaths, TheRingOfLightRoundAnArena)
{
	const auto different = CountDifferent("arena_ring.txt", {"centre_x", "centre_z", "radius"}, {"x", "z"}, [](const Row& row) {
		const auto points = creature_arena::RingPoints({row.Float("centre_x"), row.Float("centre_z")}, row.Float("radius"),
		                                               [](glm::vec2) { return 0.0f; });
		const auto& point = points.at(row.Bits("index"));
		return std::vector {point.x, point.z};
	});
	EXPECT_EQ(different, 0U);
}
