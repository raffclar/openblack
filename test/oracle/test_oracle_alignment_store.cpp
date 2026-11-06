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

#include "ECS/Systems/AlignmentSystemInterface.h"
#include "OracleData.h"

// The game's own functions give the data (tools/oracle/cases/alignment_store_cases.py). Each test compares our result
// with every row, bit for bit.

using namespace openblack;
using namespace openblack::ecs::systems;

TEST(OracleAlignmentStore, TheLandKeepsTheGameValueForTheCamera)
{
	const auto table = oracle::Table::Load("alignment_store.txt");
	ASSERT_TRUE(table.has_value());
	const auto alignment = table->Column("alignment");
	const auto land = table->Column("land");
	const auto audio = table->Column("audio");
	ASSERT_TRUE(alignment.has_value() && land.has_value() && audio.has_value());
	ASSERT_GT(table->Rows(), 1000u);
	size_t wrong = 0;
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		const auto ours = alignment::LandValue(table->Float(row, *alignment));
		const auto ourAudio = alignment::AudioAlignment(ours);
		if (std::bit_cast<uint32_t>(ours) != table->Bits(row, *land) ||
		    std::bit_cast<uint32_t>(ourAudio) != table->Bits(row, *audio))
		{
			ADD_FAILURE_AT(__FILE__, __LINE__)
			    << "alignment " << oracle::Hex(table->Float(row, *alignment)) << ": the game gives "
			    << oracle::Hex(table->Float(row, *land)) << " and " << oracle::Hex(table->Float(row, *audio)) << ", ours gives "
			    << oracle::Hex(ours) << " and " << oracle::Hex(ourAudio);
			if (++wrong > 20)
			{
				return;
			}
		}
	}
}

TEST(OracleAlignmentStore, TheSkyMovesToTheLandAsTheGameSky)
{
	const auto table = oracle::Table::Load("sky_land.txt");
	ASSERT_TRUE(table.has_value());
	const auto sky = table->Column("sky");
	const auto land = table->Column("land");
	const auto milliseconds = table->Column("milliseconds");
	const auto after = table->Column("after");
	ASSERT_TRUE(sky.has_value() && land.has_value() && milliseconds.has_value() && after.has_value());
	ASSERT_GT(table->Rows(), 1000u);
	size_t wrong = 0;
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		const auto ours = alignment::SkyLandValue(table->Float(row, *sky), table->Float(row, *land),
		                                          static_cast<int64_t>(table->Bits(row, *milliseconds)));
		if (std::bit_cast<uint32_t>(ours) != table->Bits(row, *after))
		{
			ADD_FAILURE_AT(__FILE__, __LINE__)
			    << "sky " << oracle::Hex(table->Float(row, *sky)) << ", land " << oracle::Hex(table->Float(row, *land)) << ", "
			    << table->Bits(row, *milliseconds) << " ms: the game gives " << oracle::Hex(table->Float(row, *after))
			    << ", ours gives " << oracle::Hex(ours);
			if (++wrong > 20)
			{
				return;
			}
		}
	}
}
