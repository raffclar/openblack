/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "VillagerBreeding.h"

#include <array>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <glm/geometric.hpp>
#include <glm/gtx/vec_swizzle.hpp>

#include "3D/MapCoords.h"
#include "Audio/AnimEffectKeys.h"
#include "Audio/AudioManagerInterface.h"
#include "Audio/GameSoundEffects.h"
#include "Audio/Sound.h"
#include "Common/GameRandom.h"
#include "ECS/Archetypes/VillagerArchetype.h"
#include "ECS/BuildingSites.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/AtHome.h"
#include "ECS/Components/Breeding.h"
#include "ECS/Components/CarriedByTornado.h"
#include "ECS/Components/HandGrab.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/Poisoned.h"
#include "ECS/Components/ScriptControl.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownDesire.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/VillagerDeath.h"
#include "ECS/Components/WallHug.h"
#include "ECS/CrecheRules.h"
#include "ECS/Creches.h"
#include "ECS/Map.h"
#include "ECS/Registry.h"
#include "ECS/Statistics.h"
#include "ECS/Systems/LivingActionSystemInterface.h"
#include "ECS/Systems/ReactionSystemInterface.h"
#include "ECS/Systems/SkySystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/Systems/TownDesireSystemInterface.h"
#include "ECS/Systems/TownSystemInterface.h"
#include "ECS/VillagerAge.h"
#include "ECS/VillagerBreedingRules.h"
#include "ECS/VillagerSkeletons.h"
#include "ECS/WallHugRules.h"
#include "ECS/WorldObjects.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "VillagerAnimate.h"
#include "VillagerFood.h"
#include "VillagerHome.h"
#include "VillagerReactions.h"

using namespace openblack;
using namespace openblack::ecs::components;
namespace villager_breeding = openblack::ecs::villager_breeding;
namespace villager_home = openblack::ecs::villager_home;
namespace villager_age = openblack::ecs::villager_age;
namespace villager_food = openblack::ecs::villager_food;
namespace world_objects = openblack::ecs::world_objects;
namespace creche = openblack::creche;
namespace creches = openblack::creches;

