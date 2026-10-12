/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "VillagerFire.h"

#include <cmath>

#include <algorithm>
#include <chrono>
#include <numbers>

#include <LNDFile.h>
#include <glm/geometric.hpp>
#include <glm/gtx/euler_angles.hpp>

#include "3D/AllMeshes.h"
#include "3D/L3DAnim.h"
#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "Common/GUtilsDistance.h"
#include "Common/GameRandom.h"
#include "ECS/Components/AtHome.h"
#include "ECS/Components/DetailMeshes.h"
#include "ECS/Components/Fire.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/LivingReaction.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/ScriptControl.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownDesire.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/VillagerDeath.h"
#include "ECS/Components/WallHug.h"
#include "ECS/GraveyardRules.h"
#include "ECS/Graveyards.h"
#include "ECS/Registry.h"
#include "ECS/Systems/FireSystemInterface.h"
#include "ECS/Systems/LivingActionSystemInterface.h"
#include "ECS/Systems/ReactionSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/Systems/TownSystemInterface.h"
#include "ECS/VillagerAfterDeath.h"
#include "ECS/VillagerAnimation.h"
#include "ECS/VillagerCarry.h"
#include "ECS/VillagerCheckRules.h"
#include "ECS/VillagerClips.h"
#include "ECS/VillagerDroppedLog.h"
#include "ECS/VillagerMemory.h"
#include "ECS/WalkerPlacement.h"
#include "ECS/WorldObjects.h"
#include "Fire/ViaPoint.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"
#include "VillagerDisciples.h"
#include "VillagerHome.h"
#include "VillagerReactions.h"

using namespace openblack;
using namespace openblack::ecs::components;
namespace villager_fire = openblack::ecs::villager_fire;
namespace villager_home = openblack::ecs::villager_home;
namespace world_objects = openblack::ecs::world_objects;
namespace villager_clips = openblack::ecs::villager_clips;

