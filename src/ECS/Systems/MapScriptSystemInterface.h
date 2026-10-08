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
	/// Not script state: the game's start-up writes them (the skip-tutorial question and its answer) and the scripts
	/// only read them
	TutorialSkipFlags tutorialSkipFlags;
};
} // namespace openblack

namespace openblack::ecs::systems
{
/// The map script's globals (VERSION, SET_LAND_NUMBER, the influence multipliers, the firefly rewards, the tutorial-skip
/// bits), made with the Game and kept until it goes (Locator::mapScriptSystem). The land load and the script reboot
/// reset their parts
class MapScriptSystemInterface
{
public:
	virtual ~MapScriptSystemInterface() = default;

	[[nodiscard]] virtual MapScriptGlobals& Globals() noexcept = 0;
	[[nodiscard]] virtual const MapScriptGlobals& Globals() const noexcept = 0;
};
} // namespace openblack::ecs::systems
