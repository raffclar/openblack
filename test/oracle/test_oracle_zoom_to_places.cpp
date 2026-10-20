/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <algorithm>
#include <array>
#include <bit>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Camera/ZoomToPlaces.h"
#include "Common/X87.h"
#include "OracleData.h"

using namespace openblack;

namespace
{
/// The fake altitude of the data: (map x + map z) times this float, each step rounded to a float
constexpr float k_AltitudeScale = std::bit_cast<float>(0x35C80000u);

[[nodiscard]] float FakeAltitude(int32_t mapX, int32_t mapZ)
{
	return x87::Mul(x87::RoundToFloat(static_cast<double>(mapX) + static_cast<double>(mapZ)), k_AltitudeScale);
}

[[nodiscard]] glm::vec3 Vec(const oracle::Table& table, size_t row, const std::array<size_t, 3>& columns)
{
	return {table.Float(row, columns[0]), table.Float(row, columns[1]), table.Float(row, columns[2])};
}

[[nodiscard]] std::array<size_t, 3> Columns(const oracle::Table& table, std::string_view x, std::string_view y,
                                            std::string_view z)
{
	return {table.Column(x).value(), table.Column(y).value(), table.Column(z).value()};
}

[[nodiscard]] std::vector<uint32_t> Bits(const glm::vec3& value)
{
	return {std::bit_cast<uint32_t>(value.x), std::bit_cast<uint32_t>(value.y), std::bit_cast<uint32_t>(value.z)};
}
} // namespace

// The temple and realm keys start the same flights as the game and keep the same state, bit for bit
TEST(OracleZoomToPlaces, TheKeysAsInTheGame)
{
	const auto table = oracle::Table::Load("zoom_to_places.txt");
	ASSERT_TRUE(table.has_value());
	ASSERT_GT(table->Rows(), 0u);
	const auto origin = Columns(*table, "ox", "oy", "oz");
	const auto focus = Columns(*table, "fx", "fy", "fz");
	const auto savedOrigin = Columns(*table, "sox", "soy", "soz");
	const auto savedFocus = Columns(*table, "sfx", "sfy", "sfz");
	const auto flightOrigin = Columns(*table, "rox", "roy", "roz");
	const auto flightFocus = Columns(*table, "rfx", "rfy", "rfz");
	const auto savedOriginAfter = Columns(*table, "sox_after", "soy_after", "soz_after");
	const auto savedFocusAfter = Columns(*table, "sfx_after", "sfy_after", "sfz_after");
	const auto column = [&table](std::string_view name) { return table->Column(name).value(); };

	size_t different = 0;
	for (size_t row = 0; row < table->Rows(); ++row)
	{
		const zoom_to::ZoomToPlaces::State before {
		    .lastTap = table->Bits(row, column("last")),
		    .flownTo = table->Bits(row, column("state")) != 0,
		    .returnTo = {.origin = Vec(*table, row, savedOrigin), .focus = Vec(*table, row, savedFocus)}};
		zoom_to::ZoomToPlaces keys(before);
		const bool fight = table->Bits(row, column("fight")) != 0;
		const zoom_to::Press press {.now = table->Bits(row, column("now")),
		                            .current = {.origin = Vec(*table, row, origin), .focus = Vec(*table, row, focus)},
		                            .place = {table->Float(row, column("px")), table->Float(row, column("pz"))},
		                            .distance = table->Float(row, column("distance")),
		                            .pitch = table->Float(row, column("pitch")),
		                            .tapAllowed = table->Bits(row, column("tap")) != 0,
		                            .clickDistance = fight ? std::nullopt : std::optional(table->Float(row, column("click")))};
		const auto flight = keys.Act(press, FakeAltitude);

		std::vector<uint32_t> ours {flight.has_value() ? 1u : 0u};
		std::vector<uint32_t> game {table->Bits(row, column("flights"))};
		if (flight.has_value() && game[0] != 0)
		{
			for (const auto& part : {Bits(flight->view.origin), Bits(flight->view.focus)})
			{
				ours.insert(ours.end(), part.begin(), part.end());
			}
			ours.push_back(std::bit_cast<uint32_t>(flight->rise));
			for (const auto& part : {Bits(Vec(*table, row, flightOrigin)), Bits(Vec(*table, row, flightFocus))})
			{
				game.insert(game.end(), part.begin(), part.end());
			}
			game.push_back(table->Bits(row, column("rise")));
		}
		const auto& after = keys.GetState();
		ours.push_back(after.flownTo ? 1u : 0u);
		ours.push_back(after.lastTap);
		for (const auto& part : {Bits(after.returnTo.origin), Bits(after.returnTo.focus)})
		{
			ours.insert(ours.end(), part.begin(), part.end());
		}
		game.push_back(table->Bits(row, column("state_after")));
		game.push_back(table->Bits(row, column("last_after")));
		for (const auto& part : {Bits(Vec(*table, row, savedOriginAfter)), Bits(Vec(*table, row, savedFocusAfter))})
		{
			game.insert(game.end(), part.begin(), part.end());
		}
		if (ours != game)
		{
			++different;
			if (different <= 20)
			{
				std::string text;
				for (size_t i = 0; i < std::max(ours.size(), game.size()); ++i)
				{
					text += " " + std::to_string(i) + ":" +
					        (i < game.size() ? oracle::Hex(std::bit_cast<float>(game[i])) : std::string("-")) + "/" +
					        (i < ours.size() ? oracle::Hex(std::bit_cast<float>(ours[i])) : std::string("-"));
				}
				ADD_FAILURE() << "row " << row << " (game/ours):" << text;
			}
		}
	}
	EXPECT_EQ(different, 0u);
}