namespace
{
constexpr float k_TwoPi = 2.0f * std::numbers::pi_v<float>;
/// A reaction younger than this many turns makes a villager nearer than its run-away distance run back
constexpr uint32_t k_NewReactionTurns = 25;
/// Past the safe radius by up to this much
constexpr float k_SafeMargin = 2.0f;
/// A blaze needs a fireman for every this many times a villager's radius round all its burning fires; the town's
/// nearness counts within this distance; and a blaze is fought when its need comes to more than this
constexpr float k_FiremanSpacing = 4.0f;
constexpr float k_TownReach = 400.0f;
constexpr float k_FightThreshold = 0.1f;
/// A fireman beats a fire from within this far beyond the edge it stands back by
constexpr float k_BeatingRange = 2.0f;
/// A beat cools the fire as a burn of this much
constexpr float k_BeatBurn = -8.0f;
/// The fire a fireman fights takes in another fire within twice this far of it, instead of the fireman reacting to it
constexpr float k_MergeReachBase = 10.0f;
/// A villager on fire from its own fire runs at least this far each time, up to this much further
constexpr float k_OnFireRun = 4.0f;
constexpr float k_OnFireRunMore = 6.0f;
/// Running from a fire that set it alight, it goes this much further than the fire's reach at most
constexpr float k_RunFromFire = 10.0f;
/// Going round a fire, a villager keeps this much further off than its own radius
constexpr float k_SkirtMargin = 1.0f;
/// The scan of a blaze for a way round it gives up after this many detours
constexpr int k_MostDetours = 1000;
/// The game's angles: a whole turn in 2048, and a villager turning towards something by a quarter of 1024 a turn
constexpr int32_t k_AngleUnits = 2048;
constexpr int32_t k_LookStep = 0x100;

ecs::Registry& Entities()
{
	return Locator::entitiesRegistry::value();
}

entt::entity EntityOf(LivingAction& action)
{
	return Entities().ToEntity(action);
}

ecs::systems::LivingActionSystemInterface& Living()
{
	return Locator::livingActionSystem::value();
}

VillagerStates State(const LivingAction& action, LivingAction::Index index)
{
	return Living().VillagerGetState(action, index);
}

/// The state a villager is in, when that is an end in itself, or else the one it is walking to
VillagerStates FinalState(const LivingAction& action)
{
	return Living().VillagerGetFinalState(action);
}

void SetTopState(LivingAction& action, VillagerStates state)
{
	Living().VillagerSetState(action, LivingAction::Index::Top, state, false);
}

void SetPrevious(LivingAction& action, VillagerStates state)
{
	Living().VillagerSetState(action, LivingAction::Index::Previous, state, true);
}

const GVillagerStateTableInfo& StateInfo(VillagerStates state)
{
	return Locator::infoConstants::value().villagerStateTable.at(static_cast<size_t>(state));
}

float GameFloatRandom(float range)
{
	return Locator::gameRandom::has_value() ? Locator::gameRandom::value().GameFloatRand(range) : 0.0f;
}

const GVillagerInfo* InfoOf(entt::entity villager)
{
	const auto* person = Entities().TryGet<const Villager>(villager);
	if (person == nullptr)
	{
		return nullptr;
	}
	const auto& infos = Locator::infoConstants::value().villager;
	const auto kind = static_cast<size_t>(GVillagerInfo::Find(person->tribe, person->number));
	return kind < infos.size() ? &infos[kind] : nullptr;
}

/// The villager moves at the speed of a state, as its table says
void SetStateSpeed(entt::entity villager, VillagerStates state)
{
	auto* wallHug = Entities().TryGet<WallHug>(villager);
	const auto* info = InfoOf(villager);
	if (wallHug == nullptr || info == nullptr)
	{
		return;
	}
	const auto& speeds = info->speedGroup;
	const std::array<SpeedState, 6> table {speeds.speedDefault, speeds.speedFleeing, speeds.speed2,
	                                       speeds.speed3,       speeds.speed4,       speeds.speed5};
	const auto index = std::min<size_t>(StateInfo(state).speedIndex, table.size() - 1);
	wallHug->speed = GetSpeedStateSpeed(table.at(index));
}

VillagerFireState& FireStateOf(entt::entity villager)
{
	auto& registry = Entities();
	return registry.AllOf<VillagerFireState>(villager) ? registry.Get<VillagerFireState>(villager)
	                                                   : registry.Assign<VillagerFireState>(villager);
}

glm::vec2 Across(const glm::vec3& point)
{
	return {point.x, point.z};
}

glm::vec3 PositionOf(entt::entity object)
{
	const auto* transform = Entities().TryGet<const Transform>(object);
	return transform != nullptr ? transform->position : glm::vec3(0.0f);
}

/// The game's distance between two points, on the ground, through map positions
float Distance(const glm::vec3& a, const glm::vec3& b)
{
	return gutils::GetDistanceInMetres(a, b);
}

/// A villager's radius across the ground
float RadiusOf(entt::entity object)
{
	return world_objects::SizeOf(object).radius;
}

/// A point of the ground as a map position, at no height
map_coords::MapCoords ToCoords(glm::vec2 point)
{
	return map_coords::FromMetres(point);
}

glm::vec2 CoordsToMetres(const map_coords::MapCoords& coords)
{
	return map_coords::ToMetres(coords);
}

/// The way from one point to another as one of the game's 2048 angles of a turn, in radians
float AngleBetween(glm::vec2 from, glm::vec2 to)
{
	const auto d = to - from;
	auto units = static_cast<int32_t>(std::atan2(d.y, d.x) * static_cast<float>(k_AngleUnits) / k_TwoPi);
	units &= k_AngleUnits - 1;
	return static_cast<float>(units) * k_TwoPi / static_cast<float>(k_AngleUnits);
}

/// A map position a distance along an angle, x by its cosine and z by its sine, in whole map units
map_coords::MapCoords PosFromAngle(float angle, float distance)
{
	return {map_coords::ToFixedGUtils(std::cos(angle) * distance), map_coords::ToFixedGUtils(std::sin(angle) * distance), 0.0f};
}

/// The point a distance from an object, on the villager's side of it, straight away from it
glm::vec2 FleeingPosition(const glm::vec3& villager, const glm::vec3& object, float distance)
{
	const glm::vec2 away = Across(villager) - Across(object);
	if (away == glm::vec2(0.0f))
	{
		return Across(villager);
	}
	return Across(villager) + away * (distance / glm::length(away));
}

/// The way a villager faces, as an angle across the land, from how walking has turned it
float FacingOf(const Transform& transform)
{
	const auto x = transform.rotation * glm::vec3(1.0f, 0.0f, 0.0f);
	return -std::atan2(-x.z, x.x) - glm::radians(90.0f);
}

void Face(Transform& transform, float angle)
{
	transform.rotation = glm::eulerAngleY(-angle - glm::radians(90.0f));
}

/// Turns the villager towards an object by a quarter of 1024 a turn; whether it faces it. Nothing to look at is looked at.
bool LookAtObject(entt::entity villager, entt::entity object)
{
	auto* transform = Entities().TryGet<Transform>(villager);
	if (transform == nullptr || object == entt::null || !Entities().Valid(object))
	{
		return true;
	}
	const float step = static_cast<float>(k_LookStep) * k_TwoPi / static_cast<float>(k_AngleUnits);
	const float target = AngleBetween(Across(transform->position), Across(PositionOf(object)));
	const float diff = std::remainder(target - FacingOf(*transform), k_TwoPi);
	if (std::abs(diff) < step)
	{
		Face(*transform, target);
		return true;
	}
	Face(*transform, FacingOf(*transform) + (diff > 0.0f ? step : -step));
	return false;
}

/// Whether the state's animation has played through as many times since the villager went into it
bool IsReadyForNewAnimation(const LivingAction& action, AnimId animation, uint32_t times)
{
	return villager_clips::ClipPlayed(action, animation, times);
}

bool IsFireFightingState(VillagerStates state)
{
	return state == VillagerStates::PutOutFireByBeating || state == VillagerStates::PutOutFireWithWater ||
	       state == VillagerStates::GetWaterToPutOutFire || state == VillagerStates::MoveAroundFire;
}

/// The villager goes back to what it remembered, as any change of state goes: it may pause a second first, and a state
/// that won't be gone into leaves it deciding what to do. Nothing is remembered afterwards. With nothing remembered it
/// decides what to do rather than standing still for good.
void PopFromPrevious(LivingAction& action)
{
	const auto previous = State(action, LivingAction::Index::Previous);
	SetTopState(action, villager_fire::StateToGoBackTo(static_cast<VillagerStates>(StateInfo(previous).resumeState)));
	SetPrevious(action, VillagerStates::InvalidState);
}

/// The villager lets go of the reaction it took up, so the living about a fire can take it up again in their time
void StopReacting(entt::entity villager)
{
	auto& state = FireStateOf(villager);
	state.reaction = 0;
	state.reactionTarget = entt::null;
	if (Locator::reactionSystem::has_value())
	{
		Locator::reactionSystem::value().StopReacting(villager);
	}
}

/// Whether a thing can still be reacted to: it exists, and a villager is not dying
bool IsAvailable(entt::entity thing)
{
	auto& registry = Entities();
	if (thing == entt::null || !registry.Valid(thing))
	{
		return false;
	}
	const auto* action = registry.TryGet<const LivingAction>(thing);
	return action == nullptr || !registry.AllOf<Villager>(thing) || FinalState(*action) != VillagerStates::Dying;
}

/// Whether the hand holds a thing. The hand doesn't pick up objects yet, so it never does
bool IsInHand(entt::entity /*thing*/)
{
	return false;
}

/// Whether a reaction going on ends once what it reacts to is in the hand, from its kind's table
bool ReactionFinishesInHand(uint32_t reaction)
{
	if (reaction == 0 || !Locator::reactionSystem::has_value())
	{
		return false;
	}
	const auto active = Locator::reactionSystem::value().Find(reaction);
	return active.has_value() && Locator::infoConstants::value()
	                                     .reaction.at(static_cast<size_t>(active->source.type))
	                                     .whetherReactionFinishesIfInitiatorInHand != 0;
}

/// How many turns the reaction to a fire has gone on, none when it has gone
std::optional<uint32_t> ReactionAge(uint32_t reaction)
{
	if (reaction == 0 || !Locator::reactionSystem::has_value())
	{
		return std::nullopt;
	}
	for (const auto& active : Locator::reactionSystem::value().GetReactions())
	{
		if (active.id == reaction)
		{
			return active.age;
		}
	}
	return std::nullopt;
}

const ReactionInfo& FireReaction()
{
	return Locator::infoConstants::value().reaction.at(static_cast<size_t>(Reaction::ReactToFire));
}

/// Where a villager stands to beat a fire: on the line from where the fire burns out through the villager, beyond the
/// object's radius or the fire's safe radius, whichever is wider, by its own radius and a little more at random
std::optional<glm::vec2> GetFireFightingPos(entt::entity villager, entt::entity fire)
{
	const auto reach = fire != entt::null ? Locator::fireSystem::value().GetReach(fire) : std::nullopt;
	if (!reach.has_value())
	{
		return std::nullopt;
	}
	const float angle = AngleBetween(Across(reach->centre), Across(PositionOf(villager)));
	float radius = reach->safeRadius < reach->defaultRadius ? reach->defaultRadius : reach->safeRadius;
	radius += RadiusOf(villager);
	radius += GameFloatRandom(1.0f);
	return CoordsToMetres(ToCoords(Across(reach->position)) + PosFromAngle(angle, radius));
}

/// Whether a villager stands within reach of beating a fire: beyond the edge it stands back by, by less than a margin
bool InRange(entt::entity villager, entt::entity fire, float margin)
{
	const auto reach = fire != entt::null ? Locator::fireSystem::value().GetReach(fire) : std::nullopt;
	if (!reach.has_value())
	{
		return false;
	}
	const float distance = Distance(PositionOf(villager), reach->position);
	const float radius =
	    (reach->safeRadius < reach->defaultRadius ? reach->defaultRadius : reach->safeRadius) + RadiusOf(villager);
	return distance > radius && distance < radius + margin;
}

/// Walks to a point to go into a state there. Like the game, it goes into the walk and the state it walks to at once, so
/// the state it leaves hears where it is heading, not that it walks: leaving a fire's reaction to walk on with it keeps
/// the reaction
void WalkTo(LivingAction& action, glm::vec2 goal, VillagerStates destination)
{
	villager_home::SetupMoveTo(action, goal, destination);
}

/// Goes round the fire it is about to a point, then on into a state
bool SetupMoveAroundFire(LivingAction& action, glm::vec2 point, VillagerStates then)
{
	SetTopState(action, VillagerStates::MoveAroundFire);
	if (State(action, LivingAction::Index::Top) != VillagerStates::MoveAroundFire)
	{
		return false;
	}
	FireStateOf(EntityOf(action)).walkTarget = point;
	SetPrevious(action, then);
	return true;
}

/// Picks the fire of the blaze nearest it to beat and sets off there; false when there is none or no place to stand. It
/// has decided once it has a place, whether or not it could set off
bool DecideHowToPutOutFire(LivingAction& action, entt::entity fire)
{
	const auto villager = EntityOf(action);
	auto& state = FireStateOf(villager);
	state.fire = Locator::fireSystem::value().NearestSafeFire(fire, PositionOf(villager)).value_or(entt::null);
	if (state.fire == entt::null)
	{
		return false;
	}
	const auto place = GetFireFightingPos(villager, state.fire);
	if (!place.has_value())
	{
		return false;
	}
	SetupMoveAroundFire(action, *place, VillagerStates::PutOutFireByBeating);
	return true;
}

/// The villager's walk's goal
glm::vec2& GoalOf(entt::entity villager)
{
	return Entities().Get<WallHug>(villager).goal;
}
} // namespace

