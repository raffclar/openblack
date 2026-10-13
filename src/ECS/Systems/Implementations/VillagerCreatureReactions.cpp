/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "VillagerCreatureReactions.h"

#include <algorithm>

#include <glm/gtx/vec_swizzle.hpp>

#include "3D/MapCoords.h"
#include "Common/GUtilsAngle.h"
#include "Common/GUtilsDistance.h"
#include "Common/GameRandom.h"
#include "Creature/CreatureHelp.h"
#include "Creature/CreatureSpells.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/CreatureSpells.h"
#include "ECS/Components/Dance.h"
#include "ECS/Components/Disciple.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/LivingReaction.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownCreatureAttitude.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/VillagerCreatureInspection.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CreatureFightSystemInterface.h"
#include "ECS/Systems/CreatureHelpSystemInterface.h"
#include "ECS/Systems/LivingActionSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/Systems/TownDesireSystemInterface.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "ObjectMeasures.h"
#include "VillagerFightWatch.h"
#include "VillagerHome.h"
#include "VillagerReactions.h"
#include "VillagerWorship.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;
namespace creature_attitude = openblack::ecs::creature_attitude;
namespace villager_creature = openblack::ecs::villager_creature;
using creature_attitude::Attitude;

namespace
{
/// Game turns in a second
constexpr uint32_t k_TurnsPerSecond = 10;
/// The number of the town's wants a villager's state may serve
constexpr int k_TownDesires = 17;

ecs::Registry& EntityRegistry()
{
	return Locator::entitiesRegistry::value();
}

const GVillagerInfo& InfoOf(const Villager& villager)
{
	const auto& infos = Locator::infoConstants::value().villager;
	return infos.at(static_cast<size_t>(GVillagerInfo::Find(villager.tribe, villager.number)));
}

const ReactionInfo& RowOf(Reaction type)
{
	return Locator::infoConstants::value().reaction.at(static_cast<size_t>(type));
}

const GVillagerStateTableInfo& StateInfo(VillagerStates state)
{
	return Locator::infoConstants::value().villagerStateTable.at(static_cast<size_t>(state));
}

VillagerStates FinalStateOf(const LivingAction& action)
{
	return Locator::livingActionSystem::value().VillagerGetFinalState(action);
}

void SetTop(LivingAction& action, VillagerStates state)
{
	Locator::livingActionSystem::value().VillagerSetState(action, LivingAction::Index::Top, state, false);
}

uint32_t Random(uint32_t range)
{
	return Locator::gameRandom::has_value() ? Locator::gameRandom::value().GameRand(range) : 0;
}

glm::vec2 PositionOf(entt::entity entity)
{
	return glm::xz(EntityRegistry().Get<const Transform>(entity).position);
}

/// The creature the villager reacts to, while it is there
std::optional<entt::entity> CreatureOf(entt::entity villager)
{
	const auto thing = villager_reactions::ReactingTo(villager);
	if (!thing.has_value() || !EntityRegistry().AllOf<Creature, Transform>(*thing))
	{
		return std::nullopt;
	}
	return thing;
}

entt::entity TownOf(entt::entity villager)
{
	const auto* component = EntityRegistry().TryGet<const Villager>(villager);
	if (component == nullptr || component->town == entt::null || !EntityRegistry().Valid(component->town) ||
	    !EntityRegistry().AllOf<Town>(component->town))
	{
		return entt::null;
	}
	return component->town;
}

/// How impressive an action of the creature's table is
ImpressiveType ImpressiveOf(uint32_t action)
{
	const auto& actions = Locator::infoConstants::value().creatureAction;
	return action < actions.size() ? static_cast<ImpressiveType>(actions.at(action).field0xc)
	                               : ImpressiveType::CreatureNotImpressive;
}

/// The plan a creature carries out, when it has something to act on
const creature_planner::Plan* PlanOf(entt::entity creature)
{
	const auto* mind = EntityRegistry().TryGet<const CreatureMindState>(creature);
	if (mind == nullptr || !mind->planner.current.has_value() || !mind->planner.current->object.has_value() ||
	    mind->planner.current->action == 0)
	{
		return nullptr;
	}
	return &*mind->planner.current;
}

ImpressiveType LastImpressive(entt::entity creature)
{
	const auto* mind = EntityRegistry().TryGet<const CreatureMindState>(creature);
	return ImpressiveOf(mind != nullptr ? mind->lastActionDone.value_or(0) : 0);
}

bool Invisible(entt::entity creature)
{
	const auto* spells = EntityRegistry().TryGet<const CreatureSpells>(creature);
	if (spells == nullptr)
	{
		return false;
	}
	const auto phase = spells->spells[creature_spells::Spell::Invisible].phase;
	return phase == creature_spells::Phase::Starting || phase == creature_spells::Phase::Holding ||
	       phase == creature_spells::Phase::Finishing;
}

/// How much the villager's town wants what the state it works towards does for it, times how much that state does
std::optional<float> Wanted(entt::entity villager, entt::entity town)
{
	const auto* action = EntityRegistry().TryGet<const LivingAction>(villager);
	if (action == nullptr || !Locator::townDesireSystem::has_value())
	{
		return std::nullopt;
	}
	const auto& info = StateInfo(FinalStateOf(*action));
	if (info.field0x4 < 0 || info.field0x4 >= k_TownDesires)
	{
		return std::nullopt;
	}
	return Locator::townDesireSystem::value().GetRawDesire(town, static_cast<TownDesireInfo>(info.field0x4)) * info.field0x8;
}

/// The villager walks to a point on its way to or from the creature, then goes into a state
void MoveTowards(LivingAction& action, glm::vec2 point, VillagerStates then)
{
	if (ecs::villager_home::SetupMoveTo(action, point, then))
	{
		Locator::livingActionSystem::value().VillagerSetState(action, LivingAction::Index::Top,
		                                                      VillagerStates::MoveTowardsCreatureReaction, true);
	}
}

uint32_t StopReacting(LivingAction& action)
{
	villager_reactions::StopReacting(action);
	return 0;
}

/// Every turn of reacting to a creature, the villager follows its town's feeling about it: called to worship it goes;
/// with the creature in a fight it stops; indifferent it stops, frightened it runs, and respecting it goes to bow
void FollowTownFeeling(LivingAction& action)
{
	if (ecs::villager_worship::CheckNeededForWorship(action) == 1)
	{
		return;
	}
	auto& registry = EntityRegistry();
	const auto villager = registry.ToEntity(action);
	const auto creature = CreatureOf(villager);
	if (!creature.has_value())
	{
		return;
	}
	if (Locator::creatureFightSystem::has_value() && Locator::creatureFightSystem::value().IsFighting(*creature))
	{
		ecs::villager_fight_watch::WatchCreaturesFight(action, *creature);
		return;
	}
	const auto town = TownOf(villager);
	if (town == entt::null)
	{
		return;
	}
	const auto final = FinalStateOf(action);
	switch (villager_creature::AttitudeOf(town, *creature))
	{
	case Attitude::Indifferent:
		StopReacting(action);
		break;
	case Attitude::Fear:
		if (final != VillagerStates::FleeingFromCreatureReaction)
		{
			SetTop(action, VillagerStates::FleeingFromCreatureReaction);
		}
		break;
	case Attitude::Respect:
		if (final < VillagerStates::InitialiseRespectCreatureReaction ||
		    (final > VillagerStates::FinishRespectCreatureReaction && final != VillagerStates::TurnToFaceCreatureReaction))
		{
			SetTop(action, VillagerStates::InitialiseRespectCreatureReaction);
		}
		break;
	default:
		break;
	}
}

creature_attitude::Timing TimingOf(entt::entity villager, entt::entity creature, float distance, bool again)
{
	const auto& row = RowOf(Reaction::ReactToCreature);
	const auto town = TownOf(villager);
	return {
	    .tableTurns = again ? row.numGameTurnsForNormalThingsBeforeReactingAgain : row.numGameTurnsForNormalThingsToReact,
	    .maxReactionDistance = row.maxReactionDistance,
	    .howImportantIsDistance = row.howImportantIsDistance,
	    .distance = distance,
	    .impressive = LastImpressive(creature),
	    .attitude = town != entt::null ? std::optional(villager_creature::AttitudeOf(town, creature)) : std::nullopt,
	};
}
} // namespace

