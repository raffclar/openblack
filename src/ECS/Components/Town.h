/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <array>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>

#include "Enums.h"

namespace openblack::ecs::components
{

struct Town
{
	uint32_t id;
	/// The player whose town it is
	PlayerNames owner {PlayerNames::NEUTRAL};
	bool uninhabitable = false;
	/// Its people without a home, the newest first
	std::vector<entt::entity> homelessVillagers;
	/// Its buildings, the newest first
	std::vector<entt::entity> abodes;
	/// When its player gained it, against the player's other towns: earlier gained, smaller
	uint32_t gained {0};
	/// How many of its people are hurt, their life under seven tenths
	uint32_t injured {0};
	/// What its people's deaths came to, by the player put down for each (by player number) and what killed them
	std::array<std::array<float, static_cast<size_t>(DeathReason::_COUNT)>, 8> deathsByKiller {};
	/// The forest of the lone trees about it, made as the land is laid out; a tree replanted near the town joins it
	std::optional<uint32_t> scenicForest;
	/// Where its scenic forest was made: the town's centre then
	glm::vec2 scenicForestCentre {0.0f};
	/// The things its people play with, the newest first: footballs
	std::vector<entt::entity> playthings;
	/// The share of its people wanted at worship, 0 to 1, set by its totem; always none while it has no worship site
	float worshipShare {0.0f};
	/// The worship site its people worship at, none until one is made for it
	entt::entity worshipSite {entt::null};
	/// A script has stopped a worship site being made for it
	bool cannotHaveWorshipSite {false};
	/// Its people counted at worship (dancing, hiding, eating or resting at its site), and those on their way there,
	/// each counted once
	std::vector<entt::entity> worshippers;
	std::vector<entt::entity> onWayToWorship;
	/// Where its people gather, worked out the first time it is wanted and again once a new building stands on it
	std::optional<glm::vec2> congregationPos;
	/// The turn its latest emergency was called, 0 when it is in none
	uint32_t emergencyTurn {0};
	/// Its worship share when an emergency stopped its worship, given back once the emergency is over; 0 when none
	float worshipShareBeforeEmergency {0.0f};
	/// Its own graveyard, where its dead are counted: the first of its graveyards to work, none until then
	entt::entity graveyard {entt::null};
	/// Its crèche, where its children spend their days: the first of its crèches to work, none until then. When it goes
	/// another of the town's working civic buildings may stand in for it.
	entt::entity creche {entt::null};
	/// Its wants have just changed (a store stocked from empty, a building site added, a death): its waiting disciples
	/// look for work again soon. Up for the rest of the turn it is raised in and the whole of the next.
	bool desiresChanged {false};
	bool desiresChangedLastTurn {false};
	/// The buildings it plans to build later (houses, civic buildings, its player's temple), in the order it planned them
	std::vector<entt::entity> planned;
	/// The game turn one of its villagers last asked it to start a planned building: it is asked at most once a turn
	std::optional<uint32_t> askedToBuildTurn;
};

} // namespace openblack::ecs::components