uint8_t villager_fire::ReactToFirePriority(entt::entity villager, entt::entity object, uint32_t reaction)
{
	auto& registry = Entities();
	auto& fires = Locator::fireSystem::value();
	const auto reach = registry.Valid(object) ? fires.GetReach(object) : std::nullopt;
	const auto* action = registry.TryGet<const LivingAction>(villager);
	if (!reach.has_value() || action == nullptr || FinalState(*action) == VillagerStates::OnFire)
	{
		return 0;
	}
	const float distance = Distance(PositionOf(villager), reach->position);
	const auto& info = FireReaction();
	const float urgency = (reach->maxRadius != 0.0f ? reach->radius / reach->maxRadius : 0.0f) * 0.5f + 1.0f;
	const float priority = urgency * static_cast<float>(info.priority);
	constexpr float k_Most = 255.0f;
	const auto result = static_cast<uint8_t>(priority < k_Most ? map_coords::FtoL(priority) : 255);
	// The age of the reaction weighed, which a fire spreads as soon as it makes it
	const auto age = ReactionAge(reaction);
	if (age.has_value() && *age < k_NewReactionTurns && distance < info.minDistanceToRunAwayFromObject)
	{
		return result;
	}
	if (distance < reach->safeRadius)
	{
		return result;
	}
	const auto* state = registry.TryGet<const VillagerFireState>(villager);
	// A fireman takes another fire into its blaze only while reacting to nothing else
	const auto* current = registry.TryGet<const LivingReaction>(villager);
	const bool freeToMerge = current == nullptr || current->reaction == 0 || current->type == Reaction::ReactToFire;
	if (!freeToMerge || !IsFireFightingState(FinalState(*action)))
	{
		return result;
	}
	const auto mine = state != nullptr ? state->fire : entt::null;
	const auto myReach = mine != entt::null ? fires.GetReach(mine) : std::nullopt;
	if (!myReach.has_value())
	{
		return result;
	}
	if (fires.InSameBlaze(object, mine))
	{
		return 0;
	}
	const float merge = k_MergeReachBase + info.maxReactionDistance;
	if (Distance(myReach->position, reach->position) > 2.0f * merge)
	{
		return result;
	}
	// A fireman takes the fire into the blaze it fights instead
	fires.MergeBlazes(mine, object);
	return 0;
}

