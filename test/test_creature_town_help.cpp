/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <gtest/gtest.h>

#include "Creature/CreatureIdleMind.h"
#include "Creature/CreaturePlanActions.h"
#include "Creature/CreatureTownHelp.h"

using namespace openblack;
using namespace openblack::creature_mind;
namespace plan_actions = openblack::creature_plan_actions;
namespace help = openblack::creature_town_help;

namespace
{
uint32_t First(uint32_t /*range*/)
{
	return 0;
}
uint32_t Last(uint32_t range)
{
	return range - 1;
}

const plan_actions::Executor& ActionNamed(std::string_view name)
{
	const auto* action = plan_actions::For(name);
	EXPECT_NE(action, nullptr) << name;
	static const plan_actions::Executor k_None {};
	return action != nullptr ? *action : k_None;
}
} // namespace

// Every town help action of the game's table has an agenda, on the thing that the game's table names
TEST(CreatureTownHelp, TheActionsActOnTheirThings)
{
	using plan_actions::Target;
	EXPECT_EQ(ActionNamed("GiveFoodFromFieldToStoragePit").use, Target::FieldWithFood);
	EXPECT_EQ(ActionNamed("GiveFruitFromTreeToStoragePit").use, Target::TreeBigEnough);
	EXPECT_EQ(ActionNamed("GiveMagicFoodToStoragePit").use, Target::MagicFoodPile);
	EXPECT_EQ(ActionNamed("GiveMagicWoodToStoragePit").use, Target::MagicWoodPile);
	EXPECT_EQ(ActionNamed("DropCowInStoragePit").use, Target::Cow);
	EXPECT_EQ(ActionNamed("DropCowInStoragePit").target, Target::BuiltStoragePit);
	EXPECT_EQ(ActionNamed("PutFoodFromFieldByWorshipSite").target, Target::WorshipSite);
	EXPECT_EQ(ActionNamed("PutFishByWorshipSite").target, Target::WorshipSite);
	EXPECT_EQ(ActionNamed("BringVillagersToWorshipSite").use, Target::VillagerNotWorshipping);
	EXPECT_EQ(ActionNamed("MakeBreederDisciple").target, Target::BreederVillager);
	EXPECT_EQ(ActionNamed("TakeVillagerHomeToSleep").target, Target::VillagerFarFromHome);
	EXPECT_EQ(ActionNamed("RaiseTotemPole").target, Target::PlayerTotem);
	EXPECT_EQ(ActionNamed("LowerTotemPole").target, Target::PlayerTotem);
	EXPECT_EQ(ActionNamed("PlayfullyKissVillager").target, Target::Villager);
}

// Food from a field: it walks to within one and a half times its height of the field, faces it, takes a handful of
// wheat, goes to where it can throw into the store, faces the store (where the action counts as done) and throws
TEST(CreatureTownHelp, FieldFoodGoesToTheStore)
{
	const auto agenda = plan_actions::Agenda(ActionNamed("GiveFoodFromFieldToStoragePit"), 5, {0.0f, 0.0f},
	                                         {.instrument = 9, .instrumentPoint = {30.0f, 40.0f}, .height = 10.0f}, First);
	ASSERT_TRUE(agenda.has_value());
	ASSERT_EQ(agenda->size(), 6);
	EXPECT_EQ((*agenda)[0].movement.kind, Movement::Kind::ToPoint);
	EXPECT_FLOAT_EQ((*agenda)[0].movement.maxDistance, 15.0f);
	EXPECT_EQ((*agenda)[1].movement.kind, Movement::Kind::TurnToFaceObject);
	EXPECT_EQ((*agenda)[1].movement.object, 9u);
	EXPECT_EQ((*agenda)[2].order.kind, ObjectOrder::Kind::TakeFromPile);
	EXPECT_EQ((*agenda)[2].order.handful, ObjectOrder::Handful::Wheat);
	EXPECT_EQ((*agenda)[2].order.object, 9u);
	EXPECT_EQ((*agenda)[3].movement.kind, Movement::Kind::ToThrowPosition);
	EXPECT_EQ((*agenda)[3].face, creature_face::Cue::Compassion);
	EXPECT_EQ((*agenda)[4].effect, Effect::Completed);
	EXPECT_EQ((*agenda)[5].order.kind, ObjectOrder::Kind::ThrowInStore);
	EXPECT_EQ((*agenda)[5].order.object, 5u);
}

