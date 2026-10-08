/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "FireObjectTraits.h"

#include <algorithm>
#include <utility>
#include <vector>

#include <spdlog/spdlog.h>

#include "3D/L3DMesh.h"
#include "3D/LandIslandInterface.h"
#include "ECS/Abodes.h"
#include "ECS/AnimalAI.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/AnimatedStatic.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/Feature.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/FireImmunity.h"
#include "ECS/Components/FishFarm.h"
#include "ECS/Components/Forest.h"
#include "ECS/Components/Fragment.h"
#include "ECS/Components/MagicFireBall.h"
#include "ECS/Components/MagicTeleport.h"
#include "ECS/Components/MagicTree.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/SpellIcon.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/TotemStatue.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/Effects/EffectValues.h"
#include "ECS/Effects/Reactions.h"
#include "ECS/Fields.h"
#include "ECS/Life.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Physics/PhysicsObjects.h"
#include "ECS/Registry.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Villager/VillagerDeath.h"
#include "FireEffect.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/Objects/MagicFireBall.h"
#include "Magic/Objects/MagicTree.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;

namespace
{
/// Whether the object has the flag component
template <typename Flag>
bool HasFlag(entt::entity object)
{
	if (!Locator::entitiesRegistry::has_value())
	{
		return false;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	return registry.Valid(object) && registry.AllOf<Flag>(object);
}

/// Sets or clears the flag component on a live object
template <typename Flag>
void SetFlag(entt::entity object, bool value)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(object))
	{
		return;
	}
	if (value)
	{
		registry.AssignOrReplaceState<Flag>(object);
	}
	else
	{
		registry.RemoveState<Flag>(object);
	}
}

/// Every object loses the flag component
template <typename Flag>
void ClearFlag()
{
	if (!Locator::entitiesRegistry::has_value())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	std::vector<entt::entity> flagged;
	// read through the const registry: no storage is made for a flag no object has
	std::as_const(registry).Each<const Flag>([&flagged](entt::entity object, const auto&...) { flagged.push_back(object); });
	for (const auto object : flagged)
	{
		registry.RemoveState<Flag>(object);
	}
}

float LandAt(float x, float z)
{
	return Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(glm::vec2(x, z)) : 0.0f;
}

/// The abode's own GAbodeInfo: its abode number and mesh (a lookup by number alone would give the tribeless records),
/// else the first of that number. (inferred: the original keeps the info on the object; openblack's abode has none, so
/// the match by number and mesh, and the fallback, are the port's)
const GAbodeInfo* AbodeInfoOf(const Abode& abode, entt::id_type mesh)
{
	const GAbodeInfo* first = nullptr;
	for (const auto& info : Locator::infoConstants::value().abode)
	{
		if (info.abodeNumber != abode.type)
		{
			continue;
		}
		if (resources::HashIdentifier(info.meshId) == mesh)
		{
			return &info;
		}
		if (first == nullptr)
		{
			first = &info;
		}
	}
	return first;
}
} // namespace

const GAbodeInfo* fire::traits::AbodeInfo(entt::entity object)
{
	if (!Locator::infoConstants::has_value())
	{
		return nullptr;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto* abode = registry.TryGet<const Abode>(object);
	if (abode == nullptr)
	{
		return nullptr;
	}
	const auto* mesh = registry.TryGet<const Mesh>(object);
	return AbodeInfoOf(*abode, mesh != nullptr ? mesh->id : 0);
}

const GObjectInfo* fire::traits::InfoOf(entt::entity object)
{
	if (!Locator::infoConstants::has_value())
	{
		return nullptr;
	}
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(object))
	{
		return nullptr;
	}
	const auto& constants = Locator::infoConstants::value();
	if (const auto* fireBall = registry.TryGet<const MagicFireBall>(object))
	{
		return &constants.magicFireBall.at(static_cast<size_t>(fireBall->infoRow));
	}
	if (const auto* info = physics::PhysicsObjects::ObjectInfo(object))
	{
		return info;
	}
	// a field is an Abode with its GFieldTypeInfo; row 0 for all (the 6 rows are identical)
	if (registry.AllOf<Field>(object))
	{
		return &constants.fieldType.at(0);
	}
	if (const auto* abode = registry.TryGet<const Abode>(object))
	{
		const auto* mesh = registry.TryGet<const Mesh>(object);
		return AbodeInfoOf(*abode, mesh != nullptr ? mesh->id : 0);
	}
	if (const auto* feature = registry.TryGet<const Feature>(object))
	{
		return &constants.feature.at(static_cast<size_t>(feature->type));
	}
	return nullptr;
}

float fire::traits::CombustionTemperature(entt::entity object)
{
	const auto* info = InfoOf(object);
	return info != nullptr ? info->combustionTemperature : 0.0f;
}

float fire::traits::HeatCapacity(entt::entity object)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* info = InfoOf(object);
	if (info == nullptr)
	{
		return 0.0f;
	}
	if (const auto* fireBall = registry.TryGet<const MagicFireBall>(object))
	{
		// r^2 x 0.0625 x info.heatCapacity x the effect's strength
		const float r = Radius(object);
		static_cast<void>(fireBall);
		return r * r * 0.0625f * info->heatCapacity * magic::fireball::Strength(object);
	}
	return info->heatCapacity;
}

