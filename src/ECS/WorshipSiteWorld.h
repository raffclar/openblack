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

#include <entt/core/fwd.hpp>
#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "Enums.h"

namespace openblack::ecs
{
class Registry;
}

namespace openblack::ecs::worship_site
{

/// What the worship sites need of the rest of the game: the land, the worship site's model, its tribes' altars and
/// how many live in a town
class WorldInterface
{
public:
	virtual ~WorldInterface() = default;

	[[nodiscard]] virtual Registry& Entities() = 0;
	/// Which land of the story this is
	[[nodiscard]] virtual int32_t LandNumber() const = 0;
	/// One of the points laid out in the worship site's model, about the temple it stands at; none without the model
	[[nodiscard]] virtual std::optional<glm::vec3> SitePoint(uint32_t index) const = 0;
	/// The worship site's model as it stands at a temple: wearing the temple's own skin once the temple has one, which
	/// changes with its player's alignment
	[[nodiscard]] virtual entt::id_type SiteMesh(entt::entity temple) = 0;
	/// The model of a tribe's altar
	[[nodiscard]] virtual entt::id_type AltarMesh(Tribe tribe) const = 0;
	[[nodiscard]] virtual float LandHeightAt(glm::vec2 point) const = 0;
	/// How many people live in a town, with a home or without
	[[nodiscard]] virtual uint32_t PopulationOf(entt::entity town) const = 0;
};

} // namespace openblack::ecs::worship_site