// Holding food it can eat already, it takes no handful; without a field to use and nothing to give, there is no agenda
TEST(CreatureTownHelp, FoodInTheHandIsGivenAsItIs)
{
	const auto& action = ActionNamed("GiveFoodFromFieldToStoragePit");
	const auto holding = plan_actions::Agenda(action, 5, {0.0f, 0.0f}, {.holdingEatable = true, .height = 10.0f}, First);
	ASSERT_TRUE(holding.has_value());
	EXPECT_EQ(holding->size(), 3);
	EXPECT_FALSE(plan_actions::Agenda(action, 5, {0.0f, 0.0f}, {.height = 10.0f}, First).has_value());
}

// Fruit and magic food are taken from within a fifth more than both radii; magic wood looks like one of three things
TEST(CreatureTownHelp, FruitAndMagicPilesAreTakenFromNear)
{
	const plan_actions::Situation situation {.instrument = 9, .instrumentRadius = 6.0f, .radius = 4.0f, .height = 10.0f};
	const auto fruit = plan_actions::Agenda(ActionNamed("GiveFruitFromTreeToStoragePit"), 5, {}, situation, First);
	ASSERT_TRUE(fruit.has_value());
	EXPECT_FLOAT_EQ((*fruit)[0].movement.maxDistance, 12.0f);
	EXPECT_EQ((*fruit)[2].order.handful, ObjectOrder::Handful::Fruit);
	const auto woodFirst = plan_actions::Agenda(ActionNamed("GiveMagicWoodToStoragePit"), 5, {}, situation, First);
	const auto woodLast = plan_actions::Agenda(ActionNamed("GiveMagicWoodToStoragePit"), 5, {}, situation, Last);
	ASSERT_TRUE(woodFirst.has_value() && woodLast.has_value());
	EXPECT_EQ((*woodFirst)[2].order.handful, ObjectOrder::Handful::EvergreenBranch);
	EXPECT_EQ((*woodLast)[2].order.handful, ObjectOrder::Handful::Logs);
	EXPECT_EQ(help::HandfulPot(ObjectOrder::Handful::HardwoodBranch), PotInfo::TreeBranchHardwoodInHand);
	EXPECT_EQ(help::HandfulPot(ObjectOrder::Handful::Wheat), PotInfo::WheatInHand);
}

// Food by a worship site: the plan acts on the creature before it takes the handful; it puts the food down where the
// site keeps its food
TEST(CreatureTownHelp, FieldFoodIsPutByTheWorshipSite)
{
	const auto& action = ActionNamed("PutFoodFromFieldByWorshipSite");
	EXPECT_FALSE(plan_actions::Agenda(action, 5, {}, {.instrument = 9, .height = 10.0f}, First).has_value());
	const auto agenda =
	    plan_actions::Agenda(action, 5, {}, {.instrument = 9, .height = 10.0f, .siteFood = glm::vec2 {7.0f, 8.0f}}, First);
	ASSERT_TRUE(agenda.has_value());
	ASSERT_EQ(agenda->size(), 6);
	EXPECT_EQ((*agenda)[2].kind, Step::Kind::ActOnItself);
	EXPECT_EQ((*agenda)[3].order.kind, ObjectOrder::Kind::TakeFromPile);
	EXPECT_EQ((*agenda)[4].movement.point, glm::vec2(7.0f, 8.0f));
	EXPECT_FLOAT_EQ((*agenda)[4].movement.maxDistance, 10.0f);
	EXPECT_EQ((*agenda)[5].order.kind, ObjectOrder::Kind::Discard);
	EXPECT_EQ((*agenda)[5].order.animation, 97u);
	EXPECT_EQ((*agenda)[5].effect, Effect::Completed);
}

