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

#include <array>
#include <string_view>
#include <vector>

#include "Enums.h"

/// The personality weights that the AI soak gives to the computer gods. This is a tool of developer builds. It makes
/// gods that attack more or care for their creatures more, so that a long game covers more of the mind. The weights come
/// from a stream of their own, so the game's random streams stay as they are.
namespace openblack::computer_player::personality
{

/// A need of the mind that a personality weighs, and the weight that a god has by default
struct Aspect
{
	std::string_view name;
	float builtIn;
};

/// The needs of the middle layer of the mind. A land script can set the weight of any of them by name.
inline constexpr std::array<Aspect, 19> k_Aspects {{
    {"GiveToPlayer", 0.25f},
    {"TalktoPlayer", 0.25f},
    {"ExpandInfluence", 0.75f},
    {"Lookaftercreature", 0.4f},
    {"TeachCreature", 0.5f},
    {"PlayWithCreature", 0.25f},
    {"ReactToCreatureEmergency", 0.95f},
    {"AttendTownDesires", 0.7f},
    {"ExpandTown", 0.7f},
    {"AttendToWorshippersWorshippers", 0.7f},
    {"IncreaseTownPopulation", 0.7f},
    {"DefeatPlayer", 0.1f},
    {"PlayerWithMoreSpells", 0.1f},
    {"PlayerWithBetterCreature", 0.1f},
    {"PlayerWithMoreWorshippers", 0.1f},
    {"ReactToPlayer", 1.0f},
    {"CheckTownsForBeliefAttack", 1.0f},
    {"CheckTownForDamage", 1.0f},
    {"CheckTownForProtection", 1.0f},
}};

/// The least and the most share of its built-in weight that a god gets
constexpr float k_LeastShare = 0.25f;
constexpr float k_MostShare = 1.75f;
/// No weight is above this value
constexpr float k_HighestWeight = 1.0f;

/// A weight to set for a need
struct Weight
{
	std::string_view name;
	float value;
};

/// A stream of numbers for one god, which depends only on the seed and the god
class Stream
{
public:
	Stream(uint32_t seed, PlayerNames god) noexcept;

	/// The next number of the stream
	[[nodiscard]] uint32_t Next() noexcept;
	/// The next number as a share from 0 up to (not with) 1, in steps of 2 to the power of -24
	[[nodiscard]] float NextShare() noexcept;

private:
	uint32_t _state;
};

/// The weight of one need for a share from 0 up to 1: the built-in weight times a share between the least and the most
/// share, and not above the highest weight
[[nodiscard]] float WeightOf(const Aspect& aspect, float share) noexcept;

/// The weights of a god for a seed, one for each need, in the order of k_Aspects
[[nodiscard]] std::vector<Weight> WeightsFor(uint32_t seed, PlayerNames god);

} // namespace openblack::computer_player::personality