namespace
{

constexpr std::array<audio::SoundId, villager_breeding::k_BabyCries> k_BabyCrySounds = {
    audio::SoundId::G_BabyCry_01, audio::SoundId::G_BabyCry_02, audio::SoundId::G_BabyCry_03, audio::SoundId::G_BabyCry_04,
    audio::SoundId::G_BabyCry_05, audio::SoundId::G_BabyCry_06, audio::SoundId::G_BabyCry_07, audio::SoundId::G_BabyCry_08,
    audio::SoundId::G_BabyCry_09, audio::SoundId::G_BabyCry_10,
};

auto& Entities()
{
	return Locator::entitiesRegistry::value();
}

entt::entity EntityOf(LivingAction& action)
{
	return Entities().ToEntity(action);
}

const auto& Living()
{
	return Locator::livingActionSystem::value();
}

void SetTopState(LivingAction& action, VillagerStates state)
{
	Living().VillagerSetState(action, LivingAction::Index::Top, state, false);
}

VillagerStates TopState(const LivingAction& action)
{
	return Living().VillagerGetState(action, LivingAction::Index::Top);
}

auto GameFloat()
{
	return [](float limit) { return Locator::gameRandom::value().GameFloatRand(limit); };
}

auto GameInt()
{
	return [](uint32_t limit) { return Locator::gameRandom::value().GameRand(limit); };
}

const GVillagerInfo& InfoOf(const Villager& villager)
{
	const auto& infos = Locator::infoConstants::value().villager;
	return infos.at(static_cast<size_t>(GVillagerInfo::Find(villager.tribe, villager.number)));
}

bool IsVillager(entt::entity entity)
{
	return entity != entt::null && Entities().Valid(entity) && Entities().AllOf<Villager, LivingAction>(entity);
}

/// Whether a villager can be called on: about, not held or flying or carried off, and neither dead nor dying
bool IsAvailable(entt::entity villager)
{
	if (!IsVillager(villager) || Entities().AnyOf<InHand, InPhysics, CarriedByTornado, VillagerDeath>(villager))
	{
		return false;
	}
	return Living().VillagerGetFinalState(Entities().Get<const LivingAction>(villager)) != VillagerStates::Dying;
}

bool IsFemale(entt::entity villager)
{
	return Entities().Get<const Villager>(villager).sex == Villager::Sex::FEMALE;
}

bool IsChild(entt::entity villager)
{
	return Entities().Get<const Villager>(villager).lifeStage == Villager::LifeStage::Child;
}

bool IsAtHome(entt::entity villager)
{
	return Entities().AllOf<AtHome>(villager);
}

glm::vec2 PositionOf(entt::entity entity)
{
	return glm::xz(Entities().Get<const Transform>(entity).position);
}

entt::entity TownOf(entt::entity villager)
{
	const auto town = Entities().Get<const Villager>(villager).town;
	return town != entt::null && Entities().Valid(town) && Entities().AllOf<Town>(town) ? town : entt::null;
}

entt::entity AbodeOf(entt::entity villager)
{
	const auto abode = Entities().Get<const Villager>(villager).abode;
	return abode != entt::null && Entities().Valid(abode) && Entities().AllOf<Abode>(abode) ? abode : entt::null;
}

entt::entity LoverOf(entt::entity villager)
{
	const auto* lover = Entities().TryGet<const Lover>(villager);
	return lover != nullptr && IsVillager(lover->partner) ? lover->partner : entt::null;
}

void SetLover(entt::entity villager, entt::entity partner)
{
	if (partner == entt::null)
	{
		Entities().Remove<Lover>(villager);
	}
	else
	{
		Entities().AssignOrReplace<Lover>(villager, partner);
	}
}

void Face(entt::entity villager, entt::entity other)
{
	Living().VillagerFace(villager, PositionOf(other));
}

/// Whether two villagers stand within a gap of each other, edge to edge
bool AreTouching(entt::entity a, entt::entity b, float gap)
{
	return villager_breeding::Touching(glm::distance(PositionOf(a), PositionOf(b)), world_objects::SizeOf(a).radius,
	                                   world_objects::SizeOf(b).radius, gap);
}

/// The villager falls pregnant, for her kind's time, and makes for home unless there already
void GetsPregnant(entt::entity villager)
{
	auto& person = Entities().Get<Villager>(villager);
	person.pregnancy = static_cast<int16_t>(InfoOf(person).timePregnantFor);
	Entities().SetDirty();
	if (!IsAtHome(villager))
	{
		villager_home::GoHome(Entities().Get<LivingAction>(villager));
	}
}

/// Both villagers turn to each other and make love for their time together
bool StartLoving(entt::entity villager, entt::entity mate, VillagerStates state)
{
	if (!IsAvailable(mate))
	{
		return false;
	}
	const auto turns = villager_breeding::LoveTurns(InfoOf(Entities().Get<const Villager>(mate)).havingSexTime,
	                                                InfoOf(Entities().Get<const Villager>(villager)).havingSexTime);
	auto& mateAction = Entities().Get<LivingAction>(mate);
	SetTopState(mateAction, state);
	SetLover(mate, villager);
	mateAction.turnsUntilStateChange = turns;
	auto& action = Entities().Get<LivingAction>(villager);
	SetTopState(action, state);
	SetLover(villager, mate);
	action.turnsUntilStateChange = turns;
	return true;
}

/// Two villagers meet: touching already, they face each other and the first goes on into the state; else both walk to
/// stand a gap apart and go into it there. Whether they were touching.
bool MeetUp(entt::entity villager, entt::entity other, VillagerStates state, float gap)
{
	auto& action = Entities().Get<LivingAction>(villager);
	if (AreTouching(villager, other, gap))
	{
		Face(villager, other);
		Face(other, villager);
		SetTopState(action, state);
		return true;
	}
	const auto here = PositionOf(villager);
	const auto there = PositionOf(other);
	villager_home::SetupMoveTo(action, villager_breeding::MeetingPoint(here, there, gap), state);
	villager_home::SetupMoveTo(Entities().Get<LivingAction>(other), villager_breeding::MeetingPoint(there, here, gap), state);
	return false;
}

/// Whether a child's mother is about: alive and able, a woman of its own tribe
bool IsMotherAlive(entt::entity child)
{
	const auto mother = Entities().Get<const Villager>(child).mother;
	if (!IsAvailable(mother))
	{
		return false;
	}
	const auto& her = Entities().Get<const Villager>(mother);
	return her.tribe == Entities().Get<const Villager>(child).tribe && her.sex == Villager::Sex::FEMALE;
}

/// A child goes home once it has grown up or when hungry, and lets go of a mother no longer about. 1 when it went home.
uint32_t CheckChild(LivingAction& action)
{
	const auto child = EntityOf(action);
	if (!IsChild(child))
	{
		return villager_home::GoHome(action);
	}
	if (!IsMotherAlive(child))
	{
		Entities().Get<Villager>(child).mother = entt::null;
	}
	if (villager_food::IsHungry(child))
	{
		return villager_home::GoHome(action);
	}
	return 0;
}

/// The sound bank and keys of the children at play at a crèche
constexpr std::string_view k_CrecheSoundBank = "editor.sad";
constexpr audio::AnimEffectKeys k_CrecheSound {.object = audio::SoundObject::Abode, .action = audio::SoundAction::PlayfulGrunt};
/// How close a homeless child must be to the crèche, edge to edge, to stroll on there at night
constexpr float k_CrecheTouch = 0.001f;

/// A child goes to its town's crèche when the town has one that works, walking to its door
bool ChildGotoCreche(LivingAction& action)
{
	const auto creche = creches::CrecheOf(TownOf(EntityOf(action)));
	if (creche == entt::null || !villager_home::IsFunctional(creche))
	{
		return false;
	}
	// TODO(villagers): along the town's footpaths where there are any; openblack has no footpath walking yet
	villager_home::SetupMoveTo(action, villager_home::ArrivePosition(creche), VillagerStates::ChildAtCreche);
	return true;
}

/// A child at the crèche goes on to the next point of its stroll along the crèche's paths, from its door when it
/// stands there
void StrollOn(LivingAction& action, entt::entity creche)
{
	const auto child = EntityOf(action);
	auto& person = Entities().Get<Villager>(child);
	const auto door = villager_home::ArrivePosition(creche);
	const auto* walk = Entities().TryGet<const WallHug>(child);
	const auto here = walk != nullptr ? walk->position : ecs::wall_hug::ToWhole(PositionOf(child));
	const auto leg = creche::NextStrollLeg({.path = person.crechePath, .leg = person.crecheLeg}, creches::PathsOf(creche),
	                                       here == ecs::wall_hug::ToWhole(door), GameInt());
	person.crechePath = static_cast<int16_t>(leg.stroll.path);
	person.crecheLeg = static_cast<int16_t>(leg.stroll.leg);
	glm::vec2 goal = door;
	if (leg.point.has_value())
	{
		goal = creche::Strayed(creches::PathPoint(creche, *leg.point).value_or(door), GameFloat());
	}
	villager_home::SetupMoveTo(action, goal, VillagerStates::ChildAtCreche);
}

/// The sound of children at play at the crèche
void PlayCrecheSound(entt::entity creche)
{
	if (Locator::audio::has_value())
	{
		Locator::audio::value().PlayAnimEffect(std::string(k_CrecheSoundBank), k_CrecheSound.ToArray(), creche,
		                                       Entities().Get<const Transform>(creche).position);
	}
}

/// Whether the first of a home's people is in it
bool FirstOfHomeIsIn(entt::entity abode)
{
	const auto& people = Entities().Get<const Abode>(abode).inhabitants;
	return !people.empty() && IsVillager(people.front()) && IsAtHome(people.front());
}

/// Whether a child, by its checks, has something to do other than follow its mother
bool ChildHasOtherThingsToDo(LivingAction& action)
{
	return CheckChild(action) != 0 || villager_home::OfferToTownDesires(action) == 1 || ChildGotoCreche(action);
}

std::optional<VillagerInfo> InfoRowFor(Tribe tribe, VillagerNumber number)
{
	try
	{
		return GVillagerInfo::Find(tribe, number);
	}
	catch (const std::runtime_error&)
	{
		return std::nullopt;
	}
}

/// A child is born to its mother: a boy of a job of her tribe, else a girl of her kind, at her side, a year old, into her
/// home (or her town without one), who then decides what to do. None when the child couldn't be made.
entt::entity ChildBorn(entt::entity mother)
{
	auto& registry = Entities();
	const auto& her = registry.Get<const Villager>(mother);
	const auto motherRow = GVillagerInfo::Find(her.tribe, her.number);
	auto row = motherRow;
	if (const auto job = villager_breeding::NewbornJob(InfoOf(her).boyGirlChance, GameInt()); job.has_value())
	{
		row = InfoRowFor(her.tribe, *job).value_or(motherRow);
	}
	const auto at = registry.Get<const Transform>(mother).position;
	const auto child = ecs::archetypes::VillagerArchetype::Create(at, at, row, villager_breeding::k_NewbornAge,
	                                                              ecs::archetypes::VillagerArchetype::Joins::Nobody);
	if (child == entt::null)
	{
		return entt::null;
	}
	if (villager_skeleton::Is(mother))
	{
		villager_skeleton::Set(child, true);
	}
	// TODO(villager-reproduction): in a multiplayer game a player counting births in its town as a win condition counts
	// this one
	auto& towns = Locator::townSystem::value();
	if (const auto abode = AbodeOf(mother); abode != entt::null)
	{
		towns.AddVillagerToAbode(abode, child);
	}
	else if (const auto town = TownOf(mother); town != entt::null)
	{
		towns.AddVillagerToTown(town, child);
	}
	registry.Get<Villager>(child).mother = mother;
	if (const auto owner = world_objects::PlayerOf(mother); owner.has_value())
	{
		player_statistics::CountBirth(registry, *owner);
	}
	if (registry.AllOf<Poisoned>(mother))
	{
		registry.AssignOrReplace<Poisoned>(child);
	}
	registry.SetDirty();
	villager_breeding::ChildDecideWhatToDo(registry.Get<LivingAction>(child));
	return child;
}

/// The nearest building of a town strictly within reach of a point
entt::entity NearestHome(entt::entity town, glm::vec2 point, float reach)
{
	auto& registry = Entities();
	entt::entity nearest = entt::null;
	float best = reach;
	for (const auto abode : registry.Get<const Town>(town).abodes)
	{
		if (!registry.Valid(abode) || !registry.AllOf<Abode, Transform>(abode))
		{
			continue;
		}
		const float distance = glm::distance(point, PositionOf(abode));
		if (distance < best)
		{
			best = distance;
			nearest = abode;
		}
	}
	return nearest;
}

/// Where a town's people take food: its working storage pit's door
std::optional<glm::vec2> FoodDropOff(entt::entity town)
{
	const auto pit = ecs::construction::StoragePitOf(town);
	if (pit.has_value() && villager_home::IsFunctional(*pit))
	{
		return villager_home::ArrivePosition(*pit);
	}
	// TODO(villager-hunger-health): without a working storage pit a town keeps a temporary pot; not in openblack yet
	return std::nullopt;
}

bool TownWantsSleepMost(entt::entity town)
{
	return town == entt::null || Locator::townDesireSystem::value().GetMostWanted(town) == TownDesireInfo::ForSleep;
}

} // namespace