// A cow: picked up unless held, the store faced, the throw place reached (done there) and the cow thrown in
TEST(CreatureTownHelp, ACowIsDroppedInTheStore)
{
	const auto& action = ActionNamed("DropCowInStoragePit");
	const auto agenda = plan_actions::Agenda(action, 5, {}, {.instrument = 9, .height = 10.0f}, First);
	ASSERT_TRUE(agenda.has_value());
	ASSERT_EQ(agenda->size(), 4);
	EXPECT_EQ((*agenda)[0].order.kind, ObjectOrder::Kind::PickUp);
	EXPECT_EQ((*agenda)[0].order.object, 9u);
	EXPECT_EQ((*agenda)[1].movement.kind, Movement::Kind::TurnToFaceObject);
	EXPECT_EQ((*agenda)[2].movement.kind, Movement::Kind::ToThrowPosition);
	EXPECT_EQ((*agenda)[2].effect, Effect::Completed);
	EXPECT_EQ((*agenda)[3].order.kind, ObjectOrder::Kind::ThrowInStore);
	const auto held = plan_actions::Agenda(action, 5, {}, {.instrument = 9, .height = 10.0f, .holdingCow = true}, First);
	ASSERT_TRUE(held.has_value());
	EXPECT_EQ(held->size(), 3);
	EXPECT_TRUE(help::IsCowKind(AnimalInfo::Zebra));
	EXPECT_TRUE(help::IsCowKind(AnimalInfo::PuzzleCow));
	EXPECT_FALSE(help::IsCowKind(AnimalInfo::Lion));
}

// A breeder: picked up, stroked (done there), made a breeder and put down
TEST(CreatureTownHelp, AVillagerIsMadeABreeder)
{
	const auto agenda = plan_actions::Agenda(ActionNamed("MakeBreederDisciple"), 5, {}, {.height = 10.0f}, First);
	ASSERT_TRUE(agenda.has_value());
	ASSERT_EQ(agenda->size(), 4);
	EXPECT_EQ((*agenda)[1].order.kind, ObjectOrder::Kind::Keep);
	EXPECT_EQ((*agenda)[1].order.animation, 100u);
	EXPECT_EQ((*agenda)[1].effect, Effect::Completed);
	EXPECT_EQ((*agenda)[2].kind, Step::Kind::MakeDisciple);
	EXPECT_EQ((*agenda)[2].object, 5u);
	EXPECT_EQ((*agenda)[3].order.kind, ObjectOrder::Kind::Discard);
	EXPECT_TRUE(help::CanMakeBreeder({.available = true, .hasTown = true, .townBreeders = 2}));
	EXPECT_FALSE(help::CanMakeBreeder({.available = true, .hasTown = true, .townBreeders = 3}));
	EXPECT_FALSE(help::CanMakeBreeder({.available = true, .hasTown = true, .disciple = true}));
	EXPECT_FALSE(help::CanMakeBreeder({.available = true, .hasTown = false}));
}

// A villager far from home is carried to two metres from its home and put down
TEST(CreatureTownHelp, AVillagerIsTakenHome)
{
	const auto& action = ActionNamed("TakeVillagerHomeToSleep");
	EXPECT_FALSE(plan_actions::Agenda(action, 5, {}, {.height = 10.0f}, First).has_value());
	const auto agenda = plan_actions::Agenda(action, 5, {}, {.height = 10.0f, .villagerHome = 6u}, First);
	ASSERT_TRUE(agenda.has_value());
	ASSERT_EQ(agenda->size(), 4);
	EXPECT_EQ((*agenda)[1].movement.kind, Movement::Kind::GoNearObject);
	EXPECT_EQ((*agenda)[1].movement.object, 6u);
	EXPECT_FLOAT_EQ((*agenda)[1].movement.maxDistance, 2.0f);
	EXPECT_FALSE(help::FarFromHome(40.0f));
	EXPECT_TRUE(help::FarFromHome(40.5f));
}

// The totem moves by a fiftieth each turn, and to the end once less than that is left
TEST(CreatureTownHelp, ATotemMovesByAStepEachTurn)
{
	EXPECT_FLOAT_EQ(help::MoveTotemTurn(0.5f, true).share, 0.52f);
	EXPECT_FALSE(help::MoveTotemTurn(0.5f, true).done);
	EXPECT_FLOAT_EQ(help::MoveTotemTurn(0.99f, true).share, 1.0f);
	EXPECT_TRUE(help::MoveTotemTurn(0.99f, true).done);
	EXPECT_FLOAT_EQ(help::MoveTotemTurn(0.5f, false).share, 0.48f);
	EXPECT_TRUE(help::MoveTotemTurn(0.01f, false).done);
	EXPECT_FLOAT_EQ(help::MoveTotemTurn(0.01f, false).share, 0.0f);
}

