/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <entt/entity/entity.hpp>

#include "ECS/Components/SoundTag.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"

namespace openblack::ecs
{

/// Whether an attached sound tag must go in this game turn. An attached tag goes when its object is gone or has no
/// position. A tag on its own object and the tag of a point never go here.
///
/// This rule changes the synced world. Thus every computer uses it in each game turn, also a computer with no audio.
[[nodiscard]] inline bool AttachedSoundTagLost(const Registry& registry, const components::SoundTag& tag)
{
	if (tag.owner == entt::null)
	{
		return false;
	}
	return !registry.Valid(tag.owner) || registry.TryGet<const components::Transform>(tag.owner) == nullptr;
}

} // namespace openblack::ecs
