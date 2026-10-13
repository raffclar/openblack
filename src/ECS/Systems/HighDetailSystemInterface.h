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

#include <entt/entity/fwd.hpp>
#include <glm/mat4x4.hpp>

#include "ECS/HighDetailRules.h"

namespace openblack::ecs::systems
{

/// The villagers scripts draw in high detail during their cinemas, such as the opening's family
class HighDetailSystemInterface
{
public:
	virtual ~HighDetailSystemInterface() = default;

	/// Draws a villager in high detail: the opening's family and the creature trainer change into their detailed models.
	/// One already drawn so stays as it is.
	virtual void Make(entt::entity thing) = 0;
	/// Draws it as usual again, in its own model
	virtual void Release(entt::entity thing) = 0;
	/// Draws all the high-detail villagers as usual again. Gives false when there was none.
	virtual bool ReleaseAll() = 0;
	/// Gives a high-detail villager an order. False when the thing isn't drawn in high detail.
	virtual bool Order(entt::entity thing, high_detail_rules::ThingSpecial special, bool on) = 0;
	/// Each frame: they are all drawn as usual again once no script holds the cinema bars. When this releases one or
	/// more of them, the land's draw list is made again on the next frame.
	virtual void Update() = 0;
	/// Each frame, after the villagers in view get their poses: the eyes of those in view blink and turn. They move by
	/// the game time of the frame, in whole milliseconds. Then the system puts the eyes on the heads.
	virtual void PlaceEyes(uint32_t frameGameMs, const glm::mat4& viewProjection) = 0;
};

} // namespace openblack::ecs::systems