// The totem step waits a turn, then moves the totem each turn until the end; with no town to worship, it gives up
TEST(CreatureTownHelp, TheTotemStepMovesTheTownShare)
{
	IdleMind mind;
	mind.agenda = help::MoveTotem(4, true);
	ASSERT_EQ(mind.agenda.size(), 4);
	EXPECT_FLOAT_EQ(mind.agenda[0].movement.maxDistance, 12.0f);
	EXPECT_EQ(mind.agenda[2].animation, 52u);
	mind.step = 3;
	Senses senses {.seconds = 0.1f, .totemShare = 0.97f};
	auto commands = Think(mind, senses, First);
	EXPECT_FALSE(commands.totem.has_value());
	commands = Think(mind, senses, First);
	ASSERT_TRUE(commands.totem.has_value());
	EXPECT_EQ(commands.totem->totem, 4u);
	EXPECT_FLOAT_EQ(commands.totem->share, 0.99f);
	EXPECT_EQ(mind.step, 3u);
	senses.totemShare = 0.99f;
	commands = Think(mind, senses, First);
	ASSERT_TRUE(commands.totem.has_value());
	EXPECT_FLOAT_EQ(commands.totem->share, 1.0f);
	EXPECT_EQ(commands.effect, Effect::Completed);
	EXPECT_EQ(mind.step, 4u);

	IdleMind gone;
	gone.agenda = help::MoveTotem(4, false);
	gone.step = 3;
	senses.totemShare.reset();
	static_cast<void>(Think(gone, senses, First));
	static_cast<void>(Think(gone, senses, First));
	EXPECT_EQ(gone.step, gone.agenda.size());
}

// Playing: frightening keeps two seconds facing and wide eyes; playing with a villager points for five seconds between
// two faces; a kiss plays its clip with whatever the hand holds
TEST(CreatureTownHelp, PlayingWithVillagers)
{
	const auto frighten = help::FrightenVillagers(3, 10.0f);
	ASSERT_EQ(frighten.size(), 3);
	EXPECT_FLOAT_EQ(frighten[0].movement.maxDistance, 30.0f);
	EXPECT_FLOAT_EQ(frighten[1].seconds, 2.0f);
	EXPECT_EQ(frighten[2].animation, 61u);
	EXPECT_EQ(frighten[2].heldEyes, Eyes::Wide);

	const auto play = help::PlayWithVillager(3, 10.0f, Last);
	ASSERT_EQ(play.size(), 6);
	EXPECT_EQ(play[2].animation, 67u);
	EXPECT_EQ(play[3].kind, Step::Kind::Face);
	EXPECT_EQ(play[3].animation, 25u);
	EXPECT_EQ(play[4].order.kind, ObjectOrder::Kind::PointAt);
	ASSERT_TRUE(play[4].order.seconds.has_value());
	EXPECT_FLOAT_EQ(*play[4].order.seconds, 5.0f);
	EXPECT_EQ(play[4].effect, Effect::Completed);
	EXPECT_EQ(play[4].face, creature_face::Cue::Smile);
	EXPECT_EQ(help::PlayWithVillager(3, 10.0f, First).size(), 5);

	const auto kiss = help::KissVillager(3, 10.0f);
	ASSERT_EQ(kiss.size(), 3);
	EXPECT_EQ(kiss[2].kind, Step::Kind::Kiss);
	// A kiss of the air has no creature to kiss
	EXPECT_FALSE(kiss[2].object.has_value());
	EXPECT_EQ(HandsNeededFor(kiss[2]), HandsNeed::Any);
}

// Each player watches its own creatures, also a computer god with no hands. Only in a game alone does the local player
// watch a creature of no player.
TEST(CreatureTownHelp, ThePlayerOfACreatureWatchesIt)
{
	using creature_town_help::WatchingPlayer;
	EXPECT_EQ(WatchingPlayer(PlayerNames::PLAYER_TWO, false, PlayerNames::PLAYER_ONE), PlayerNames::PLAYER_TWO);
	EXPECT_EQ(WatchingPlayer(PlayerNames::PLAYER_THREE, true, PlayerNames::PLAYER_ONE), PlayerNames::PLAYER_THREE);
	EXPECT_EQ(WatchingPlayer(PlayerNames::NEUTRAL, false, PlayerNames::PLAYER_ONE), PlayerNames::PLAYER_ONE);
	EXPECT_FALSE(WatchingPlayer(PlayerNames::NEUTRAL, true, PlayerNames::PLAYER_ONE).has_value());
}
