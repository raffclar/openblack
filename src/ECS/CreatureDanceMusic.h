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

#include <glm/vec3.hpp>

#include "Audio/GameMusic.h"

/// The music of a creature's dance with villagers, which the camera hears near the creature
namespace openblack::ecs::creature_dance_music
{

/// The music the camera hears: of the creature nearest to it, nearer than 75 metres, that dances with villagers. The
/// first dancer of the dance must be a villager, and the camera must be no more than 60 metres above or below the land
/// under the creature. None when no creature dances so.
[[nodiscard]] std::optional<audio::GameMusic::TurnInputs::CreatureDance> Heard(glm::vec3 camera);

} // namespace openblack::ecs::creature_dance_music
