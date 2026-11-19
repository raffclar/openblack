/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <optional>
#include <span>
#include <string>
#include <string_view>

/// The choices of a generated land and its path. GeneratedLand.h makes the land from them.
namespace openblack::generated_land
{
/// The choices of the land generator that the editor shows. Each choice is a whole number, so that the path of the land
/// keeps it exactly. The first values of the choices give the first settings of the generator.
struct Choices
{
	uint64_t seed {1};
	/// How much of the map the island takes, in hundredths, from -30 (small) to 30 (large)
	int32_t landAmount {0};
	/// How many mountain ranges cross the island
	int32_t ranges {0};
	/// The altitude of a crest of a range before the erosion, and of the plains and the hills
	int32_t mountainHeight {300};
	int32_t plainHeight {42};
	int32_t hillHeight {100};
	/// How many hollows of the low land hold the lakes and lagoons
	int32_t hollows {6};
	/// The drops of rain of the erosion, in thousands
	int32_t rain {400};
	/// A corner is on a river when the water of this many corners flows through it
	int32_t riverFlow {900};
	/// The temperature at the north edge and at the south edge of the map, in hundredths
	int32_t northTemperature {22};
	int32_t southTemperature {95};
	/// The direction of the wind, in the order of landgen::climate::Wind, or -1 when the seed picks the direction
	int32_t wind {-1};
	/// How many massifs the island has, or -1 when the seed picks the number
	int32_t massifs {-1};
	/// The height in metres of the highest ground that the land can have. The lands of the story stop at 171 m.
	int32_t topHeight {1250};
	/// How many stands of trees the land has, in hundredths of the usual stands. Only for tests of the frame rate.
	int32_t trees {100};
	/// The type of the land, in the order of landgen::landtypes::Type, or -1 when the seed picks the type
	int32_t landType {-1};
	/// 1 lets the land be an archipelago: the seed can pick it, and the archipelago type makes it. With 0, the seed picks
	/// another type. The user did not agree to archipelagos yet, so the first value is 0.
	int32_t archipelago {0};

	bool operator==(const Choices&) const = default;
};

/// The land type of an archipelago, which needs the archipelago choice
constexpr int32_t k_ArchipelagoType = 4;

/// The range of a choice, the name that the path of the land gives it, and the name that the editor shows
struct ChoiceRange
{
	std::string_view key;
	std::string_view label;
	int32_t Choices::* member;
	int32_t lowest;
	int32_t highest;
};

/// The range of each choice other than the seed
[[nodiscard]] std::span<const ChoiceRange> ChoiceRanges();
/// The choices with each value moved into its range
[[nodiscard]] Choices Clamped(Choices choices);

/// A generated land has no file. Its path names its seed and each choice that is not at its first value, so that the
/// game can make the land again: "landgen:7" or "landgen:7;ranges=5;rain=600".
constexpr std::string_view k_PathPrefix = "landgen:";
[[nodiscard]] std::string PathOf(const Choices& choices);
[[nodiscard]] std::string PathOf(uint64_t seed);
/// The choices of the path of a generated land, or nothing for another path or for a value out of its range
[[nodiscard]] std::optional<Choices> ChoicesOf(std::string_view path);
/// The seed of the path of a generated land, or nothing for another path
[[nodiscard]] std::optional<uint64_t> SeedOf(std::string_view path);
} // namespace openblack::generated_land