void villager_fire::SetupReactToFire(entt::entity villager, entt::entity object, uint32_t reaction)
{
	auto& registry = Entities();
	auto* action = registry.TryGet<LivingAction>(villager);
	if (action == nullptr || !registry.Valid(object))
	{
		return;
	}
	auto& state = FireStateOf(villager);
	state.reactionTarget = object;
	// The villager remembers what it was doing, unless it was reacting already
	if (state.reaction == 0)
	{
		villager_memory::StorePreviousState(*action);
	}
	villager_home::LeaveHome(villager);
	SetTopState(*action, VillagerStates::ReactToFire);
	// The reaction it takes up, which a fire spreads as soon as it makes it
	FireStateOf(villager).reaction = reaction;
}

uint32_t villager_fire::ReactToFire(LivingAction& action)
{
	auto& registry = Entities();
	const auto villager = EntityOf(action);
	auto& fires = Locator::fireSystem::value();
	auto& state = FireStateOf(villager);
	const auto object = state.reactionTarget;
	const auto reach = object != entt::null && registry.Valid(object) ? fires.GetReach(object) : std::nullopt;
	if (!reach.has_value())
	{
		return 0;
	}
	state.fire = object;
	if (!LookAtObject(villager, object))
	{
		return 1;
	}
	const auto here = PositionOf(villager);
	const float distance = Distance(here, reach->position);
	const auto& info = FireReaction();
	const auto age = ReactionAge(state.reaction);
	const bool recent = age.has_value() && *age < k_NewReactionTurns;
	if ((recent && distance < info.minDistanceToRunAwayFromObject) || distance < reach->safeRadius)
	{
		// Too near: back to the run-away distance while the fire is new, later just past its safe radius
		const float runTo = recent
		                        ? GameFloatRandom(info.maxDistanceToRunAwayFromObject - info.minDistanceToRunAwayFromObject) +
		                              info.minDistanceToRunAwayFromObject
		                        : GameFloatRandom(k_SafeMargin) + reach->safeRadius;
		WalkTo(action, FleeingPosition(here, reach->position, runTo), VillagerStates::ReactToFire);
		SetStateSpeed(villager, VillagerStates::ReactToFire);
		return 1;
	}
	if (IsFireFightingState(FinalState(action)))
	{
		// A fireman already: back to what it remembered, letting go of this reaction
		EndReaction(action);
		return 1;
	}
	// A villager of a town weighs whether the blaze needs it: by how urgent its fires are, how many more firemen fit round
	// it and how near home it is
	const auto* person = registry.TryGet<const Villager>(villager);
	if (person != nullptr && person->town != entt::null && registry.Valid(person->town))
	{
		const auto* town = registry.TryGet<const Transform>(person->town);
		const auto blaze = fires.GetBlaze(object);
		if (town != nullptr && blaze.has_value())
		{
			const float townNearness = gutils::GetDistanceModifier(Distance(town->position, here), k_TownReach);
			const float need = blaze->burningRadius * k_TwoPi / (RadiusOf(villager) * k_FiremanSpacing);
			const float share = need != 0.0f ? (need - static_cast<float>(blaze->firemen)) / need : 0.0f;
			const float score = blaze->burningPriority * share * townNearness;
			if (score > k_FightThreshold && DecideHowToPutOutFire(action, object))
			{
				return 1;
			}
		}
	}
	// Otherwise it goes round the fire on its way
	SetupMoveAroundFire(action, GoalOf(villager), State(action, LivingAction::Index::Previous));
	return 1;
}