Attitude villager_creature::AttitudeOf(entt::entity town, entt::entity creature)
{
	const auto* watching = EntityRegistry().TryGet<const TownCreatureAttitude>(town);
	if (watching == nullptr)
	{
		return Attitude::Curious;
	}
	const auto found = std::ranges::find(watching->creatures, creature, &TownCreatureAttitude::Watched::creature);
	return found != watching->creatures.end() ? found->feeling.attitude : Attitude::Curious;
}

void villager_creature::StartWatching(entt::entity town, entt::entity creature, uint32_t turn)
{
	auto& registry = EntityRegistry();
	auto* found = registry.TryGet<TownCreatureAttitude>(town);
	auto& watching = found != nullptr ? *found : registry.Assign<TownCreatureAttitude>(town);
	if (std::ranges::find(watching.creatures, creature, &TownCreatureAttitude::Watched::creature) == watching.creatures.end())
	{
		watching.creatures.push_back({.creature = creature, .feeling = creature_attitude::StartWatching(turn)});
	}
}

void villager_creature::SetFeeling(entt::entity town, entt::entity creature, Attitude attitude, uint32_t turn)
{
	StartWatching(town, creature, turn);
	auto& watching = EntityRegistry().Get<TownCreatureAttitude>(town);
	const auto found = std::ranges::find(watching.creatures, creature, &TownCreatureAttitude::Watched::creature);
	found->feeling.attitude = attitude;
	found->feeling.shownTurn = turn;
}