bool villager_breeding::IsSexuallyActive(entt::entity villager)
{
	if (!IsVillager(villager) || !Locator::infoConstants::has_value())
	{
		return false;
	}
	const auto& person = Entities().Get<const Villager>(villager);
	const auto& info = InfoOf(person);
	return IsSexuallyActive(villager_age::AgeNow(person), info.startHavingSexAge, info.stopHavingSexAge);
}

bool villager_breeding::IsPregnant(entt::entity villager)
{
	if (!IsVillager(villager))
	{
		return false;
	}
	const auto& person = Entities().Get<const Villager>(villager);
	return IsPregnant(person.sex == Villager::Sex::FEMALE, person.pregnancy);
}

void villager_breeding::GoAndHaveSexWith(entt::entity villager, entt::entity mate)
{
	MeetUp(villager, mate, VillagerStates::StartHavingSex, k_MeetingGap);
	SetLover(villager, mate);
	SetLover(mate, villager);
}

void villager_breeding::LetPartnerGo(entt::entity villager)
{
	if (!IsVillager(villager) || TopState(Entities().Get<const LivingAction>(villager)) != VillagerStates::HavingSex)
	{
		return;
	}
	const auto partner = LoverOf(villager);
	if (partner == entt::null)
	{
		return;
	}
	SetLover(partner, entt::null);
	villager_home::DecideWhatToDo(Entities().Get<LivingAction>(partner));
}