uint32_t villager_fire::PutOutFireByBeating(LivingAction& action)
{
	const auto villager = EntityOf(action);
	auto& fires = Locator::fireSystem::value();
	auto& state = FireStateOf(villager);
	if (!InRange(villager, state.fire, k_BeatingRange))
	{
		if (const auto place = GetFireFightingPos(villager, state.fire))
		{
			SetupMoveAroundFire(action, *place, VillagerStates::PutOutFireByBeating);
			return 1;
		}
		SetTopState(action, VillagerStates::DecideWhatToDo);
		return 1;
	}
	if (!LookAtObject(villager, state.fire))
	{
		return 1;
	}
	if (!IsReadyForNewAnimation(action, AnimId::PPutOutFire, 1))
	{
		return 1;
	}
	const auto reach = fires.GetReach(state.fire);
	if (!reach.has_value() || !reach->aboveReactionTemperature)
	{
		SetTopState(action, VillagerStates::DecideWhatToDo);
		return 1;
	}
	// Once its arms have swung, it beats the fire every turn: the turns in the state are never counted afresh
	fires.ApplyBurn(state.fire, k_BeatBurn, std::nullopt);
	return 1;
}

uint32_t villager_fire::MoveAroundFire(LivingAction& action)
{
	const auto villager = EntityOf(action);
	auto& fires = Locator::fireSystem::value();
	auto& state = FireStateOf(villager);
	auto target = ToCoords(state.walkTarget);
	const auto here = PositionOf(villager);
	// Within a step of where it was going, at the speed it walks: on with what it was going to do there
	if (villager_home::AreWeThere(villager, state.walkTarget))
	{
		PopFromPrevious(action);
		SetPrevious(action, VillagerStates::DecideWhatToDo);
		return 1;
	}
	const auto reach = state.fire != entt::null ? fires.GetReach(state.fire) : std::nullopt;
	if (!reach.has_value())
	{
		return 0;
	}
	const auto from = ToCoords(Across(here));
	const float margin = RadiusOf(villager) + k_SkirtMargin;
	bool changed = false;
	float side = 0.0f;
	auto via = fire::GetViaPoint(from, target, ToCoords(Across(reach->position)), reach->safeRadius, margin, 0.0f);
	if (via.angle != 0.0f)
	{
		side = via.angle;
		target = via.point;
		changed = true;
	}
	if (via.detour && via.inside)
	{
		// Where it was going lies in the fire: it stops there and decides again
		SetPrevious(action, VillagerStates::DecideWhatToDo);
		state.walkTarget = CoordsToMetres(target);
		GoalOf(villager) = CoordsToMetres(target);
		return 1;
	}
	const auto members = fires.GetBlazeMembers(state.fire);
	int detours = 0;
	for (size_t i = 0; i < members.size();)
	{
		const auto memberReach = fires.GetReach(members[i]);
		if (!memberReach.has_value())
		{
			++i;
			continue;
		}
		via = fire::GetViaPoint(from, target, ToCoords(Across(memberReach->position)), memberReach->safeRadius, margin, side);
		if (via.angle != 0.0f || (via.detour && via.inside))
		{
			side = via.angle;
			target = via.point;
			changed = true;
			// Every fire is looked at again on the way to the new point
			i = 0;
			if (++detours >= k_MostDetours)
			{
				return 0;
			}
		}
		else
		{
			++i;
		}
	}
	if (!changed)
	{
		target = ToCoords(state.walkTarget);
	}
	WalkTo(action, CoordsToMetres(target), VillagerStates::MoveAroundFire);
	// The full speed of its state, wounds, town and its own way of going included
	ecs::systems::villager_reactions::SetStateSpeed(villager, State(action, LivingAction::Index::Previous));
	return 1;
}

void villager_fire::SetupOnFire(entt::entity villager, entt::entity fire)
{
	auto& registry = Entities();
	auto* action = registry.TryGet<LivingAction>(villager);
	// Not one that is held, dying or dead (the hand doesn't hold villagers yet, and nothing flies freely)
	if (action == nullptr || !Locator::livingActionSystem::has_value() || registry.AllOf<VillagerDeath>(villager) ||
	    FinalState(*action) == VillagerStates::Dying)
	{
		return;
	}
	villager_memory::StorePreviousState(*action);
	auto& state = FireStateOf(villager);
	state.walkTarget = GoalOf(villager);
	villager_home::LeaveHome(villager);
	SetTopState(*action, VillagerStates::DecideWhatToDo);
	FireStateOf(villager).fire = fire;
	SetTopState(*action, VillagerStates::OnFire);
}

namespace
{
/// The villager is out of the fire's reach: it walks on to where it was going and goes back to what it was doing
void FinishBeingOnFire(LivingAction& action)
{
	const auto villager = EntityOf(action);
	GoalOf(villager) = FireStateOf(villager).walkTarget;
	PopFromPrevious(action);
}
} // namespace

