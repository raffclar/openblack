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
#include <optional>
#include <string>
#include <vector>

#include <glm/vec2.hpp>
#include <gtest/gtest.h>

#include "3D/BlockTexture.h"
#include "3D/VillageLights.h"
#include "Animals/GrazerRules.h"
#include "Animals/Zoomer.h"
#include "Audio/AtmosAudio.h"
#include "Audio/LipSync.h"
#include "Audio/SoundMap.h"
#include "OracleData.h"

// The game's own functions give the data (tools/oracle/cases/float_sweep_3_cases.py). Each test compares our result
// with every row, bit for bit, and shows the first rows that differ.

using namespace openblack;

namespace
{
constexpr size_t k_FailuresShown = 20;

/// Loads a pinned table and finds its columns, in the order of the names
struct Pinned
{
	std::optional<oracle::Table> table;
	std::vector<size_t> columns;

	Pinned(std::string_view name, std::initializer_list<std::string_view> names)
	    : table(oracle::Table::Load(name))
	{
		if (!table.has_value())
		{
			return;
		}
		for (const auto column : names)
		{
			const auto index = table->Column(column);
			if (!index.has_value())
			{
				table.reset();
				return;
			}
			columns.push_back(*index);
		}
	}

	[[nodiscard]] uint32_t Bits(size_t row, size_t column) const { return table->Bits(row, columns.at(column)); }
	[[nodiscard]] float Float(size_t row, size_t column) const { return table->Float(row, columns.at(column)); }
	[[nodiscard]] int32_t Int(size_t row, size_t column) const { return static_cast<int32_t>(Bits(row, column)); }
};

/// Counts the rows that differ and shows the first of them
class Differences
{
public:
	void Add(const std::string& what)
	{
		++_count;
		if (_count <= k_FailuresShown)
		{
			ADD_FAILURE() << what;
		}
	}
	[[nodiscard]] size_t Count() const { return _count; }

private:
	size_t _count {0};
};
} // namespace

TEST(OracleFloatSweep3, TheConeWeightsAreTheGameTable)
{
	const Pinned pinned("cone_weights.txt", {"texel", "w0", "w1", "w2", "w3"});
	ASSERT_TRUE(pinned.table.has_value());
	ASSERT_EQ(pinned.table->Rows(), 256u);
	const auto weights = block_texture::ComputeConeWeights();
	Differences differences;
	for (size_t row = 0; row < pinned.table->Rows(); ++row)
	{
		const auto texel = pinned.Bits(row, 0);
		for (size_t corner = 0; corner < 4; ++corner)
		{
			const auto ours = weights.at(texel).at(corner);
			if (ours != pinned.Bits(row, corner + 1))
			{
				differences.Add("texel " + std::to_string(texel) + " corner " + std::to_string(corner) + ": the game gives " +
				                std::to_string(pinned.Bits(row, corner + 1)) + ", ours gives " + std::to_string(ours));
			}
		}
	}
	EXPECT_EQ(differences.Count(), 0u);
}

TEST(OracleFloatSweep3, TheLipSyncTransformIsTheGameTransform)
{
	const Pinned pinned("fft.txt", {"case", "n", "sign", "index", "input", "output"});
	ASSERT_TRUE(pinned.table.has_value());
	ASSERT_GT(pinned.table->Rows(), 1000u);
	Differences differences;
	size_t row = 0;
	while (row < pinned.table->Rows())
	{
		const auto n = pinned.Int(row, 1);
		const bool forward = pinned.Int(row, 2) == 1;
		std::vector<float> data(static_cast<size_t>(2 * n));
		for (size_t i = 0; i < data.size(); ++i)
		{
			data[i] = pinned.Float(row + i, 4);
		}
		audio::lip_sync::Fft(data, n, forward);
		for (size_t i = 0; i < data.size(); ++i)
		{
			if (std::bit_cast<uint32_t>(data[i]) != pinned.Bits(row + i, 5))
			{
				differences.Add("case " + std::to_string(pinned.Bits(row, 0)) + " index " + std::to_string(i) +
				                ": the game gives " + oracle::Hex(pinned.Float(row + i, 5)) + ", ours gives " +
				                oracle::Hex(data[i]));
			}
		}
		row += data.size();
	}
	EXPECT_EQ(differences.Count(), 0u);
}

