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

#include <glm/geometric.hpp>

#include "Common/GUtilsDistance.h"
#include "Creature/CreatureActionAgendas.h"
#include "Creature/CreatureBringHome.h"
#include "Creature/CreatureCastAgenda.h"
#include "Creature/CreatureDance.h"
#include "Creature/CreatureEmoteAgendas.h"
#include "Creature/CreatureExplorationAgendas.h"
#include "Creature/CreatureFireAgenda.h"
#include "Creature/CreatureFriendAgendas.h"
#include "Creature/CreatureKindAgendas.h"
#include "Creature/CreatureLayers.h"
#include "Creature/CreatureOddAgendas.h"
#include "Creature/CreaturePlayerAgendas.h"
#include "Creature/CreatureRestAndHome.h"
#include "Creature/CreatureSacrifice.h"
#include "Creature/CreatureScriptAgendas.h"
#include "Creature/CreatureSteal.h"
#include "Creature/CreatureTempleDance.h"
#include "Creature/CreatureTownHelp.h"
#include "Creature/CreatureTownTheft.h"
#include "Enums.h"

using namespace openblack;
using namespace openblack::creature_plan_actions;
namespace animations = openblack::creature_layers::animations;
using creature_mind::Activity;

namespace
{
/// Actions of the creature's animation set that have no name elsewhere
constexpr size_t k_Sneeze = 62;
/// Gazing at the view lasts this long
constexpr float k_GazeSeconds = 6.0f;

/// A chance from 0 to 1 in fine steps, from the whole-number random
float RandomFraction(const creature_mind::Random& random)
{
	constexpr uint32_t k_Steps = 1U << 24U;
	return static_cast<float>(random(k_Steps)) / static_cast<float>(k_Steps);
}

constexpr std::array k_Executors {
    Executor {.action = "EatAfterExamining", .target = Target::Food, .build = Build::Eat, .activity = Activity::Eat},
    Executor {.action = "EatAlive", .target = Target::Food, .build = Build::EatAlive, .activity = Activity::Eat},
    Executor {
        .action = "EatFromFoodPile", .target = Target::MagicFoodPile, .build = Build::EatFromPile, .activity = Activity::Eat},
    Executor {.action = "EatFromStoragePit",
              .target = Target::StoragePitWithFood,
              .build = Build::EatFromStoragePit,
              .activity = Activity::Eat},
    // The data of the game gives the action to eat from a field the name "EatFromContainer"
    Executor {
        .action = "EatFromContainer", .target = Target::FieldWithFood, .build = Build::EatFromField, .activity = Activity::Eat},
    Executor {.action = "EatFromTree", .target = Target::TreeBigEnough, .build = Build::EatFromTree, .activity = Activity::Eat},
    Executor {.action = "SleepOnTheSpot", .build = Build::Sleep, .activity = Activity::Sleep},
    Executor {.action = "Poo", .target = Target::PooedOn, .build = Build::Poo, .activity = Activity::Poo},
    Executor {.action = "Fart", .build = Build::Fart, .activity = Activity::Poo},
    Executor {.action = "PooAtHome", .build = Build::PooAtHome, .activity = Activity::Poo},
    Executor {.action = "Puke", .build = Build::Puke, .activity = Activity::Puke},
    Executor {.action = "DrinkFromTheSea", .build = Build::Drink, .activity = Activity::Drink},
    Executor {.action = "FishAndEat", .build = Build::FishAndEat, .activity = Activity::Eat},
    Executor {.action = "GiveFishToStoragePit", .target = Target::BuiltStoragePit, .build = Build::GiveFishToStore},
    Executor {.action = "TakeFishHome", .build = Build::TakeFishHome},
    // Bringing things home: a thing far from home, food, and wheat from a field
    Executor {.action = "BringHome", .target = Target::BringableHome, .build = Build::BringHome},
    Executor {.action = "BringFoodHome", .target = Target::Food, .build = Build::BringFoodHome},
    Executor {.action = "TakeFoodFromFieldToHome", .target = Target::FieldWithFood, .build = Build::TakeFieldFoodHome},
    Executor {.action = "GetHigh", .target = Target::Anything, .build = Build::GetHigh, .activity = Activity::Eat},
    // Trees: given to a town's store as wood, or planted by something
    Executor {.action = "GiveWoodFromTreeToStoragePit",
              .target = Target::BuiltStoragePit,
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
    Executor {.action = "ExamineByLooking", .target = Target::Examinable, .build = Build::ExamineByLooking},
    Executor {.action = "ExamineByFollowing", .target = Target::InterestingVillager, .build = Build::ExamineByFollowing},
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
    // Stomping on a thing and eating it, and knocking a living thing down with a rock and eating it
    Executor {.action = "StompAndEat", .target = Target::Stompable, .build = Build::StompAndEat, .activity = Activity::Eat},
    Executor {.action = "StoneAndEat",
              .target = Target::StonableAndPickable,
              .use = Target::ThrowingDamage,
              .notWhatItHolds = true,
              .build = Build::StoneAndEat,
              .activity = Activity::Eat},
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
    // It walks to the nearest coast and throws the thing into the nearest sea, by its map of the land
    Executor {.action = "ThrowInTheSea",
              .target = Target::SeaThrowable,
              .build = Build::ThrowInTheSea,
              .activity = Activity::PlayWithObject},
    Executor {.action = "ThrowToImpress",
              .target = Target::Anything,
              .use = Target::ThrowableToImpress,
              .notWhatItHolds = true,
              .build = Build::Hurl,
              .activity = Activity::Hurl},
    Executor {.action = "KickTree", .target = Target::Tree, .build = Build::Kick},
    Executor {.action = "PullSillyFaces", .target = Target::Villager, .build = Build::PullSillyFaces},
    Executor {.action = "Hurl",
              .target = Target::Stonable,
              .use = Target::ThrowingDamage,
              .notWhatItHolds = true,
              .build = Build::Hurl,
              .activity = Activity::Hurl},
    Executor {.action = "Stomp", .target = Target::Stompable, .build = Build::Stomp},
    Executor {.action = "Kick", .target = Target::Kickable, .build = Build::Kick},
    Executor {.action = "ShowCreatureYouHateHim", .target = Target::Creature, .build = Build::ShowHate},
    Executor {.action = "SitDown",
              .target = Target::Anything,
              .build = Build::SitDown,
              .activity = Activity::Sit,
              .recency = Recency::TwoSeconds},
    Executor {.action = "BeIdle", .build = Build::BeIdle, .activity = Activity::BeIdle},
    Executor {.action = "HangAroundAtHome", .build = Build::HangAround, .activity = Activity::HangAround},
    Executor {.action = "CommunicateState", .build = Build::ShowDesire, .activity = Activity::ShowDesire},
    Executor {.action = "WaveAtPlayer", .build = Build::FaceCameraEmote, .animation = animations::k_FriendlyWave},
    Executor {.action = "LookAtHand", .build = Build::LookAtHand},
    Executor {.action = "PointAtCamera", .build = Build::PointAtCamera},
    Executor {.action = "BePatheticToPlayer", .build = Build::BePathetic},
    Executor {.action = "HowlAtPlayer", .build = Build::Howl},
    Executor {.action = "RunAwayFromObject", .target = Target::Frightening, .build = Build::RunFromObject},
    Executor {.action = "BeFrightenedOnTheSpot", .build = Build::BeFrightened},
    Executor {.action = "BeSad", .target = Target::Anything, .build = Build::BeSad},
    Executor {.action = "Scratch", .build = Build::Emote, .animation = animations::k_Scratch, .recency = Recency::TwoSeconds},
    Executor {.action = "Shiver", .build = Build::Emote, .animation = animations::k_Cold},
    Executor {.action = "Sneeze", .build = Build::Emote, .animation = k_Sneeze},
    Executor {.action = "ShowHotness", .build = Build::Emote, .animation = animations::k_Hot},
    Executor {.action = "RunAwayFromPlayer", .build = Build::RunFromPlayer},
    Executor {.action = "ShowImpressiveAnimation",
              .target = Target::Villager,
              .build = Build::ShowOff,
              .recency = Recency::ShowedOff},
    Executor {.action = "DanceWithVillagers",
              .target = Target::DanceVillager,
              .build = Build::DanceWithVillagers,
              .danceType = creature_dance::k_ImpressDanceType},
    // The other dances with villagers. The playful dance with them and the story need a villager near the creature,
    // as the dance to impress does. The table keeps only the first 32 letters of a name.
    Executor {.action = "DancePlayfullyWithVillagersParti",
              .target = Target::DanceVillager,
              .build = Build::DanceWithVillagers,
              .danceType = creature_dance::k_PlayfulDanceType},
    Executor {.action = "TellVillagersAStory",
              .target = Target::DanceVillager,
              .build = Build::DanceWithVillagers,
              .danceType = creature_dance::k_StoryDanceType},
    Executor {.action = "PlayGameWithVillagers",
              .target = Target::Villager,
              .build = Build::DanceWithVillagers,
              .danceType = creature_dance::k_GameDanceType},
    Executor {.action = "DancePlayfullyWithVillagersWatch",
              .target = Target::Villager,
              .build = Build::DanceWithVillagers,
              .danceType = creature_dance::k_WatchedDanceType},
    Executor {.action = "DanceAmorouslyWithVillagers",
              .target = Target::Villager,
              .build = Build::DanceWithVillagers,
              .danceType = creature_dance::k_AmorousDanceType},
    Executor {.action = "PracticeDance", .build = Build::PracticeDance},
    Executor {.action = "Stroke", .target = Target::Strokeable, .build = Build::StrokeAndHeal},
    Executor {.action = "SmileAtFriend", .target = Target::CreatureFreeForJoint, .build = Build::SmileAt},
    Executor {.action = "HowlAtFriend", .target = Target::Creature, .build = Build::HowlAtFriend},
    Executor {.action = "PineForFriend", .target = Target::Creature, .build = Build::PineForFriend},
    Executor {.action = "LookForFriend", .target = Target::Creature, .build = Build::LookForFriend},
    Executor {.action = "InspectCreature", .target = Target::Creature, .build = Build::InspectCreature},
    Executor {.action = "Fight", .target = Target::FightableCreature, .build = Build::Fight},
    Executor {.action = "LookAtFlyingObject", .target = Target::Anything, .build = Build::LookAtFlyingObject},
    Executor {.action = "WaveAtFriend", .target = Target::CreatureFreeForJoint, .build = Build::WaveAtFriend},
    Executor {.action = "BeHappyWithFriend", .target = Target::CreatureFreeForJoint, .build = Build::BeHappyWithFriend},
    Executor {.action = "PooWithFriend", .target = Target::CreatureFreeForJoint, .build = Build::PooWithFriend},
    Executor {.action = "SitWithFriend", .target = Target::CreatureFreeForJoint, .build = Build::SitWithFriend},
    Executor {.action = "SleepWithFriend", .target = Target::CreatureFreeForJoint, .build = Build::SleepWithFriend},
    Executor {.action = "DrinkWithFriend", .target = Target::CreatureFreeForJoint, .build = Build::DrinkWithFriend},
    Executor {.action = "HelpTownWithFriend", .target = Target::CreatureFreeForJoint, .build = Build::HelpTownWithFriend},
    Executor {.action = "AttackWithFriend", .target = Target::CreatureFreeForJoint, .build = Build::AttackTownWithFriend},
    Executor {.action = "KissFriend", .target = Target::CreatureFreeForJoint, .build = Build::KissFriend},
    Executor {.action = "TalkToFriend", .target = Target::CreatureFreeForJoint, .build = Build::TalkToFriend},
    Executor {.action = "TellFriendAJoke", .target = Target::CreatureFreeForJoint, .build = Build::TellFriendAJoke},
    Executor {.action = "DanceWithFriend", .target = Target::CreatureFreeForJoint, .build = Build::DanceWithFriend},
    Executor {.action = "ArgueWithFriend", .target = Target::CreatureFreeForJoint, .build = Build::ArgueWithFriend},
    Executor {.action = "MopeAboutWithFriend", .target = Target::CreatureFreeForJoint, .build = Build::MopeAboutWithFriend},
    Executor {.action = "ShowOffToFriend", .target = Target::CreatureFreeForJoint, .build = Build::ShowOffToFriend},
    Executor {
        .action = "GetAttentionFromFriend", .target = Target::CreatureNotFreeForJoint, .build = Build::GetAttentionFromFriend},
    Executor {.action = "TellCreatureToSodOff", .target = Target::CreatureFreeForJoint, .build = Build::TellCreatureToSodOff},
    Executor {.action = "ExamineOtherCreatureWithFriend",
              .target = Target::CreatureFreeForJoint,
              .use = Target::Creature,
              .build = Build::ExamineOtherCreatureWithFriend},
    Executor {
        .action = "SitOnTopOfHillWithFriend", .target = Target::CreatureFreeForJoint, .build = Build::SitOnTopOfHillWithFriend},
    Executor {
        .action = "EatFromFieldWithFriend", .target = Target::CreatureFreeForJoint, .build = Build::EatFromFieldWithFriend},
    Executor {.action = "PlayThrowingGameWithFriend",
              .target = Target::CreatureFreeForJoint,
              .use = Target::Toy,
              .build = Build::PlayThrowingGameWithFriend},
    Executor {
        .action = "GiveFriendToy", .target = Target::CreatureFreeForJoint, .use = Target::Toy, .build = Build::GiveFriendToy},
    Executor {.action = "ShowFriendCitadel", .target = Target::CreatureFreeForJoint, .build = Build::ShowFriendCitadel},
    Executor {.action = "LookAtMountains", .build = Build::LookAtMountains},
    Executor {.action = "LookOutToSea", .build = Build::LookOutToSea, .leavesHome = true},
    // It dances alone on the nearest coast, by its map of the land
    Executor {.action = "DanceOnYourOwnByTheSea", .build = Build::DanceByTheSea},
    Executor {.action = "LookAtSun", .build = Build::LookAtSun},
    Executor {.action = "LookAtMoon", .build = Build::LookAtMoon},
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
    // To amuse a creature that seems friendly, it casts a spell on creatures that it picks at random
    Executor {.action = "CastAmusingSpellOnCreature", .target = Target::FriendlyCreature, .build = Build::CastAmusing},
    // The miracles that attack: the power-ups of lightning, the fireball and its power-ups, the explosion and its
    // power-ups, the tornado and the lightning storm
    Executor {.action = "CastLightningBoltPU1", .target = Target::Anything, .build = Build::CastLightning},
    Executor {.action = "CastLightningBoltPU2", .target = Target::Anything, .build = Build::CastLightning},
    Executor {.action = "CastFireball", .target = Target::Anything, .build = Build::CastFireball},
    Executor {.action = "CastFireballPU1", .target = Target::Anything, .build = Build::CastFireball},
    Executor {.action = "CastFireballPU2", .target = Target::Anything, .build = Build::CastFireball},
    Executor {.action = "CastExplosion", .target = Target::Anything, .build = Build::CastExplosion},
    Executor {.action = "CastExplosionPU1", .target = Target::Anything, .build = Build::CastAggressive},
    Executor {.action = "CastExplosionPU2", .target = Target::Anything, .build = Build::CastAggressive},
    Executor {.action = "CastTornado", .target = Target::Anything, .build = Build::CastAggressive},
    Executor {.action = "CastLightningStorm", .target = Target::Anything, .build = Build::CastAggressive},
    // Fire: putting it out with the water miracle, setting a thing alight with a burning thing, and the action of
    // starting a fire, which the game never carries out
    Executor {.action = "PutOutFireWithMagicWater", .target = Target::Burning, .build = Build::CastWater},
    Executor {.action = "SetFireToObject", .target = Target::Unburnt, .build = Build::SetFire},
    Executor {.action = "StartFire", .build = Build::Never},
    // The desire to follow the player copies the player's last deed again; the action itself makes no steps
    Executor {.action = "MimicPlayer", .build = Build::MimicPlayer},
    Executor {.action = "SleepAtHome", .build = Build::SleepAtHome, .activity = Activity::Sleep},
    Executor {.action = "WaveAtObject", .target = Target::Anything, .build = Build::WaveAt},
    Executor {.action = "PutDown", .build = Build::PutDown, .activity = Activity::PutDown},
    Executor {.action = "CastHealSpellPU1", .target = Target::HurtVillager, .build = Build::CastHelpful},
    // The helpful miracles cast the way the heal is: the food power-up, the shields at anything, and wood into a storage
    // pit. The power-up of the shield casts the plain shield.
    Executor {.action = "CastMagicFoodPU1", .target = Target::Anything, .build = Build::CastHelpful},
    Executor {.action = "CastMagicWood", .target = Target::StoragePit, .build = Build::CastHelpful},
    Executor {.action = "CastPhysicalShield", .target = Target::Anything, .build = Build::CastHelpful},
    Executor {.action = "CastShieldPU1", .target = Target::Anything, .build = Build::CastHelpful},
    // Food into a store, the shield at anything, water on itself while it burns, and a miracle to impress villagers
    Executor {.action = "CastMagicFood", .target = Target::BuiltFoodStore, .build = Build::CastFood},
    Executor {.action = "CastShield", .target = Target::Anything, .build = Build::CastShield},
    Executor {.action = "CastMagicWater", .build = Build::CastWaterOnItself},
    Executor {.action = "CastImpressiveSpell", .target = Target::Villager, .build = Build::CastToImpress},
    Executor {.action = "CastMagicForest", .target = Target::Villager, .build = Build::CastForest},
    // The miracles that a creature casts for a town: food by a worship site, wood by a building site or a workshop, the
    // shield at a built storage pit, and water on crops, trees and fields. The power-up of the water on crops casts
    // plain water, as all the water casts do.
    Executor {.action = "CastMagicFoodByWorshipSite", .target = Target::WorshipSite, .build = Build::CastFood},
    Executor {.action = "CastMagicWoodByBuildingSite", .target = Target::BuildingBeingBuilt, .build = Build::CastHelpful},
    Executor {.action = "CastMagicWoodByWorkshop",
              .target = Target::Workshop,
              .build = Build::CastHelpful,
              .castMagic = static_cast<uint32_t>(MagicType::Wood)},
    Executor {.action = "CastShieldAroundTown", .target = Target::BuiltStoragePit, .build = Build::CastShield},
    Executor {.action = "SprinkleMagicWaterOnCrops", .target = Target::Waterable, .build = Build::CastWater},
    Executor {.action = "SprinkleMagicWaterPU1OnCrops", .target = Target::Waterable, .build = Build::CastWater},
    Executor {.action = "WaterTree", .target = Target::AnyTree, .build = Build::CastWater},
    Executor {.action = "WaterField", .target = Target::DryField, .build = Build::CastWater},
    Executor {.action = "WaterTreeForTown",
              .target = Target::Anything,
              .use = Target::AnyTree,
              .build = Build::WaterTreeForTown},
    // The power-up of watering a tree for a town asks the creature to know the water power-up, but casts plain water
    Executor {.action = "WaterTreePU1ForTown",
              .target = Target::Anything,
              .use = Target::AnyTree,
              .build = Build::WaterTreeForTown},
    // The one-shot miracles that it casts: the one in its hand, or one that it picks up first. The game's data cuts
    // some of these names short.
    Executor {.action = "CastOneOffSpellInHandAggressive",
              .target = Target::Anything,
              .build = Build::CastOneShotInHand,
              .oneShot = OneShotKind::Aggressive},
    Executor {.action = "CastOneOffSpellInHandCompassiona",
              .target = Target::Anything,
              .build = Build::CastOneShotInHand,
              .oneShot = OneShotKind::Compassionate},
    Executor {.action = "CastOneOffSpellInHandPlayful",
              .target = Target::Creature,
              .build = Build::CastOneShotInHand,
              .oneShot = OneShotKind::Playful},
    Executor {.action = "PickUpAndCastOneOffSpellAggressi",
              .target = Target::Anything,
              .use = Target::OneShotAggressive,
              .build = Build::PickUpAndCastOneShot,
              .oneShot = OneShotKind::Aggressive},
    Executor {.action = "PickUpAndCastOneOffSpellCompassi",
              .target = Target::Anything,
              .use = Target::OneShotCompassionate,
              .build = Build::PickUpAndCastOneShot,
              .oneShot = OneShotKind::Compassionate},
    Executor {.action = "PickUpAndCastOneOffSpellPlayful",
              .target = Target::Creature,
              .use = Target::OneShotPlayful,
              .build = Build::PickUpAndCastOneShot,
              .oneShot = OneShotKind::Playful},
    Executor {.action = "DeadForever", .build = Build::DeadForever},
    Executor {.action = "PointOutHighlight", .target = Target::Anything, .build = Build::PointOutHighlight},
    // The creature shows the player that it saw a deed of the player
    Executor {.action = "NoticeHelpfulAction", .target = Target::Anything, .build = Build::Notice},
    Executor {.action = "NoticeAggressiveAction", .target = Target::Anything, .build = Build::Notice},
    Executor {.action = "NoticeAction", .target = Target::Anything, .build = Build::Notice},
    Executor {.action = "NoticeStealingAction", .target = Target::Anything, .build = Build::Notice},
    Executor {.action = "NoticePlayfulAction", .target = Target::Anything, .build = Build::Notice},
    Executor {.action = "SleepByObject",
              .target = Target::Anything,
              .build = Build::SleepByObject,
              .activity = Activity::Sleep},
    // What scripts force on creatures to stage their scenes
    Executor {.action = "LookForever", .target = Target::Anything, .build = Build::LookForever},
    Executor {.action = "LookButDontApproach", .target = Target::Anything, .build = Build::LookButDontApproach},
    Executor {.action = "LookAtCamera", .build = Build::LookAtCamera},
    Executor {.action = "PointAtObject", .target = Target::Anything, .build = Build::PointAtThing},
    // What a script's cut scene makes every creature free to think for itself do
    Executor {.action = "LookAtCameraInWideScreen", .build = Build::LookAtCameraInCutScene},
    // What it does with its player to get attention, or to tell the player what it thinks
    Executor {.action = "GoToHand", .build = Build::GoToHand},
    Executor {.action = "PointAtHand", .build = Build::PointAtHand},
    Executor {.action = "ShowPlayerHowNiceYouThinkHeIs", .build = Build::ShowHowNice},
    Executor {.action = "PrayToPlayer", .build = Build::PrayToPlayer},
    Executor {.action = "ThrowAtCamera", .target = Target::Pickable, .use = Target::Pickable, .build = Build::ThrowAtCamera},
    Executor {.action = "WatchPlayer", .build = Build::WatchPlayer},
    Executor {.action = "GoToMiddleOfScreen", .build = Build::GoToMiddleOfScreen},
    Executor {.action = "FollowPlayer", .build = Build::FollowPlayer},
    Executor {.action = "BeCrossWithPlayer", .build = Build::BeCrossWithPlayer},
    Executor {.action = "ShowPlayerAnObject", .use = Target::Pickable, .build = Build::ShowPlayerAnObject},
    Executor {.action = "BeSillyWithPlayer", .build = Build::BeSillyWithPlayer},
    // Getting better: resting, healing itself, and the one-shot miracles that restore health. The game's data cuts the
    // names of the last two short.
    Executor {.action = "RestToGetBetter", .build = Build::RestToGetBetter},
    Executor {.action = "GoHomeToRecover", .build = Build::GoHomeToRecover},
    Executor {.action = "HealHimself", .build = Build::HealItself},
    Executor {.action = "RestOnTheSpot", .build = Build::RestOnTheSpot},
    Executor {.action = "CastOneOffSpellInHandToRestoreHe",
              .build = Build::CastRestoreHealthInHand,
              .oneShot = OneShotKind::RestoreHealth},
    Executor {.action = "PickUpAndCastOneOffSpellToRestor",
              .use = Target::OneShotRestoreHealth,
              .build = Build::PickUpAndCastRestoreHealth,
              .oneShot = OneShotKind::RestoreHealth},
    // Home and the temple
    Executor {.action = "GoHome", .build = Build::GoHome},
    Executor {.action = "RunHome", .build = Build::RunHome},
    Executor {.action = "EnterCitadel", .build = Build::EnterTemple},
    Executor {.action = "PrayAtCitadel", .build = Build::PrayAtTemple},
    Executor {.action = "LookAtCitadel", .target = Target::TemplePart, .build = Build::LookAtTemple},
    // Helping a town: food and wood to its store, food and fish by a worship site, a cow in its store, a breeder, a
    // villager taken home or to a worship site, its totem raised or lowered
    Executor {.action = "GiveFoodFromFieldToStoragePit",
              .target = Target::BuiltStoragePit,
              .use = Target::FieldWithFood,
              .build = Build::GiveFieldFoodToStore},
    Executor {.action = "GiveFruitFromTreeToStoragePit",
              .target = Target::BuiltStoragePit,
              .use = Target::TreeBigEnough,
              .build = Build::GiveFruitToStore},
    Executor {.action = "GiveMagicFoodToStoragePit",
              .target = Target::BuiltStoragePit,
              .use = Target::MagicFoodPile,
              .build = Build::GiveMagicFoodToStore},
    Executor {.action = "GiveMagicWoodToStoragePit",
              .target = Target::BuiltStoragePit,
              .use = Target::MagicWoodPile,
              .build = Build::GiveMagicWoodToStore},
    Executor {.action = "PutFoodFromFieldByWorshipSite",
              .target = Target::WorshipSite,
              .use = Target::FieldWithFood,
              .build = Build::PutFieldFoodBySite},
    Executor {.action = "PutFishByWorshipSite", .target = Target::WorshipSite, .build = Build::PutFishBySite},
    Executor {.action = "DropCowInStoragePit",
              .target = Target::BuiltStoragePit,
              .use = Target::Cow,
              .build = Build::DropCowInStore},
    Executor {.action = "MakeBreederDisciple", .target = Target::BreederVillager, .build = Build::MakeBreeder},
    Executor {.action = "TakeVillagerHomeToSleep", .target = Target::VillagerFarFromHome, .build = Build::TakeVillagerHome},
    Executor {.action = "BringVillagersToWorshipSite",
              .target = Target::WorshipSite,
              .use = Target::VillagerNotWorshipping,
              .build = Build::BringVillagerToSite},
    Executor {.action = "RaiseTotemPole", .target = Target::PlayerTotem, .build = Build::RaiseTotem},
    Executor {.action = "LowerTotemPole", .target = Target::PlayerTotem, .build = Build::LowerTotem},
    // Playing with villagers
    Executor {.action = "PlayfullyFrightenVillagers", .target = Target::Villager, .build = Build::FrightenVillagers},
    Executor {.action = "PlayfullyInteractWithVillager", .target = Target::Villager, .build = Build::PlayWithVillager},
    Executor {.action = "PlayfullyKissVillager", .target = Target::Villager, .build = Build::KissVillager},
    // The places of its map of the land: hills, coasts, towns and the beach
    Executor {.action = "GoToHillAndLook", .build = Build::GoToHillAndLook},
    Executor {.action = "GoToHillAndSit", .build = Build::GoToHillAndSit},
    Executor {.action = "GoToHillAndWalkAlongRidge", .build = Build::WalkAlongRidge},
    Executor {.action = "ExploreCoastline", .build = Build::ExploreCoast},
    Executor {.action = "ExploreTowns", .build = Build::ExploreTowns},
    Executor {.action = "SitDownOnBeach", .build = Build::SitDownOnBeach, .leavesHome = true},
    // The odd actions. The desire to behave strangely chooses the last action. Nothing in the game chooses the other two,
    // but a script can.
    Executor {.action = "ShowLearntLesson", .build = Build::ShowLearntLesson},
    Executor {.action = "LookConfused", .build = Build::LookConfused},
    Executor {.action = "BehaveStrangely", .build = Build::BehaveStrangely},
    // The sacrifice: no desire chooses it, but a script or a copy of the player can make the creature do it
    Executor {.action = "Sacrifice", .target = Target::AnyLiving, .build = Build::Sacrifice},
    // The dance outside a worship site goes to the heart of the creature's own temple; the dance around a thing goes to
    // any thing
    Executor {.action = "DanceOutsideWorshipSite", .target = Target::WorshipSite, .build = Build::DanceOutsideTemple},
    Executor {.action = "DanceAroundArtefact", .target = Target::Anything, .build = Build::DanceAroundThing},
    // The desire to steal: the creature takes a thing that it can steal to its temple
    Executor {.action = "StealObjectAndTakeToCitadel", .target = Target::Stealable, .build = Build::StealAndPutByTemple},
    // The thefts that only a copy of the player starts: a stolen thing to a town of its player, and food or wood from a
    // farm or a store of another player into a store of its player. The theft from a farm makes the agenda of the gift
    // of food from a field.
    Executor {.action = "StealObjectAndTakeToTown", .target = Target::Stealable, .build = Build::StealAndPutInTown},
    Executor {.action = "StealFoodFromFarm",
              .target = Target::OwnStoragePit,
              .use = Target::OthersField,
              .build = Build::GiveFieldFoodToStore},
    Executor {.action = "StealFoodFromStoragePit",
              .target = Target::OwnStoragePit,
              .use = Target::OthersStoragePit,
              .build = Build::StealFoodFromStore},
    Executor {.action = "StealWoodFromStoragePit",
              .target = Target::OwnStoragePit,
              .use = Target::OthersStoragePit,
              .build = Build::StealWoodFromStore},
};

/// The actions of the game's table that need a thing to use, by their names there
constexpr std::array<std::string_view, 37> k_NeedThingToUse {
    "StoneAndEat",
    "Hurl",
    "ShowPlayerAnObject",
    "GiveFoodFromFieldToStoragePit",
    "GiveFruitFromTreeToStoragePit",
    "GiveMagicFoodToStoragePit",
    "GiveWoodFromTreeToStoragePit",
    "GiveMagicWoodToStoragePit",
    "HelpRepairHouse",
    "BringToTown",
    "GiveToCreature",
    "ThrowAtCamera",
    "StartFire",
    "BuildHome",
    "GiveFoodToCreature",
    "PlayThrowingGameWithPlayer",
    "PickUpAndCastOneOffSpellAggressi",
    "PickUpAndCastOneOffSpellCompassi",
    "PickUpAndCastOneOffSpellPlayful",
    "PickUpAndCastOneOffSpellToRestor",
    "PlayThrowingGameWithFriend",
    "PutFoodFromFieldByWorshipSite",
    "GiveWoodFromTreeToBuildingSite",
    "PlantTree",
    "ThrowToImpress",
    "GiveWoodFromTreeToWorkshop",
    "ExamineOtherCreatureWithFriend",
    "GiveFriendToy",
    "SetFireToObject",
    "DropCowInStoragePit",
    "ThrowBallAtObject",
    "BringVillagersToWorshipSite",
    "StealFoodFromFarm",
    "StealFoodFromStoragePit",
    "StealWoodFromStoragePit",
    "WaterTreeForTown",
    "WaterTreePU1ForTown",
};

/// The trip to the shoal to bring food out of the sea, none when it already has food in its hand; it walks straight to the
/// shoal
std::optional<creature_mind::FishingTrip> TripFor(const Situation::Fishing& fishing)
{
	if (fishing.holdingFood)
	{
		return std::nullopt;
	}
	return creature_mind::FishingTrip {.shoal = fishing.shoal, .arriveWithin = fishing.arriveWithin};
}

/// The agenda of a town help action (see creature_town_help)
std::optional<std::vector<creature_mind::Step>> TownHelpAgenda(const Executor& executor, std::optional<uint32_t> object,
                                                               glm::vec2 objectPoint, const Situation& situation,
                                                               const creature_mind::Random& random)
{
	namespace help = creature_town_help;
	const uint32_t thing = object.value_or(0);
	switch (executor.build)
	{
	case Build::GiveFieldFoodToStore:
	case Build::GiveFruitToStore:
	case Build::GiveMagicFoodToStore:
	case Build::GiveMagicWoodToStore:
	{
		// It takes a handful unless its hand holds what it gives: food it can eat, or a tree for wood
		const bool wood = executor.build == Build::GiveMagicWoodToStore;
		std::optional<help::HandfulSource> take;
		if (!(wood ? situation.holdingTree : situation.holdingEatable))
		{
			if (!situation.instrument.has_value())
			{
				return std::nullopt;
			}
			take = help::HandfulSource {.thing = *situation.instrument, .point = situation.instrumentPoint};
			take->within = help::ThingReach(situation.radius, situation.instrumentRadius);
			if (executor.build == Build::GiveFieldFoodToStore)
			{
				take->within = help::FieldReach(situation.height);
			}
			else if (executor.build == Build::GiveFruitToStore)
			{
				take->look = creature_mind::ObjectOrder::Handful::Fruit;
			}
			else if (wood)
			{
				take->look = help::MagicWoodLook(random);
			}
		}
		return help::GiveHandfulToStore(take, thing, situation.height);
	}
	case Build::PutFieldFoodBySite:
	{
		// It needs a field to take food from, and the place where the site keeps its food
		if (!situation.instrument.has_value() || !situation.siteFood.has_value())
		{
			return std::nullopt;
		}
		std::optional<help::HandfulSource> take;
		if (!situation.holdingEatable)
		{
			take = help::HandfulSource {.thing = *situation.instrument, .point = situation.instrumentPoint};
		}
		return help::PutFieldFoodBySite(take, *situation.siteFood, situation.height);
	}
	case Build::PutFishBySite:
		return creature_mind::TakeFishTo(TripFor(*situation.fishing), objectPoint, situation.height);
	case Build::DropCowInStore:
		// It needs a cow to use, also when it holds one
		if (!situation.instrument.has_value())
		{
			return std::nullopt;
		}
		return help::DropCowInStore(situation.holdingCow ? std::nullopt : situation.instrument, thing);
	case Build::MakeBreeder:
		return help::MakeBreeder(thing);
	case Build::TakeVillagerHome:
		if (!situation.villagerHome.has_value())
		{
			return std::nullopt;
		}
		return help::TakeVillagerHome(situation.holdingVillager ? std::nullopt : object, *situation.villagerHome);
	case Build::BringVillagerToSite:
		if (!situation.instrument.has_value())
		{
			return std::nullopt;
		}
		return help::BringVillagerToSite(situation.holdingVillager ? std::nullopt : situation.instrument, thing);
	case Build::RaiseTotem:
	case Build::LowerTotem:
		return help::MoveTotem(thing, executor.build == Build::RaiseTotem);
	case Build::FrightenVillagers:
		return help::FrightenVillagers(thing, situation.height);
	case Build::PlayWithVillager:
		return help::PlayWithVillager(thing, situation.height, random);
	case Build::KissVillager:
		return help::KissVillager(thing, situation.height);
	default:
		break;
	}
	return std::nullopt;
}

/// The agenda of an action that uses the creature's map of the land. Without the places that the agenda needs, the
/// action fails.
std::optional<std::vector<creature_mind::Step>>
ExplorationAgenda(Build build, std::optional<uint32_t> object, const Situation& situation, const creature_mind::Random& random)
{
	if (!situation.explorePlace.has_value())
	{
		return std::nullopt;
	}
	const auto place = *situation.explorePlace;
	switch (build)
	{
	case Build::GoToHillAndLook:
		return creature_exploration::GoToHillAndLook(place, situation.height);
	case Build::GoToHillAndSit:
		return creature_exploration::GoToHillAndSit(place, situation.height);
	case Build::WalkAlongRidge:
		return creature_exploration::GoToHillAndWalkAlongRidge(place, situation.height);
	case Build::ExploreCoast:
	case Build::ExploreTowns:
		return creature_exploration::Explore(place, situation.height);
	case Build::LookAtMountains:
		return creature_exploration::LookAtMountains(place, random);
	default:
		break;
	}
	if (!situation.exploreSea.has_value())
	{
		return std::nullopt;
	}
	const auto sea = *situation.exploreSea;
	switch (build)
	{
	case Build::SitDownOnBeach:
		return creature_exploration::SitDownOnBeach(place, sea, situation.height, random);
	case Build::LookOutToSea:
		return creature_exploration::LookOutToSea(place, sea, situation.height, random);
	case Build::ThrowInTheSea:
		// It picks the thing up first, unless its hand already holds something
		if (!object.has_value())
		{
			return std::nullopt;
		}
		return creature_exploration::ThrowInTheSea(situation.handFull ? std::nullopt : object, place, sea, situation.height);
	case Build::DanceByTheSea:
		return creature_exploration::DanceByTheSea(place, sea, situation.height);
	default:
		return std::nullopt;
	}
}
} // namespace

bool creature_plan_actions::OneShot::Is(OneShotKind kind) const
{
	switch (kind)
	{
	case OneShotKind::Aggressive:
		return aggressive;
	case OneShotKind::Compassionate:
		return compassionate;
	case OneShotKind::Playful:
		return playful;
	case OneShotKind::RestoreHealth:
		return restoresHealth;
	}
	return false;
}

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
	switch (executor.build)
	{
	case Build::CastLightning:
	case Build::CastHelpful:
	case Build::CastPlayful:
	case Build::CastAggressive:
	case Build::CastExplosion:
	case Build::CastFireball:
	case Build::CastWater:
	case Build::CastFood:
	case Build::CastShield:
	case Build::CastWaterOnItself:
	case Build::CastToImpress:
	case Build::CastAmusing:
	case Build::CastForest:
	case Build::WaterTreeForTown:
	case Build::HealItself:
		return true;
	default:
		return false;
	}
}