uint32_t villager_fire::OnFire(LivingAction& action)
{
	auto& registry = Entities();
	const auto villager = EntityOf(action);
	auto& fires = Locator::fireSystem::value();
	if (!registry.AllOf<Fire>(villager))
	{
		// Its own fire has gone out
		FinishBeingOnFire(action);
		return 1;
	}
	const auto here = PositionOf(villager);
	const bool burning = fires.IsOnFire(villager);
	const auto& state = FireStateOf(villager);
	glm::vec2 goal;
	if (state.fire != entt::null && state.fire != villager)
	{
		const auto external = fires.GetReach(state.fire);
		if (!external.has_value())
		{
			return 0;
		}
		float distance = 0.0f;
		if (burning)
		{
			distance = GameFloatRandom(k_RunFromFire) + external->maxRadius;
		}
		else
		{
			if (external->maxRadius < Distance(here, external->position))
			{
				FinishBeingOnFire(action);
				return 1;
			}
			distance = GameFloatRandom(external->maxRadius - external->radius + 1.0f) + external->radius;
		}
		goal = FleeingPosition(here, external->position, distance);
	}
	else
	{
		if (!burning)
		{
			FinishBeingOnFire(action);
			return 1;
		}
		const float angle = GameFloatRandom(k_TwoPi);
		const float distance = GameFloatRandom(k_OnFireRunMore) + k_OnFireRun;
		goal = CoordsToMetres(ToCoords(Across(here)) + PosFromAngle(angle, distance));
	}
	WalkTo(action, goal, VillagerStates::OnFire);
	SetStateSpeed(villager, VillagerStates::OnFire);
	if (State(action, LivingAction::Index::Previous) == VillagerStates::InvalidState)
	{
		SetPrevious(action, VillagerStates::DecideWhatToDo);
	}
	return 1;
}

uint32_t villager_fire::PutOutFireWithWater(LivingAction& action)
{
	SetTopState(action, VillagerStates::DecideWhatToDo);
	return 1;
}

bool villager_fire::EnterPutOutFire(LivingAction& action, VillagerStates from, VillagerStates /*to*/)
{
	// Between the fire-fighting states it stays as it is
	if (IsFireFightingState(from))
	{
		return true;
	}
	const auto villager = EntityOf(action);
	auto& state = FireStateOf(villager);
	auto& fires = Locator::fireSystem::value();
	const bool valid = state.fire != entt::null && Entities().Valid(state.fire) && Entities().AllOf<Fire>(state.fire);
	// Refused, the villager decides what to do instead (the change of state sees to that)
	if (!valid || state.reaction == 0 || !ReactionAge(state.reaction).has_value())
	{
		if (!valid)
		{
			state.fire = entt::null;
		}
		if (StateInfo(from).isReactionState != 0)
		{
			StopReacting(villager);
		}
		return false;
	}
	// Already one of the blaze's firemen, it is refused but stays listed, keeping its reaction
	if (fires.IsFiremanOf(state.fire, villager))
	{
		return false;
	}
	fires.AddFireman(state.fire, villager);
	return true;
}

bool villager_fire::ExitPutOutFire(LivingAction& action, VillagerStates next)
{
	// It stays one of the blaze's firemen going on to another fire-fighting state, or to a state that isn't an end in
	// itself (walking somewhere on the way)
	if (IsFireFightingState(next) || StateInfo(next).isFinalState == 0)
	{
		ExitReaction(action, next);
		return false;
	}
	const auto villager = EntityOf(action);
	auto& state = FireStateOf(villager);
	if (state.fire != entt::null)
	{
		auto& fires = Locator::fireSystem::value();
		if (!fires.IsFiremanOf(state.fire, villager))
		{
			state.fire = entt::null;
			return false;
		}
		fires.RemoveFireman(state.fire, villager);
	}
	state.fire = entt::null;
	ExitReaction(action, next);
	return false;
}

bool villager_fire::EnterOnFire(LivingAction& action, VillagerStates /*from*/, VillagerStates /*to*/)
{
	const auto villager = EntityOf(action);
	const auto fire = FireStateOf(villager).fire;
	auto& fires = Locator::fireSystem::value();
	if (fire != entt::null && Entities().Valid(fire) && Entities().AllOf<Fire>(fire))
	{
		// It is listed with the firemen of the blaze it runs from
		if (fires.IsFiremanOf(fire, villager))
		{
			return false;
		}
		fires.AddFireman(fire, villager);
	}
	return true;
}

bool villager_fire::ExitOnFire(LivingAction& action, VillagerStates /*next*/)
{
	const auto villager = EntityOf(action);
	auto& state = FireStateOf(villager);
	if (state.fire != entt::null)
	{
		auto& fires = Locator::fireSystem::value();
		if (fires.IsFiremanOf(state.fire, villager))
		{
			fires.RemoveFireman(state.fire, villager);
		}
		state.fire = entt::null;
	}
	return false;
}

bool villager_fire::ExitReaction(LivingAction& action, VillagerStates next)
{
	if (StateInfo(next).isReactionState == 0)
	{
		StopReacting(EntityOf(action));
	}
	return false;
}

bool villager_fire::ReactionValidate(LivingAction& action)
{
	const auto villager = EntityOf(action);
	const auto& state = FireStateOf(villager);
	const auto target = state.reactionTarget;
	// The reaction ends when what it reacts to has gone or is no longer available, or, for a reaction that ends then,
	// when the hand has picked it up
	if (!IsAvailable(target) || (ReactionFinishesInHand(state.reaction) && IsInHand(target)))
	{
		PopFromPrevious(action);
	}
	return true;
}

bool villager_fire::IsRunningOnFire(entt::entity villager)
{
	const auto* action = Entities().TryGet<const LivingAction>(villager);
	return action != nullptr && Locator::livingActionSystem::has_value() && FinalState(*action) == VillagerStates::OnFire;
}

