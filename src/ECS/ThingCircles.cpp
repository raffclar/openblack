/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ThingCircles.h"

#include "3D/L3DMesh.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/AnimatedStatic.h"
#include "ECS/Components/BuildingDamage.h"
#include "ECS/Components/DeadTree.h"
#include "ECS/Components/Feature.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/Flowers.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/SpellDispenser.h"
#include "ECS/Components/SpellIcon.h"
#include "ECS/Components/TeleportStone.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/Registry.h"
#include "ECS/ThingModel.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;

wall_hug::ThingShape thing_circles::ShapeOf(const Registry& registry, entt::entity thing)
{
	if (registry.AnyOf<Field>(thing))
	{
		return wall_hug::ThingShape::None;
	}
	if (registry.AnyOf<Tree>(thing))
	{
		return wall_hug::ThingShape::Trunk;
	}
	if (registry.AnyOf<Temple>(thing))
	{
		return wall_hug::ThingShape::TempleRing;
	}
	if (registry.AnyOf<WorshipSite>(thing))
	{
		return wall_hug::ThingShape::WorshipSiteBalls;
	}
	// A piece that broke off a building is a rock, with the outline of the rock model
	if (registry.AnyOf<Abode, Feature, AnimatedStatic, Flowers, MobileStatic, DeadTree, SpellDispenser, TeleportStone,
	                   WorshipAltar, BuildingPiece, WorshipIcon>(thing))
	{
		return wall_hug::ThingShape::ModelBox;
	}
	return wall_hug::ThingShape::None;
}

std::vector<wall_hug::BlockingCircle> thing_circles::CirclesOf(const Registry& registry, entt::entity thing)
{
	const auto* transform = registry.TryGet<const Transform>(thing);
	const auto shape = ShapeOf(registry, thing);
	if (transform == nullptr || shape == wall_hug::ThingShape::None)
	{
		return {};
	}
	wall_hug::ThingOnMap onMap {.shape = shape,
	                            .position = transform->position,
	                            .rotation = transform->rotation,
	                            .scale = transform->scale.x,
	                            .boxCentre = glm::vec3(0.0f),
	                            .boxHalfSize = glm::vec3(0.0f),
	                            .fence = false};
	if (shape == wall_hug::ThingShape::ModelBox)
	{
		// An altar has its model's box before the model shows
		const auto model = thing_model::ModelOf(registry, thing);
		if (!model.has_value() || !Locator::resources::has_value() || !Locator::resources::value().GetMeshes().Contains(*model))
		{
			return {};
		}
		const auto box = Locator::resources::value().GetMeshes().Handle(*model)->GetBoundingBox();
		onMap.boxCentre = box.Center();
		onMap.boxHalfSize = box.Size() * 0.5f;
		if (const auto* mobileStatic = registry.TryGet<const MobileStatic>(thing);
		    mobileStatic != nullptr && Locator::infoConstants::has_value())
		{
			const auto& info = Locator::infoConstants::value().mobileStatic.at(static_cast<size_t>(mobileStatic->type));
			onMap.fence = wall_hug::IsFenceModel(info.meshId);
		}
	}
	return wall_hug::CirclesOf(onMap);
}