uint32_t villager_breeding::WomanSpecial(LivingAction& action)
{
	const auto villager = EntityOf(action);
	if (!IsPregnant(villager) || Entities().AllOf<ScriptControlled>(villager))
	{
		return 0;
	}
	auto& person = Entities().Get<Villager>(villager);
	const auto turn = Locator::time::has_value() ? Locator::time::value().GetTurn() : 0u;
	const auto countdown = CountDown(person.pregnancy, turn - person.lastCheckTurn);
	person.pregnancy = countdown.pregnancy;
	if (countdown.birthDue)
	{
		return StartsGivingBirth(action);
	}
	return 0;
}

void villager_breeding::BedTimeConceptions(entt::entity villager)
{
	auto& registry = Entities();
	const auto town = TownOf(villager);
	const auto abode = AbodeOf(villager);
	const float sleep =
	    town != entt::null ? Locator::townDesireSystem::value().GetRawDesire(town, TownDesireInfo::ForSleep) : 0.0f;
	if (abode == entt::null || !BedTimeMayConceive(town != entt::null, sleep, IsSexuallyActive(villager)))
	{
		return;
	}
	const auto& people = registry.Get<const Abode>(abode).inhabitants;
	std::vector<entt::entity> members;
	std::vector<Housemate> home;
	size_t self = 0;
	for (const auto person : people)
	{
		if (!IsVillager(person))
		{
			continue;
		}
		if (person == villager)
		{
			self = members.size();
		}
		members.push_back(person);
		home.push_back({.male = !IsFemale(person), .atHome = IsAtHome(person)});
	}
	// A woman's own chance comes from the first male in; a man going to bed gives every woman in hers
	const bool female = IsFemale(villager);
	const auto* abodeInfo = world_objects::AbodeInfoOf(abode);
	const uint32_t childRoom = abodeInfo != nullptr ? abodeInfo->maxChildrenInAbode : 0;
	for (const auto index : villager_breeding::BedTimeConceptions(female, self, home))
	{
		const auto woman = members.at(index);
		uint32_t children = 0;
		uint32_t pregnant = 0;
		for (const auto person : members)
		{
			children += IsChild(person) ? 1 : 0;
			pregnant += IsPregnant(person) ? 1 : 0;
		}
		const auto herTown = TownOf(woman);
		const bool conceives = ConceivesAtHome({
		    .pregnant = IsPregnant(woman),
		    .hasTown = herTown != entt::null,
		    .hasAbode = AbodeOf(woman) != entt::null,
		    .sexuallyActive = IsSexuallyActive(woman),
		    .childrenWanted = herTown != entt::null ? Locator::townDesireSystem::value().GetSignificanceToVillager(
		                                                  herTown, TownDesireInfo::ForChildren)
		                                            : 0.0f,
		    .children = children,
		    .pregnantWomen = pregnant,
		    .childRoom = childRoom,
		});
		if (conceives)
		{
			GetsPregnant(woman);
		}
	}
}