bool villager_fire::CanCatchFire(entt::entity villager)
{
	auto& registry = Entities();
	const auto* action = registry.TryGet<const LivingAction>(villager);
	if (action == nullptr || !Locator::livingActionSystem::has_value())
	{
		return true;
	}
	// Dying, inside its home, or gone to hide in a building nearby. (One held in the hand neither, but the hand doesn't
	// hold villagers yet.)
	return FinalState(*action) != VillagerStates::Dying && !registry.AllOf<AtHome>(villager) &&
	       State(*action, LivingAction::Index::Top) != VillagerStates::GoAndHideInNearbyBuilding;
}

bool villager_fire::IsFireMan(entt::entity villager)
{
	const auto* action = Entities().TryGet<const LivingAction>(villager);
	if (action == nullptr || !Locator::livingActionSystem::has_value())
	{
		return false;
	}
	const auto final = FinalState(*action);
	return IsFireFightingState(final) || final == VillagerStates::ReactToFire;
}

void villager_fire::StopFireFighting(entt::entity villager)
{
	auto& registry = Entities();
	auto* action = registry.TryGet<LivingAction>(villager);
	auto* state = registry.TryGet<VillagerFireState>(villager);
	if (action == nullptr || state == nullptr || state->fire == entt::null)
	{
		return;
	}
	auto& fires = Locator::fireSystem::value();
	if (FinalState(*action) != VillagerStates::MoveAroundFire)
	{
		fires.RemoveFireman(state->fire, villager);
		state->fire = entt::null;
		SetTopState(*action, VillagerStates::DecideWhatToDo);
		return;
	}
	auto next =
	    StateToGoBackTo(static_cast<VillagerStates>(StateInfo(State(*action, LivingAction::Index::Previous)).resumeState));
	if (IsFireFightingState(next))
	{
		next = VillagerStates::DecideWhatToDo;
	}
	fires.RemoveFireman(state->fire, villager);
	state->fire = entt::null;
	if (State(*action, LivingAction::Index::Top) == VillagerStates::MoveAroundFire)
	{
		SetTopState(*action, next);
	}
	else
	{
		// Walking there: both the walk and the fire state it was heading for are left, so it lets go of the reaction
		const auto top = State(*action, LivingAction::Index::Top);
		Living().VillagerCallExitState(*action, LivingAction::Index::Top, next);
		if (FinalState(*action) != top)
		{
			Living().VillagerCallExitState(*action, LivingAction::Index::Final, next);
		}
		Living().VillagerSetState(*action, LivingAction::Index::Final, next, true);
		SetPrevious(*action, VillagerStates::InvalidState);
	}
	if (IsFireFightingState(State(*action, LivingAction::Index::Top)) &&
	    State(*action, LivingAction::Index::Previous) == VillagerStates::InvalidState)
	{
		SetPrevious(*action, VillagerStates::DecideWhatToDo);
	}
}

void villager_fire::EndReaction(LivingAction& action)
{
	const auto villager = EntityOf(action);
	PopFromPrevious(action);
	if (StateInfo(FinalState(action)).isReactionState != 0)
	{
		SetTopState(action, VillagerStates::DecideWhatToDo);
	}
	if (FireStateOf(villager).reaction != 0)
	{
		StopReacting(villager);
	}
}

entt::entity villager_fire::FireOf(entt::entity villager)
{
	const auto* state = Entities().TryGet<const VillagerFireState>(villager);
	return state != nullptr ? state->fire : entt::null;
}

using villager_clips::IsOnWater;

namespace
{
/// The town and the players remember a death and who it is put down to: the town by killer and cause, the victim's
/// player one more of its people lost, the killer one more killed, and one more sacrificed for a sacrifice
void RememberDeath(entt::entity villager, const villager_fire::DeathCause& cause)
{
	auto& registry = Entities();
	const auto killer = cause.killer.value_or(PlayerNames::NEUTRAL);
	const auto& person = registry.Get<const Villager>(villager);
	auto* town = registry.Valid(person.town) ? registry.TryGet<Town>(person.town) : nullptr;
	if (town != nullptr)
	{
		const auto killerIndex = static_cast<size_t>(killer);
		const auto reasonIndex = static_cast<size_t>(cause.reason);
		if (killerIndex < town->deathsByKiller.size() && reasonIndex < town->deathsByKiller.at(killerIndex).size())
		{
			town->deathsByKiller.at(killerIndex).at(reasonIndex) += cause.weight;
		}
		const auto owner = town->owner;
		registry.Each<Player>([owner, killer](entt::entity, Player& player) {
			if (player.name == owner)
			{
				++player.villagersLost;
			}
			if (player.name == killer)
			{
				++player.villagersKilled;
			}
		});
	}
	if (cause.reason == DeathReason::Sacrifice)
	{
		registry.Each<Player>([killer](entt::entity, Player& player) {
			if (player.name == killer)
			{
				++player.sacrifices;
			}
		});
	}
}
} // namespace

