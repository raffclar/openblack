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

#include <gtest/gtest.h>

#include "ECS/FixedOutlines.h"
#include "OracleData.h"

using namespace openblack;
using namespace openblack::ecs;

namespace
{

/// The same float, or both zero of either sign
bool Same(float ours, uint32_t game)
{
	return std::bit_cast<uint32_t>(ours) == game || (ours == 0.0f && std::bit_cast<float>(game) == 0.0f);
}

struct Inputs
{
	fixed_outlines::Rows rows;
	glm::vec2 place;
	glm::vec3 centre;
	glm::vec3 half;
	float scale;
	float angle;
};

Inputs InputsOf(const oracle::Table& table, size_t row)
{
	const auto value = [&](const std::string& name) { return table.Float(row, *table.Column(name)); };
	Inputs inputs {};
	for (int i = 0; i < 3; ++i)
	{
		for (int j = 0; j < 3; ++j)
		{
			inputs.rows.at(static_cast<size_t>(i))[j] = value("r" + std::to_string(i) + std::to_string(j));
		}
	}
	inputs.place = {value("tx"), value("tz")};
	inputs.centre = {value("cx"), value("cy"), value("cz")};
	inputs.half = {value("hx"), value("hy"), value("hz")};
	inputs.scale = value("scale");
	inputs.angle = value("angle");
	return inputs;
}

/// Whether a circle is the game's circle with the given column names
bool SameCircle(const oracle::Table& table, size_t row, const std::string& name, const fixed_outlines::Circle& circle)
{
	const auto bits = [&](const std::string& column) { return table.Bits(row, *table.Column(name + column)); };
	return Same(circle.centre.x, bits("x")) && Same(circle.centre.y, bits("z")) && Same(circle.radius, bits("r")) &&
	       Same(circle.radiusSquared, bits("r2"));
}

} // namespace

TEST(OracleFixedOutlines, BoxOutlinesAsTheGameMakesThem)
{
	const auto table = oracle::Table::Load("fixed_outline_box.txt");
	ASSERT_TRUE(table.has_value());
	ASSERT_GT(table->Rows(), 3000u);
	size_t different = 0;
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		const auto in = InputsOf(*table, row);
		const auto outline = fixed_outlines::OfModelBox(in.rows, in.place, in.angle, in.scale, in.centre, in.half);
		bool same =
		    SameCircle(*table, row, "b", outline.bounds) && outline.parts.size() == table->Bits(row, *table->Column("count"));
		for (size_t i = 0; same && i < outline.parts.size(); ++i)
		{
			same = SameCircle(*table, row, "p" + std::to_string(i) + "_", outline.parts.at(i));
		}
		if (!same && ++different <= 5)
		{
			ADD_FAILURE() << "row " << row << " gives another outline";
		}
	}
	EXPECT_EQ(different, 0u);
}

TEST(OracleFixedOutlines, ProbeTouchesAsTheGameSays)
{
	const auto table = oracle::Table::Load("fixed_outline_box.txt");
	ASSERT_TRUE(table.has_value());
	size_t different = 0;
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		const auto in = InputsOf(*table, row);
		const auto outline = fixed_outlines::OfModelBox(in.rows, in.place, in.angle, in.scale, in.centre, in.half);
		const glm::vec2 probe {table->Float(row, *table->Column("px")), table->Float(row, *table->Column("pz"))};
		const bool hit = fixed_outlines::Collides(outline, fixed_outlines::Probe(probe));
		if (hit != (table->Bits(row, *table->Column("hit")) != 0) && ++different <= 5)
		{
			ADD_FAILURE() << "row " << row << " gives another answer";
		}
	}
	EXPECT_EQ(different, 0u);
}

TEST(OracleFixedOutlines, TempleOutlineAsTheGameMakesIt)
{
	const auto table = oracle::Table::Load("fixed_outline_temple.txt");
	ASSERT_TRUE(table.has_value());
	ASSERT_GT(table->Rows(), 300u);
	size_t different = 0;
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		const auto in = InputsOf(*table, row);
		const auto outline = fixed_outlines::OfModelBox(in.rows, in.place, in.angle, in.scale, in.centre, in.half);
		const auto ring = fixed_outlines::TempleRing(in.place, in.angle);
		bool same = SameCircle(*table, row, "b", outline.bounds) && ring.size() == 33;
		for (size_t i = 0; same && i < ring.size(); ++i)
		{
			same = SameCircle(*table, row, "p" + std::to_string(i) + "_", ring.at(i));
		}
		if (!same && ++different <= 5)
		{
			ADD_FAILURE() << "row " << row << " gives another temple outline";
		}
	}
	EXPECT_EQ(different, 0u);
}
