/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "HandToolTips.h"

#include <glm/geometric.hpp>
#include <glm/gtx/vec_swizzle.hpp>

#include "3D/LandIslandInterface.h"
#include "Camera/Camera.h"
#include "Camera/CameraDrag.h"
#include "Camera/ScriptCameraModel.h"
#include "ECS/AltarOffering.h"
#include "ECS/BuildingSites.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Construction.h"
#include "ECS/Components/Feature.h"
#include "ECS/Components/HandGrab.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Reward.h"
#include "ECS/Components/ScriptHighlight.h"
#include "ECS/Components/SpellDispenser.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/ObjectPhysics.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CameraHelpSystemInterface.h"
#include "ECS/Systems/HandGrabSystemInterface.h"
#include "ECS/Systems/ResourceStoreSystemInterface.h"
#include "ECS/TempleConstruction.h"
#include "ECS/TownHomes.h"
#include "ECS/WorldObjects.h"
#include "Locator.h"

namespace openblack::gui::hand_tooltip
{

namespace
{
using namespace ecs::components;

constexpr PlayerNames k_LocalPlayer = PlayerNames::PLAYER_ONE;

/// Whether a building of the world belongs to the local player or to no player, none for a thing that is not a
/// building that the player can build or repair
std::optional<bool> OwnOrNoPlayer(const ecs::Registry& registry, entt::entity object)
{
	if (registry.AllOf<Abode>(object))
	{
		// A building belongs to the player of its town. A neutral town has the neutral player.
		const auto town = ecs::town_homes::TownOfAbode(registry, object);
		const auto* data = town != entt::null && registry.Valid(town) ? registry.TryGet<const Town>(town) : nullptr;
		return data == nullptr || data->owner == k_LocalPlayer;
	}
	// A miracle dispenser is a building of its town in the same way
	if (const auto* dispenser = registry.TryGet<const SpellDispenser>(object))
	{
		const auto* data = registry.Valid(dispenser->town) ? registry.TryGet<const Town>(dispenser->town) : nullptr;
		return data == nullptr || data->owner == k_LocalPlayer;
	}
	if (const auto* temple = registry.TryGet<const Temple>(object))
	{
		return temple->owner == k_LocalPlayer;
	}
	if (const auto* site = registry.TryGet<const WorshipSite>(object))
	{
		return site->player == k_LocalPlayer;
	}
	return std::nullopt;
}
} // namespace

uint32_t CameraFeatures()
{
	return Locator::cameraHelpSystem::has_value() ? Locator::cameraHelpSystem::value().Get().features
	                                              : camera_drag::k_DefaultFeatures;
}

CameraFacts CameraNow()
{
	CameraFacts facts {.features = CameraFeatures()};
	if (!Locator::camera::has_value())
	{
		return facts;
	}
	const auto& camera = Locator::camera::value();
	facts.tricons = camera.GetModel().GetHandCues().tricons;
	const auto origin = camera.GetOrigin();
	// The land at the middle of the screen
	if (const auto hit = camera.RaycastScreenCoordToLand({0.5f, 0.5f}, true); hit.has_value())
	{
		facts.centreDistance = glm::distance(origin, hit->position);
	}
	facts.pitch = script_camera::HeadingAndPitchFromPoints(origin, camera.GetFocus()).pitch;
	return facts;
}

std::optional<BuildingToolTips> BuildingOf(entt::entity object)
{
	const auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(object) || registry.AllOf<Feature>(object))
	{
		return std::nullopt;
	}
	const auto own = OwnOrNoPlayer(registry, object);
	if (!own.has_value())
	{
		return std::nullopt;
	}
	const bool built = ecs::construction::IsBuilt(object);
	const float life = ecs::world_objects::LifeOf(object);
	const bool whole = built && life >= 1.0f;
	if (!OffersBuildingToolTips(whole, *own, CameraFeatures()))
	{
		return std::nullopt;
	}
	const float completed = BuildingCompleted(built, ecs::construction::BuiltOf(object), life);
	// The wood that the site wants, which takes the wood that a hand brings
	const auto* site = ecs::construction::BroughtWoodSite(object);
	const auto wood = site != nullptr ? std::optional(ecs::construction::WoodNeededToBuild(site->town, object)) : std::nullopt;
	return BuildingToolTips {.wood = WoodRequired(wood, completed), .completed = completed};
}

