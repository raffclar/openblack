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

namespace openblack
{
/// The tutorial-skip bits of the game's flags word, cleared at every new game by the skip-tutorial question and set by
/// the answer to its SkipBox; read by the scripts through CAN_SKIP_TUTORIAL (bit 23), CAN_SKIP_CREATURE_TRAINING
/// (bit 24) and IS_KEEPING_OLD_CREATURE (bit 25)
struct TutorialSkipFlags
{
	bool canSkipTutorial {false};
	bool canSkipCreatureTraining {false};
	bool isKeepingOldCreature {false};
};
} // namespace openblack

namespace openblack::ecs::components
{
/// Values of the game's global state that the map script sets or reads (only data so far)
struct MapScriptGlobals
{
	static constexpr size_t k_MagicCount = 42;

	/// VERSION; CREATE_FLOCK reads its town from N5 from 2.1 on, else from N4
	float version {0.0f};
	/// SET_LAND_NUMBER: 0 until the script sets it
	int32_t landNumber {0};
	/// SET_TOWN_INFLUENCE_MULTIPLIER / SET_PLAYER_INFLUENCE_MULTIPLIER: back to 1 before the map script runs (read by
	/// the towns' and the citadels' influence)
	float townInfluenceMultiplier {1.0f};
	float playerInfluenceMultiplier {1.0f};
	/// FIRE_FLY_SPELL_REWARD_PROB, by magic type (the first magic effect whose name matches without case; an unknown
	/// name gives 42 and is dropped). Not reset between lands.
	std::array<float, k_MagicCount> fireFlySpellRewardProbability {};
	/// The running sums of the table above, remade on every change
	std::array<float, k_MagicCount> fireFlySpellRewardCumulative {};
	/// The land's balances, all 1 unless its script sets them (SET_GLOBAL_LAND_BALANCE, by index): 4 the villagers'
	/// speed, 5 the wood value of trees, 7 the belief speed. Read, written and reset through the land balance service
	/// (land_balance::), back to 1 when a land loads
	std::array<float, 8> landBalance {1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};
	/// How much of a town's boredom with each kind of reaction wears off at each of its turns, 1 unless the land's script
	/// sets it (SET_LOST_TOWN_SCALE). Through the land balance service too, and reset with landBalance
	float lostTownScale {1.0f};
	/// Not script state: the game's start-up writes them (the skip-tutorial question and its answer) and the scripts
	/// only read them
	TutorialSkipFlags tutorialSkipFlags;
};
} // namespace openblack::ecs::components
