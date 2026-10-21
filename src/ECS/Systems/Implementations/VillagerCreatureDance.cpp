/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "VillagerCreatureDance.h"

#include <algorithm>
#include <ranges>

#include <glm/gtx/vec_swizzle.hpp>

#include "3D/CreatureBody.h"
#include "3D/MapCoords.h"
#include "Creature/CreatureDance.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureDance.h"
#include "ECS/Components/Dance.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownDesire.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Dances.h"
#include "ECS/Registry.h"
#include "ECS/Systems/LivingActionSystemInterface.h"
#include "ECS/Systems/ReactionSystemInterface.h"
#include "ECS/VillagerAnimation.h"
#include "ECS/WallHugRules.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/Impressiveness.h"
#include "MagicLiving.h"
#include "VillagerArtefactDance.h"
#include "VillagerDance.h"
#include "VillagerDisciples.h"
#include "VillagerFootpath.h"
#include "VillagerHome.h"
#include "VillagerReactions.h"

using namespace openblack;
using namespace openblack::ecs::components;
namespace villager_creature_dance = openblack::ecs::villager_creature_dance;
using openblack::map_coords::MapCoords;

namespace
{
/// The move of a dance that turns its dancers, which a villager back from a reaction dances whatever its group does
constexpr uint32_t k_TurnMove = 9;

ecs::Registry& Entities()
{
	return Locator::entitiesRegistry::value();
}

const GVillagerInfo& InfoOf(const Villager& villager)
{
	return Locator::infoConstants::value().villager.at(static_cast<size_t>(RowOf(villager, GVillagerInfo::Find)));
}

/// Where the villager stands, in whole map units: where its walk holds it, unless something else moved it since
glm::ivec2 WholePositionOf(entt::entity villager)
{
	auto& registry = Entities();
	const auto metres = glm::xz(registry.Get<const Transform>(villager).position);
	const auto* wallHug = registry.TryGet<const WallHug>(villager);
	return wallHug != nullptr && wallHug->placedAt == metres ? wallHug->position : ecs::wall_hug::ToWhole(metres);
}

/// The dance a villager dances in, when it dances in a group of it
const Dance* PlacedDanceOf(entt::entity villager)
{
	auto& registry = Entities();
	const auto dance = ecs::dances::DanceOf(registry, villager);
	if (dance == entt::null || !ecs::villager_dance::SlotOf(villager).has_value())
	{
		return nullptr;
	}
	return &registry.Get<const Dance>(dance);
}

/// The villager goes into a state as any change of state goes
void ChangeTo(LivingAction& action, VillagerStates state)
{
	static_cast<void>(Locator::livingActionSystem::value().VillagerChangeTopState(action, state));
}
} // namespace

bool villager_creature_dance::AvailableToBeSummoned(entt::entity villager)
{
	auto& registry = Entities();
	const auto* person = registry.TryGet<const Villager>(villager);
	if (person == nullptr || !Locator::infoConstants::has_value() || ecs::dances::DanceOf(registry, villager) != entt::null ||
	    person->lifeStage == Villager::LifeStage::Child || villager_disciple::TypeOf(villager) != VillagerDisciple::None)
	{
		return false;
	}
	const auto life = systems::magic_living::LifeOf(villager).value_or(person->life);
	return life > InfoOf(*person).damageThresholdToGoHome;
}

std::vector<entt::entity> villager_creature_dance::SummonOrder(entt::entity town)
{
	auto& registry = Entities();
	std::vector<entt::entity> order;
	if (town == entt::null || !registry.Valid(town) || !registry.AllOf<Town>(town))
	{
		return order;
	}
	for (const auto abode : registry.Get<const Town>(town).abodes)
	{
		const auto* building = registry.Valid(abode) ? registry.TryGet<const Abode>(abode) : nullptr;
		if (building == nullptr)
		{
			continue;
		}
		for (const auto villager : building->inhabitants)
		{
			if (registry.Valid(villager) && registry.AllOf<Villager, LivingAction, Transform>(villager) &&
			    AvailableToBeSummoned(villager))
			{
				order.push_back(villager);
			}
		}
	}
	// The creature calls the last one found first
	std::ranges::reverse(order);
	return order;
}

bool villager_creature_dance::StartWorshippingCreature(entt::entity villager, entt::entity dance)
{
	auto& registry = Entities();
	auto* action = registry.TryGet<LivingAction>(villager);
	if (action == nullptr || !ecs::dances::AddDancer(registry, dance, villager, villager_dance::DanceSex(villager)) ||
	    !villager_dance::SlotOf(villager).has_value())
	{
		return false;
	}
	auto& living = Locator::livingActionSystem::value();
	// A villager whose state leaves a dance on its way out goes straight under control, so that it keeps its new place
	if (villager_artefact_dance::IsDanceState(living.VillagerGetFinalState(*action)))
	{
		action->states.at(static_cast<size_t>(LivingAction::Index::Top)) =
		    static_cast<uint8_t>(VillagerStates::ControlledByCreature);
	}
	ChangeTo(*action, VillagerStates::ControlledByCreature);
	const auto& info = InfoOf(registry.Get<const Villager>(villager));
	systems::villager_reactions::SetSpeed(villager, static_cast<int32_t>(info.speedGroup.speed3));
	return true;
}