void villager_fire::DieByEffect(entt::entity villager, std::optional<DeathCause> cause)
{
	auto& registry = Entities();
	auto* action = registry.TryGet<LivingAction>(villager);
	// One killed in the air dies only once it has landed
	if (action == nullptr || registry.AnyOf<VillagerDeath, InPhysics>(villager))
	{
		return;
	}
	// The player here hears of a death with a cause
	if (cause.has_value())
	{
		villager_after_death::Hear(villager, cause->reason, cause->killer);
	}
	// The wood it shows falls where it dies as a log; the rest of what it carries, food and wood, is lost
	if (Locator::dynamicsSystem::has_value())
	{
		villager_dropped_log::Drop(Locator::dynamicsSystem::value(), villager, {});
	}
	if (auto* person = registry.TryGet<Villager>(villager))
	{
		const auto wood = villager_carry::PutDown(*person, ResourceType::Wood, 0);
		const auto food = villager_carry::PutDown(*person, ResourceType::Food, 0);
		if (auto* tally = registry.Valid(person->town) ? registry.TryGet<TownResourceTally>(person->town) : nullptr;
		    tally != nullptr)
		{
			tally->woodCarried -= static_cast<float>(wood);
			tally->foodCarried -= static_cast<float>(food);
		}
	}
	// A disciple's work ends with it
	villager_disciple::SetType(villager, VillagerDisciple::None);
	const auto town = registry.Get<const Villager>(villager).town;
	if (cause.has_value())
	{
		// Its player's alignment moves for the death, and its town counts it
		villager_after_death::MoveOwnersAlignment(villager, cause->reason);
		RememberDeath(villager, *cause);
		villager_after_death::CountInTown(town, cause->killer, cause->reason);
	}
	// Its town's graveyard counts it among its burials
	graveyards::TownPersonDied(town);
	// It dies: no life left, falling, then lying dead as long as its town has no graveyard to take it to
	if (auto* person = registry.TryGet<Villager>(villager))
	{
		world_objects::CountInjury(villager, world_objects::LifeOf(villager), 0.0f);
		person->life = 0.0f;
	}
	if (auto* life = registry.TryGet<ObjectLife>(villager))
	{
		life->life = 0.0f;
	}
	// Its body lies less long when the town it belonged to as it died had a working graveyard
	const auto* info = InfoOf(villager);
	const auto corpseTurns = info != nullptr
	                             ? graveyard::CorpseTurns(graveyards::HasWorkingGraveyard(town), info->dyingTimeWithGraveyard,
	                                                      info->dyingTimeWithoutGraveyard)
	                             : 0u;
	registry.Assign<VillagerDeath>(villager, VillagerDeath {
	                                             .turnsLeft = corpseTurns,
	                                             .reason = cause.has_value() ? std::optional(cause->reason) : std::nullopt,
	                                             .killer = cause.has_value() ? cause->killer : std::nullopt,
	                                             .onBack = cause.has_value() && cause->onBack,
	                                             // Nobody is told of a death at home
	                                             .noticed = registry.AllOf<AtHome>(villager),
	                                         });
	villager_home::LeaveHome(villager);
	// It falls where it is: whatever walk it was on ends
	walker_placement::Stop(registry, villager);
	SetTopState(*action, VillagerStates::Dying);
	// Dying, it leaves its town's worship site, its home, ending the couple it was one of, and its town
	villager_after_death::LeaveWorship(villager, town);
	if (Locator::townSystem::has_value())
	{
		Locator::townSystem::value().RemoveDeadVillager(villager);
	}
}

uint32_t villager_fire::Dying(LivingAction& action)
{
	const auto villager = EntityOf(action);
	if (!Entities().AllOf<VillagerDeath>(villager))
	{
		return 0;
	}
	// As it begins to fall the people about it are told, unless it died at home
	auto& death = Entities().Get<VillagerDeath>(villager);
	if (!death.noticed)
	{
		death.noticed = true;
		villager_after_death::TellThoseAbout(villager);
	}
	// It falls, into the water on water, on its back when a fall left it so, and lies dead once the fall has played
	const auto fall = ecs::villager_animation::DyingClip(IsOnWater(PositionOf(villager)), death.onBack);
	if (IsReadyForNewAnimation(action, fall, 1))
	{
		SetTopState(action, VillagerStates::Dead);
	}
	return 1;
}

bool villager_fire::ExitDead(LivingAction&, VillagerStates next)
{
	return !ecs::villager_check_rules::BodyMayLeaveDeath(next);
}

uint32_t villager_fire::Dead(LivingAction& action)
{
	auto& registry = Entities();
	const auto villager = EntityOf(action);
	auto* death = registry.TryGet<VillagerDeath>(villager);
	if (death == nullptr)
	{
		// Eaten or downed: it lies still
		return 0;
	}
	// A burning body stops burning, and the flesh goes at once, leaving the skeleton
	if (registry.AllOf<Fire>(villager) && Locator::fireSystem::has_value())
	{
		Locator::fireSystem::value().Forget(villager);
	}
	// A body a script controls is left to the script: it keeps its flesh and lies on until the script lets it go
	const bool scripted = registry.AllOf<ScriptControlled>(villager);
	if (!scripted)
	{
		// Every body, man, woman or child, becomes the man's skeleton once its flesh goes. One already wearing the
		// skeleton (a skeleton villager that died, or a body a script made bones) has nothing left to lose.
		const auto skeletonModel = resources::HashIdentifier(MeshId::PersonSkeletonMale);
		auto* mesh = registry.TryGet<Mesh>(villager);
		if (mesh != nullptr && mesh->id != skeletonModel)
		{
			// The flesh goes in a puff of smoke, and the soul rises
			villager_after_death::SmokePuff(villager);
			villager_after_death::SoulRises(villager);
			mesh->id = skeletonModel;
			if (auto* detail = registry.TryGet<DetailMeshes>(villager))
			{
				detail->meshes = {skeletonModel, skeletonModel, skeletonModel};
			}
			registry.SetDirty();
		}
		death->skeleton = true;
		// It lies there its time, and one turn more, and goes
		if (death->turnsLeft-- == 0)
		{
			// It goes in a puff of smoke
			villager_after_death::SmokePuff(villager);
			world_objects::Remove(villager);
			return 5;
		}
	}
	return 1;
}