TEST(OracleFloatSweep3, TheSkyTypeIsTheGameSkyType)
{
	const Pinned pinned("sky_type.txt", {"night_full", "dusk_start", "dusk_end", "day_full", "time", "sky"});
	ASSERT_TRUE(pinned.table.has_value());
	ASSERT_GT(pinned.table->Rows(), 500u);
	Differences differences;
	for (size_t row = 0; row < pinned.table->Rows(); ++row)
	{
		const DayNightTimes times {.nightFull = pinned.Float(row, 0),
		                           .duskStart = pinned.Float(row, 1),
		                           .duskEnd = pinned.Float(row, 2),
		                           .dayFull = pinned.Float(row, 3)};
		const auto ours = audio::AtmosAudio::CalculateSkyType(pinned.Float(row, 4), times);
		if (std::bit_cast<uint32_t>(ours) != pinned.Bits(row, 5))
		{
			differences.Add("time " + oracle::Hex(pinned.Float(row, 4)) + ": the game gives " +
			                oracle::Hex(pinned.Float(row, 5)) + ", ours gives " + oracle::Hex(ours));
		}
	}
	EXPECT_EQ(differences.Count(), 0u);
}

TEST(OracleFloatSweep3, TheVillageLightIntensityIsTheGameIntensity)
{
	const Pinned pinned("village_light.txt", {"hour", "intensity"});
	ASSERT_TRUE(pinned.table.has_value());
	ASSERT_GT(pinned.table->Rows(), 300u);
	Differences differences;
	for (size_t row = 0; row < pinned.table->Rows(); ++row)
	{
		const auto ours = village_lights::Intensity(pinned.Float(row, 0));
		if (std::bit_cast<uint32_t>(ours) != pinned.Bits(row, 1))
		{
			differences.Add("hour " + oracle::Hex(pinned.Float(row, 0)) + ": the game gives " +
			                oracle::Hex(pinned.Float(row, 1)) + ", ours gives " + oracle::Hex(ours));
		}
	}
	EXPECT_EQ(differences.Count(), 0u);
}

TEST(OracleFloatSweep3, TheAmbienceAlignmentIsTheGameValue)
{
	const Pinned pinned("atmos_alignment.txt", {"alignment", "goodness", "value"});
	ASSERT_TRUE(pinned.table.has_value());
	ASSERT_GT(pinned.table->Rows(), 300u);
	Differences differences;
	for (size_t row = 0; row < pinned.table->Rows(); ++row)
	{
		const auto ours = audio::AtmosAudio::CalculateAlignmentValue(pinned.Float(row, 0));
		if (std::bit_cast<uint32_t>(ours) != pinned.Bits(row, 2))
		{
			differences.Add("alignment " + oracle::Hex(pinned.Float(row, 0)) + ": the game gives " +
			                oracle::Hex(pinned.Float(row, 2)) + ", ours gives " + oracle::Hex(ours));
		}
	}
	EXPECT_EQ(differences.Count(), 0u);
}

TEST(OracleFloatSweep3, TheZoomerGlidesAsTheGameZoomer)
{
	const Pinned pinned("zoomer.txt", {"case", "kind", "a", "b", "c", "value", "speed"});
	ASSERT_TRUE(pinned.table.has_value());
	ASSERT_GT(pinned.table->Rows(), 1000u);
	Differences differences;
	animals::Zoomer zoomer;
	for (size_t row = 0; row < pinned.table->Rows(); ++row)
	{
		switch (pinned.Bits(row, 1))
		{
		case 0:
			zoomer = animals::Zoomer(pinned.Float(row, 2));
			break;
		case 1:
			zoomer.SetTarget(pinned.Float(row, 2), pinned.Float(row, 3), pinned.Float(row, 4));
			break;
		default:
			zoomer.Step(pinned.Float(row, 2));
			break;
		}
		if (std::bit_cast<uint32_t>(zoomer.Value()) != pinned.Bits(row, 5) ||
		    std::bit_cast<uint32_t>(zoomer.Speed()) != pinned.Bits(row, 6))
		{
			differences.Add("case " + std::to_string(pinned.Bits(row, 0)) + " row " + std::to_string(row) +
			                ": the game gives " + oracle::Hex(pinned.Float(row, 5)) + " at " +
			                oracle::Hex(pinned.Float(row, 6)) + ", ours gives " + oracle::Hex(zoomer.Value()) + " at " +
			                oracle::Hex(zoomer.Speed()));
		}
	}
	EXPECT_EQ(differences.Count(), 0u);
}