bool villager_creature_dance::SendToPlace(entt::entity villager, entt::entity dance)
{
	auto& registry = Entities();
	const auto place = villager_dance::SlotOf(villager);
	auto* action = registry.TryGet<LivingAction>(villager);
	if (!place.has_value() || action == nullptr)
	{
		return false;
	}
	const auto& info = InfoOf(registry.Get<const Villager>(villager));
	const auto run = static_cast<int32_t>(info.speedGroup.speedFleeing) * 2;
	systems::villager_reactions::SetSpeed(villager, run);
	// The villager walks the dance's footpaths where it has any, else straight to its place
	static_cast<void>(villager_footpath::SetupMoveToOnFootpath(*action, dance, *place, VillagerStates::ControlledByCreature));
	// Its new state set its speed: the run is set again
	systems::villager_reactions::SetSpeed(villager, run);
	return true;
}

void villager_creature_dance::TellToDance(entt::entity villager)
{
	auto* action = Entities().TryGet<LivingAction>(villager);
	if (action != nullptr && PlacedDanceOf(villager) != nullptr)
	{
		ChangeTo(*action, VillagerStates::WorshippingCreature);
	}
}

void villager_creature_dance::WorshipCreature(entt::entity villager, entt::entity owner, DanceInfo type)
{
	auto& registry = Entities();
	const auto* person = registry.TryGet<const Villager>(villager);
	if (person == nullptr || owner == entt::null || !registry.Valid(owner) || !Locator::infoConstants::has_value() ||
	    !Locator::reactionSystem::has_value())
	{
		return;
	}
	const auto town = person->town;
	const auto* creature = registry.TryGet<const Creature>(owner);
	if (creature == nullptr || town == entt::null || !registry.Valid(town) || !registry.AllOf<Town>(town))
	{
		return;
	}
	const auto& constants = Locator::infoConstants::value();
	const auto row = static_cast<size_t>(type);
	const auto species = creature::InfoRow(creature->species);
	if (type == DanceInfo::None || row >= constants.dance.size() || species >= constants.creature.size())
	{
		return;
	}
	const auto* stats = registry.TryGet<const TownStats>(town);
	const auto people = stats != nullptr ? stats->adults + stats->children : 0;
	const auto& info = constants.creature.at(species);
	const auto size =
	    magic::CreatureImpressiveSize(ShownSize(*creature), info.impressiveSizeAddition, info.impressiveSizeMultiplier);
	const auto belief = creature_dance::WorshipBelief(constants.town.populationForUnmodifiedBelief, people, size,
	                                                  constants.dance.at(row).impressiveness);
	Locator::reactionSystem::value().GiveTownBelief(town, creature->owner, belief, villager, GuidanceAlignment::Good);
}

uint32_t villager_creature_dance::ControlledByCreature(LivingAction& action)
{
	auto& registry = Entities();
	const auto villager = registry.ToEntity(action);
	const auto* dance = PlacedDanceOf(villager);
	if (dance == nullptr)
	{
		return 1;
	}
	const auto place = villager_dance::SlotOf(villager);
	const auto here = WholePositionOf(villager);
	const auto slot = ecs::wall_hug::ToWhole(*place);
	// Only a villager that stands right at the middle of the dance walks to its place
	if (here != slot && here == glm::ivec2(dance->place.x, dance->place.z))
	{
		auto& living = Locator::livingActionSystem::value();
		if (living.VillagerSetCurrentAndDestinationState(action, VillagerStates::MoveToPos,
		                                                 VillagerStates::ControlledByCreature))
		{
			villager_home::SetupMobileMoveTo(action, *place);
		}
	}
	return 1;
}

bool villager_creature_dance::ExitControlledByCreature(LivingAction& action, VillagerStates next)
{
	if (next == VillagerStates::InHand || next == VillagerStates::Flying || villager_animation::IsDeathState(next))
	{
		auto& registry = Entities();
		const auto villager = registry.ToEntity(action);
		// It leaves the list of the first creature that called it
		bool left = false;
		registry.Each<CreatureDance>([villager, &left](entt::entity, CreatureDance& dance) {
			const auto found = std::ranges::find(dance.dancers, villager);
			if (left || found == dance.dancers.end())
			{
				return;
			}
			dance.dancers.erase(found);
			left = true;
		});
		ecs::dances::RemoveDancer(registry, villager);
		return false;
	}
	return next != VillagerStates::ControlledByCreature && next != VillagerStates::WorshippingCreature &&
	       next != VillagerStates::DecideWhatToDo;
}

uint32_t villager_creature_dance::WorshippingCreature(LivingAction& action)
{
	const auto villager = Entities().ToEntity(action);
	const auto* dance = PlacedDanceOf(villager);
	if (dance == nullptr)
	{
		return 1;
	}
	if (dance->state == Dance::State::Dancing)
	{
		return villager_dance::PerformDance(action, VillagerStates::WorshippingCreature);
	}
	villager_home::GoHome(action);
	return 1;
}

uint32_t villager_creature_dance::RestartWorshippingCreature(LivingAction& action)
{
	const auto villager = Entities().ToEntity(action);
	if (PlacedDanceOf(villager) != nullptr)
	{
		return villager_dance::PerformDance(action, VillagerStates::WorshippingCreature, k_TurnMove);
	}
	villager_home::GoHome(action);
	return 1;
}
