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

#include "ECS/FixedOutlines.h"
#include "ECS/Systems/FixedOutlineSystemInterface.h"

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "ECS System implementations should only be included in Locator.cpp"
#endif

namespace openblack::ecs
{
class Registry;
}

namespace openblack::ecs::systems
{

class FixedOutlineSystem final: public FixedOutlineSystemInterface
{
public:
	void Insert(entt::entity entity) override;
	[[nodiscard]] bool CollidesWithFixed(const map_coords::MapCoords& place) const override;
	[[nodiscard]] bool IsWater(const map_coords::MapCoords& place) const override;
	[[nodiscard]] bool IsOkToCreateAt(const map_coords::MapCoords& place) const override;

	/// The outline that the game gives a thing as it is now, none for a kind of thing with no outline or a thing with
	/// no model
	[[nodiscard]] static std::optional<fixed_outlines::Outline> OutlineOf(const Registry& registry, entt::entity entity);
};

} // namespace openblack::ecs::systems
