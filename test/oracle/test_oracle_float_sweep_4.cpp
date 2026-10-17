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
#include <string>
#include <string_view>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Common/X87.h"
#include "ECS/DanceMoves.h"
#include "ECS/DanceShapes.h"
#include "Magic/FlockMiracleRules.h"
#include "Magic/ReactionTimingRules.h"
#include "OracleData.h"
#include "Particles/GestureTrail.h"
#include "Particles/LightSheet.h"

using namespace openblack;

namespace
{

/// Loads a table of the game's results and checks that it has rows
oracle::Table Load(std::string_view name)
{
	auto table = oracle::Table::Load(name);
	EXPECT_TRUE(table.has_value()) << name;
	if (!table.has_value())
	{
		return {};
	}
	EXPECT_GT(table->Rows(), 0u) << name;
	return *table;
}

/// The index of a column, or a failure when the table does not have it
size_t Column(const oracle::Table& table, std::string_view name)
{
	const auto column = table.Column(name);
	EXPECT_TRUE(column.has_value()) << name;
	return column.value_or(0);
}

/// Counts a different row and reports the first ones
class Differences
{
public:
	void Check(bool same, size_t row, const std::string& what)
	{
		if (same)
		{
			return;
		}
		++_count;
		if (_count <= 10)
		{
			ADD_FAILURE() << "row " << row << ": " << what;
		}
	}
	[[nodiscard]] size_t Count() const { return _count; }

private:
	size_t _count {0};
};

bool SameBits(float a, uint32_t b)
{
	return std::bit_cast<uint32_t>(a) == b;
}

std::string Pair(float ours, float game)
{
	return "ours " + oracle::Hex(ours) + ", the game " + oracle::Hex(game);
}

} // namespace

TEST(OracleFloatSweep4, SpinKeptEachStepAsTheGameDoes)
{
	const auto table = Load("spin_kept.txt");
	const auto in = Column(table, "per_second");
	const auto out = Column(table, "per_step");
	Differences differences;
	for (size_t row = 0; row < table.Rows(); ++row)
	{
		const float ours = x87::Pow(table.Float(row, in), 0.005000000074505806);
		differences.Check(SameBits(ours, table.Bits(row, out)), row, Pair(ours, table.Float(row, out)));
	}
	EXPECT_EQ(differences.Count(), 0u);
}

TEST(OracleFloatSweep4, StandardReactionTimesAsTheGameDoes)
{
	using namespace magic::reaction_timing;
	const auto table = Load("reaction_timing.txt");
	const auto reach = Column(table, "reach");
	const auto importance = Column(table, "importance");
	const auto distance = Column(table, "distance");
	const auto react = Column(table, "react");
	Differences differences;
	for (size_t r = 0; r < table.Rows(); ++r)
	{
		const Row row {.turnsToReact = table.Bits(r, Column(table, "turns")),
		               .turnsBeforeReactingAgain = table.Bits(r, Column(table, "again")),
		               .creatureTurnsToReact = table.Bits(r, Column(table, "creature_turns")),
		               .creatureTurnsBeforeReactingAgain = table.Bits(r, Column(table, "creature_again")),
		               .distance = {.maxDistance = table.Float(r, reach), .importance = table.Float(r, importance)}};
		const float at = table.Float(r, distance);
		const std::array<int32_t, 4> ours {
		    StandardTurnsToReact(row, Reacting::Villager, at), StandardTurnsBeforeReactingAgain(row, Reacting::Villager, at),
		    StandardTurnsToReact(row, Reacting::Creature, at), StandardTurnsBeforeReactingAgain(row, Reacting::Creature, at)};
		for (size_t i = 0; i < ours.size(); ++i)
		{
			const auto game = static_cast<int32_t>(table.Bits(r, react + i));
			differences.Check(ours.at(i) == game, r,
			                  "time " + std::to_string(i) + ": ours " + std::to_string(ours.at(i)) + ", the game " +
			                      std::to_string(game));
		}
	}
	EXPECT_EQ(differences.Count(), 0u);
}

TEST(OracleFloatSweep4, FlockNumberAsTheGameDoes)
{
	const auto table = Load("flock_number.txt");
	const auto number = Column(table, "number");
	const auto power = Column(table, "power");
	const auto made = Column(table, "made");
	Differences differences;
	for (size_t row = 0; row < table.Rows(); ++row)
	{
		const int ours = magic::flock::NumberToCreate(table.Bits(row, number), table.Float(row, power));
		const auto game = static_cast<int32_t>(table.Bits(row, made));
		differences.Check(ours == game, row, "ours " + std::to_string(ours) + ", the game " + std::to_string(game));
	}
	EXPECT_EQ(differences.Count(), 0u);
}

TEST(OracleFloatSweep4, DanceShapesAsTheGameMakesThem)
{
	// The circle, the wavy circle, the spiral, the wave and the octagon, in the order of the table
	constexpr std::array<size_t, 5> k_Index {0, 1, 2, 3, 10};
	const auto table = Load("dance_shapes.txt");
	const auto shape = Column(table, "shape");
	const auto k = Column(table, "k");
	const auto x = Column(table, "x");
	const auto z = Column(table, "z");
	Differences differences;
	for (size_t row = 0; row < table.Rows(); ++row)
	{
		const auto built = ecs::dance_shapes::Build(k_Index.at(table.Bits(row, shape)));
		const auto& point = built.at(table.Bits(row, k));
		const glm::ivec2 game {static_cast<int32_t>(table.Bits(row, x)), static_cast<int32_t>(table.Bits(row, z))};
		differences.Check(point == game, row,
		                  "shape " + std::to_string(table.Bits(row, shape)) + " point " + std::to_string(table.Bits(row, k)) +
		                      ": ours " + std::to_string(point.x) + ", " + std::to_string(point.y) + ", the game " +
		                      std::to_string(game.x) + ", " + std::to_string(game.y));
	}
	EXPECT_EQ(differences.Count(), 0u);
}

