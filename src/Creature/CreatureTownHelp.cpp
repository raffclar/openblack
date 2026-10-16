/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureTownHelp.h"

#include <array>

#include "Creature/CreatureLayers.h"

using namespace openblack;
using namespace openblack::creature_town_help;
using creature_face::Cue;
using creature_mind::Effect;
using creature_mind::Eyes;
using creature_mind::Gaze;
using creature_mind::Movement;
using creature_mind::ObjectOrder;
namespace animations = openblack::creature_layers::animations;

namespace
{
/// The clip that puts a thing down
constexpr size_t k_PutDownAnimation = 97;
/// The clip that strokes what it holds
constexpr size_t k_StrokeAnimation = 100;
/// The kiss clip
constexpr size_t k_KissAnimation = 212;

Step PickUp(uint32_t thing)
{
	return {.kind = Step::Kind::Object,
	        .order = {.kind = ObjectOrder::Kind::PickUp, .object = thing},
	        .gaze = Gaze {.object = thing, .whileGoing = true}};
}

Step FaceThing(uint32_t thing, Cue face = Cue::None, Effect effect = Effect::None)
{
	return {.kind = Step::Kind::Move,
	        .seconds = k_FaceSeconds,
	        .movement = {.kind = Movement::Kind::TurnToFaceObject, .object = thing},
	        .effect = effect,
	        .face = face,
	        .gaze = Gaze {.object = thing}};
}

Step GoNear(uint32_t thing, float distance)
{
	return {.kind = Step::Kind::Move,
	        .movement = {.kind = Movement::Kind::GoNearObject, .object = thing, .maxDistance = distance},
	        .gaze = Gaze {.object = thing, .whileGoing = true}};
}

Step WalkTo(glm::vec2 point, float within)
{
	return {.kind = Step::Kind::Move,
	        .movement = {.kind = Movement::Kind::ToPoint, .point = point, .maxDistance = within},
	        .gaze = creature_mind::GoingLook(point)};
}

Step PutDown(Cue face = Cue::None, Effect effect = Effect::None)
{
	return {.kind = Step::Kind::Object,
	        .effect = effect,
	        .order = {.kind = ObjectOrder::Kind::Discard, .animation = k_PutDownAnimation},
	        .face = face};
}

Step TakeHandful(const HandfulSource& take)
{
	return {.kind = Step::Kind::Object,
	        .effect = Effect::TookFromPile,
	        .order = {.kind = ObjectOrder::Kind::TakeFromPile, .handful = take.look, .object = take.thing}};
}

Step PullFace(uint32_t villager, const Random& random)
{
	constexpr uint32_t k_FaceCount = 10;
	constexpr float k_FaceHeldSeconds = 5.0f;
	return {.kind = Step::Kind::Face,
	        .seconds = k_FaceHeldSeconds,
	        .animation = animations::k_FirstFace + random(k_FaceCount),
	        .gaze = Gaze {.object = villager}};
}
} // namespace

float creature_town_help::FieldReach(float height)
{
	return height * k_FieldReachHeights;
}

float creature_town_help::ThingReach(float radius, float thingRadius)
{
	return (radius + thingRadius) * k_ThingReach;
}

PotInfo creature_town_help::HandfulPot(ObjectOrder::Handful look)
{
	switch (look)
	{
	case ObjectOrder::Handful::Fruit:
		return PotInfo::TreeFruitInHand;
	case ObjectOrder::Handful::EvergreenBranch:
		return PotInfo::TreeBranchEvergreenInHand;
	case ObjectOrder::Handful::HardwoodBranch:
		return PotInfo::TreeBranchHardwoodInHand;
	case ObjectOrder::Handful::Logs:
		return PotInfo::TreeLogsInHand;
	case ObjectOrder::Handful::Wheat:
		break;
	}
	return PotInfo::WheatInHand;
}

