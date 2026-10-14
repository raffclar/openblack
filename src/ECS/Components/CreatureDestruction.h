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

#include <glm/vec3.hpp>

#include "Creature/CreatureStomp.h"

namespace openblack::ecs::components
{

/// The spheres round the parts of the body of a creature that knock things about while it lies down, gets up, faints
/// or fights
struct CreatureDestruction
{
	/// The spheres knock things about now
	bool on {false};
	/// Where each part was at its last check, and the radius of its sphere
	std::array<glm::vec3, creature_stomp::k_PartCount> last {};
	std::array<float, creature_stomp::k_PartCount> radius {};
	/// The body action at the last frame, to see when a new one starts
	creature_stomp::BodyStart seen {};
};

} // namespace openblack::ecs::components
