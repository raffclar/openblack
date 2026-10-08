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
#include <vector>

#include <entt/entity/entity.hpp>

#include "Common/Zoomer.h"

namespace openblack::ecs::components
{

/// The miracle and worship fields of a Town, kept apart from components::Town. Assigned
/// by TownArchetype; Worship/TownMagic.cpp and Worship/WorshipPercentage.cpp own them.
struct TownMagic
{
	static constexpr size_t k_MagicTypes = 42;

	/// the town holds the magic, one flag per magic type
	std::array<bool, k_MagicTypes> held {};
	/// the TownSpellIcon list (the town centre's icons, TownCentreSpellIcon entities; a new one goes at the
	/// head, so the newest is first)
	std::vector<entt::entity> spellIcons;
	/// the WorshipSite the town belongs to
	entt::entity worshipSite {entt::null};
	/// the worship percentage 0..1 the totem drag sets
	float worshipPercentage {0.0f};
	/// villagers at the site (dancing or hiding), counted by the site
	int32_t worshipping {0};
	/// villagers on their way to it, and their list
	int32_t onWayToWorship {0};
	std::vector<entt::entity> onWayVillagers;
	/// "the script forbids a worship site" (= !SET_CAN_BUILD_WORSHIPSITE)
	bool forbidWorshipSite {false};
};

/// The worship part of a TotemStatue (on its plinth's entity, next to components::TotemStatue): the
/// percentage and the Zoomer that raises it
struct TotemWorship
{
	float percentage {0.0f};
	Zoomer rise {};      ///< the percentage moving to its new value in |change| x 5200 ms (in ms)
	bool rising {false}; ///< the rising loop (sample 0xB) plays
	int32_t lastMs {0};  ///< the engine ms of the last advance
};

} // namespace openblack::ecs::components