void villager_breeding::OrphanChildrenOf(entt::entity villager, entt::entity town)
{
	for (const auto child : Locator::townSystem::value().ChildrenOf(town, villager))
	{
		// One that is about mourns her first, then goes home
		if (IsAvailable(child))
		{
			SetTopState(Entities().Get<LivingAction>(child), VillagerStates::MornDeath);
		}
		Entities().Get<Villager>(child).mother = entt::null;
	}
}

uint32_t villager_breeding::MournMother(LivingAction& action)
{
	return villager_home::GoHome(action);
}

uint32_t villager_breeding::ChildAtCreche(LivingAction& action)
{
	if (CheckChild(action) == 1)
	{
		return 1;
	}
	const auto child = EntityOf(action);
	const auto town = TownOf(child);
	if (town == entt::null)
	{
		return 0;
	}
	if (villager_home::OfferToTownDesires(action) == 1)
	{
		return 1;
	}
	const auto creche = creches::CrecheOf(town);
	if (creche == entt::null)
	{
		return 0;
	}
	const bool daytime = Locator::skySystem::has_value() && creche::IsDaytime(Locator::skySystem::value().GetCurrentSkyType());
	const auto abode = AbodeOf(child);
	const bool firstIn = abode != entt::null && FirstOfHomeIsIn(abode);
	const bool touching = abode == entt::null && AreTouching(child, creche, k_CrecheTouch);
	switch (creche::AtTheCreche(daytime, villager_home::IsFunctional(creche), abode != entt::null, firstIn, touching))
	{
	case creche::AtCreche::Stroll:
		PlayCrecheSound(creche);
		StrollOn(action, creche);
		return 1;
	case creche::AtCreche::StrollQuietly:
		StrollOn(action, creche);
		return 0;
	case creche::AtCreche::GoHome:
		villager_home::GoHome(action);
		return 0;
	case creche::AtCreche::Wait:
		break;
	}
	return 0;
}