void villager_creature::TownTurn(entt::entity town, uint32_t turn)
{
	auto& registry = EntityRegistry();
	auto* watching = registry.TryGet<TownCreatureAttitude>(town);
	if (watching == nullptr)
	{
		return;
	}
	// A creature that went is forgotten
	std::erase_if(watching->creatures, [&registry](const TownCreatureAttitude::Watched& watched) {
		return !registry.Valid(watched.creature) || !registry.AllOf<Creature>(watched.creature);
	});
	for (auto& watched : watching->creatures)
	{
		// What it last finished doing tells the town how to feel about it, as long as it means to act on something.
		// TODO(villager-social): the game also takes a hungry creature meaning to act on something it keeps (whose meaning
		// isn't known yet) as frightening
		std::optional<Attitude> shown;
		if (const auto* plan = PlanOf(watched.creature))
		{
			const auto thing = static_cast<entt::entity>(*plan->object);
			const auto* mind = registry.TryGet<const CreatureMindState>(watched.creature);
			shown = creature_attitude::AttitudeShown(mind->lastActionDone.value_or(0),
			                                         registry.Valid(thing) && registry.AllOf<Villager>(thing));
		}
		const bool changed = creature_attitude::Turn(watched.feeling, shown, turn, k_TurnsPerSecond);
		if (changed && Locator::creatureHelpSystem::has_value())
		{
			Locator::creatureHelpSystem::value().Raise(watched.creature,
			                                           {.kind = creature_help::Kind::TownAttitudeToCreature,
			                                            .parameter = static_cast<uint32_t>(watched.feeling.attitude)});
		}
	}
}