ObjectOrder::Handful creature_town_help::MagicWoodLook(const Random& random)
{
	constexpr std::array k_Looks {ObjectOrder::Handful::EvergreenBranch, ObjectOrder::Handful::HardwoodBranch,
	                              ObjectOrder::Handful::Logs};
	return k_Looks.at(random(static_cast<uint32_t>(k_Looks.size())));
}

std::vector<Step> creature_town_help::GiveHandfulToStore(const std::optional<HandfulSource>& take, uint32_t store, float height)
{
	std::vector<Step> agenda;
	if (take.has_value())
	{
		agenda.push_back({.kind = Step::Kind::Move,
		                  .movement = {.kind = Movement::Kind::ToPoint, .point = take->point, .maxDistance = take->within}});
		agenda.push_back({.kind = Step::Kind::Move,
		                  .seconds = k_FaceSeconds,
		                  .movement = {.kind = Movement::Kind::TurnToFaceObject, .object = take->thing}});
		agenda.push_back(TakeHandful(*take));
	}
	agenda.push_back({.kind = Step::Kind::Move,
	                  .movement = {.kind = Movement::Kind::ToThrowPosition, .object = store, .maxDistance = height},
	                  .face = Cue::Compassion,
	                  .gaze = Gaze {.object = store}});
	agenda.push_back(FaceThing(store, Cue::Compassion, Effect::Completed));
	agenda.push_back({.kind = Step::Kind::Object, .order = {.kind = ObjectOrder::Kind::ThrowInStore, .object = store}});
	return agenda;
}

std::vector<Step> creature_town_help::PutFieldFoodBySite(const std::optional<HandfulSource>& take, glm::vec2 siteFood,
                                                         float height)
{
	std::vector<Step> agenda;
	if (take.has_value())
	{
		agenda.push_back({.kind = Step::Kind::Move,
		                  .movement = {.kind = Movement::Kind::ToPoint, .point = take->point, .maxDistance = height}});
		agenda.push_back({.kind = Step::Kind::Move,
		                  .seconds = k_FaceSeconds,
		                  .movement = {.kind = Movement::Kind::TurnToFaceObject, .object = take->thing}});
		agenda.push_back({.kind = Step::Kind::ActOnItself});
		agenda.push_back(TakeHandful(*take));
	}
	agenda.push_back(WalkTo(siteFood, height));
	agenda.push_back(PutDown(Cue::None, Effect::Completed));
	return agenda;
}

std::vector<Step> creature_town_help::DropCowInStore(std::optional<uint32_t> pickUp, uint32_t store)
{
	std::vector<Step> agenda;
	if (pickUp.has_value())
	{
		agenda.push_back(PickUp(*pickUp));
	}
	agenda.push_back(FaceThing(store));
	agenda.push_back({.kind = Step::Kind::Move,
	                  .movement = {.kind = Movement::Kind::ToThrowPosition, .object = store},
	                  .effect = Effect::Completed,
	                  .face = Cue::Compassion,
	                  .gaze = Gaze {.object = store}});
	agenda.push_back({.kind = Step::Kind::Object, .order = {.kind = ObjectOrder::Kind::ThrowInStore, .object = store}});
	return agenda;
}

std::vector<Step> creature_town_help::MakeBreeder(uint32_t villager)
{
	return {
	    PickUp(villager),
	    {.kind = Step::Kind::Object,
	     .effect = Effect::Completed,
	     .order = {.kind = ObjectOrder::Kind::Keep, .animation = k_StrokeAnimation},
	     .face = Cue::Compassion,
	     .gaze = Gaze {.object = villager},
	     .main = true},
	    {.kind = Step::Kind::MakeDisciple, .object = villager},
	    PutDown(),
	};
}

std::vector<Step> creature_town_help::TakeVillagerHome(std::optional<uint32_t> pickUp, uint32_t home)
{
	std::vector<Step> agenda;
	if (pickUp.has_value())
	{
		agenda.push_back(PickUp(*pickUp));
	}
	agenda.push_back(GoNear(home, k_NearHome));
	agenda.push_back(FaceThing(home));
	agenda.push_back(PutDown());
	return agenda;
}

