/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "BonfireArchetype.h"

#include <glm/vec3.hpp>

#include "MobileStaticArchetype.h"
#include "Particles/PSysManager.h"

using namespace openblack;
using namespace openblack::ecs::archetypes;

entt::entity BonfireArchetype::Create(const glm::vec3& position, float yAngleRadians, float scale)
{
	// A rock with mobile static info 8 (MSH_B_CAMPFIRE) with the angle and scale, and the spot visual
	// SPOT_VISUAL_BONFIRE (25), 1.0, -1 (forever), on it. The temperature the callers pass (CREATE_BONFIRE's F1, 100
	// from a mobile static creation) is not used. No lantern light.
	const auto entity =
	    MobileStaticArchetype::Create(position, MobileStaticInfo::Bonfire, 0.0f, 0.0f, yAngleRadians, 0.0f, scale);
	constexpr int k_SpotVisualBonfire = 25;
	psys::manager::CreateSpotVisual(k_SpotVisualBonfire, position, -1.0f, entity);
	return entity;
}