uint8_t villager_creature::Priority(entt::entity villager, entt::entity creature)
{
	auto& registry = EntityRegistry();
	if (!Locator::infoConstants::has_value() || !registry.Valid(creature) || !registry.AllOf<Creature, Transform>(creature))
	{
		return 0;
	}
	const auto town = TownOf(villager);
	const auto* disciple = registry.TryGet<const Disciple>(villager);
	const auto& creatureRow = RowOf(Reaction::ReactToCreature);
	const float distance = gutils::GetDistanceInMetres(registry.Get<const Transform>(villager).position,
	                                                   registry.Get<const Transform>(creature).position);
	creature_attitude::Watch watch {
	    .dancing = registry.AllOf<Dancer>(villager),
	    .creatureInvisible = Invisible(creature),
	    .withinReach = distance <= creatureRow.maxDistanceToRunAwayFromObject,
	    .hasTown = town != entt::null,
	    .disciple = disciple != nullptr && disciple->type != VillagerDisciple::None,
	    .creaturePriority = creatureRow.priority,
	    .fleePriority = RowOf(Reaction::FleeFromObject).priority,
	    .followPriority = RowOf(Reaction::FollowObject).priority,
	};
	if (town != entt::null)
	{
		watch.attitude = AttitudeOf(town, creature);
		watch.sameOwner = registry.Get<const Town>(town).owner == registry.Get<const Creature>(creature).owner;
		if (watch.attitude == Attitude::Respect && watch.sameOwner && !watch.disciple)
		{
			watch.wanted = Wanted(villager, town);
		}
	}
	return creature_attitude::VillagerPriority(watch);
}

uint32_t villager_creature::TurnsToReact(entt::entity villager, entt::entity creature, float distance)
{
	return creature_attitude::TurnsToReact(TimingOf(villager, creature, distance, false));
}

uint32_t villager_creature::TurnsBeforeReactingAgain(entt::entity villager, entt::entity creature, float distance)
{
	// What it means to do next, as its plan says
	const auto* mind = EntityRegistry().TryGet<const CreatureMindState>(creature);
	const uint32_t meant = mind != nullptr && mind->planner.current.has_value() ? mind->planner.current->action : 0;
	const bool impressive = ImpressiveOf(meant) != ImpressiveType::CreatureNotImpressive;
	return creature_attitude::TurnsBeforeReactingAgain(TimingOf(villager, creature, distance, true), impressive);
}

std::optional<VillagerStates> villager_creature::Setup(entt::entity villager, entt::entity creature)
{
	auto& registry = EntityRegistry();
	if (!registry.Valid(creature) || !registry.AllOf<Creature>(creature))
	{
		return std::nullopt;
	}
	const auto town = TownOf(villager);
	if (town == entt::null)
	{
		return VillagerStates::InspectCreatureReaction;
	}
	StartWatching(town, creature, Locator::time::has_value() ? Locator::time::value().GetTurn() : 0);
	switch (AttitudeOf(town, creature))
	{
	case Attitude::Curious:
	case Attitude::Respect:
		return VillagerStates::InspectCreatureReaction;
	case Attitude::Fear:
		return VillagerStates::FleeingFromCreatureReaction;
	default:
		return std::nullopt;
	}
}

uint32_t villager_creature::InspectCreatureReaction(LivingAction& action)
{
	auto& registry = EntityRegistry();
	const auto villager = registry.ToEntity(action);
	if (!CreatureOf(villager).has_value())
	{
		return StopReacting(action);
	}
	// It means to look at the creature from twenty metres, for a second and a half and up to three more
	const auto look = static_cast<uint32_t>(creature_attitude::TurnsIn(creature_attitude::k_LookSeconds, k_TurnsPerSecond));
	const auto extra =
	    static_cast<uint32_t>(creature_attitude::TurnsIn(creature_attitude::k_FirstLookExtraSeconds, k_TurnsPerSecond));
	registry.AssignOrReplace<VillagerCreatureInspection>(
	    villager, VillagerCreatureInspection {.keep = creature_attitude::k_FirstLookMetres, .lookTurns = look + Random(extra)});
	SetTop(action, VillagerStates::PerformInspectCreatureReaction);
	FollowTownFeeling(action);
	return 0;
}