bool creature_plan_actions::NeedsThingToUse(std::string_view action)
{
	return std::ranges::find(k_NeedThingToUse, action) != k_NeedThingToUse.end();
}

namespace
{
/// The joint actions with a friend. Without what the action needs (the friend, a clear place, a coast and a sea, or a
/// town with a storage pit), the creature cannot do it.
std::optional<std::vector<creature_mind::Step>> FriendsAgenda(const Executor& executor, uint32_t friendly,
                                                              const Situation& situation, const creature_mind::Random& random)
{
	if (!situation.friendly.has_value())
	{
		return std::nullopt;
	}
	const creature_mind::Friends friends {
	    .self = situation.self,
	    .friendly = friendly,
	    .selfPoint = situation.position,
	    .friendPoint = situation.friendly->point,
	    .height = situation.height,
	    .friendHeight = situation.friendly->height,
	};
	switch (executor.build)
	{
	case Build::BeHappyWithFriend:
		return creature_mind::BeHappyWithFriend(friends);
	case Build::PooWithFriend:
		return creature_mind::PooWithFriend(friends, random);
	case Build::SitWithFriend:
	case Build::SleepWithFriend:
	{
		if (!situation.clearArea.has_value())
		{
			return std::nullopt;
		}
		const bool sit = executor.build == Build::SitWithFriend;
		const float radius = sit ? creature_mind::SitTogetherRadius(friends.height, friends.friendHeight)
		                         : creature_mind::SleepTogetherRadius(friends.height, friends.friendHeight);
		return sit ? creature_mind::SitWithFriend(friends, *situation.clearArea, radius, random)
		           : creature_mind::SleepWithFriend(friends, *situation.clearArea, radius, random);
	}
	case Build::DrinkWithFriend:
		if (!situation.explorePlace.has_value() || !situation.exploreSea.has_value())
		{
			return std::nullopt;
		}
		return creature_mind::DrinkWithFriend(friends, *situation.explorePlace, *situation.exploreSea, random);
	case Build::HelpTownWithFriend:
	case Build::AttackTownWithFriend:
		if (!situation.friendsTown.has_value())
		{
			return std::nullopt;
		}
		return creature_mind::TownWithFriend(friends, *situation.friendsTown,
		                                     executor.build == Build::HelpTownWithFriend ? creature_desires::Desire::Compassion
		                                                                                 : creature_desires::Desire::Anger);
	case Build::KissFriend:
		if (!situation.kissingDistance.has_value())
		{
			return std::nullopt;
		}
		return creature_mind::KissFriend(friends, *situation.kissingDistance,
		                                 (situation.standing.y + situation.friendly->position.y) * 0.5f, random);
	case Build::TalkToFriend:
		return creature_mind::TalkToFriend(situation.self, friendly, situation.height, random);
	case Build::TellFriendAJoke:
		return creature_mind::TellFriendAJoke(situation.self, friendly, situation.height, random);
	case Build::DanceWithFriend:
		return creature_mind::DanceWithFriend(situation.self, friendly, situation.height);
	case Build::ArgueWithFriend:
		return creature_mind::ArgueWithFriend(situation.self, friendly, situation.height, random);
	case Build::MopeAboutWithFriend:
		return creature_mind::MopeAboutWithFriend(situation.self, friendly, situation.height, random);
	case Build::ShowOffToFriend:
		if (!situation.homePlace.has_value())
		{
			return std::nullopt;
		}
		return creature_mind::ShowOffToFriend(situation.self, friendly, situation.height, *situation.homePlace, random);
	case Build::TellCreatureToSodOff:
		return creature_mind::TellCreatureToSodOff(situation.self, friendly, situation.standing, situation.friendly->position,
		                                           situation.friendly->height, random);
	case Build::ExamineOtherCreatureWithFriend:
		if (!situation.instrument.has_value())
		{
			return std::nullopt;
		}
		return creature_mind::ExamineOtherCreatureWithFriend(friendly, *situation.instrument, random);
	case Build::SitOnTopOfHillWithFriend:
		if (!situation.clearArea.has_value())
		{
			return std::nullopt;
		}
		return creature_mind::SitOnTopOfHillWithFriend(
		    friends, *situation.clearArea, creature_mind::HillTogetherRadius(situation.radius, situation.friendly->radius),
		    situation.seesFriend, random);
	case Build::EatFromFieldWithFriend:
		if (!situation.friendsField.has_value())
		{
			return std::nullopt;
		}
		return creature_mind::EatFromFieldWithFriend(friends, situation.friendsField->pile, situation.friendsField->point);
	case Build::PlayThrowingGameWithFriend:
		if (!situation.instrument.has_value())
		{
			return std::nullopt;
		}
		return creature_mind::PlayThrowingGameWithFriend(friends, *situation.instrument, situation.standing,
		                                                 situation.friendly->position, situation.handFull);
	case Build::GiveFriendToy:
		if (!situation.instrument.has_value())
		{
			return std::nullopt;
		}
		return creature_mind::GiveFriendToy(situation.self, friendly, *situation.instrument, situation.handFull, random);
	case Build::ShowFriendCitadel:
		if (!situation.templeHeart.has_value() || !situation.head.has_value())
		{
			return std::nullopt;
		}
		return creature_mind::ShowFriendCitadel(
		    friends, {.point = situation.templeHeart->point, .radius = situation.templeHeart->radius}, situation.radius,
		    situation.friendly->radius, *situation.head, situation.seesFriend, random);
	default:
		return std::nullopt;
	}
}

/// The agendas that bring a thing home. Without a home, the creature cannot do them.
std::optional<std::vector<creature_mind::Step>> BringHomeAgenda(const Executor& executor, uint32_t object,
                                                                glm::vec2 objectPoint, const Situation& situation)
{
	namespace bring = creature_bring_home;
	if (!situation.home.has_value())
	{
		return std::nullopt;
	}
	switch (executor.build)
	{
	case Build::BringHome:
		// It picks up the thing only when its hand is empty
		return bring::BringHome(situation.handFull ? std::nullopt : std::optional(object), object, situation.thingLiving,
		                        *situation.home, situation.height);
	case Build::BringFoodHome:
		// It picks up the food unless it holds food already
		return bring::BringFoodHome(situation.holdingEatable ? std::nullopt : std::optional(object), situation.thingLiving,
		                            *situation.home, situation.height);
	case Build::TakeFieldFoodHome:
	{
		// It takes wheat from the field unless it holds food already
		std::optional<bring::Field> take;
		if (!situation.holdingEatable)
		{
			take = bring::Field {.field = object, .point = objectPoint};
		}
		return bring::TakeFoodFromFieldHome(take, *situation.home, situation.height);
	}
	default:
		return std::nullopt;
	}
}
} // namespace

