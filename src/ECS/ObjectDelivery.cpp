/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ObjectDelivery.h"

#include "Audio/Audio.h"
#include "Audio/Services/Guidance.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Life.h"
#include "ECS/ObjectGhosts.h"
#include "ECS/ObjectResources.h"
#include "ECS/Physics/FromHand.h"
#include "ECS/Registry.h"
#include "ECS/ResourceStores.h"
#include "ECS/StoragePitStore.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Trees.h"
#include "InfoConstants.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::components;

namespace
{
/// The resource type and amount of the object given: a tree's wood (ecs::TreeWood), a pot's or a pile's own type
/// and amount, an animal's food, a fence's wood (any other static object has none)
std::pair<ResourceType, uint32_t> ResourceOf(entt::entity object)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (registry.AnyOf<Tree, DeadTree>(object))
	{
		return {ResourceType::Wood, ecs::TreeWood(object)};
	}
	if (const auto* animal = registry.TryGet<const Animal>(object); animal != nullptr && Locator::infoConstants::has_value())
	{
		const auto& info = Locator::infoConstants::value().animal.at(static_cast<size_t>(animal->type));
		return {ResourceType::Food, ecs::resource_stores::AnimalFood(static_cast<uint32_t>(info.foodType), info.foodValue)};
	}
	if (const auto* statics = registry.TryGet<const MobileStatic>(object); statics != nullptr)
	{
		if (!ecs::physics::from_hand::IsFence(object))
		{
			return {ResourceType::None, 0};
		}
		// the wood value of the fence's own info row (every fence the game makes has one)
		const auto& info = Locator::infoConstants::value().mobileStatic.at(static_cast<size_t>(statics->type));
		const auto* transform = registry.TryGet<const Transform>(object);
		const float scale = transform != nullptr ? transform->scale.x : 1.0f;
		return {ResourceType::Wood, ecs::resource_stores::FenceWood(info.woodValue, ecs::life::LifeOf(object), scale)};
	}
	if (const auto* pot = registry.TryGet<const Pot>(object); pot != nullptr && Locator::infoConstants::has_value())
	{
		const auto type = Locator::infoConstants::value().pot.at(static_cast<size_t>(pot->type)).resourceType;
		return {type, ecs::object_resources::GetResource(object, type)};
	}
	// any other class: its resource type and amount through ecs::object_resources (a scaffold: WOOD, value x 2500)
	const auto type = ecs::object_resources::GetResourceType(object);
	return {type, type != ResourceType::None ? ecs::object_resources::GetResource(object, type) : 0};
}
} // namespace

uint32_t ecs::object_delivery::DoDeleteObjectAndTakeResource(entt::entity structure, entt::entity object,
                                                             const pot_resource::Dropper& is)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(structure) || !registry.Valid(object))
	{
		return 0;
	}
	// the structure adds the object's resource, poisoned or not
	const auto [type, amount] = ResourceOf(object);
	const uint32_t taken = object_resources::AddResource(structure, type, amount, is, object_resources::IsPoisoned(object));
	// with something taken and an interface the creature is told what was done (a storage pit's deed, its draw
	// included; (pending) the other structures' deeds and the creature's mimicry); with the local interface the
	// resource drop sound at the structure (a storage pit's guidance type is none, silent; (inferred) a worship
	// site's the same)
	if (taken != 0 && is.hasInterface)
	{
		[[maybe_unused]] const auto deed = StoragePitStore::DoCreatureMimicAfterAddingResource(structure, type, is);
	}
	if (taken != 0 && is.hasInterface && is.isMyInterface)
	{
		audio::guidance::PlayResourceDropRemark(registry.Get<const Transform>(structure).position,
		                                        audio::guidance::RainType::None);
	}
	// wood that is not a pot: bank InGame, sample 155 G_TreeMulch_01 + a counter cycling 0..3, owner the object, 3D at
	// its point, not gated by what was taken
	if (type == ResourceType::Wood && !registry.AllOf<Pot>(object))
	{
		audio::PlayOptions options;
		options.sample = {audio::Bank(audio::SfxBank::InGame), 155 + audio::NextCounter(audio::Counter::TreeMulch)};
		options.owner = audio::Owner::Thing(object);
		options.is3D = true;
		options.track = false;
		options.position = registry.Get<const Transform>(object).position;
		audio::PlaySoundEffect(options);
	}
	// the 500 ms ghost of the object's model where it was (draw only), then the object is deleted
	object_ghosts::Add(object);
	ToBeDeleted(object);
	return taken;
}