TEST(OracleFloatSweep3, TheGrazerScaleAndSteerAreTheGameMaths)
{
	const Pinned pinned("grazer_steer.txt", {"kind", "a", "b", "c", "d", "e", "result", "x", "z"});
	ASSERT_TRUE(pinned.table.has_value());
	ASSERT_GT(pinned.table->Rows(), 500u);
	Differences differences;
	for (size_t row = 0; row < pinned.table->Rows(); ++row)
	{
		if (pinned.Bits(row, 0) == 0)
		{
			const auto ours = animals::grazers::Scale(pinned.Int(row, 1), pinned.Int(row, 2), pinned.Int(row, 3));
			if (ours != pinned.Int(row, 6))
			{
				differences.Add("Scale(" + std::to_string(pinned.Int(row, 1)) + ", " + std::to_string(pinned.Int(row, 2)) +
				                ", " + std::to_string(pinned.Int(row, 3)) + "): the game gives " +
				                std::to_string(pinned.Int(row, 6)) + ", ours gives " + std::to_string(ours));
			}
			continue;
		}
		glm::ivec2 step {pinned.Int(row, 2), pinned.Int(row, 3)};
		const glm::ivec2 push {pinned.Int(row, 4), pinned.Int(row, 5)};
		const bool usedUp = animals::grazers::AddSteer(step, push, static_cast<uint16_t>(pinned.Bits(row, 1)));
		if (usedUp != (pinned.Bits(row, 6) != 0) || step.x != pinned.Int(row, 7) || step.y != pinned.Int(row, 8))
		{
			differences.Add("steer row " + std::to_string(row) + ": the game gives (" + std::to_string(pinned.Int(row, 7)) +
			                ", " + std::to_string(pinned.Int(row, 8)) + "), ours gives (" + std::to_string(step.x) + ", " +
			                std::to_string(step.y) + ")");
		}
	}
	EXPECT_EQ(differences.Count(), 0u);
}

TEST(OracleFloatSweep3, TheSoundScanDistanceIsTheGameDistance)
{
	const Pinned pinned("sound_scan.txt",
	                    {"centre_x", "centre_z", "cell_x", "cell_z", "distance_squared", "metres_x", "metres_z"});
	ASSERT_TRUE(pinned.table.has_value());
	ASSERT_GT(pinned.table->Rows(), 500u);
	Differences differences;
	for (size_t row = 0; row < pinned.table->Rows(); ++row)
	{
		const auto ours =
		    audio::sound_map::DistanceSquared(pinned.Int(row, 0), pinned.Int(row, 1), pinned.Int(row, 2), pinned.Int(row, 3));
		const auto metresX = audio::sound_map::WholeMetres(pinned.Int(row, 2));
		const auto metresZ = audio::sound_map::WholeMetres(pinned.Int(row, 3));
		if (std::bit_cast<uint32_t>(ours) != pinned.Bits(row, 4) || metresX != pinned.Bits(row, 5) ||
		    metresZ != pinned.Bits(row, 6))
		{
			differences.Add("row " + std::to_string(row) + ": the game gives " + oracle::Hex(pinned.Float(row, 4)) +
			                ", ours gives " + oracle::Hex(ours));
		}
	}
	EXPECT_EQ(differences.Count(), 0u);
}