float fire::traits::DefenceMultiplierBurn(entt::entity object)
{
	const auto* info = InfoOf(object);
	return info != nullptr ? info->defenceMultiplierBurn : 0.0f;
}

float fire::traits::BurningPriority(entt::entity object)
{
	const auto* info = InfoOf(object);
	return info != nullptr ? info->burningPriority : 0.0f;
}

glm::vec3 fire::traits::FireCentre(entt::entity object)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* transform = registry.TryGet<const Transform>(object);
	if (transform == nullptr)
	{
		return glm::vec3(0.0f);
	}
	if (registry.AllOf<WorshipSite>(object))
	{
		// its centre position, the altitude made relative to the land
		const glm::vec3 centre = object::WorshipSiteCentre(object);
		return {centre.x, centre.y - LandAt(centre.x, centre.z), centre.z};
	}
	// x, z and the height above the land
	const float height = transform->position.y - LandAt(transform->position.x, transform->position.z);
	if (registry.AllOf<DeadTree>(object) && Locator::resources::has_value())
	{
		// the world matrix x the mesh centre for x, z; the altitude it computes is then overwritten by the object's own
		// height
		if (const auto* mesh = registry.TryGet<const Mesh>(object))
		{
			auto& meshes = Locator::resources::value().GetMeshes();
			if (meshes.Contains(mesh->id))
			{
				const glm::vec3 centre = meshes.Handle(mesh->id)->GetBoundingBox().Center();
				const glm::vec3 world = transform->position + transform->rotation * (centre * transform->scale);
				return {world.x, height, world.z};
			}
		}
	}
	return {transform->position.x, height, transform->position.z};
}

float fire::traits::DefaultFireRadius(entt::entity object)
{
	return object::GetDefaultFireRadius(object); // with the dead tree's and the worship site's own
}

float fire::traits::Height(entt::entity object)
{
	return object::GetHeight(object); // with the fireball's own
}

float fire::traits::Radius(entt::entity object)
{
	return object::GetRadius(object); // with the field's, fish farm's, food pile's and fireball's own
}

float fire::traits::RainCoolingMultiplier(entt::entity object)
{
	if (const auto* fireBall = Locator::entitiesRegistry::value().TryGet<const MagicFireBall>(object))
	{
		return fireBall->affectedByRain ? 0.01f : 0.0f; // cast by a script: 0
	}
	return 0.01f;
}

bool fire::traits::IsAvailable(entt::entity object)
{
	return ecs::IsAvailable(object);
}

bool fire::traits::IsObjectInMap(entt::entity object)
{
	// the object's "in the map" flag: clear for a held or thrown object and for a fireball, which never enters the map
	return map_cells::IsObjectInMap(object);
}

bool fire::traits::IsMultiCellStatic(entt::entity object)
{
	// the classes ecs::map_cells inserts as multi-cell fixed objects (or as a fish farm): one list of classes for both.
	// WorshipSite and CitadelHeart (openblack's Temple entity) are citadel parts, AnimatedStatic is a Feature, DeadTree
	// and Fragment are rocks (mobile statics). A footpath is not one. (approximate) The other citadel parts, the football
	// pitch and the prayer site have no component of
	// their own in openblack yet
	return ecs::map_cells::IsMultiCellStaticClass(object);
}

bool fire::traits::IsVillager(entt::entity object)
{
	return Locator::entitiesRegistry::value().AllOf<Villager>(object);
}

bool fire::traits::IsCreature(entt::entity object)
{
	// the mourners pass on whoever started the fire, who may be gone by now
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	return registry.Valid(object) && registry.AllOf<Creature>(object);
}

bool fire::traits::InHand(entt::entity object)
{
	if (!Locator::handSystem::has_value())
	{
		return false;
	}
	const auto held = Locator::handSystem::value().GetHeldObject();
	return held.has_value() && *held == object;
}

bool fire::traits::IsBurnReceiver(entt::entity object, float burn)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(object))
	{
		return false;
	}
	// a pot with something in it
	if (const auto* pot = registry.TryGet<const Pot>(object))
	{
		return pot->amount != 0;
	}
	// a field takes a burn only while the hand may pick it up (inferred: while it has food to give)
	if (const auto* field = registry.TryGet<const Field>(object))
	{
		return burn <= 0.0f || field->food > 0.0f;
	}
	// a villager refuses only a heal when dead (inferred: dead villagers are gone from openblack's world, so that
	// branch is not ported)
	return true;
}

bool fire::traits::CannotBeSetOnFire(entt::entity object)
{
	return HasFlag<components::CannotBeSetOnFire>(object);
}

void fire::traits::SetCannotBeSetOnFire(entt::entity object, bool value)
{
	SetFlag<components::CannotBeSetOnFire>(object, value);
}