uint32_t villager_creature::PerformInspectCreatureReaction(LivingAction& action)
{
	auto& registry = EntityRegistry();
	const auto villager = registry.ToEntity(action);
	const auto creature = CreatureOf(villager);
	auto* inspection = registry.TryGet<VillagerCreatureInspection>(villager);
	if (!creature.has_value() || inspection == nullptr)
	{
		return StopReacting(action);
	}
	--inspection->lookTurns;
	if (inspection->keep < creature_attitude::k_CloseEnoughMetres)
	{
		// As close as it will go, it stops once it has looked long enough
		if (inspection->lookTurns == 0)
		{
			return StopReacting(action);
		}
	}
	else if (inspection->lookTurns == 0)
	{
		SetTop(action, VillagerStates::ApproachCreatureReaction);
		FollowTownFeeling(action);
		return 0;
	}
	else
	{
		villager_reactions::LookAt(villager, registry.Get<const Transform>(*creature).position);
	}
	FollowTownFeeling(action);
	return 0;
}

uint32_t villager_creature::ApproachCreatureReaction(LivingAction& action)
{
	auto& registry = EntityRegistry();
	const auto villager = registry.ToEntity(action);
	const auto creature = CreatureOf(villager);
	auto* inspection = registry.TryGet<VillagerCreatureInspection>(villager);
	if (!creature.has_value() || inspection == nullptr)
	{
		return StopReacting(action);
	}
	// Further than it means to be, it steps closer, and comes back here
	if (const auto step = creature_attitude::StepCloser(PositionOf(villager), PositionOf(*creature), inspection->keep))
	{
		MoveTowards(action, *step, VillagerStates::ApproachCreatureReaction);
		FollowTownFeeling(action);
		return 0;
	}
	// There, it looks again, for a second and a half and up to seven more, and means to stand eight metres closer next
	const auto look = static_cast<uint32_t>(creature_attitude::TurnsIn(creature_attitude::k_LookSeconds, k_TurnsPerSecond));
	const auto extra =
	    static_cast<uint32_t>(creature_attitude::TurnsIn(creature_attitude::k_LaterLookExtraSeconds, k_TurnsPerSecond));
	inspection->lookTurns = look + Random(extra);
	inspection->keep -= creature_attitude::k_StepCloserMetres;
	SetTop(action, VillagerStates::PerformInspectCreatureReaction);
	FollowTownFeeling(action);
	return 0;
}

uint32_t villager_creature::InitialiseRespectCreatureReaction(LivingAction& action)
{
	auto& registry = EntityRegistry();
	const auto villager = registry.ToEntity(action);
	const auto creature = CreatureOf(villager);
	if (!creature.has_value())
	{
		return StopReacting(action);
	}
	const auto place = creature_attitude::BowingPlace(PositionOf(villager), PositionOf(*creature),
	                                                  object_measures::TwoDRadius(registry, villager),
	                                                  object_measures::TwoDRadius(registry, *creature));
	MoveTowards(action, place, VillagerStates::TurnToFaceCreatureReaction);
	FollowTownFeeling(action);
	return 0;
}

uint32_t villager_creature::TurnToFaceCreatureReaction(LivingAction& action)
{
	auto& registry = EntityRegistry();
	const auto villager = registry.ToEntity(action);
	const auto creature = CreatureOf(villager);
	if (!creature.has_value())
	{
		return StopReacting(action);
	}
	if (villager_reactions::LookAt(villager, registry.Get<const Transform>(*creature).position,
	                               villager_reactions::LookSpeed::Quick))
	{
		// Facing it, it starts to count the time it bows
		action.turnsUntilStateChange = 0;
		SetTop(action, VillagerStates::PerformRespectCreatureReaction);
	}
	FollowTownFeeling(action);
	return 0;
}

