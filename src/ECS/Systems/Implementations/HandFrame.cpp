/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The hand's matrix as the original builds it and the up of the empty hand (the end of the hand's placement). Wiki:
// docs/bw1-notes/hand-and-interface.md, "Where the hand is placed".

#define LOCATOR_IMPLEMENTATIONS

#include <glm/geometric.hpp>

#include "3D/LandIslandInterface.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "HandSystem.h"
#include "HandSystemDetail.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;

glm::mat3 HandSystem::HandMatrixRotation(glm::vec3 up) noexcept
{
	// d = (-sin H, 0, cos H), horizontal towards the camera, with the heading H taken before drawing from the camera ->
	// mouse ray only when |ray.x| or |ray.z| > 0.01; side = norm(d x up), skipped when zero; the model's X =
	// side, Y = side x up, Z = -up (the scale and the left hand's mirrored X are the transform's)
	if (std::abs(_mouseRayDirection.x) > 0.01f || std::abs(_mouseRayDirection.z) > 0.01f)
	{
		_handHeadingBack = -glm::normalize(glm::vec3(_mouseRayDirection.x, 0.0f, _mouseRayDirection.z));
	}
	// (openblack guard) the 1e-6 fallbacks for a zero up or side
	up = glm::length(up) > 1e-6f ? glm::normalize(up) : glm::vec3(0.0f, 1.0f, 0.0f);
	auto side = glm::cross(_handHeadingBack, up);
	side = glm::length(side) > 1e-6f ? glm::normalize(side) : glm::vec3(1.0f, 0.0f, 0.0f);
	return glm::mat3(side, glm::cross(side, up), -up);
}

glm::vec3 HandSystem::UpdateNormalUp(float seconds) noexcept
{
	// The end of the placement: over plain land the up is the land normal at the hand's last position (read before
	// that position's last write). (pending) over an object, 0.25 toCam - hitNormal + (0, 0.5, 0) (when the hand
	// feels with a mesh intersection, and the hand higher than 1.6 hs above the ground); the rotate tricon.
	// Then the three per-axis Zoomers take the new target (in 0.4 s) only when the mouse's x changed this frame or the
	// roll is not 0 (0 in the normal hand state), and are updated every frame; the up is their value normalised
	constexpr float k_UpSeconds = 0.4f;
	const auto& transform = Locator::entitiesRegistry::value().Get<const Transform>(_hands[static_cast<size_t>(Side::Left)]);
	if (Locator::terrainSystem::has_value() && _mouse.x != _upMouseX)
	{
		_upMouseX = _mouse.x;
		const auto& terrain = Locator::terrainSystem::value();
		const glm::vec2 xz(transform.position.x, transform.position.z);
		// over an object the hand feels, its up target (_feelUp), unless the hand is lower over the land than half its
		// size (3.2 x the hand scale x 0.5): the land normal; normalised
		auto target = terrain.GetNormalAt(xz);
		if (_feelUp && transform.position.y - terrain.GetHeightAt(xz) >= 3.2f * _handScale * 0.5f &&
		    glm::length(*_feelUp) > 0.0f)
		{
			target = glm::normalize(*_feelUp);
		}
		_up.SetDestinationWithTime(target, k_UpSeconds); // the same as the three per-axis Zoomers with speed 0
	}
	_up.Update(seconds);
	const auto value = _up.GetCurrentValue();
	_normalUp = glm::length(value) > 1e-6f ? glm::normalize(value) : glm::vec3(0.0f, 1.0f, 0.0f);
	return _normalUp;
}
