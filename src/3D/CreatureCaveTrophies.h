/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <array>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "Enums.h"

namespace openblack
{

/// The trophies in the creature's room: belts hung on its attack dummies for how it fights, medals on its magic plinths
/// for how well it has learnt its miracles, and the seeds of its four best-learnt miracles hovering by the plinths
namespace CreatureCaveTrophies
{

/// The icons the game loads from data/citadel/icons: four of each of the seven belts, white to black,
/// then five of each of the five medals, wood to gem
constexpr size_t k_IconCount = 53;
constexpr uint32_t k_BeltColours = 7;
constexpr uint32_t k_BeltLevels = 4;
constexpr uint32_t k_MedalLevels = 25;
/// The belts' and medals' colours
constexpr uint32_t k_BeltColour = 0xFFA0A0A0;
constexpr uint32_t k_MedalColour = 0xFF808080;
/// The medals for the miracles best learnt, after the one for all of them, and the seeds of the same miracles
constexpr size_t k_BestMiracles = 4;
/// The points of the room's mesh the seeds hover at, the best-learnt miracle's first, every other point
constexpr uint32_t k_FirstSeedPoint = 7;
constexpr uint32_t k_SeedPointStep = 2;

/// The name of an icon's mesh, without its extension
[[nodiscard]] std::string IconName(uint32_t icon);

/// An icon shown at a point of the creature's room's mesh
struct Trophy
{
	uint32_t point;
	uint32_t icon;
	bool medal;
	/// Drawn with an environment map added: every belt and every medal past wood
	bool environmentMapped;
};

/// How far the creature has learnt a miracle it knows about, by the miracle's place in the game's table of miracles
struct MiracleLearnt
{
	uint32_t miracle;
	/// 0 to 100
	float percent;
};

/// How far a miracle is learnt, 0 to 100: the times it was seen over the times it must be seen, at most all of it
[[nodiscard]] float PercentLearnt(uint32_t timesSeen, float timesNeeded);

/// How well the creature has learnt its miracles, from 0 to 100: all of them together, and the best four, best first,
/// with the miracle each is (none for a place no miracle learnt at all has taken)
struct MiracleLearning
{
	float overall {0.0f};
	std::array<float, k_BestMiracles> best {};
	std::array<std::optional<uint32_t>, k_BestMiracles> bestMiracles {};
};

/// As the creature's room's scroll of miracles works it out, in the order of the game's table: the percentages added
/// together and taken over 42, and the four best, each taking a place only by beating the fourth, ties keeping the
/// earlier miracle
[[nodiscard]] MiracleLearning LearningOf(std::span<const MiracleLearnt> miracles);

/// A medal's level, 0 for none to 25 for the last gem, by the percentage it stands for
[[nodiscard]] int32_t MedalLevel(float percent);

/// The belts and medals shown. Fight balance is from -1 to 1, as the creature keeps it: a row of belts each way fills,
/// white first, five a colour, as it leans to its side.
[[nodiscard]] std::vector<Trophy> Choose(float fightBalance, const MiracleLearning& learning);

/// The point of the room's mesh the seed in a place by the plinths hovers at
[[nodiscard]] constexpr uint32_t SeedPoint(size_t place)
{
	return k_FirstSeedPoint + (k_SeedPointStep * static_cast<uint32_t>(place));
}

/// The seeds to make by the plinths: in each place that has none yet, the seed of the miracle learnt that well, if it has
/// one. A seed once made keeps its place until the player leaves the temple, whatever the creature learns meanwhile.
[[nodiscard]] std::array<std::optional<SpellSeedType>, k_BestMiracles>
SeedsToMake(const std::array<bool, k_BestMiracles>& shown, const std::array<SpellSeedType, k_BestMiracles>& seeds);

} // namespace CreatureCaveTrophies

} // namespace openblack