uint32_t villager_breeding::ChildDecideWhatToDo(LivingAction& action)
{
	if (!ChildHasOtherThingsToDo(action))
	{
		SetTopState(action, VillagerStates::ChildFollowsMother);
	}
	return 1;
}

bool villager_breeding::SetupBreederDisciple(LivingAction& action)
{
	auto& registry = Entities();
	const auto villager = EntityOf(action);
	if (IsPregnant(villager))
	{
		SetTopState(action, VillagerStates::GoHome);
		return true;
	}
	const auto town = TownOf(villager);
	const auto* stats = town != entt::null ? registry.TryGet<const TownStats>(town) : nullptr;
	if (stats == nullptr ||
	    !BreederMayBreed({.children = stats->children,
	                      .childPlaces = stats->childPlaces,
	                      .adults = stats->adults,
	                      .adultPlaces = stats->adultPlaces},
	                     Locator::infoConstants::value().town.amountOverCapacityThatDiscipleBreedersBreedUntil))
	{
		return false;
	}
	glm::vec2 spot;
	if (const auto home = NearestHome(town, PositionOf(villager), k_BreederHomeReach); home != entt::null)
	{
		spot = BreederSpotByHome(PositionOf(home), villager_home::ArrivePosition(home), world_objects::SizeOf(home).radius,
		                         GameInt(), GameFloat());
		// TODO(villagers): along the town's footpaths where there are any
		villager_home::SetupMoveTo(action, spot, VillagerStates::BreederDisciple);
	}
	else
	{
		const auto store = FoodDropOff(town);
		if (!store.has_value())
		{
			return false;
		}
		spot = BreederSpotByStore(*store, GameFloat());
		villager_home::SetupMoveTo(action, spot, VillagerStates::BreederDisciple);
	}
	SetLover(villager, entt::null);
	registry.AssignOrReplace<BreederSpot>(villager, spot);
	return true;
}

uint32_t villager_breeding::ReactToBreederPriority(entt::entity villager, entt::entity breeder)
{
	if (!IsVillager(breeder) || breeder == villager || !IsVillager(villager) || !IsSexuallyActive(villager) ||
	    IsFemale(villager) == IsFemale(breeder) || Entities().AllOf<ScriptControlled>(villager))
	{
		return 0;
	}
	// TODO(villager-reproduction): the game also passes over a breeder's call while a mark on the call it never seems
	// to set is up
	return Locator::infoConstants::value().reaction.at(static_cast<size_t>(Reaction::ReactToBreeder)).priority;
}

