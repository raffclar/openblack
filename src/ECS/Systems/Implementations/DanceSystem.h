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
	entt::entity Create(DanceInfo type, const glm::vec3& place, entt::entity owner, uint32_t duration,
	                    bool madeByScript) override;
	void Destroy(entt::entity dance) override;
	void ProcessTurn() override;
	[[nodiscard]] const std::array<dance_shapes::Shape, dance_shapes::k_ShapeCount>& Shapes() override;

private:
	/// The shapes, read once they are first needed
	std::optional<std::array<dance_shapes::Shape, dance_shapes::k_ShapeCount>> _shapes;
};

} // namespace openblack::ecs::systems
