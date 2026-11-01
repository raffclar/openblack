/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureObstacleThings.h"

#include <algorithm>

#include <glm/gtx/vec_swizzle.hpp>

#include "3D/AllMeshes.h"
#include "3D/L3DMesh.h"
#include "Common/X87Angle.h"
#include "ECS/BuildingSiteList.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/AnimatedStatic.h"
#include "ECS/Components/Ball.h"
#include "ECS/Components/BuildingDamage.h"
#include "ECS/Components/Construction.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureLocomotion.h"
#include "ECS/Components/DeadTree.h"
#include "ECS/Components/Feature.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/Fire.h"
#include "ECS/Components/FishFarm.h"
#include "ECS/Components/Flowers.h"
#include "ECS/Components/Forest.h"
#include "ECS/Components/HandGrab.h"
#include "ECS/Components/MagicShield.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/ScriptControl.h"
#include "ECS/Components/ScriptHighlight.h"
#include "ECS/Components/SpellDispenser.h"
#include "ECS/Components/SpellIcon.h"
#include "ECS/Components/StreetLantern.h"
#include "ECS/Components/TeleportStone.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/TownDesireFlag.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/VillageTotem.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/Vortex.h"
#include "ECS/Components/Workshop.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/Registry.h"
#include "ECS/ScriptHighlightRules.h"
#include "ECS/Systems/AlignmentSystemInterface.h"
#include "ECS/Systems/FireSystemInterface.h"
#include "ECS/Systems/LeashSystemInterface.h"
#include "ECS/ThingModel.h"
#include "ECS/WallHugRules.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/ResourcePiles.h"
#include "Magic/ShieldRules.h"
#include "Magic/VortexPullRules.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;
namespace obstacles = openblack::creature_obstacles;