uint32_t villager_breeding::StartHavingSex(LivingAction& action)
{
	const auto villager = EntityOf(action);
	const auto mate = LoverOf(villager);
	if (mate != entt::null && IsAvailable(mate))
	{
		if (!AreTouching(mate, villager, k_KissingGap))
		{
			GoAndHaveSexWith(villager, mate);
			return 1;
		}
		Face(villager, mate);
		Face(mate, villager);
		if (StartLoving(villager, mate, VillagerStates::HavingSex))
		{
			return 1;
		}
	}
	SetTopState(action, VillagerStates::DecideWhatToDo);
	return 1;
}

uint32_t villager_breeding::HavingSex(LivingAction& action)
{
	if (!villager_animate::IsReadyForNewAnimation(action))
	{
		return 1;
	}
	// The woman draws her chance as the kiss ends, pregnant already or not
	const auto villager = EntityOf(action);
	if (IsFemale(villager) && IsSexuallyActive(villager))
	{
		const auto& info = InfoOf(Entities().Get<const Villager>(villager));
		if (Locator::gameRandom::value().GameFloatRand(1.0f) < info.pregnancyChance)
		{
			GetsPregnant(villager);
		}
	}
	SetTopState(action, VillagerStates::StopHavingSex);
	return 1;
}

uint32_t villager_breeding::StopHavingSex(LivingAction& action)
{
	if (IsAtHome(EntityOf(action)))
	{
		return villager_home::GoHome(action);
	}
	SetTopState(action, VillagerStates::DecideWhatToDo);
	return 1;
}

bool villager_breeding::ExitSex(LivingAction& action, VillagerStates next)
{
	if (next == VillagerStates::StartHavingSex || next == VillagerStates::HavingSex || next == VillagerStates::StopHavingSex)
	{
		return false;
	}
	const auto villager = EntityOf(action);
	const auto partner = LoverOf(villager);
	SetLover(villager, entt::null);
	if (partner != entt::null && TopState(Entities().Get<const LivingAction>(partner)) == TopState(action))
	{
		SetLover(partner, entt::null);
		SetTopState(Entities().Get<LivingAction>(partner), VillagerStates::StopHavingSex);
	}
	return false;
}

bool villager_breeding::SexValidate(LivingAction& action)
{
	const auto villager = EntityOf(action);
	if (const auto* lover = Entities().TryGet<const Lover>(villager); lover != nullptr && !IsAvailable(lover->partner))
	{
		SetLover(villager, entt::null);
	}
	return true;
}

uint32_t villager_breeding::StartsGivingBirth(LivingAction& action)
{
	Entities().Get<Villager>(EntityOf(action)).pregnancy = 0;
	action.turnsUntilStateChange = BirthTurns(k_TurnsPerDay, GameInt());
	SetTopState(action, VillagerStates::HousewifeGivingBirth);
	return GivingBirth(action);
}

uint32_t villager_breeding::GivingBirth(LivingAction& action)
{
	--action.turnsUntilStateChange;
	if (action.turnsUntilStateChange != 0)
	{
		return 1;
	}
	const auto mother = EntityOf(action);
	if (const auto child = ChildBorn(mother); child != entt::null)
	{
		// The baby cries one of its cries, at its side
		const auto cry =
		    k_BabyCrySounds.at(Locator::gameRandom::value().LocalRand(static_cast<int32_t>(k_BabyCrySounds.size())));
		audio::PlayGameSoundEffect(static_cast<entt::id_type>(cry), Entities().Get<const Transform>(child).position);
	}
	SetTopState(action, VillagerStates::HousewifeGivenBirth);
	return 1;
}

uint32_t villager_breeding::GivenBirth(LivingAction& action)
{
	Entities().Get<Villager>(EntityOf(action)).pregnancy = 0;
	return villager_home::GoHome(action);
}

