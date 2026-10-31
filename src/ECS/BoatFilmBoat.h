/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <optional>

#include <glm/mat4x4.hpp>

namespace openblack::ecs
{
class Registry;
}

namespace openblack::ecs::boat_film
{

/// The matrix of the explorers' boat that the frame shows, if its film plays. The film makes a boat for the launch and
/// a boat for the voyage, and hides the boat of the stage that does not play. The parts on the deck take the land's
/// light of this boat's place.
[[nodiscard]] std::optional<glm::mat4> ShownBoatMatrix(const Registry& drawn);

} // namespace openblack::ecs::boat_film
