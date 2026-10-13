/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreaturePlanActions.h"

#include <algorithm>
#include <array>

#include "Creature/CreatureActionAgendas.h"
#include "Creature/CreatureCastAgenda.h"
#include "Creature/CreatureFireAgenda.h"
#include "Creature/CreatureKindAgendas.h"
#include "Creature/CreatureLayers.h"
#include "Creature/CreatureScriptAgendas.h"

using namespace openblack;
using namespace openblack::creature_plan_actions;
namespace animations = openblack::creature_layers::animations;
using creature_mind::Activity;

namespace
{
/// Actions of the creature's animation set that have no name elsewhere
constexpr size_t k_Sneeze = 62;
constexpr size_t k_LookAtMe = 69;
constexpr size_t k_PickMe = 74;
/// Following something about and gazing at the view last this long
constexpr float k_FollowSeconds = 8.0f;
constexpr float k_GazeSeconds = 6.0f;

/// A chance from 0 to 1 in fine steps, from the whole-number random
float RandomFraction(const creature_mind::Random& random)
{
	constexpr uint32_t k_Steps = 1U << 24U;
	return static_cast<float>(random(k_Steps)) / static_cast<float>(k_Steps);
}

constexpr std::array k_Executors {
    Executor {.action = "EatAfterExamining", .target = Target::Food, .build = Build::Eat, .activity = Activity::Eat},
    Executor {.action = "EatAlive", .target = Target::LiveFood, .build = Build::Eat, .activity = Activity::Eat},
    Executor {
        .action = "EatFromFoodPile", .target = Target::MagicFoodPile, .build = Build::EatFromPile, .activity = Activity::Eat},
    Executor {.action = "SleepOnTheSpot", .build = Build::Sleep, .activity = Activity::Sleep},
    Executor {.action = "Poo", .build = Build::Poo, .activity = Activity::Poo},
    Executor {.action = "Puke", .build = Build::Puke, .activity = Activity::Puke},
    Executor {.action = "DrinkFromTheSea", .build = Build::Drink, .activity = Activity::Drink},
    Executor {.action = "FishAndEat", .build = Build::FishAndEat, .activity = Activity::Eat},
    Executor {.action = "GiveFishToStoragePit", .target = Target::StoragePit, .build = Build::GiveFishToStore},
    Executor {.action = "TakeFishHome", .build = Build::TakeFishHome},
    Executor {.action = "GetHigh", .target = Target::Anything, .build = Build::GetHigh, .activity = Activity::Eat},
    // Trees: given to a town's store as wood, or planted by something
    Executor {.action = "GiveWoodFromTreeToStoragePit",
              .target = Target::StoragePit,
              .use = Target::PickableTree,
              .build = Build::GiveWoodToStore},
    Executor {.action = "GiveWoodFromTreeToBuildingSite",
              .target = Target::BuildingBeingBuilt,
              .use = Target::PickableTree,
              .build = Build::GiveWoodToStore},
    Executor {.action = "HelpRepairHouse",
              .target = Target::NeedsRepair,
              .use = Target::PickableTree,
              .build = Build::HelpRepairHouse},
    Executor {.action = "PlantTree",
              .target = Target::Anything,
              .use = Target::PlantableTree,
              .notWhatItHolds = true,
              .build = Build::PlantTree},
    Executor {.action = "ExamineByPickingUp",
              .target = Target::Pickable,
              .build = Build::ExamineByPickingUp,
              .activity = Activity::Examine},
    Executor {.action = "ExamineByLooking", .target = Target::Anything, .build = Build::ExamineByLooking},
    Executor {.action = "ExamineByFollowing", .target = Target::Living, .build = Build::ExamineByFollowing},
    Executor {.action = "ThrowBallAtObject",
              .target = Target::Anything,
              .use = Target::Toy,
              .build = Build::ThrowToy,
              .activity = Activity::PlayWithObject},
    // Practising a throw on what it may pick up, and throwing living things about
    Executor {.action = "PracticeThrow",
              .target = Target::Pickable,
              .build = Build::PracticeThrow,
              .activity = Activity::PlayWithObject},
    Executor {
        .action = "BreakRock", .target = Target::PickableRock, .build = Build::BreakRock, .activity = Activity::PlayWithObject},
    // Stamping on a rock, which breaks in two
    Executor {.action = "SmashStoneInHalf", .target = Target::Rock, .build = Build::Destroy},
    // Toys: stroking a cuddly one, taking one home, throwing a die
    // The game's data names stroking a toy "StrokToy"
    Executor {.action = "StrokToy", .target = Target::CuddlyToy, .build = Build::StrokeToy},
    Executor {.action = "TakeToyHome", .target = Target::ToyAwayFromHome, .build = Build::TakeToyHome},
    Executor {.action = "ThrowDie", .target = Target::Die, .build = Build::ThrowDie, .activity = Activity::PlayWithObject},
    Executor {.action = "PlayThrowingGameWithPlayer",
              .use = Target::Toy,
              .build = Build::ThrowToPlayer,
              .activity = Activity::PlayWithObject},
    Executor {.action = "ThrowAround",
              .target = Target::AnyLiving,
              .build = Build::PracticeThrow,
              .activity = Activity::PlayWithObject},
    Executor {.action = "KickBallAround",
              .target = Target::ToyBall,
              .build = Build::KickBallAround,
              .activity = Activity::PlayWithObject},
    // TODO(creature-kind-actions): the game walks to the nearest coast it hasn't visited and throws the thing into the
    // nearest sea it hasn't visited, by the creature's map of where it has been, which isn't here yet; until then it
    // throws the thing somewhere nearby
    Executor {.action = "ThrowInTheSea",
              .target = Target::SeaThrowable,
              .build = Build::ThrowAbout,
              .activity = Activity::PlayWithObject},
    Executor {.action = "ThrowToImpress",
              .target = Target::Anything,
              .use = Target::ThrowableToImpress,
              .notWhatItHolds = true,
              .build = Build::Hurl,
              .activity = Activity::Hurl},
    Executor {.action = "KickTree", .target = Target::Tree, .build = Build::Destroy},
    Executor {.action = "PullSillyFaces", .build = Build::Emote, .animation = animations::k_FeelPlayful},
    Executor {.action = "Hurl",
              .target = Target::Stonable,
              .use = Target::ThrowingDamage,
              .notWhatItHolds = true,
              .build = Build::Hurl,
              .activity = Activity::Hurl},
    Executor {.action = "Stomp", .target = Target::Destroyable, .build = Build::Destroy},
    Executor {.action = "Kick", .target = Target::Destroyable, .build = Build::Destroy},
    Executor {.action = "SitDown", .build = Build::SitDown, .activity = Activity::Sit},
    Executor {.action = "BeIdle", .build = Build::BeIdle, .activity = Activity::BeIdle},
    Executor {.action = "HangAroundAtHome", .build = Build::HangAround, .activity = Activity::HangAround},
    Executor {.action = "CommunicateState", .build = Build::ShowDesire, .activity = Activity::ShowDesire},
    Executor {.action = "WaveAtPlayer", .build = Build::FaceCameraEmote, .animation = animations::k_FriendlyWave},
    Executor {.action = "LookAtHand", .build = Build::FaceCameraEmote, .animation = k_LookAtMe},
    Executor {.action = "PointAtCamera", .build = Build::PointAtCamera},
    Executor {.action = "BePatheticToPlayer", .build = Build::FaceCameraEmote, .animation = k_PickMe},
    Executor {.action = "HowlAtPlayer", .build = Build::FaceCameraEmote, .animation = animations::k_Summon},
    Executor {.action = "RunAwayFromObject", .target = Target::Frightening, .build = Build::RunFromObject},
    Executor {.action = "BeFrightenedOnTheSpot", .build = Build::BeFrightened},
    Executor {.action = "BeSad", .build = Build::Emote, .animation = animations::k_Sad},
    Executor {.action = "Scratch", .build = Build::Emote, .animation = animations::k_Scratch},
    Executor {.action = "Shiver", .build = Build::Emote, .animation = animations::k_Cold},
    Executor {.action = "Sneeze", .build = Build::Emote, .animation = k_Sneeze},
    Executor {.action = "ShowHotness", .build = Build::Emote, .animation = animations::k_Hot},
    Executor {.action = "RunAwayFromPlayer", .build = Build::RunFromPlayer},
    Executor {.action = "ShowImpressiveAnimation", .build = Build::Emote, .animation = animations::k_Impress},
    Executor {
        .action = "Stroke", .target = Target::Villager, .build = Build::ApproachEmote, .animation = animations::k_FeelingNice},
    Executor {.action = "SmileAtFriend", .target = Target::Creature, .build = Build::SmileAt},
    Executor {.action = "WaveAtFriend",
              .target = Target::Creature,
              .build = Build::ApproachEmote,
              .animation = animations::k_FriendlyWave},
    Executor {.action = "LookAtMountains", .build = Build::LookAbout},
    Executor {.action = "LookOutToSea", .build = Build::LookAbout},
    Executor {.action = "LookAtSun", .build = Build::LookAbout},
    Executor {.action = "LookAtMoon", .build = Build::LookAbout},
    // The miracles it casts at things: lightning at anything, a heal at a villager hurt enough, a spell at another
    // creature
    Executor {.action = "CastLightningBolt", .target = Target::Anything, .build = Build::CastLightning},
    Executor {.action = "CastHealSpell", .target = Target::HurtVillager, .build = Build::CastHelpful},
    Executor {.action = "CastMakeCreatureFreeze", .target = Target::Creature, .build = Build::CastPlayful},
    Executor {.action = "CastMakeCreatureSmall", .target = Target::Creature, .build = Build::CastPlayful},
    Executor {.action = "CastMakeCreatureBig", .target = Target::Creature, .build = Build::CastPlayful},
    Executor {.action = "CastMakeCreatureWeak", .target = Target::Creature, .build = Build::CastPlayful},
    Executor {.action = "CastMakeCreatureStrong", .target = Target::Creature, .build = Build::CastPlayful},
    Executor {.action = "CastMakeCreatureFat", .target = Target::Creature, .build = Build::CastPlayful},
    Executor {.action = "CastMakeCreatureThin", .target = Target::Creature, .build = Build::CastPlayful},
    Executor {.action = "CastMakeCreatureInvisible", .target = Target::Creature, .build = Build::CastPlayful},
    Executor {.action = "CastMakeCreatureNice", .target = Target::Creature, .build = Build::CastPlayful},
    Executor {.action = "CastMakeCreatureAngry", .target = Target::Creature, .build = Build::CastPlayful},
    Executor {.action = "CastMakeCreatureHungry", .target = Target::Creature, .build = Build::CastPlayful},
    Executor {.action = "CastMakeCreatureFrightened", .target = Target::Creature, .build = Build::CastPlayful},
    Executor {.action = "CastMakeCreatureTired", .target = Target::Creature, .build = Build::CastPlayful},
    Executor {.action = "CastMakeCreatureIll", .target = Target::Creature, .build = Build::CastPlayful},
    Executor {.action = "CastMakeCreatureThirsty", .target = Target::Creature, .build = Build::CastPlayful},
    Executor {.action = "CastMakeCreatureItchy", .target = Target::Creature, .build = Build::CastPlayful},
    // Fire: putting it out with the water miracle, setting a thing alight with a burning thing, and the action of
    // starting a fire, which the game never carries out
    Executor {.action = "PutOutFireWithMagicWater", .target = Target::Burning, .build = Build::CastWater},
    Executor {.action = "SetFireToObject", .target = Target::Unburnt, .build = Build::SetFire},
    Executor {.action = "StartFire", .build = Build::Never},
    Executor {.action = "SleepAtHome", .build = Build::SleepAtHome, .activity = Activity::Sleep},
    Executor {.action = "WaveAtObject", .target = Target::Anything, .build = Build::WaveAt},
    Executor {.action = "PutDown", .build = Build::PutDown, .activity = Activity::PutDown},
    Executor {.action = "CastHealSpellPU1", .target = Target::HurtVillager, .build = Build::CastHelpful},
    Executor {.action = "DeadForever", .build = Build::DeadForever},
    Executor {.action = "PointOutHighlight", .target = Target::Anything, .build = Build::PointOutHighlight},
    Executor {
        .action = "SleepByObject", .target = Target::Anything, .build = Build::SleepByObject, .activity = Activity::Sleep},
    // What scripts force on creatures to stage their scenes
    Executor {.action = "LookForever", .target = Target::Anything, .build = Build::LookForever},
    Executor {.action = "LookButDontApproach", .target = Target::Anything, .build = Build::LookButDontApproach},
    Executor {.action = "LookAtCamera", .build = Build::LookAtCamera},
    Executor {.action = "PointAtObject", .target = Target::Anything, .build = Build::PointAtThing},
    // What a script's cut scene makes every creature free to think for itself do
    Executor {.action = "LookAtCameraInWideScreen", .build = Build::LookAtCameraInCutScene},
};

/// The trip to the shoal to bring food out of the sea, none when it already has food in its hand; it walks straight to the
/// shoal
std::optional<creature_mind::FishingTrip> TripFor(const Situation::Fishing& fishing)
{
	if (fishing.holdingFood)
	{
		return std::nullopt;
	}
	return creature_mind::FishingTrip {
	    .shoal = fishing.shoal, .arriveWithin = fishing.arriveWithin, .putDownFirst = fishing.putDownFirst};
}
} // namespace