uint32_t villager_creature::PerformRespectCreatureReaction(LivingAction& action)
{
	auto& registry = EntityRegistry();
	const auto villager = registry.ToEntity(action);
	const auto creature = CreatureOf(villager);
	if (!creature.has_value())
	{
		return StopReacting(action);
	}
	// The creature moved round it: it turns to face it again
	const auto towards = gutils::GetAngleFromXZ(PositionOf(villager), PositionOf(*creature));
	if (creature_attitude::DriftedFrom(registry.Get<const WallHug>(villager).gameAngle, towards))
	{
		SetTop(action, VillagerStates::TurnToFaceCreatureReaction);
		return 0;
	}
	++action.turnsUntilStateChange;
	if (static_cast<int16_t>(action.turnsUntilStateChange) >
	    creature_attitude::TurnsIn(creature_attitude::k_RespectSeconds, k_TurnsPerSecond))
	{
		SetTop(action, VillagerStates::FinishRespectCreatureReaction);
	}
	FollowTownFeeling(action);
	return 0;
}

uint32_t villager_creature::FinishRespectCreatureReaction(LivingAction& action)
{
	return StopReacting(action);
}

uint32_t villager_creature::FleeingFromCreatureReaction(LivingAction& action)
{
	auto& registry = EntityRegistry();
	const auto villager = registry.ToEntity(action);
	const auto creature = CreatureOf(villager);
	if (!creature.has_value() || !Locator::infoConstants::has_value())
	{
		return StopReacting(action);
	}
	const float run = RowOf(Reaction::ReactToCreature).maxReactionDistance * creature_attitude::k_FleeReachFactor;
	const auto here = PositionOf(villager);
	const auto there = PositionOf(*creature);
	// Far enough away it stops
	if (run * creature_attitude::k_FleeGiveUpFactor < gutils::GetDistanceInMetres(here, there))
	{
		return StopReacting(action);
	}
	const auto to = creature_attitude::FleePoint(here, there, run);
	if (!map_coords::InBounds(map_coords::FromMetres(to)))
	{
		FollowTownFeeling(action);
		return 0;
	}
	const auto& info = InfoOf(registry.Get<const Villager>(villager));
	const auto fleeing = static_cast<int32_t>(info.speedGroup.speedFleeing);
	// While the creature means to act on something, it sets off now and then, at half its fleeing speed
	if (PlanOf(*creature) != nullptr)
	{
		if (Random(creature_attitude::k_FleeChance) != 0)
		{
			return 0;
		}
		villager_reactions::SetSpeed(villager, fleeing / 2);
		MoveTowards(action, to, VillagerStates::FleeingFromCreatureReaction);
		return 0;
	}
	villager_reactions::SetSpeed(villager, fleeing);
	return 0;
}

uint32_t villager_creature::MoveTowardsCreatureReaction(LivingAction& action)
{
	auto& registry = EntityRegistry();
	const auto villager = registry.ToEntity(action);
	if (!CreatureOf(villager).has_value())
	{
		return StopReacting(action);
	}
	// Its walk over within half a metre of the goal, it goes on into the state it walked to
	const auto goal = registry.Get<const WallHug>(villager).goal;
	if (ecs::villager_home::WalkIsOverAt(villager, goal))
	{
		const auto at = ecs::villager_home::WalkPosition(villager);
		const auto goalAt = map_coords::FromMetres(goal);
		if (gutils::GetDistanceInMetres(at, glm::ivec2(goalAt.x, goalAt.z)) <= creature_attitude::k_ArrivedMetres)
		{
			registry.Remove<MoveStateFinalStepTag, MoveStateArrivedTag>(villager);
			Locator::livingActionSystem::value().VillagerSetTopStateToFinal(action);
		}
	}
	FollowTownFeeling(action);
	return 0;
}
