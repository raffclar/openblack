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

#include <gtest/gtest.h>

#include "Camera/CameraBookmarks.h"
#include "OracleData.h"

using namespace openblack;
using namespace openblack::camera_bookmarks;

TEST(OracleMarkerPulse, ClockAndSizesAsTheGameWorksThemOut)
{
	const auto table = oracle::Table::Load("marker_pulse.txt");
	ASSERT_TRUE(table.has_value());
	ASSERT_GT(table->Rows(), 2000U);
	const auto clock = table->Column("clock");
	const auto frame = table->Column("frame_ms");
	const auto after = table->Column("clock_after");
	const auto width = table->Column("half_width");
	const auto height = table->Column("half_height");
	ASSERT_TRUE(clock && frame && after && width && height);
	size_t different = 0;
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		const auto stepped =
		    StepPulseClock(static_cast<int32_t>(table->Bits(row, *clock)), static_cast<int32_t>(table->Bits(row, *frame)));
		const auto size = MarkerHalfSize(stepped);
		const bool same = stepped == static_cast<int32_t>(table->Bits(row, *after)) &&
		                  std::bit_cast<uint32_t>(size.x) == table->Bits(row, *width) &&
		                  std::bit_cast<uint32_t>(size.y) == table->Bits(row, *height);
		if (!same && different++ < 10)
		{
			ADD_FAILURE() << "clock " << table->Bits(row, *clock) << " frame " << table->Bits(row, *frame) << ": ours "
			              << stepped << " " << oracle::Hex(size.x) << " " << oracle::Hex(size.y);
		}
	}
	EXPECT_EQ(different, 0U);
}
