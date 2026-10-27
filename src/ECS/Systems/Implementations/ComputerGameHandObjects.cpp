/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "ComputerGameHandObjects.h"

#include <optional>

#include "3D/MapCoords.h"
#include "Animals/FishFarmRules.h"
#include "ComputerMindCommon.h"
#include "ECS/Archetypes/PotArchetype.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/FishFarm.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Systems/FieldSystemInterface.h"
#include "ECS/Systems/FishFarmSystemInterface.h"
#include "ECS/Systems/HandGrabSystemInterface.h"
#include "ECS/Systems/ResourceStoreSystemInterface.h"
#include "InfoConstants.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::systems::computer_world;

GameHandObjects::GameHandObjects(PlayerNames player, ComputerPlayer& state)
    : _player(player)
    , _state(state)
{
}

bool GameHandObjects::HandHasRoom() const
{
	return !HeldBy(_player).has_value();
}

void GameHandObjects::PutDownHeld(glm::vec3 hand)
{
	const auto held = HeldBy(_player);
	if (!held.has_value() || !Locator::handGrabSystem::has_value())
	{
		return;
	}
	// The hand lets go with no speed, where it is
	computer_world::ReportHand(_player, hand, _state.hand.camera);
	Locator::handGrabSystem::value().DoLetGo(
	    _player, player_commands::HandLetGo {.object = *held, .velocity = glm::vec3(0.0f), .start = {.origin = hand}});
}

bool GameHandObjects::CanPickUp(entt::entity thing) const
{
	return thing != entt::null && Entities().Valid(thing);
}

void GameHandObjects::PickUp(entt::entity thing, glm::vec3 hand)
{
	auto& registry = Entities();
	if (!Locator::handGrabSystem::has_value() || HandOf(_player) == entt::null)
	{
		return;
	}
	computer_world::ReportHand(_player, hand, _state.hand.camera);
	auto& grab = Locator::handGrabSystem::value();
	if (registry.AllOf<FishFarm>(thing))
	{
		PickUpFishFarm(thing, hand);
		return;
	}
	const auto* field = registry.TryGet<const Field>(thing);
	if (field == nullptr)
	{
		grab.DoTake(_player, thing);
		return;
	}
	// A field goes into the hand only with a crop that has grown and holds food
	if (!(field->crop.age > 0.0f) || !(field->crop.food > 0.0f) || !Locator::fieldSystem::has_value() ||
	    !Locator::infoConstants::has_value())
	{
		return;
	}
	auto& fields = Locator::fieldSystem::value();
	const auto& type = Locator::infoConstants::value().fieldType.at(static_cast<size_t>(field->type));
	// The field gives a pot of the food that a hand takes from a field, made where the field is
	const auto handful = map_coords::FtoL(type.foodValueTakenWithHand);
	const auto* at = registry.TryGet<const Transform>(thing);
	const auto pot = archetypes::PotArchetype::Create(at != nullptr ? at->position : hand, 0.0f, PotInfo::HandFood, handful,
	                                                  TownNumbered(field->town));
	grab.DoTake(_player, pot);
	fields.RemoveFood(thing, static_cast<float>(handful));
	// Then all the food left in the field goes into the pot in the hand
	const auto held = HeldBy(_player);
	auto* potData = held.has_value() ? registry.TryGet<Pot>(*held) : nullptr;
	if (potData == nullptr || !registry.AllOf<Field>(thing))
	{
		return;
	}
	const auto rest = map_coords::FtoL(registry.Get<const Field>(thing).crop.food);
	fields.RemoveFood(thing, static_cast<float>(rest));
	potData->amount += rest;
}

void GameHandObjects::PickUpFishFarm(entt::entity farm, glm::vec3 hand)
{
	auto& registry = Entities();
	if (!Locator::fishFarmSystem::has_value() || !Locator::infoConstants::has_value())
	{
		return;
	}
	// A farm whose kind holds no food gives nothing. The farm loses no fish.
	if (!(fish_farm::Full(Locator::fishFarmSystem::value().GetType()) > 0.0f))
	{
		return;
	}
	// The farm gives a pot of the food of a hand's pot of food, made where the farm is, that belongs to the farm's town
	const auto& potInfo = Locator::infoConstants::value().pot.at(static_cast<size_t>(PotInfo::HandFood));
	const auto handful = map_coords::FtoL(potInfo.foodValue);
	const auto* at = registry.TryGet<const Transform>(farm);
	const auto pot = archetypes::PotArchetype::Create(at != nullptr ? at->position : hand, 0.0f, PotInfo::HandFood, handful,
	                                                  registry.Get<const FishFarm>(farm).town);
	Locator::handGrabSystem::value().DoTake(_player, pot);
}

std::optional<uint32_t> GameHandObjects::HeldPotHolds(ResourceType type) const
{
	const auto held = HeldBy(_player);
	if (!held.has_value() || !Entities().AllOf<Pot>(*held) || !Locator::resourceStoreSystem::has_value())
	{
		return std::nullopt;
	}
	const auto& stores = Locator::resourceStoreSystem::value();
	if (stores.ResourceOf(*held).type != type)
	{
		return std::nullopt;
	}
	return stores.GetResource(*held, type);
}

uint32_t GameHandObjects::ResourceIn(entt::entity thing, ResourceType type) const
{
	return Locator::resourceStoreSystem::has_value() ? Locator::resourceStoreSystem::value().GetResource(thing, type) : 0u;
}

bool GameHandObjects::TakeFromContainer(entt::entity container, ResourceType type, uint32_t amount, glm::vec3 hand)
{
	auto& registry = Entities();
	if (!Locator::resourceStoreSystem::has_value() || !Locator::handGrabSystem::has_value() || !registry.Valid(container))
	{
		return false;
	}
	auto& stores = Locator::resourceStoreSystem::value();
	const auto inPot = HeldPotHolds(type);
	stores.RemoveResource(container, type, amount);
	if (inPot.has_value())
	{
		stores.AddResource(*HeldBy(_player), type, amount, std::nullopt);
		return true;
	}
	if (HandOf(_player) == entt::null)
	{
		return false;
	}
	// A new pot of a hand's food or wood, made at the container, that belongs to the town of the container
	entt::entity town = entt::null;
	if (const auto* pile = registry.TryGet<const Pot>(container))
	{
		town = pile->town;
	}
	else if (const auto* abode = registry.TryGet<const Abode>(container))
	{
		town = TownNumbered(static_cast<int>(abode->townId));
	}
	const auto* at = registry.TryGet<const Transform>(container);
	const auto potType = type == ResourceType::Wood ? PotInfo::HandWood : PotInfo::HandFood;
	const auto pot = archetypes::PotArchetype::Create(at != nullptr ? at->position : hand, 0.0f, potType,
	                                                  static_cast<int32_t>(amount), town);
	computer_world::ReportHand(_player, hand, _state.hand.camera);
	Locator::handGrabSystem::value().DoTake(_player, pot);
	return HeldBy(_player) == std::optional<entt::entity> {pot};
}

void GameHandObjects::DropHeld(glm::vec3 hand)
{
	PutDownHeld(hand);
}

void GameHandObjects::UseHeldOn(entt::entity target, glm::vec3 hand)
{
	if (!HeldBy(_player).has_value() || !Locator::handGrabSystem::has_value())
	{
		return;
	}
	computer_world::ReportHand(_player, hand, _state.hand.camera);
	Locator::handGrabSystem::value().DoApply(_player, target);
}