std::span<const Executor> creature_plan_actions::All()
{
	return k_Executors;
}

const Executor* creature_plan_actions::For(std::string_view action)
{
	const auto found = std::ranges::find(k_Executors, action, &Executor::action);
	return found != k_Executors.end() ? &*found : nullptr;
}

bool creature_plan_actions::IsCast(const Executor& executor)
{
	return executor.build == Build::CastLightning || executor.build == Build::CastHelpful ||
	       executor.build == Build::CastPlayful || executor.build == Build::CastWater;
}

bool creature_plan_actions::Possible(const Executor& executor, const Situation& situation)
{
	switch (executor.build)
	{
	case Build::Drink:
		return situation.water.has_value();
	case Build::FishAndEat:
		return situation.fishing.has_value() && !situation.fishing->holdingFood && situation.fishing->standAt.has_value();
	case Build::GiveFishToStore:
		return situation.fishing.has_value();
	case Build::TakeFishHome:
		return situation.fishing.has_value() && situation.home.has_value();
	case Build::SleepAtHome:
		return situation.home.has_value();
	case Build::SleepByObject:
		// TODO(creature-actions-3): left to itself the game only does this leashed or far from home, with nothing
		// frightening near; until that is ported only scripts make a creature do it
		return situation.controlledByScript && (situation.clearArea.has_value() || situation.home.has_value());
	case Build::FaceCameraEmote:
	case Build::RunFromPlayer:
		return situation.camera.has_value();
	case Build::PointAtCamera:
	case Build::PointOutHighlight:
		return situation.eye.has_value();
	case Build::ShowDesire:
		return situation.showDesireAnimation.has_value();
	default:
		return true;
	}
}

