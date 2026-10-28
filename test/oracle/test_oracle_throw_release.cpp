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

#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Hand/HandGrabRules.h"
#include "Hand/ThrowLead.h"
#include "OracleData.h"

using namespace openblack;

namespace
{

[[nodiscard]] uint32_t CellBits(const oracle::Table& table, size_t row, const std::string& name)
{
	return table.Bits(row, table.Column(name).value_or(0));
}

[[nodiscard]] glm::vec3 Vector(const oracle::Table& table, size_t row, const std::string& prefix)
{
	return {std::bit_cast<float>(CellBits(table, row, prefix + "x")), std::bit_cast<float>(CellBits(table, row, prefix + "y")),
	        std::bit_cast<float>(CellBits(table, row, prefix + "z"))};
}

} // namespace

// The game's release, row by row: the velocity that the throw sends, whether the release is fast, and the turns of lead
// from the latency of the reports
TEST(OracleThrowRelease, TheVelocityFastAndLeadAreTheGames)
{
	const auto loaded = oracle::Table::Load("throw_release.txt");
	ASSERT_TRUE(loaded.has_value());
	const auto& table = *loaded;
	ASSERT_GT(table.Rows(), 2000U);
	size_t mismatches = 0;
	for (size_t row = 0; row < table.Rows(); ++row)
	{
		const auto velocity = Vector(table, row, "v");
		const auto sent = hand_grab::ThrowVelocity(velocity, false, glm::vec3(0.0f));
		const auto expected = Vector(table, row, "out_v");
		const bool fast = CellBits(table, row, "fast") != 0;
		const int lead = static_cast<int>(CellBits(table, row, "lead_turns"));
		const bool same = std::bit_cast<uint32_t>(sent.x) == std::bit_cast<uint32_t>(expected.x) &&
		                  std::bit_cast<uint32_t>(sent.y) == std::bit_cast<uint32_t>(expected.y) &&
		                  std::bit_cast<uint32_t>(sent.z) == std::bit_cast<uint32_t>(expected.z) &&
		                  hand_grab::IsFastRelease(velocity) == fast && hand_grab::IsFastRelease(sent) == fast &&
		                  hand_grab::throw_lead::LeadTurns(CellBits(table, row, "latency_ms")) == lead;
		if (!same)
		{
			if (mismatches < 5)
			{
				ADD_FAILURE() << "row " << row << ": velocity " << sent.x << " " << sent.y << " " << sent.z << ", game "
				              << expected.x << " " << expected.y << " " << expected.z << ", lead " << lead;
			}
			++mismatches;
		}
	}
	EXPECT_EQ(mismatches, 0U);
}