bool IsInteractable(entt::entity object)
{
	const auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(object))
	{
		return false;
	}
	// A building with no part built yet
	return !registry.AllOf<Abode>(object) || ecs::construction::BuiltOf(object) != 0.0f;
}

bool IsMoving(entt::entity object)
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* transform = registry.Valid(object) ? registry.TryGet<const Transform>(object) : nullptr;
	if (transform == nullptr || !registry.AllOf<InPhysics>(object))
	{
		return false;
	}
	const float ground =
	    Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(glm::xz(transform->position)) : 0.0f;
	return transform->position.y >= ground;
}

std::optional<Offer> TapOf(entt::entity object)
{
	const auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(object) || registry.AllOf<CannotBePickedUp>(object))
	{
		return std::nullopt;
	}
	// A town's building, but not its village centre, is knocked on
	if (const auto* abode = registry.TryGet<const Abode>(object))
	{
		return abode->type != AbodeNumber::TownCentre ? std::optional(TapOffer(TapKind::Knock)) : std::nullopt;
	}
	if (registry.AllOf<SpellDispenser>(object))
	{
		return TapOffer(TapKind::Knock);
	}
	if (ecs::object_physics::CanTapRock(object))
	{
		return TapOffer(TapKind::Break);
	}
	// The player's own reward chest opens once
	if (const auto* reward = registry.TryGet<const Reward>(object);
	    reward != nullptr && reward->player == k_LocalPlayer && reward->state != Reward::State::Opened)
	{
		return TapOffer(TapKind::Open);
	}
	return std::nullopt;
}

std::optional<uint32_t> TakeableTapOf(entt::entity object)
{
	// Of the things that the hand can take, only a big rock does something when tapped
	return ecs::object_physics::CanTapRock(object) ? std::optional(k_TapToBreak) : std::nullopt;
}

std::optional<Offer> ScrollOf(entt::entity object)
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* highlight = registry.Valid(object) ? registry.TryGet<const ScriptHighlight>(object) : nullptr;
	return highlight != nullptr ? std::optional(ScrollOffer(highlight->active)) : std::nullopt;
}

HeldUse UseAt(entt::entity held, std::optional<entt::entity> target)
{
	const auto& registry = Locator::entitiesRegistry::value();
	if (!target.has_value() || *target == held || !registry.Valid(*target) || !registry.Valid(held))
	{
		return HeldUse::None;
	}
	if (Locator::handGrabSystem::has_value() && Locator::handGrabSystem::value().CreatureTakesHeld(*target))
	{
		return HeldUse::Give;
	}
	// A worship site altar takes a villager, an animal, a tree or a dead tree, though it can refuse it when given
	if (ecs::altar_offering::Takes(*target, held))
	{
		return HeldUse::Sacrifice;
	}
	if (!Locator::resourceStoreSystem::has_value())
	{
		return HeldUse::None;
	}
	// A store of its resource takes it. A pot also goes onto a pot or pile of the same resource.
	const auto& stores = Locator::resourceStoreSystem::value();
	const auto resource = stores.ResourceOf(held).type;
	if (resource == ResourceType::None)
	{
		return HeldUse::None;
	}
	if (stores.IsStore(*target, resource))
	{
		return HeldUse::Supply;
	}
	const bool potOnPot =
	    registry.AllOf<Pot>(held) && registry.AllOf<Pot>(*target) && stores.ResourceOf(*target).type == resource;
	return potOnPot ? HeldUse::Supply : HeldUse::None;
}

} // namespace openblack::gui::hand_tooltip
