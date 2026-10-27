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

#include <glm/vec3.hpp>

/// The rules by which a computer god finds how much it wants to look after its creature. Pure functions of what the god
/// sees.
namespace openblack::computer_mind_rules
{

/// What the god knows of its creature when it finds how free the creature is for the god
struct CreatureFreedomFacts
{
	/// The god holds the creature on its leash
	bool leashed {false};
	/// The game turns since the god last tied the leash
	uint32_t turnsSinceTied {0};
	bool fighting {false};
	float life {1.0f};
	float exhaustion {0.0f};
	float energy {1.0f};
	/// The plan of the creature is for its desire to get its health back
	bool restoringHealth {false};
};

/// The turns after a tie of the leash during which the creature is not free for the god
constexpr float k_LeashBusyTurns = 450.0f;
/// The part of full freedom that each turn after those turns adds
constexpr float k_LeashFreedomPerTurn = 0.0022222223f;
/// Above this share of its life, energy or rest, the creature is fully fit for the god
constexpr float k_FitShare = 0.5f;

/// How free the creature is for the god to interact with, from 0 to 1. A creature off the leash is fully free. On the
/// leash it is free only some time after the last tie, when it does not fight, and less when it is weak, tired or
/// hungry. It is not free while it tries to get its health back.
[[nodiscard]] float CreatureFreedom(const CreatureFreedomFacts& facts);

/// The god sees that its creature starves: how much, from 0 to 1, by the creature's hunger
[[nodiscard]] float StarvingWish(float hunger);
/// The number that the god keeps for the starving creature: the wish in thousandths, made whole toward 0
[[nodiscard]] int32_t StarvingAmount(float wish);
/// At this share of its life or less, the creature is badly hurt
constexpr float k_DamagedLife = 0.2f;
/// At this share of illness or more, the creature is ill
constexpr float k_IllShare = 0.5f;
/// The wish to make the creature warmer or cooler is its desire times this
constexpr float k_WarmerShare = 0.5f;
constexpr float k_CoolerShare = 0.1f;

/// A journey further than this in metres is long
constexpr float k_LongJourney = 100.0f;
/// The creature runs faster than its run speed by this part on a long journey
constexpr float k_LongJourneyRun = 1.5f;
/// The range of the random number that the game draws for a short journey
constexpr float k_ShortJourneyDraw = 0.4f;

/// The journey of the creature from its place to a place is long
[[nodiscard]] bool LongJourney(glm::vec3 creature, glm::vec3 place);
/// The speed that the creature keeps for a journey, as a part of its top speed from 0 to 1: one and a half times its
/// run speed on a long journey, else the mean of its walk speed and its run speed
[[nodiscard]] float JourneySpeed(bool longJourney, float walk, float run);

} // namespace openblack::computer_mind_rules
