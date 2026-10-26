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

#include <entt/entity/entity.hpp>

#include "Enums.h"

namespace openblack::ecs::components
{

/// A scaffold: a wooden frame that a workshop makes and the hand carries to a town. Its scale gives how many scaffolds it
/// is worth.
struct Scaffold
{
	/// The town that the scaffold was made for
	entt::entity town {entt::null};
	/// The workshop that counts the scaffold as its own, none when no workshop counts it
	entt::entity workshop {entt::null};
	/// Its place in the yard of its workshop, 0 to 2
	uint8_t place {0};
	/// The game turn when the scaffold came to rest after a hand let it go. It is 0 when no hand let it go since it was
	/// made or since a hand last took it.
	uint32_t putDownTurn {0};
	/// The building that a script gave the scaffold, none when no script gave one
	std::optional<AbodeInfo> scriptBuilding;
	/// The hand let the scaffold go onto the land, not by force: the scaffold comes down gently
	bool gentle {false};
	/// The player whose hand holds the scaffold or let it go last, until it comes to rest
	std::optional<PlayerNames> handPlayer;
};

} // namespace openblack::ecs::components
