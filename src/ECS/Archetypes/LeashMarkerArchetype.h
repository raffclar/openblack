/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <optional>
#include <utility>

#include <entt/entity/fwd.hpp>

#include "Enums.h"

namespace openblack::ecs::archetypes
{

/// The ring a player sees over where their creature was sent with the leash, and the creature's footprint inside it:
/// two yellow sprites facing the camera, from the sheet of markers and footprints
class LeashMarkerArchetype
{
public:
	/// The ring and the footprint of a species, or nothing when the sheet isn't loaded
	static std::optional<std::pair<entt::entity, entt::entity>> Create(entt::entity creature, CreatureType species);
};

} // namespace openblack::ecs::archetypes