uint32_t villager_breeding::ChildFollowsMother(LivingAction& action)
{
	if (ChildHasOtherThingsToDo(action))
	{
		return 1;
	}
	const auto child = EntityOf(action);
	const auto mother = Entities().Get<const Villager>(child).mother;
	glm::vec2 centre;
	if (IsAvailable(mother))
	{
		centre = PositionOf(mother);
	}
	else if (const auto abode = AbodeOf(child); abode != entt::null)
	{
		centre = PositionOf(abode);
	}
	else
	{
		villager_home::LookForBetterHome(action);
		return 1;
	}
	const auto goal = NearMother(centre, GameFloat());
	if (!villager_home::Collides(goal))
	{
		villager_home::SetupMoveTo(action, goal, VillagerStates::ChildFollowsMother);
	}
	return 1;
}

uint32_t villager_breeding::BreederDisciple(LivingAction& action)
{
	const auto villager = EntityOf(action);
	if (IsPregnant(villager) || TownWantsSleepMost(TownOf(villager)))
	{
		SetTopState(action, VillagerStates::GoHome);
		return 1;
	}
	if (action.turnsUntilStateChange > 0)
	{
		--action.turnsUntilStateChange;
		return 1;
	}
	const auto* spot = Entities().TryGet<const BreederSpot>(villager);
	const auto amble = BreederAmble(spot != nullptr ? spot->spot : PositionOf(villager), GameFloat(), GameInt());
	villager_home::SetupMoveTo(action, amble.goal, VillagerStates::BreederDisciple);
	action.turnsUntilStateChange = amble.pause;
	return 1;
}

bool villager_breeding::EnterBreeder(LivingAction& action, VillagerStates /*previous*/, VillagerStates /*next*/)
{
	if (!Locator::reactionSystem::has_value())
	{
		return true;
	}
	// It calls to the villagers of the other sex about it, its call's time starting now
	const auto villager = EntityOf(action);
	Locator::reactionSystem::value().Create({
	    .initiator = villager,
	    .type = Reaction::ReactToBreeder,
	    .player = world_objects::PlayerOf(villager).value_or(PlayerNames::NEUTRAL),
	    .position = Entities().Get<const Transform>(villager).position,
	    .onCast = true,
	});
	return true;
}

bool villager_breeding::ExitBreeder(LivingAction& action, VillagerStates /*next*/)
{
	if (Locator::reactionSystem::has_value())
	{
		Locator::reactionSystem::value().RemoveFrom(EntityOf(action), Reaction::ReactToBreeder);
	}
	return false;
}

uint32_t villager_breeding::BreederJustLanded(LivingAction& action)
{
	const auto breeder = EntityOf(action);
	const bool female = IsFemale(breeder);
	const auto here = PositionOf(breeder);
	auto& registry = Entities();
	entt::entity nearest = entt::null;
	float best = std::numeric_limits<float>::max();
	if (Locator::entitiesMap::has_value())
	{
		// The first villager of each of the nine cells about it, the nearest of age and of the other sex
		const auto& map = Locator::entitiesMap::value();
		auto cell = map_coords::CellOf(here);
		map_coords::Spiral spiral;
		for (int32_t count = 9; count != 0; --count)
		{
			for (const auto thing : map.GetAllInCell(cell))
			{
				if (!registry.Valid(thing) || !registry.AllOf<Villager>(thing))
				{
					continue;
				}
				if (IsVillager(thing) && IsSexuallyActive(thing) && IsFemale(thing) != female)
				{
					const float distance = glm::distance(here, PositionOf(thing));
					if (distance < best)
					{
						best = distance;
						nearest = thing;
					}
				}
				break;
			}
			const auto& step = spiral.Next();
			cell += glm::ivec2(step.x, step.z);
		}
	}
	if (nearest == entt::null)
	{
		SetTopState(action, VillagerStates::DecideWhatToDo);
		return 1;
	}
	GoAndHaveSexWith(breeder, nearest);
	return 1;
}

uint32_t villager_breeding::ReactToBreeder(LivingAction& action)
{
	const auto villager = EntityOf(action);
	const auto breeder = ecs::systems::villager_reactions::ReactingTo(villager);
	if (!breeder.has_value() || !IsVillager(*breeder))
	{
		return 0;
	}
	GoAndHaveSexWith(villager, *breeder);
	return 1;
}