TEST(OracleFloatSweep4, RotatedShapePointAsTheGameDoes)
{
	const auto table = Load("rotate_point.txt");
	const auto x = Column(table, "x");
	const auto y = Column(table, "y");
	const auto angle = Column(table, "angle");
	const auto outX = Column(table, "out_x");
	const auto outY = Column(table, "out_y");
	Differences differences;
	for (size_t row = 0; row < table.Rows(); ++row)
	{
		const auto ours =
		    ecs::dance_moves::RotatePointByAngle({table.Float(row, x), table.Float(row, y)}, table.Float(row, angle));
		differences.Check(SameBits(ours.x, table.Bits(row, outX)) && SameBits(ours.y, table.Bits(row, outY)), row,
		                  "x " + Pair(ours.x, table.Float(row, outX)) + "; y " + Pair(ours.y, table.Float(row, outY)));
	}
	EXPECT_EQ(differences.Count(), 0u);
}

TEST(OracleFloatSweep4, TrailPathAsTheGameMeasuresIt)
{
	constexpr size_t k_Points = 8;
	const auto table = Load("trail_path.txt");
	const auto fraction = Column(table, "fraction");
	const auto length = Column(table, "length");
	const auto out = Column(table, "x");
	Differences differences;
	for (size_t row = 0; row < table.Rows(); ++row)
	{
		std::vector<glm::vec3> points;
		for (size_t i = 0; i < k_Points; ++i)
		{
			points.emplace_back(table.Float(row, 3 * i), table.Float(row, 3 * i + 1), table.Float(row, 3 * i + 2));
		}
		const particles::TrailPath path(std::move(points));
		const auto at = path.At(table.Float(row, fraction));
		differences.Check(SameBits(path.Length(), table.Bits(row, length)), row,
		                  "length " + Pair(path.Length(), table.Float(row, length)));
		differences.Check(SameBits(at.x, table.Bits(row, out)) && SameBits(at.y, table.Bits(row, out + 1)) &&
		                      SameBits(at.z, table.Bits(row, out + 2)),
		                  row, "point x " + Pair(at.x, table.Float(row, out)));
	}
	EXPECT_EQ(differences.Count(), 0u);
}

TEST(OracleFloatSweep4, LightSheetWaveAndStrengthsAsTheGameDoes)
{
	constexpr size_t k_Points = 10;
	const auto table = Load("light_sheet.txt");
	const auto height = Column(table, "height");
	const auto shift = Column(table, "shift");
	const auto heights = Column(table, "h0");
	const auto strengths = Column(table, "s0");
	Differences differences;
	for (size_t row = 0; row < table.Rows(); ++row)
	{
		particles::LightSheet sheet;
		sheet.Start(std::vector<glm::vec3>(k_Points, glm::vec3(0.0f)), 0xFFFFFFu, table.Float(row, height),
		            table.Float(row, shift));
		for (size_t step = 0; step < 3; ++step)
		{
			sheet.SetStrength(table.Float(row, Column(table, "strength" + std::to_string(step))));
			sheet.Update(table.Float(row, Column(table, "seconds" + std::to_string(step))));
		}
		for (size_t i = 0; i < k_Points; ++i)
		{
			differences.Check(SameBits(sheet.Heights().at(i), table.Bits(row, heights + i)), row,
			                  "height " + std::to_string(i) + " " + Pair(sheet.Heights().at(i), table.Float(row, heights + i)));
			differences.Check(SameBits(sheet.Strengths().at(i), table.Bits(row, strengths + i)), row,
			                  "strength " + std::to_string(i) + " " +
			                      Pair(sheet.Strengths().at(i), table.Float(row, strengths + i)));
		}
	}
	EXPECT_EQ(differences.Count(), 0u);
}

TEST(OracleFloatSweep4, ForceFieldPulseAndFadeAsTheGameDoes)
{
	constexpr size_t k_Points = 10;
	const auto table = Load("force_field.txt");
	const auto cx = Column(table, "cx");
	const auto cz = Column(table, "cz");
	const auto radius = Column(table, "radius");
	const auto seconds = Column(table, "seconds");
	const auto pulsed = Column(table, "pulsed0");
	const auto faded = Column(table, "faded0");
	Differences differences;
	for (size_t row = 0; row < table.Rows(); ++row)
	{
		std::vector<glm::vec3> points;
		for (size_t i = 0; i < k_Points; ++i)
		{
			points.emplace_back(table.Float(row, 2 * i), 0.0f, table.Float(row, 2 * i + 1));
		}
		particles::LightSheet sheet;
		sheet.StartPulsed(0xFFFFFFu, 10.0f, k_Points);
		sheet.SetPoints(std::move(points));
		sheet.Pulse({table.Float(row, cx), 0.0f, table.Float(row, cz)}, table.Float(row, radius));
		for (size_t i = 0; i < k_Points; ++i)
		{
			differences.Check(SameBits(sheet.Strengths().at(i), table.Bits(row, pulsed + i)), row,
			                  "pulse " + std::to_string(i) + " " + Pair(sheet.Strengths().at(i), table.Float(row, pulsed + i)));
		}
		sheet.Update(table.Float(row, seconds));
		for (size_t i = 0; i < k_Points; ++i)
		{
			differences.Check(SameBits(sheet.Strengths().at(i), table.Bits(row, faded + i)), row,
			                  "fade " + std::to_string(i) + " " + Pair(sheet.Strengths().at(i), table.Float(row, faded + i)));
		}
	}
	EXPECT_EQ(differences.Count(), 0u);
}