std::optional<std::vector<creature_mind::Step>> creature_plan_actions::Agenda(const Executor& executor,
                                                                              std::optional<uint32_t> object,
                                                                              glm::vec2 objectPoint, const Situation& situation,
                                                                              const creature_mind::Random& random,
                                                                              const std::optional<CastInfo>& cast)
{
	if (!Possible(executor, situation) || (executor.target != Target::None && !object.has_value()) ||
	    (IsCast(executor) && !cast.has_value()))
	{
		return std::nullopt;
	}
	switch (executor.build)
	{
	case Build::Eat:
		return creature_mind::Eat(*object);
	case Build::GiveWoodToStore:
	{
		// Holding a tree, it gives that; with none to take either, it idles
		std::optional<creature_mind::TreeToTake> take;
		if (!situation.holdingTree)
		{
			if (!situation.instrument.has_value())
			{
				return creature_mind::BeIdle(random);
			}
			take = creature_mind::TreeToTake {
			    .tree = *situation.instrument, .point = situation.instrumentPoint, .radius = situation.instrumentRadius};
		}
		return creature_mind::GiveWoodToStore(take, *object, situation.radius, situation.height);
	}
	case Build::ThrowToy:
		if (!situation.instrument.has_value())
		{
			return std::nullopt;
		}
		return creature_mind::ThrowToyAt(*situation.instrument, *object, objectPoint, situation.height, random);
	case Build::HelpRepairHouse:
		if (!situation.instrument.has_value())
		{
			return std::nullopt;
		}
		return creature_mind::HelpRepairHouse(situation.handFull ? std::nullopt : situation.instrument, *object,
		                                      *situation.instrument);
	case Build::ThrowToPlayer:
		if (!situation.playerHand.has_value() || !situation.instrument.has_value())
		{
			return std::nullopt;
		}
		return creature_mind::ThrowToPlayer(*situation.instrument, *situation.playerHand, situation.handFull);
	case Build::StrokeToy:
		return creature_mind::StrokeToy(*object);
	case Build::TakeToyHome:
		if (!situation.home.has_value())
		{
			return std::nullopt;
		}
		return creature_mind::TakeToyHome(*object, *situation.home, situation.height, random,
		                                  [&random] { return RandomFraction(random); });
	case Build::ThrowDie:
		return creature_mind::ThrowDie(situation.handFull ? std::nullopt : object, *object, situation.position,
		                               situation.height, random);
	case Build::BreakRock:
		return creature_mind::BreakRock(situation.handFull ? std::nullopt : object, *object, situation.position,
		                                situation.height);
	case Build::KickBallAround:
		return creature_mind::KickBallAround(*object, random);
	case Build::GetHigh:
		return creature_mind::GetHigh(situation.holdingMushroom ? std::nullopt : object, *object);
	case Build::PracticeThrow:
		return creature_mind::PracticeThrow(situation.handFull ? std::nullopt : object, situation.position, situation.height);
	case Build::PlantTree:
		if (!situation.holdingTree && !situation.instrument.has_value())
		{
			return std::nullopt;
		}
		return creature_mind::PlantTreeBy(situation.holdingTree ? std::nullopt : situation.instrument, *object);
	case Build::EatFromPile:
		// TODO(creature-actions-3): with something it can eat in its hand already, the game does something else; what
		// isn't known yet, so it goes to the pile all the same
		return creature_mind::EatFromPile(*object, objectPoint, situation.thingRadius, situation.radius);
	case Build::Sleep:
		return creature_mind::Sleep(random);
	case Build::Poo:
		return creature_mind::Poo(random);
	case Build::Puke:
		return creature_mind::Puke();
	case Build::Drink:
		return creature_mind::Drink(situation.water->shore, situation.water->water);
	case Build::FishAndEat:
		return creature_mind::FishAndEat(*situation.fishing->standAt, situation.fishing->arriveWithin,
		                                 situation.fishing->putDownFirst);
	case Build::GiveFishToStore:
		return creature_mind::GiveFishToStore(TripFor(*situation.fishing), *object, situation.fishing->height);
	case Build::TakeFishHome:
		return creature_mind::TakeFishTo(TripFor(*situation.fishing), *situation.home, situation.fishing->height);
	case Build::ExamineByPickingUp:
		return creature_mind::ExamineByPickingUp(*object, random);
	case Build::ExamineByLooking:
		return creature_mind::ExamineByLooking(*object, situation.height, situation.thingHeight);
	case Build::ExamineByFollowing:
		return creature_mind::FollowFor(*object, k_FollowSeconds);
	case Build::ThrowAbout:
		return creature_mind::ThrowAbout(*object, random);
	case Build::Hurl:
		// With nothing to throw, it can't
		if (!situation.instrument.has_value())
		{
			return std::nullopt;
		}
		return creature_mind::HurlAt(*situation.instrument, *object, objectPoint, situation.height, situation.handFull, random);
	case Build::SleepAtHome:
		return creature_mind::SleepAtHome(*situation.home, situation.height, random);
	case Build::SleepByObject:
		// Without a clear place by the thing, it sleeps at home instead
		if (situation.clearArea.has_value())
		{
			return creature_mind::SleepByObject(*situation.clearArea, situation.radius, random);
		}
		return creature_mind::SleepAtHome(*situation.home, situation.height, random);
	case Build::SmileAt:
		return creature_mind::SmileAt(*object, situation.height, situation.chance, random);
	case Build::WaveAt:
		return creature_mind::WaveAt(*object, situation.height);
	case Build::BeFrightened:
		return creature_mind::BeFrightenedOnTheSpot();
	case Build::PutDown:
		return creature_mind::PutDown();
	case Build::DeadForever:
		return creature_mind::DeadForever();
	case Build::PointOutHighlight:
		return creature_mind::PointOutHighlight(*object, situation.eyeDistance);
	case Build::Destroy:
		return creature_mind::DestroyThing(*object);
	case Build::SitDown:
		// Told by a script where to sit, it goes there; otherwise it sits where it is
		// TODO(creature-mind): left to itself the game first looks for a clear area about four times its radius around
		// the place, and gives up when it finds none; openblack sits on the spot
		if (situation.controlledByScript && object.has_value())
		{
			return creature_mind::SitDownAt(*object, objectPoint, situation.radius, random);
		}
		return std::vector {creature_mind::SitDown(random)};
	case Build::BeIdle:
		return creature_mind::BeIdle(random);
	case Build::HangAround:
		return creature_mind::HangAround(situation.hangAround, situation.turnSeconds, random);
	case Build::ShowDesire:
		return creature_mind::Emote(*situation.showDesireAnimation);
	case Build::Emote:
		return creature_mind::Emote(executor.animation);
	case Build::FaceCameraEmote:
		return creature_mind::FaceAndEmote(*situation.camera, executor.animation);
	case Build::ApproachEmote:
		return creature_mind::ApproachAndEmote(*object, executor.animation);
	case Build::RunFromObject:
		return creature_mind::RunFrom(objectPoint);
	case Build::RunFromPlayer:
		return creature_mind::RunFrom(*situation.camera);
	case Build::LookAbout:
		return creature_mind::LookAbout(k_GazeSeconds);
	case Build::CastLightning:
		return creature_mind::CastAt(creature_mind::CastStyle::Lightning, cast->magicType, cast->gesture, *object, cast->height,
		                             random);
	case Build::CastHelpful:
		return creature_mind::CastAt(creature_mind::CastStyle::Helpful, cast->magicType, cast->gesture, *object, cast->height,
		                             random);
	case Build::CastPlayful:
		return creature_mind::CastAt(creature_mind::CastStyle::Playful, cast->magicType, cast->gesture, *object, cast->height,
		                             random);
	case Build::CastWater:
		return creature_mind::PutOutFireWithWater(*object, cast->height);
	case Build::SetFire:
		return creature_mind::SetFireTo(*object, situation.instrument, situation.handFull);
	case Build::LookForever:
		return creature_mind::LookForever(*object);
	case Build::LookButDontApproach:
		return creature_mind::LookButDontApproach(*object, situation.chance);
	case Build::LookAtCamera:
		return creature_mind::LookAtCamera(situation.hasPlayer);
	case Build::PointAtThing:
		return creature_mind::PointAtThing(*object);
	case Build::LookAtCameraInCutScene:
		return creature_mind::LookAtCameraInCutScene(situation.eye, random);
	case Build::PointAtCamera:
		return creature_mind::PointAtCamera(situation.hasPlayer, *situation.eye);
	case Build::Never:
		return std::nullopt;
	}
	return std::nullopt;
}
