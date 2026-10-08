/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

namespace openblack::ecs::components
{
/// Object life in 0..1 (Object::GetLife / SetLife) for the objects that have no other life yet: rocks, animals.
/// Villagers keep theirs in Villager::life. The getters and setters are ecs::life (ECS/Life.h).
struct Life
{
	float value {1.0f};
};
} // namespace openblack::ecs::components
