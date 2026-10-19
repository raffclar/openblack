/******************************************************************************
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

#include "Gui/LogoEffect.h"
#include "OracleData.h"

using namespace openblack;
using namespace openblack::logo_effect;

namespace
{

/// The same float, or both zero of either sign
bool Same(float ours, uint32_t game)
{
	return std::bit_cast<uint32_t>(ours) == game || (ours == 0.0f && std::bit_cast<float>(game) == 0.0f);
}

/// The column, or a failure that names it
size_t ColumnOf(const oracle::Table& table, const std::string& name)
{
	const auto column = table.Column(name);
	EXPECT_TRUE(column.has_value()) << name;
	return column.value_or(0);
}

glm::vec3 Vec3(const oracle::Table& table, size_t row, size_t first)
{
	return {table.Float(row, first), table.Float(row, first + 1), table.Float(row, first + 2)};
}

} // namespace

TEST(OracleLogoEffect, LookAtAsTheGameWorksItOut)
{
	const auto table = oracle::Table::Load("logo_look_at.txt");
	ASSERT_TRUE(table.has_value());
	ASSERT_GT(table->Rows(), 1000U);
	const auto position = ColumnOf(*table, "px");
	const auto focus = ColumnOf(*table, "fx");
	const auto matrix = ColumnOf(*table, "m0");
	size_t different = 0;
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		const auto view = LookAt(Vec3(*table, row, position), Vec3(*table, row, focus));
		const std::array ours {view.right.x, view.up.x, view.forward.x, view.right.y,  view.up.y,     view.forward.y,
		                       view.right.z, view.up.z, view.forward.z, view.offset.x, view.offset.y, view.offset.z};
		for (size_t i = 0; i < ours.size(); ++i)
		{
			if (!Same(ours.at(i), table->Bits(row, matrix + i)))
			{
				if (++different <= 5)
				{
					ADD_FAILURE() << "row " << row << " value " << i << ": ours " << ours.at(i) << ", the game's "
					              << table->Float(row, matrix + i);
				}
				break;
			}
		}
	}
	EXPECT_EQ(different, 0U);
}

TEST(OracleLogoEffect, LensAsTheGameWorksItOut)
{
	const auto table = oracle::Table::Load("logo_lens.txt");
	ASSERT_TRUE(table.has_value());
	const auto fov = ColumnOf(*table, "fov");
	const auto nearClip = ColumnOf(*table, "near");
	const auto aspect = ColumnOf(*table, "aspect");
	const auto scaleX = ColumnOf(*table, "scale_x");
	size_t different = 0;
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		const auto lens = LensOf(table->Float(row, fov), table->Float(row, nearClip), table->Float(row, aspect));
		const std::array ours {lens.clipScaleX, lens.clipScaleY, lens.extentX, lens.extentY};
		for (size_t i = 0; i < ours.size(); ++i)
		{
			if (!Same(ours.at(i), table->Bits(row, scaleX + i)) && ++different <= 5)
			{
				ADD_FAILURE() << "row " << row << " value " << i << ": ours " << ours.at(i) << ", the game's "
				              << table->Float(row, scaleX + i);
			}
		}
	}
	EXPECT_EQ(different, 0U);
}

TEST(OracleLogoEffect, ProjectionAsTheGameWorksItOut)
{
	const auto table = oracle::Table::Load("logo_project.txt");
	ASSERT_TRUE(table.has_value());
	const auto clip = ColumnOf(*table, "c0");
	const auto nearClip = ColumnOf(*table, "near");
	const auto width = ColumnOf(*table, "width");
	const auto point = ColumnOf(*table, "x");
	const auto result = ColumnOf(*table, "result");
	size_t different = 0;
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		ClipMatrix matrix {};
		for (size_t axis = 0; axis < 4; ++axis)
		{
			// The game's matrix has a column for each of across, up and depth
			matrix.across.at(axis) = table->Float(row, clip + (axis * 3));
			matrix.up.at(axis) = table->Float(row, clip + (axis * 3) + 1);
			matrix.depth.at(axis) = table->Float(row, clip + (axis * 3) + 2);
		}
		const Screen screen {.width = static_cast<int32_t>(table->Bits(row, width)),
		                     .height = static_cast<int32_t>(table->Bits(row, width + 1))};
		const auto ours = Project(matrix, table->Float(row, nearClip), screen, Vec3(*table, row, point));
		const auto game = table->Bits(row, result);
		bool same = ours.has_value() == (game != 0);
		if (same && ours.has_value())
		{
			same = (ours->onScreen ? 2U : 1U) == game && static_cast<uint32_t>(ours->pixel.x) == table->Bits(row, result + 1) &&
			       static_cast<uint32_t>(ours->pixel.y) == table->Bits(row, result + 2) &&
			       Same(ours->depth, table->Bits(row, result + 3));
		}
		if (!same && ++different <= 5)
		{
			ADD_FAILURE() << "row " << row << " differs";
		}
	}
	EXPECT_EQ(different, 0U);
}

TEST(OracleLogoEffect, UnprojectionAsTheGameWorksItOut)
{
	const auto table = oracle::Table::Load("logo_unproject.txt");
	ASSERT_TRUE(table.has_value());
	const auto axes = ColumnOf(*table, "a0");
	const auto camera = ColumnOf(*table, "cx");
	const auto extent = ColumnOf(*table, "extent_x");
	const auto nearClip = ColumnOf(*table, "near");
	const auto width = ColumnOf(*table, "width");
	const auto pixel = ColumnOf(*table, "pixel_x");
	const auto depth = ColumnOf(*table, "depth");
	const auto result = ColumnOf(*table, "x");
	size_t different = 0;
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		// The axes by rows: right, up and forward across each row
		const auto at = [&](size_t i) { return table->Float(row, axes + i); };
		const View view {.position = Vec3(*table, row, camera),
		                 .right = {at(0), at(3), at(6)},
		                 .up = {at(1), at(4), at(7)},
		                 .forward = {at(2), at(5), at(8)},
		                 .offset = glm::vec3(0.0f)};
		const Lens lens {.extentX = table->Float(row, extent),
		                 .extentY = table->Float(row, extent + 1),
		                 .nearClip = table->Float(row, nearClip),
		                 .clipScaleX = 1.0f,
		                 .clipScaleY = 1.0f};
		const Screen screen {.width = static_cast<int32_t>(table->Bits(row, width)),
		                     .height = static_cast<int32_t>(table->Bits(row, width + 1))};
		const glm::ivec2 place {static_cast<int32_t>(table->Bits(row, pixel)),
		                        static_cast<int32_t>(table->Bits(row, pixel + 1))};
		const auto ours = ScreenToWorld(view, lens, screen, place, table->Float(row, depth));
		if ((!Same(ours.x, table->Bits(row, result)) || !Same(ours.y, table->Bits(row, result + 1)) ||
		     !Same(ours.z, table->Bits(row, result + 2))) &&
		    ++different <= 5)
		{
			ADD_FAILURE() << "row " << row << ": ours " << ours.x << " " << ours.y << " " << ours.z;
		}
	}
	EXPECT_EQ(different, 0U);
}
