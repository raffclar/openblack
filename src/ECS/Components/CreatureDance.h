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

#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

namespace openblack::ecs::components
{

/// A creature's dance with the villagers of a town, and what it remembers of its dances to impress them
struct CreatureDance
{
	/// The dance it made last, none before it made one or after the dance went
	entt::entity dance {entt::null};
	/// The villagers it called to dance with it, the last called first. A villager stays on the list after the dance
	/// stops. It leaves the list only when the hand takes it, when it flies or when it dies.
	std::vector<entt::entity> dancers;
	/// Where each dancer of the dance stood in the last turn, to tell which still move
	struct Place
	{
		entt::entity dancer {entt::null};
		glm::vec3 position {0.0f};
	};
	std::vector<Place> lastPlaces;
	/// The turn it last started a dance to impress villagers, and the kind of that dance (0 before the first)
	uint32_t lastImpressiveTurn {0};
	uint32_t lastImpressiveType {0};
};

} // namespace openblack::ecs::components