bool creature_plan_actions::Possible(const Executor& executor, const Situation& situation)
{
	if (executor.leavesHome && situation.stage < k_LeaveHomeStage)
	{
		return false;
	}
	switch (executor.build)
	{
	case Build::Sacrifice:
	case Build::StealAndPutByTemple:
		// Only a creature whose player has a temple
		return situation.hasTemple;
	case Build::Drink:
		// The creature can always try to drink. When it finds no place to drink, the action fails as it starts.
		return true;
	case Build::FishAndEat:
		return situation.fishing.has_value() && !situation.fishing->holdingFood && situation.fishing->standAt.has_value();
	case Build::GiveFishToStore:
		return situation.fishing.has_value();
	case Build::TakeFishHome:
		return situation.fishing.has_value() && situation.home.has_value();
	case Build::PutFishBySite:
		return situation.fishing.has_value();
	case Build::SleepAtHome:
		// Only with a temple, and not too far from home
		return situation.home.has_value() && situation.homeDistance.has_value() &&
		       creature_mind::NotTooFarFromHome(situation.hasTemple, *situation.homeDistance);
	case Build::PrayAtTemple:
		return situation.home.has_value() && situation.homeDistance.has_value() &&
		       creature_mind::NotTooFarFromHome(situation.hasTemple, *situation.homeDistance);
	case Build::GoHome:
		return situation.home.has_value() && situation.homeDistance.has_value() &&
		       creature_mind::FarFromHome(*situation.homeDistance);
	case Build::GoHomeToRecover:
	case Build::RunHome:
		return situation.home.has_value();
	case Build::RestToGetBetter:
	case Build::RestOnTheSpot:
		return situation.nothingFrighteningNear;
	case Build::CastRestoreHealthInHand:
		// Only with a one-shot miracle that restores health in its hand
		return situation.heldOneShot.has_value() && situation.heldOneShot->Is(executor.oneShot);
	case Build::PooAtHome:
		return situation.home.has_value() &&
		       !(gutils::GetDistanceInMetres(situation.position, *situation.home) > creature_mind::k_NearHomeToPoo);
	case Build::SleepByObject:
		// Told by a script, it needs a clear place by the thing or a home. Left to itself, the conditions are below.
		return !situation.controlledByScript || situation.clearArea.has_value() || situation.home.has_value();
	case Build::FaceCameraEmote:
	case Build::ShowDesire:
	case Build::Howl:
	case Build::BePathetic:
		return situation.camera.has_value();
	case Build::RunFromPlayer:
		return situation.playerHand.has_value();
	case Build::PointAtCamera:
	case Build::PointOutHighlight:
		return situation.eye.has_value();
	case Build::Notice:
		// It turns to the camera only when it has a player
		return !situation.hasPlayer || situation.eye.has_value();
	case Build::Fight:
		return situation.healthyEnoughToFight;
	// The joint actions with a friend that need a desire of the creature's own to be a little strong
	case Build::DrinkWithFriend:
		return situation.water > k_SlightDesire;
	case Build::PooWithFriend:
		return situation.pooDesire > k_SlightDesire;
	case Build::SleepWithFriend:
		return situation.tiredness > k_SlightDesire;
	case Build::BeHappyWithFriend:
		return situation.sadness < k_SlightDesire;
	// Helping the nearest town with a friend when it does not want to attack the town more than to help it
	case Build::HelpTownWithFriend:
		return situation.attackNearestTown.has_value() && !*situation.attackNearestTown;
	case Build::AttackTownWithFriend:
		return situation.attackNearestTown.has_value() && *situation.attackNearestTown;
	case Build::CastOneShotInHand:
		// Only with a one-shot miracle of the action's kind in its hand
		return situation.heldOneShot.has_value() && situation.heldOneShot->Is(executor.oneShot);
	case Build::LookAtHand:
		// Only while the player's hand moves
		return situation.handMoving;
	case Build::GoToHand:
		return situation.awayFromHand;
	case Build::GoToMiddleOfScreen:
		return creature_player_attention::AwayFromMiddleOfScreen(situation.hasPlayer, situation.lookedAt);
	case Build::FollowPlayer:
	case Build::ShowPlayerAnObject:
		return creature_player_attention::FarFromCamera(situation.lookedAt);
	case Build::BeCrossWithPlayer:
		return creature_player_attention::IsCross(situation.attitudeToPlayer);
	default:
		return true;
	}
}