std::optional<obstacles::Thing> creature_obstacle_things::ThingOf(const Registry& registry, entt::entity entity)
{
	const auto* transform = registry.TryGet<const Transform>(entity);
	if (transform == nullptr || registry.AnyOf<InHand>(entity))
	{
		return std::nullopt;
	}
	obstacles::Thing thing {.avoidance = obstacles::Avoidance::Object,
	                        .shape = obstacles::Shape::ModelBox,
	                        .position = glm::xz(transform->position),
	                        .yAngle = obstacles::YAngleOf(transform->rotation),
	                        .scale = std::abs(transform->scale.x),
	                        .boxCentre = glm::vec3(0.0f),
	                        .boxHalfSize = glm::vec3(0.0f),
	                        .height = 0.0f,
	                        .width = 0.0f,
	                        .burning = false,
	                        .hasFire = registry.AnyOf<Fire>(entity),
	                        .justBegun = false,
	                        .walking = false,
	                        .playerAlignment = 0.0f,
	                        .worshipSites = {},
	                        .owner = PlayerNames::NEUTRAL};
	thing.burning = thing.hasFire && Locator::fireSystem::has_value() && Locator::fireSystem::value().IsOnFire(entity);
	// The world keeps a highlight upright at its full size. Only its draw turns it and changes its size for the camera.
	if (registry.AllOf<ScriptHighlight>(entity))
	{
		thing.yAngle = 0.0f;
		thing.scale = script_highlights::k_WorldScale;
	}

	if (const auto* other = registry.TryGet<const CreatureLocomotion>(entity))
	{
		const auto* body = registry.TryGet<const Creature>(entity);
		thing.avoidance = obstacles::Avoidance::Creature;
		thing.shape = obstacles::Shape::Creature;
		thing.width = other->shownRadius;
		thing.height = body != nullptr ? obstacles::k_HeightPerSize * ShownSize(*body) : 0.0f;
		// Walking along a route of its own: setting off on it, or on its way
		thing.walking = other->follower.IsFollowing();
		return thing;
	}
	if (registry.AnyOf<Villager, Animal, FishFarm, StreetLantern, TownDesireFlag, WorkshopFlag, WorshipSiteFlag>(entity))
	{
		return std::nullopt;
	}
	if (const auto* shield = registry.TryGet<const MagicShield>(entity))
	{
		// Every shield's object is the solid shield's model's box, the spiritual one at its radius's scale
		thing.avoidance = obstacles::Avoidance::Shield;
		thing.owner = shield->player;
		const auto* dome = registry.TryGet<const ShieldDome>(entity);
		thing.scale = dome != nullptr ? dome->solidScale : shield->radius * magic::shield::k_DomeScalePerRadius;
		const auto meshId = resources::HashIdentifier(MeshId::SpellSolidShield);
		if (Locator::resources::has_value() && Locator::resources::value().GetMeshes().Contains(meshId))
		{
			const auto box = Locator::resources::value().GetMeshes().Handle(meshId)->GetBoundingBox();
			thing.boxCentre = box.Center();
			thing.boxHalfSize = box.Size() * 0.5f;
			thing.height = obstacles::ModelHeight(thing.boxHalfSize, thing.scale);
			thing.width = obstacles::ModelWidth(thing.boxHalfSize, thing.scale);
		}
		return thing;
	}

	if (const auto* vortex = registry.TryGet<const Vortex>(entity))
	{
		// A vortex is one circle at its place, as wide and tall as its unseen cylinder. The way out of a land is kept
		// clear of unless the creature may be taken through it; the way in and the volcano's mouth by their height, as
		// anything else lying on the land
		thing.avoidance =
		    vortex->type == VortexType::In ? obstacles::Avoidance::VortexOutOfTheLand : obstacles::Avoidance::Object;
		thing.shape = obstacles::Shape::AtPlace;
		const auto meshId = resources::HashIdentifier(MeshId::SpellVortexCylinder);
		if (Locator::resources::has_value() && Locator::resources::value().GetMeshes().Contains(meshId))
		{
			const auto box = Locator::resources::value().GetMeshes().Handle(meshId)->GetBoundingBox();
			thing.boxCentre = box.Center();
			thing.boxHalfSize = box.Size() * 0.5f;
			thing.height = obstacles::ModelHeight(thing.boxHalfSize, thing.scale);
			thing.width = obstacles::ModelWidth(thing.boxHalfSize, thing.scale);
		}
		return thing;
	}

	// The model's box gives most things their size. An altar has its model's box before the model shows.
	if (const auto model = ecs::thing_model::ModelOf(registry, entity);
	    model.has_value() && Locator::resources::has_value() && Locator::resources::value().GetMeshes().Contains(*model))
	{
		const auto box = Locator::resources::value().GetMeshes().Handle(*model)->GetBoundingBox();
		thing.boxCentre = box.Center();
		thing.boxHalfSize = box.Size() * 0.5f;
		thing.height = obstacles::ModelHeight(thing.boxHalfSize, thing.scale);
		thing.width = obstacles::ModelWidth(thing.boxHalfSize, thing.scale);
	}

	if (const auto* temple = registry.TryGet<const Temple>(entity))
	{
		thing.avoidance = obstacles::Avoidance::Building;
		thing.shape = obstacles::Shape::Temple;
		thing.yAngle = temple->yAngle;
		thing.playerAlignment =
		    Locator::alignmentSystem::has_value() ? Locator::alignmentSystem::value().GetPlayerAlignment(temple->owner) : 0.0f;
		if (const auto* worship = registry.TryGet<const CitadelWorship>(entity))
		{
			std::ranges::transform(worship->sites, thing.worshipSites.begin(),
			                       [](entt::entity site) { return site != entt::null; });
		}
	}
	else if (registry.AnyOf<Tree>(entity))
	{
		thing.avoidance = obstacles::Avoidance::Tree;
		thing.shape = obstacles::Shape::Trunk;
	}
	else if (registry.AnyOf<BigForest>(entity))
	{
		thing.avoidance = obstacles::Avoidance::Forest;
	}
	else if (registry.AnyOf<Field>(entity))
	{
		thing.avoidance = obstacles::Avoidance::BurningUnlessScripted;
		thing.width = obstacles::k_FieldWidth;
	}
	else if (registry.AnyOf<DeadTree>(entity))
	{
		thing.avoidance = obstacles::Avoidance::BurningUnlessScripted;
		thing.shape = obstacles::Shape::AtPlace;
	}
	else if (registry.AnyOf<AnimatedStatic>(entity))
	{
		thing.avoidance = obstacles::Avoidance::Always;
		thing.shape = obstacles::Shape::Scenery;
	}
	else if (const auto* mobileStatic = registry.TryGet<const MobileStatic>(entity))
	{
		// Fences are the outline of their boxes, rocks and the like one circle at their place
		const auto fence = Locator::infoConstants::has_value() &&
		                   ecs::wall_hug::IsFenceModel(
		                       Locator::infoConstants::value().mobileStatic.at(static_cast<size_t>(mobileStatic->type)).meshId);
		thing.shape = fence ? obstacles::Shape::ModelBox : obstacles::Shape::AtPlace;
		// A thing that does not move builds its turn and scale as a thing that can lean, from its three angles
		thing.placement = obstacles::Placement::Leaning;
		const auto angles = x87::GameAnglesYxz(transform->rotation[0], transform->rotation[2]);
		thing.yAngle = angles.y;
		thing.xAngle = angles.x;
		thing.zAngle = angles.z;
	}
	else if (registry.AnyOf<BuildingPiece>(entity))
	{
		// A piece that broke off a building is a rock of the rock model: one circle at its place, from its three angles.
		// A creature walks round it only while it burns, and the footpaths never go round it.
		thing.avoidance = obstacles::Avoidance::Burning;
		thing.shape = obstacles::Shape::AtPlace;
		thing.placement = obstacles::Placement::Leaning;
		const auto angles = x87::GameAnglesYxz(transform->rotation[0], transform->rotation[2]);
		thing.yAngle = angles.y;
		thing.xAngle = angles.x;
		thing.zAngle = angles.z;
	}
	else if (registry.AnyOf<TeleportStone>(entity))
	{
		thing.shape = obstacles::Shape::AtPlace;
		thing.width = obstacles::k_TeleportStoneWidth;
	}
	else if (const auto* pot = registry.TryGet<const Pot>(entity))
	{
		thing.shape = obstacles::Shape::AtPlace;
		if (Locator::infoConstants::has_value())
		{
			const auto& info = Locator::infoConstants::value().pot.at(static_cast<size_t>(pot->type));
			if (info.potType == PotType::PileFood)
			{
				// A food pile is as wide as it is raised by what it holds
				thing.avoidance = obstacles::Avoidance::BurningUnlessScripted;
				thing.width *= magic::piles::ProportionRaised(ResourceType::Food, pot->amount, info.maxAmountInPot);
			}
		}
	}
	else if (registry.AnyOf<Mobile, Ball, ScriptHighlight>(entity))
	{
		thing.shape = obstacles::Shape::AtPlace;
	}
	else if (registry.AnyOf<Abode, Feature, Flowers, SpellDispenser, VillageTotem, WorshipSite, WorshipAltar, WorshipIcon>(
	             entity))
	{
		thing.avoidance = obstacles::Avoidance::Building;
	}

	if (thing.avoidance == obstacles::Avoidance::Building)
	{
		// A building only just begun, with a site taking wood brought to it, isn't walked round yet
		const auto* progress = registry.TryGet<const BuildProgress>(entity);
		const auto* sites = registry.TryGet<const BuildingSites>(entity);
		thing.justBegun = sites != nullptr && ecs::building_site_list::TakingBroughtWood(*sites) != nullptr &&
		                  progress != nullptr && progress->built <= k_JustBegun;
	}
	return thing;
}

obstacles::Walker creature_obstacle_things::WalkerOf(const Registry& registry, entt::entity entity, const Creature& body,
                                                     float radius, glm::vec2 position)
{
	const bool scripted = registry.AnyOf<ScriptControlled>(entity);
	const bool leashed = Locator::leashSystem::has_value() && Locator::leashSystem::value().IsLeashed(entity);
	return {.position = position,
	        .height = obstacles::k_HeightPerSize * ShownSize(body),
	        .radius = radius,
	        .scriptControlled = scripted,
	        .owner = body.owner,
	        .mayBeTakenByVortex = vortex::CreatureMayBeTaken(body.mayGoThroughVortex, leashed, scripted)};
}
