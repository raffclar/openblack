/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "MovingShadowCasters.h"

#include "ECS/Components/Animal.h"
#include "ECS/Components/DeadTree.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/Forest.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Villager.h"
#include "ECS/LandShadowCasters.h"
#include "ECS/Registry.h"

namespace openblack::ecs
{

using namespace components;

MovingShadow MovingShadowOf(const Registry& registry, entt::entity entity)
{
	if (registry.AnyOf<Tree, BigForest, DeadTree, Pot, Field>(entity))
	{
		return MovingShadow::Texture;
	}
	if (registry.AnyOf<Villager, Animal>(entity))
	{
		return MovingShadow::Shape;
	}
	return LandShadowOf(registry, entity) == LandShadow::None ? MovingShadow::None : MovingShadow::Shape;
}

} // namespace openblack::ecs