bool creature_plan_actions::Allowed(const Executor& executor, const Situation& situation)
{
	switch (executor.build)
	{
	case Build::LookAtSun:
		return creature_emote::SunVisible(situation.night);
	case Build::LookAtMoon:
		return creature_emote::MoonVisible(situation.night);
	case Build::SleepAtHome:
		return creature_emote::NotTooFarFromHome(situation.hasTemple, situation.homeDistance);
	case Build::Sleep:
		return creature_emote::LeashedOrTooFarFromHome(situation.leashed, situation.hasTemple, situation.homeDistance);
	case Build::SleepByObject:
		// Only on the leash or far from home, and the thing must have nothing frightening near it. The game measures
		// that from the creature, not from the thing, so the test does not depend on the thing.
		return creature_emote::LeashedOrTooFarFromHome(situation.leashed, situation.hasTemple, situation.homeDistance) &&
		       situation.nothingFrighteningNear;
	case Build::RunFromPlayer:
		return creature_emote::PlayerNearbyAndFrightening(situation.handDistance, situation.attitudeToPlayer);
	default:
		return true;
	}
}

bool creature_plan_actions::DoneTooRecently(const Executor& executor, uint32_t turnsSinceDone, uint32_t lastShowOffTurn,
                                            uint32_t turn, uint32_t turnsPerSecond)
{
	switch (executor.recency)
	{
	case Recency::TwoSeconds:
		return creature_emote::DoneWithin(turnsSinceDone, turnsPerSecond, creature_emote::k_RecentlySeconds);
	case Recency::ShowedOff:
		return creature_emote::ShowedOffRecently(lastShowOffTurn, turn);
	case Recency::None:
		break;
	}
	return false;
}

