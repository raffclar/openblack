/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstddef>
#include <cstdint>

#include <bit>

#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Camera/ScriptCameraModel.h"
#include "Gui/WorldTextDepth.h"
#include "OracleData.h"

using namespace openblack;

// The depth of a label is the game's to the bit: the pitch from the camera's origin and focus, then the pull towards the
// camera
TEST(OracleWorldTextDepth, DrawnDepthAsInTheGame)
{
	const auto table = oracle::Table::Load("world_text_depth.txt");
	ASSERT_TRUE(table.has_value());
	const auto ox = table->Column("origin_x");
	const auto oy = table->Column("origin_y");
	const auto oz = table->Column("origin_z");
	const auto fx = table->Column("focus_x");
	const auto fy = table->Column("focus_y");
	const auto fz = table->Column("focus_z");
	const auto nearClip = table->Column("near");
	const auto depth = table->Column("depth");
	const auto drawn = table->Column("drawn");
	ASSERT_TRUE(ox && oy && oz && fx && fy && fz && nearClip && depth && drawn);
	ASSERT_GT(table->Rows(), 0u);
	size_t different = 0;
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		const glm::vec3 origin {table->Float(row, *ox), table->Float(row, *oy), table->Float(row, *oz)};
		const glm::vec3 focus {table->Float(row, *fx), table->Float(row, *fy), table->Float(row, *fz)};
		const float pitch = script_camera::HeadingAndPitchFromPoints(origin, focus).pitch;
		const float ours = gui::world_text::DrawnDepth(table->Float(row, *depth), pitch, table->Float(row, *nearClip));
		const auto game = table->Bits(row, *drawn);
		if (std::bit_cast<uint32_t>(ours) != game)
		{
			++different;
			if (different <= 20)
			{
				ADD_FAILURE() << "row " << row << ": the game gives " << std::bit_cast<float>(game) << ", ours gives " << ours;
			}
		}
	}
	EXPECT_EQ(different, 0u);
}
