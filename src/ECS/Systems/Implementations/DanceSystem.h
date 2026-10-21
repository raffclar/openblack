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
#include <optional>

#include "ECS/Systems/DanceSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs::systems
{

class DanceSystem final: public DanceSystemInterface
{
public:
	entt::entity Create(DanceInfo type, const glm::vec3& place, entt::entity owner, uint32_t duration, bool madeByScript,
	                    entt::entity town) override;
	void Destroy(entt::entity dance) override;
	void ProcessTurn() override;
	void ProcessCameraKeys(bool gripsLand) override;
	void CheckCamera() override;
	[[nodiscard]] const std::array<dance_shapes::Shape, dance_shapes::k_ShapeCount>& Shapes() override;

private:
	/// The camera of a dance takes over from the camera, if the camera can be left
	static void StartCamera(entt::entity dance);

	/// The last place of the hand on the land. The hand keeps this place while it is off the land.
	std::optional<glm::vec3> _hand;
	/// The shapes, read once they are first needed
	std::optional<std::array<dance_shapes::Shape, dance_shapes::k_ShapeCount>> _shapes;
};

} // namespace openblack::ecs::systems