bool creature_plan_actions::Buildable(const Executor& executor, const Situation& situation)
{
	switch (executor.build)
	{
	case Build::SleepAtHome:
	case Build::PrayAtTemple:
	case Build::GoHome:
		// The agendas of these actions need only the home
		return situation.home.has_value();
	default:
		return Possible(executor, situation);
	}
}

std::optional<std::vector<creature_mind::Step>> creature_plan_actions::Agenda(const Executor& executor,
                                                                              std::optional<uint32_t> object,
                                                                              glm::vec2 objectPoint, const Situation& situation,
                                                                              const creature_mind::Random& random,
                                                                              const std::optional<CastInfo>& cast)
{
	if (!Buildable(executor, situation) || (executor.target != Target::None && !object.has_value()) ||
	    (IsCast(executor) && !cast.has_value()))
	{
		return std::nullopt;
	}
	switch (executor.build)
	{
	case Build::Eat:
		return creature_mind::Eat(*object, situation.holdingEatable);
	case Build::EatAlive:
		return creature_mind::EatAlive(*object, situation.holdingEatable, random);
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
		// With something it can eat in its hand already, it makes no steps, and so decides again at once
		if (situation.holdingEatable)
		{
			return std::nullopt;
		}
		return creature_mind::EatFromPile(*object, objectPoint, situation.thingRadius, situation.radius);
	case Build::EatFromStoragePit:
		// The creature eats from the food pile of the pit. A pit with no food pile gives no agenda.
		if (!situation.storeFood.has_value())
		{
			return std::nullopt;
		}
		return creature_mind::EatFromStoragePit(situation.storeFood->pile, situation.storeFood->point, situation.height);
	case Build::EatFromField:
	case Build::EatFromTree:
		// TODO(creature-eat-from-stores): When the hand holds food, the game makes no steps for these two actions. The
		// result of that empty agenda is not known, thus the creature does not do the action.
		if (situation.holdingEatable)
		{
			return std::nullopt;
		}
		return executor.build == Build::EatFromField
		           ? creature_mind::EatFromField(*object, objectPoint, situation.height)
		           : creature_mind::EatFromTree(*object, objectPoint, situation.thingRadius, situation.radius);
	case Build::Sleep:
		// At a clear place near it; without one it sleeps at home, as the action to sleep at home does
		if (situation.clearArea.has_value())
		{
			return creature_emote::SleepAtClearPlace(*situation.clearArea, situation.radius, random);
		}
		if (situation.home.has_value())
		{
			return creature_mind::SleepAtHome(*situation.home, situation.height, random);
		}
		// TODO(creature-rest-and-home): the game keeps the place where a creature without a temple was made as its
		// home, and sleeps there. openblack does not keep that place, so such a creature sleeps where it stands.
		return creature_mind::Sleep(random);
	case Build::Poo:
		return creature_mind::Poo(random,
		                          object.has_value() ? std::optional(creature_mind::PooBy {.object = *object}) : std::nullopt);
	case Build::Fart:
		// The creature farts by the thing that the planner gives it, as for a poo. Without a thing, it farts by itself.
		return creature_mind::Fart(random, creature_mind::PooBy {.object = object.value_or(situation.self),
		                                                         .itself = object.value_or(situation.self) == situation.self});
	case Build::Puke:
		return creature_mind::Puke();
	case Build::Drink:
		if (!situation.drink.has_value())
		{
			return std::nullopt;
		}
		return creature_drink::Drink(*situation.drink, situation.height);
	case Build::FishAndEat:
		return creature_mind::FishAndEat(*situation.fishing->standAt, situation.fishing->arriveWithin);
	case Build::GiveFishToStore:
		return creature_mind::GiveFishToStore(TripFor(*situation.fishing), *object, situation.fishing->height);
	case Build::TakeFishHome:
		return creature_mind::TakeFishTo(TripFor(*situation.fishing), *situation.home, situation.fishing->height);
	case Build::BringHome:
	case Build::BringFoodHome:
	case Build::TakeFieldFoodHome:
		return BringHomeAgenda(executor, *object, objectPoint, situation);
	case Build::ExamineByPickingUp:
		return creature_mind::ExamineByPickingUp(*object, situation.handFull, random);
	case Build::ExamineByLooking:
		return creature_mind::ExamineByLooking(*object, situation.height, situation.thingHeight);
	case Build::ExamineByFollowing:
		return creature_mind::ExamineByFollowing(*object);
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
	case Build::PooAtHome:
		return creature_mind::PooAtHome(*situation.home, situation.height, situation.self, random);
	case Build::SleepByObject:
		// Without a clear place by the thing, it sleeps at home instead
		if (situation.clearArea.has_value())
		{
			return creature_mind::SleepByObject(*situation.clearArea, situation.radius, random);
		}
		if (!situation.home.has_value())
		{
			return std::nullopt;
		}
		return creature_mind::SleepAtHome(*situation.home, situation.height, random);
	case Build::SmileAt:
		return creature_mind::SmileAt(*object, situation.height, situation.chance, random);
	case Build::ShowOff:
		return creature_mind::ShowImpressiveAnimation(object, situation.height, random);
	case Build::WaveAt:
		return creature_mind::WaveAt(*object, situation.height);
	case Build::InspectCreature:
		return creature_mind::InspectCreature(*object, situation.height);
	case Build::Fight:
		return std::vector {creature_mind::Step {.kind = creature_mind::Step::Kind::Fight, .object = *object}};
	case Build::LookAtFlyingObject:
		return creature_mind::LookAtFlyingObject(*object, random);
	case Build::LookAtHand:
		return creature_mind::LookAtHand();
	case Build::BeFrightened:
		return creature_mind::BeFrightenedOnTheSpot();
	case Build::PutDown:
		return creature_mind::PutDown();
	case Build::DeadForever:
		return creature_mind::DeadForever();
	case Build::PointOutHighlight:
		return creature_mind::PointOutHighlight(*object, situation.eyeDistance);
	case Build::Notice:
	{
		const auto kind = creature_mind::NoticeKindOf(executor.action);
		if (!kind.has_value())
		{
			return std::nullopt;
		}
		return creature_mind::Notice(*kind, *object, situation.handFull, situation.hasPlayer,
		                             situation.eye.value_or(glm::vec3(0.0f)), random);
	}
	case Build::Destroy:
		return creature_mind::DestroyThing(*object);
	case Build::Stomp:
		return creature_mind::Stomp(*object, random);
	case Build::StompAndEat:
		return creature_mind::StompAndEat(*object, situation.holdingEatable);
	case Build::StoneAndEat:
		// With nothing to throw, it can't
		if (!situation.instrument.has_value())
		{
			return std::nullopt;
		}
		return creature_mind::StoneAndEat(*situation.instrument, *object, situation.height, situation.holdingRock);
	case Build::Kick:
		return creature_mind::KickThing(*object);
	case Build::ShowHate:
		return creature_mind::ShowCreatureYouHateHim(*object, situation.height, situation.thingHeight, random);
	case Build::SitDown:
		// Told by a script where to sit, it goes to the thing itself. Left to itself, it sits at a clear place about the
		// thing, and gives up when there is none.
		if (situation.controlledByScript)
		{
			return creature_emote::SitDownBy(*object, objectPoint, situation.radius, situation.sitExtraSeconds);
		}
		if (!situation.clearArea.has_value())
		{
			return std::nullopt;
		}
		return creature_emote::SitDownBy(*object, *situation.clearArea, situation.radius, situation.sitExtraSeconds);
	case Build::BeIdle:
		return creature_mind::BeIdle(random);
	case Build::HangAround:
		return creature_mind::HangAround(situation.hangAround, situation.turnSeconds, random);
	case Build::ShowDesire:
		return creature_mind::Communicate(*situation.camera);
	case Build::Emote:
		return creature_mind::Emote(executor.animation);
	case Build::FaceCameraEmote:
		return creature_mind::FaceAndEmote(*situation.camera, executor.animation);
	case Build::ApproachEmote:
		return creature_mind::ApproachAndEmote(*object, executor.animation);
	case Build::WaveAtFriend:
		return creature_mind::WaveAtFriend(situation.self, *object);
	case Build::BeHappyWithFriend:
	case Build::PooWithFriend:
	case Build::SitWithFriend:
	case Build::SleepWithFriend:
	case Build::DrinkWithFriend:
	case Build::HelpTownWithFriend:
	case Build::AttackTownWithFriend:
	case Build::KissFriend:
	case Build::TalkToFriend:
	case Build::TellFriendAJoke:
	case Build::DanceWithFriend:
	case Build::ArgueWithFriend:
	case Build::MopeAboutWithFriend:
	case Build::ShowOffToFriend:
	case Build::TellCreatureToSodOff:
	case Build::ExamineOtherCreatureWithFriend:
	case Build::SitOnTopOfHillWithFriend:
	case Build::EatFromFieldWithFriend:
	case Build::PlayThrowingGameWithFriend:
	case Build::GiveFriendToy:
	case Build::ShowFriendCitadel:
		return FriendsAgenda(executor, *object, situation, random);
	case Build::GetAttentionFromFriend:
		return creature_mind::GetAttentionFromFriend(*object, random);
	case Build::HowlAtFriend:
		return creature_mind::HowlAtFriend(*object);
	case Build::PineForFriend:
		return creature_mind::PineForFriend(*object);
	case Build::LookForFriend:
		return creature_mind::LookForFriend(*object, situation.height);
	case Build::DanceWithVillagers:
		// With no clear place about the villager the action fails
		if (!situation.danceArea.has_value())
		{
			return std::nullopt;
		}
		return creature_dance::Agenda({.villager = *object,
		                               .place = *situation.danceArea,
		                               .type = executor.danceType,
		                               .fewestDancers = situation.fewestDancers,
		                               .beats = situation.danceBeats,
		                               .height = situation.height});
	case Build::PracticeDance:
		return creature_dance::PracticeDance();
	case Build::RunFromObject:
		return creature_emote::RunAway({.point = objectPoint, .object = object}, random);
	case Build::RunFromPlayer:
		return creature_emote::RunAway({.point = {situation.playerHand->x, situation.playerHand->z}}, random);
	case Build::Howl:
		return creature_emote::HowlAtPlayer(*situation.camera);
	case Build::BeSad:
		return creature_emote::BeSad(situation.self, situation.turnsPerSecond, random);
	case Build::PullSillyFaces:
		return creature_emote::PullSillyFaces(*object, situation.height, random);
	case Build::BePathetic:
		return creature_emote::BePathetic(*situation.camera, situation.height);
	case Build::StrokeAndHeal:
		return creature_emote::StrokeAndHeal(*object, situation.holdingStrokeable);
	case Build::LookAtSun:
		return creature_emote::LookAtSky(creature_emote::k_SunPoint, false, random);
	case Build::LookAtMoon:
		return creature_emote::LookAtSky(situation.moon, situation.evil, random);
	case Build::LookAbout:
		return creature_mind::LookAbout(k_GazeSeconds);
	case Build::GoToHillAndLook:
	case Build::GoToHillAndSit:
	case Build::WalkAlongRidge:
	case Build::ExploreCoast:
	case Build::ExploreTowns:
	case Build::SitDownOnBeach:
	case Build::LookAtMountains:
	case Build::LookOutToSea:
	case Build::ThrowInTheSea:
	case Build::DanceByTheSea:
		return ExplorationAgenda(executor.build, object, situation, random);
	case Build::CastLightning:
		return creature_mind::CastAt(creature_mind::CastStyle::Lightning, cast->magicType, cast->gesture, *object, cast->height,
		                             random);
	case Build::CastHelpful:
		return creature_mind::CastAt(creature_mind::CastStyle::Helpful, cast->magicType, cast->gesture, *object, cast->height,
		                             random);
	case Build::CastPlayful:
		return creature_mind::CastAt(creature_mind::CastStyle::Playful, cast->magicType, cast->gesture, *object, cast->height,
		                             random);
	case Build::CastAggressive:
		return creature_mind::CastAt(creature_mind::CastStyle::Aggressive, cast->magicType, cast->gesture, *object,
		                             cast->height, random);
	case Build::CastExplosion:
		return creature_mind::CastAt(creature_mind::CastStyle::Explosion, cast->magicType, cast->gesture, *object, cast->height,
		                             random);
	case Build::CastFireball:
		// With no clear area near it, the creature can't cast a fireball
		if (!cast->clearArea.has_value())
		{
			return std::nullopt;
		}
		return creature_mind::CastFireballAt(cast->magicType, cast->gesture, *object, cast->targetLiving, *cast->clearArea,
		                                     cast->height, random);
	case Build::CastWater:
		return creature_mind::PutOutFireWithWater(*object, cast->height);
	case Build::CastFood:
		return creature_mind::CastFoodAt(cast->magicType, cast->gesture, *object);
	case Build::CastShield:
		// When the plan is about nothing, the creature idles and does not cast the shield
		if (!situation.activity.has_value())
		{
			return creature_mind::BeIdle(random);
		}
		return creature_mind::CastShieldAt(cast->magicType, *object);
	case Build::CastWaterOnItself:
		// With no clear area near it, the creature cannot stop its own fire
		if (!cast->clearArea.has_value())
		{
			return std::nullopt;
		}
		return creature_mind::CastWaterOnItself(cast->magicType, situation.self, *cast->clearArea, cast->height);
	case Build::CastToImpress:
		return creature_mind::CastToImpress(cast->magicType != 0 ? std::optional(cast->magicType) : std::nullopt, *object,
		                                    cast->height, random);
	case Build::CastAmusing:
		return creature_mind::CastAmusingAt(cast->amusingSpells, *object, cast->height, random);
	case Build::WaterTreeForTown:
		// Without a tree to use, the creature cannot water a tree for the town
		if (!situation.instrument.has_value())
		{
			return std::nullopt;
		}
		return creature_mind::CastWaterAt(static_cast<uint32_t>(MagicType::Water), *situation.instrument, cast->height);
	case Build::CastForest:
		// The creature casts the forest at the place where the thing was when it made the agenda
		if (!cast->place.has_value())
		{
			return std::nullopt;
		}
		return creature_mind::CastForestAt(cast->magicType, *object, *cast->place, cast->height);
	case Build::SetFire:
		return creature_mind::SetFireTo(*object, situation.instrument, situation.handFull);
	case Build::LookForever:
		return creature_mind::LookForever(*object);
	case Build::LookButDontApproach:
		return creature_mind::LookButDontApproach(*object, situation.lookExtraSeconds);
	case Build::LookAtCamera:
		return creature_mind::LookAtCamera(situation.hasPlayer);
	case Build::PointAtThing:
		return creature_mind::PointAtThing(*object);
	case Build::LookAtCameraInCutScene:
		return creature_mind::LookAtCameraInCutScene(situation.eye, random);
	case Build::PointAtCamera:
		return creature_mind::PointAtCamera(situation.hasPlayer, *situation.eye);
	case Build::CastOneShotInHand:
		return creature_mind::CastOneShotAt({.magicType = situation.heldOneShot->magicType,
		                                     .target = *object,
		                                     .height = situation.height,
		                                     .getAway = executor.oneShot != OneShotKind::Compassionate,
		                                     // The game pulls the face of compassion for each kind of globe in the hand
		                                     .face = creature_face::Cue::Compassion});
	case Build::PickUpAndCastOneShot:
		// Without a one-shot miracle to use, the action fails
		if (!situation.instrument.has_value() || !situation.usedOneShot.has_value())
		{
			return std::nullopt;
		}
		return creature_mind::CastOneShotAt({.magicType = situation.usedOneShot->magicType,
		                                     .target = *object,
		                                     .height = situation.height,
		                                     .pickUp = situation.handFull ? std::nullopt : situation.instrument,
		                                     .used = situation.instrument,
		                                     .getAway = true,
		                                     .face = executor.oneShot == OneShotKind::Aggressive ? creature_face::Cue::Anger
		                                             : executor.oneShot == OneShotKind::Compassionate
		                                                 ? creature_face::Cue::Compassion
		                                                 : creature_face::Cue::Playfulness});
	case Build::GoToHand:
		return creature_player_attention::GoToHand(random);
	case Build::PointAtHand:
		// Without a hand of its player, or a camera, the action fails
		if (!situation.playerHand.has_value() || !situation.camera.has_value())
		{
			return std::nullopt;
		}
		return creature_player_attention::PointAtHand(*situation.camera, *situation.playerHand);
	case Build::ShowHowNice:
		if (!situation.camera.has_value())
		{
			return std::nullopt;
		}
		return creature_player_attention::ShowHowNice(*situation.camera, situation.attitudeToPlayer);
	case Build::PrayToPlayer:
		if (!situation.camera.has_value())
		{
			return std::nullopt;
		}
		return creature_player_attention::PrayToPlayer(*situation.camera);
	case Build::ThrowAtCamera:
		// Without a camera of its player, or a thing to throw, the action fails
		if (!situation.eye.has_value() || !situation.hasPlayer || !situation.instrument.has_value())
		{
			return std::nullopt;
		}
		return creature_player_attention::ThrowAtCamera(situation.handFull ? std::nullopt : situation.instrument,
		                                                *situation.eye);
	case Build::WatchPlayer:
		return creature_player_attention::WatchPlayer();
	case Build::GoToMiddleOfScreen:
	case Build::FollowPlayer:
	case Build::BeCrossWithPlayer:
	case Build::BeSillyWithPlayer:
		// The creature needs the camera to turn to it
		if (!situation.camera.has_value())
		{
			return std::nullopt;
		}
		switch (executor.build)
		{
		case Build::GoToMiddleOfScreen:
			return creature_player_attention::GoToMiddleOfScreen(*situation.camera, situation.height);
		case Build::FollowPlayer:
			return creature_player_attention::FollowPlayer(*situation.camera, situation.height);
		case Build::BeCrossWithPlayer:
			return creature_player_attention::BeCrossWithPlayer(*situation.camera, situation.height, random);
		default:
			return creature_player_attention::BeSillyWithPlayer(*situation.camera, random);
		}
	case Build::ShowPlayerAnObject:
		// Without a camera, or a thing to show, the action fails
		if (!situation.camera.has_value() || !situation.instrument.has_value())
		{
			return std::nullopt;
		}
		return creature_player_attention::ShowPlayerAnObject(situation.handFull ? std::nullopt : situation.instrument,
		                                                     *situation.camera, situation.height);
	case Build::RestToGetBetter:
		return creature_mind::RestToGetBetter();
	case Build::GoHomeToRecover:
		return creature_mind::GoHomeToRecover(*situation.home, situation.height);
	case Build::HealItself:
		return creature_mind::HealItself(static_cast<uint32_t>(MagicType::Heal), situation.standing);
	case Build::RestOnTheSpot:
		return creature_mind::RestOnTheSpot(situation.self,
		                                    creature_mind::k_RestOnTheSpotSeconds +
		                                        (creature_mind::k_RestOnTheSpotExtraSeconds * RandomFraction(random)));
	case Build::CastRestoreHealthInHand:
		return creature_mind::CastRestoreHealthInHand(situation.heldOneShot->magicType, situation.self);
	case Build::PickUpAndCastRestoreHealth:
		// Without a one-shot miracle that restores health to use, the action fails
		if (!situation.instrument.has_value() || !situation.usedOneShot.has_value())
		{
			return std::nullopt;
		}
		return creature_mind::PickUpAndCastRestoreHealth(
		    situation.usedOneShot->magicType, situation.self, *situation.instrument, !situation.handFull,
		    creature_mind::k_RestoreHealthWaitSeconds +
		        (creature_mind::k_RestoreHealthWaitExtraSeconds * RandomFraction(random)));
	case Build::GoHome:
		return creature_mind::GoHome(*situation.home, situation.height);
	case Build::RunHome:
		return creature_mind::RunHome(*situation.home, situation.height,
		                              random(creature_mind::k_FrightenedBeforeRunningHomeLots) == 0);
	case Build::EnterTemple:
	{
		// Without a home and the heart of its player's temple, the action fails
		if (!situation.home.has_value() || !situation.templeHeart.has_value())
		{
			return std::nullopt;
		}
		const auto& heart = *situation.templeHeart;
		return creature_mind::EnterTemple(*situation.home, heart.point,
		                                  creature_mind::TempleStop(situation.standing, heart.point, heart.radius),
		                                  situation.height);
	}
	case Build::PrayAtTemple:
		return creature_mind::PrayAtTemple(*situation.home, situation.height);
	case Build::LookAtTemple:
		return creature_mind::LookAtTemple(*object, random(creature_mind::k_PointAtTempleLots) == 0);
	case Build::GiveFieldFoodToStore:
	case Build::GiveFruitToStore:
	case Build::GiveMagicFoodToStore:
	case Build::GiveMagicWoodToStore:
	case Build::PutFieldFoodBySite:
	case Build::PutFishBySite:
	case Build::DropCowInStore:
	case Build::MakeBreeder:
	case Build::TakeVillagerHome:
	case Build::BringVillagerToSite:
	case Build::RaiseTotem:
	case Build::LowerTotem:
	case Build::FrightenVillagers:
	case Build::PlayWithVillager:
	case Build::KissVillager:
		return TownHelpAgenda(executor, object, objectPoint, situation, random);
	case Build::ShowLearntLesson:
		return creature_odd::ShowLearntLesson();
	case Build::LookConfused:
		return creature_odd::LookConfused(situation.camera);
	case Build::BehaveStrangely:
		return creature_odd::BehaveStrangely(situation.turnsPerSecond, random);
	case Build::Sacrifice:
		// Without a worship site at its temple, the creature has nowhere to take the thing
		if (!situation.sacrificeAltar.has_value())
		{
			return std::nullopt;
		}
		return creature_sacrifice::Sacrifice(situation.holdingLiving ? std::nullopt : object, *situation.sacrificeAltar,
		                                     situation.height);
	case Build::DanceOutsideTemple:
		// Without the heart of its player's temple, the action fails
		if (!situation.templeHeart.has_value())
		{
			return std::nullopt;
		}
		return creature_temple_dance::DanceOutsideTemple(situation.templeHeart->temple, situation.height,
		                                                 situation.turnsPerSecond);
	case Build::DanceAroundThing:
		return creature_temple_dance::DanceAroundThing(*object, situation.height, situation.turnsPerSecond);
	case Build::StealAndPutByTemple:
		if (!situation.home.has_value())
		{
			return std::nullopt;
		}
		// It picks up the thing only when its hand is empty
		return creature_steal::StealAndPutByTemple(situation.handFull ? std::nullopt : object, *situation.home,
		                                           situation.height);
	case Build::StealAndPutInTown:
		// Without a storage pit in a town of its player, the agenda has no steps
		if (!situation.ownTownPit.has_value())
		{
			return std::nullopt;
		}
		// It picks up the thing only when its hand is empty
		return creature_town_theft::StealAndPutInTown(situation.handFull ? std::nullopt : object, *situation.ownTownPit,
		                                              situation.height);
	case Build::StealFoodFromStore:
	case Build::StealWoodFromStore:
		if (!situation.instrument.has_value())
		{
			return std::nullopt;
		}
		return creature_town_theft::StealFromStore(*situation.instrument, *object,
		                                           executor.build == Build::StealWoodFromStore
		                                               ? creature_mind::ObjectOrder::Handful::Logs
		                                               : creature_mind::ObjectOrder::Handful::Wheat,
		                                           situation.height);
	case Build::Never:
	case Build::MimicPlayer:
		return std::nullopt;
	}
	return std::nullopt;
}

bool creature_plan_actions::CanPooBy(bool alive, bool fieldOrFishFarm, float height)
{
	return !alive && !fieldOrFishFarm && height > k_PooedOnTallerThan;
}