bool fire::traits::NotHurtByFire(entt::entity object)
{
	return HasFlag<components::NotHurtByFire>(object);
}

void fire::traits::SetNotHurtByFire(entt::entity object, bool value)
{
	SetFlag<components::NotHurtByFire>(object, value);
}

float fire::traits::ReduceLifeDueToBurning(entt::entity object, float damage, std::optional<PlayerNames> player)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (registry.AllOf<Field>(object))
	{
		// takes damage x totalFoodInField of its food, life stays 1 (row 0: the 6 rows
		// are identical, see InfoOf)
		RemoveFieldFood(object, damage * Locator::infoConstants::value().fieldType.at(0).totalFoodInField);
		return 1.0f;
	}
	// no harm when it is not hurt by fire. TODO(belief): the town's aggressor (from the burn's effect values and the
	// damage)
	if (NotHurtByFire(object))
	{
		return life::LifeOf(object);
	}
	// ReduceLife(damage, player): the abodes' own (ecs::abodes: the site, StopBeingFunctional, the town's emergency);
	// a creature's own, not ported yet, so it takes no harm; the common one for the rest
	if (registry.AllOf<Abode>(object))
	{
		return abodes::ReduceLife(object, damage, player);
	}
	if (registry.AllOf<Creature>(object))
	{
		if (auto logger = spdlog::get("game"); logger != nullptr)
		{
			SPDLOG_LOGGER_DEBUG(logger, "Fire: creature {} takes no harm from burning (not ported)",
			                    static_cast<uint32_t>(object));
		}
		return life::LifeOf(object);
	}
	return life::ReduceLife(object, damage);
}

void fire::traits::DestroyedByEffect(entt::entity object, std::optional<PlayerNames> player, float amount)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!ecs::IsAvailable(object))
	{
		return;
	}
	if (registry.AllOf<Villager>(object))
	{
		// a villager dies by the spell with the caller's player and amount (the fire: its player and 0, as in the original)
		villager::DestroyedByEffect(object, player, amount);
		return;
	}
	if (registry.AllOf<Animal>(object))
	{
		ecs::animal_ai::DestroyedByEffect(object); // SetDying (ECS/AnimalAI)
		return;
	}
	if (registry.AllOf<Field>(object))
	{
		// the crops, growth and food go to 0 and the fire's temperature with them; then the fire is deleted
		auto& field = registry.Get<Field>(object);
		field.crops = 0;
		field.growth = 0.0f;
		field.food = 0.0f;
		if (auto* effect = fire::Find(object))
		{
			effect->temperature = 0.0f;
			fire::ToBeDeleted(*effect);
		}
		return;
	}
	if (registry.AllOf<Abode>(object))
	{
		// TODO: the abode's own (ghost, building site, life 1); the abode stays
		return;
	}
	// the rest is deleted (trees, dead trees, piles, mobile objects, features: a feature has no override of its own)
	physics::PhysicsObjects::RemoveObject(object);
	// its fire flags (components) go with it
	ecs::map_cells::RemoveMapObject(object); // out of its cell's lists
	registry.Destroy(object);
	registry.SetDirty();
}

void fire::traits::StartOnFire(entt::entity object)
{
	auto& registry = Locator::entitiesRegistry::value();
	// the multi-cell fixed objects, dead trees and pots: the object's reactions go (trees and the living ones don't).
	// The same holds for rocks, mobile statics, animated statics, fragments, fields, fish farms, big forests, the
	// citadel and worship classes, spell icons, etc.; the pot's also for the piles and magic food/wood (openblack's
	// Pot). The classes ported here: Abode (with Field), Feature, MobileStatic (rocks),
	// AnimatedStatic, Fragment, DeadTree, Pot; the rest (not ported)
	if (registry.AnyOf<Abode, Field, Feature, MobileStatic, AnimatedStatic, Fragment, DeadTree, Pot>(object))
	{
		effects::reactions::RemoveAllReactionsInitiatedByObject(object);
	}
	// a magic tree's REACT_TO_MAGIC_TREE goes (Magic/Objects/MagicTree)
	if (registry.AllOf<MagicTree>(object))
	{
		magic::magic_tree::StartOnFire(object);
	}
}

void fire::traits::EndOnFire(entt::entity object)
{
	auto& registry = Locator::entitiesRegistry::value();
	// a dead tree, while available: REACT_TO_WOOD with its player and 0.
	// (inferred: a dead tree has no owner in openblack: neutral)
	if (ecs::IsAvailable(object) && registry.AllOf<DeadTree>(object))
	{
		effects::reactions::CreateReaction(object, Reaction::ReactToWood, PlayerNames::NEUTRAL, false);
	}
	// TODO: a pot re-creates its reaction
	// a magic tree: REACT_TO_MAGIC_TREE again (no availability test there, unlike the dead tree's)
	if (registry.Valid(object) && registry.AllOf<MagicTree>(object))
	{
		magic::magic_tree::EndOnFire(object);
	}
}

void fire::traits::Clear()
{
	ClearFlag<components::CannotBeSetOnFire>();
	ClearFlag<components::NotHurtByFire>();
}