std::vector<Step> creature_town_help::BringVillagerToSite(std::optional<uint32_t> pickUp, uint32_t site)
{
	std::vector<Step> agenda;
	if (pickUp.has_value())
	{
		agenda.push_back(PickUp(*pickUp));
	}
	agenda.push_back(FaceThing(site));
	agenda.push_back(GoNear(site, 0.0f));
	agenda.push_back(PutDown(Cue::Compassion));
	return agenda;
}

std::vector<Step> creature_town_help::MoveTotem(uint32_t totem, bool raise)
{
	return {
	    GoNear(totem, k_NearTotem),
	    FaceThing(totem),
	    {.kind = Step::Kind::Action, .animation = animations::k_Summon, .face = Cue::Playfulness},
	    {.kind = raise ? Step::Kind::RaiseTotem : Step::Kind::LowerTotem, .effect = Effect::Completed, .object = totem},
	};
}

TotemTurn creature_town_help::MoveTotemTurn(float share, bool raise)
{
	if (raise)
	{
		if (1.0f - share < k_TotemStep)
		{
			return {.share = 1.0f, .done = true};
		}
		return {.share = share + k_TotemStep, .done = false};
	}
	if (share < k_TotemStep)
	{
		return {.share = 0.0f, .done = true};
	}
	return {.share = share - k_TotemStep, .done = false};
}

std::vector<Step> creature_town_help::FrightenVillagers(uint32_t villager, float height)
{
	Step frightened {.kind = Step::Kind::Action, .animation = animations::k_Frightened};
	frightened.heldEyes = Eyes::Wide;
	auto turn = FaceThing(villager);
	turn.seconds = k_FrightenFaceSeconds;
	return {GoNear(villager, k_FrightenHeights * height), turn, frightened};
}

std::vector<Step> creature_town_help::PlayWithVillager(uint32_t villager, float height, const Random& random)
{
	constexpr uint32_t k_PlayfulLots = 3;
	std::vector<Step> agenda {GoNear(villager, k_PlayHeights * height), FaceThing(villager)};
	if (random(k_PlayfulLots) > 0)
	{
		agenda.push_back({.kind = Step::Kind::Action, .animation = animations::k_FeelPlayful});
	}
	agenda.push_back(PullFace(villager, random));
	agenda.push_back({.kind = Step::Kind::Object,
	                  .effect = Effect::Completed,
	                  .order = {.kind = ObjectOrder::Kind::PointAt, .object = villager, .seconds = k_PlayPointSeconds},
	                  .face = Cue::Smile,
	                  .gaze = Gaze {.object = villager},
	                  .main = true});
	agenda.push_back(PullFace(villager, random));
	return agenda;
}

std::vector<Step> creature_town_help::KissVillager(uint32_t villager, float height)
{
	return {GoNear(villager, k_PlayHeights * height), FaceThing(villager),
	        Step {.kind = Step::Kind::Kiss, .animation = k_KissAnimation}};
}

bool creature_town_help::IsCowKind(AnimalInfo type)
{
	switch (type)
	{
	case AnimalInfo::Cow:
	case AnimalInfo::Sheep:
	case AnimalInfo::Goat:
	case AnimalInfo::Horse:
	case AnimalInfo::Pig:
	case AnimalInfo::Zebra:
	case AnimalInfo::Tortoise:
	case AnimalInfo::PuzzleCow:
	case AnimalInfo::PuzzleHorse:
	case AnimalInfo::PuzzlePig:
	case AnimalInfo::PuzzleSheep:
	case AnimalInfo::PuzzleTortoise:
		return true;
	default:
		return false;
	}
}

bool creature_town_help::FarFromHome(float distanceFromHome)
{
	return distanceFromHome > k_FarFromHome;
}

bool creature_town_help::CanMakeBreeder(const BreederFacts& facts)
{
	return facts.available && facts.hasTown && !facts.disciple && !facts.follower && facts.townBreeders < k_MostBreeders;
}
