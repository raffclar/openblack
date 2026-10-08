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
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "Enums.h"

namespace openblack::ecs::components
{

/// A flock of animals (invisible, only simulation data). Made by CREATE_FLOCK or, for an animal created without one,
/// its own. The herd's AI is in ECS/AnimalAI.h, a script's flock of villagers or animals in ECS/Flocks.h and
/// ECS/ScriptContainers.h: `members` is the original's list reversed, so the leader (the tail) is the first member.
struct Flock
{
	/// The script's id (CREATE_ANIMAL / CREATE_NEW_ANIMAL look flocks up by it); an animal's own flock has none
	int32_t id {-1};
	/// The flock info and the player CREATE_FLOCK gives it; none for an animal's own flock. (pending) their readers
	std::optional<uint32_t> info;
	std::optional<PlayerNames> player;
	glm::vec3 domainCentre {0.0f};      ///< CREATE_FLOCK's second position
	glm::vec3 savedDomainCentre {0.0f}; ///< The position it was created at
	uint16_t domainRadius {0x50};
	uint16_t flockDistance {0x1E}; ///< An animal's own flock: (int)info.flockDistance
	entt::entity town {entt::null};
	/// The original's list reversed: front() the tail, back() the head; ecs::flocks::AddMember keeps the list
	/// ascending by the living's order key from the head
	std::vector<entt::entity> members;
	uint32_t maxMembers {0}; ///< The most members it has had
	/// the birds' flight (docs/bw1-notes/animals.md): the flock altitude (0: info.altitudeNormal), the followers'
	/// state, the state after a leg, the follow mode (2 next member, 3 formation, 7 landing)
	float altitude {0.0f};
	uint8_t followState {43};
	uint8_t afterMove {0};
	uint8_t followMode {0};
	/// The leader's turns since it last moved the herd (checked against stayTime to keep the leader in its domain)
	uint32_t leaderTurns {0};
	/// CHANGE_INNER_OUTER_PROPERTIES' calm, truncated to an integer. (pending) its reader
	int32_t calm {0};
};

} // namespace openblack::ecs::components
