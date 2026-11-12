/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Testbed scenarios for the objects that the creature uses.

#include <array>
#include <numbers>

#include "3D/TempleInteriorInterface.h"
#include "Common/PortableMath.h"
#include "Creature/CreatureCave.h"
#include "Creature/CreatureFeedback.h"
#include "Creature/CreatureFight.h"
#include "Creature/CreatureIdleMind.h"
#include "Creature/CreatureLayers.h"
#include "Creature/CreatureObjectActions.h"
#include "TestbedScenarioBuilders.h"
#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

void testbed_scenarios::AddObjects(std::vector<Scenario>& all)
{
	// Balls all round a tiger facing the camera, south: it reaches for each in turn, looks it over and puts it down,
	// then goes back to the middle and faces south again
	constexpr float k_Reach = 10.0f;
	constexpr float k_Diagonal = 7.0f;
	const std::array<glm::vec2, 6> k_Round {glm::vec2 {0.0f, -k_Reach},          glm::vec2 {-k_Diagonal, -k_Diagonal},
	                                        glm::vec2 {k_Diagonal, -k_Diagonal}, glm::vec2 {-k_Reach, 0.0f},
	                                        glm::vec2 {k_Reach, 0.0f},           glm::vec2 {0.0f, k_Reach}};
	std::vector<ObjectSetup> balls;
	std::vector<Command> reaching;
	for (size_t i = 0; i < k_Round.size(); ++i)
	{
		balls.push_back({.type = MobileObjectInfo::Ball, .offset = k_Round.at(i)});
		reaching.push_back(OnObject(Kind::PickUp, 0, i, 1.0f));
		reaching.push_back(Play(Kind::Examine, 0, creature_object_actions::k_KeepAnimationCount - 1, 0.5f, true));
		reaching.push_back(Act(Kind::PutDown, 0, 0.5f, true));
		reaching.push_back(Go(Kind::WalkTo, 0, {0.0f, 0.0f}, 0.5f));
		reaching.push_back(Go(Kind::TurnToFace, 0, {0.0f, -60.0f}, 0.0f));
	}
	all.push_back({
	    .id = "objects.reach",
	    .name = "Picking up, looking over, putting down",
	    .facet = Facet::Objects,
	    .description = "A tiger facing the camera with balls in front of it, to its front left and right, either side "
	                   "and behind it. It picks each up in turn, examines it and puts it down, then goes back to the "
	                   "middle.",
	    .expected = "It reaches for each ball the way it lies, in front, to the side or behind, picking it up with its "
	                "hand; holds it up to look at it; and puts it down gently.",
	    .framing = {.shot = Shot::Follow, .distance = 1.3f},
	    .creatures = {Posed(CreatureType::Tiger, {0.0f, 0.0f})},
	    .objects = balls,
	    .commands = reaching,
	    .repeatFrom = 0,
	});

	std::vector<Command> examining {OnObject(Kind::PickUp, 0, 0, 1.0f)};
	for (size_t way = 0; way < creature_object_actions::k_KeepAnimationCount; ++way)
	{
		examining.push_back(Play(Kind::Examine, 0, way, 1.0f, true));
	}
	examining.push_back(Act(Kind::PutDown, 0, 1.0f, true));
	all.push_back({
	    .id = "objects.examine",
	    .name = "Looking a thing over four ways",
	    .facet = Facet::Objects,
	    .description = "A tiger picks up a pot in front of it and strokes, shakes, smells and examines it in turn, then "
	                   "puts it down.",
	    .expected = "Each way of looking the pot over plays its own animation with the pot held in the hand.",
	    .framing = {.shot = Shot::Follow, .distance = 1.2f},
	    .creatures = {Posed(CreatureType::Tiger, {0.0f, 0.0f})},
	    .objects = {{.type = MobileObjectInfo::EgyptPotA, .offset = {0.0f, -k_Reach}}},
	    .commands = examining,
	    .repeatFrom = 0,
	});

	all.push_back({
	    .id = "objects.throw_put_down_toss",
	    .name = "Throwing at a target, putting down and tossing away",
	    .facet = Facet::Objects,
	    .description = "A tiger picks up a ball and throws it at a pillar of rock 60 units east, fetches it and puts it "
	                   "down, fetches it again and tosses it away.",
	    .expected = "The throw flies flat and hard towards the pillar. The put-down lets go of the ball at rest at the "
	                "finger ends. The toss drops it aside. Each comes to rest where it lands.",
	    .framing = {.shot = Shot::Overview, .include = {{60.0f, 30.0f}, {-30.0f, -30.0f}}},
	    .creatures = {Posed(CreatureType::Tiger, {-20.0f, 0.0f}, 270.0f)},
	    .objects = {{.type = MobileObjectInfo::Ball, .offset = {-20.0f, -k_Reach}},
	                {.type = FeatureInfo::FatPilarChalk, .offset = {60.0f, 0.0f}}},
	    .commands = {OnObject(Kind::PickUp, 0, 0, 1.0f), Go(Kind::ThrowAt, 0, {60.0f, 0.0f}, 0.5f),
	                 OnObject(Kind::PickUp, 0, 0, 1.0f), Act(Kind::PutDown, 0, 0.5f, true), OnObject(Kind::PickUp, 0, 0, 1.0f),
	                 Act(Kind::Discard, 0, 0.5f, true), Go(Kind::WalkTo, 0, {-20.0f, 0.0f}, 1.0f)},
	    .repeatFrom = 0,
	});

	all.push_back({
	    .id = "objects.toss_close_up",
	    .name = "Tossing a ball away, close up",
	    .facet = Facet::Objects,
	    .description = "A tiger picks up a ball in front of it and tosses it away, again and again, seen close up.",
	    .expected = "The toss lets go of the ball as the hand swings out; the ball leaves at six tenths of the speed of "
	                "the middle of the hand's finger ends at that moment, sideways and a little up, and drops beside the "
	                "tiger. Every toss of the tiger leaves at the same speed in the tiger's own frame.",
	    .framing = {.shot = Shot::Follow, .distance = 0.6f},
	    .creatures = {Posed(CreatureType::Tiger, {0.0f, 0.0f})},
	    .objects = {{.type = MobileObjectInfo::Ball, .offset = {0.0f, -k_Reach}}},
	    .commands = {OnObject(Kind::PickUp, 0, 0, 1.0f), Act(Kind::Discard, 0, 0.5f, true),
	                 Go(Kind::WalkTo, 0, {0.0f, 0.0f}, 1.0f)},
	    .repeatFrom = 0,
	});

	all.push_back({
	    .id = "objects.held_in_fingers",
	    .name = "A ball held in the fingers, close up",
	    .facet = Facet::Objects,
	    .description = "A tiger picks up a ball in front of it, looks it over four ways and puts it down, again and "
	                   "again, seen close up.",
	    .expected = "The ball hangs below the middle of the finger ends and turns with the hand. The put-down lets go "
	                "of the ball at rest where the finger ends are.",
	    .framing = {.shot = Shot::Follow, .distance = 0.6f},
	    .creatures = {Posed(CreatureType::Tiger, {0.0f, 0.0f})},
	    .objects = {{.type = MobileObjectInfo::Ball, .offset = {0.0f, -k_Reach}}},
	    .commands = examining,
	    .repeatFrom = 0,
	});

	all.push_back({
	    .id = "objects.pickup_hold",
	    .name = "Holding a villager, a tree and a ball after the pick-up, close up",
	    .facet = Facet::Objects,
	    .description = "A tiger with, in front of it from left to right, a villager, a birch and a ball. It picks up the "
	                   "villager, stands, walks a few steps and puts her down. Then it does the same with the birch and "
	                   "with the ball, again and again, seen close up.",
	    .expected = "When the pick-up ends, the arm stays up in the hold pose and the held thing stays in the hand. It does "
	                "not jump away from the hand when the creature stands, walks or starts to put it down.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Follow, .distance = 0.8f},
	    .creatures = {Posed(CreatureType::Tiger, {0.0f, 0.0f})},
	    .objects = {{.type = VillagerInfo::NorseHousewifeFemale,
	                 .offset = {-6.0f, -k_Reach},
	                 .indestructible = true,
	                 .inScript = true},
	                {.type = TreeInfo::Birch, .offset = {0.0f, -k_Reach}},
	                {.type = MobileObjectInfo::Ball, .offset = {6.0f, -k_Reach}}},
	    .commands = {OnObject(Kind::PickUp, 0, 0, 1.5f), Go(Kind::WalkTo, 0, {-6.0f, 10.0f}, 0.5f),
	                 Act(Kind::PutDown, 0, 1.0f, true), Go(Kind::WalkTo, 0, {0.0f, 0.0f}, 0.5f),
	                 OnObject(Kind::PickUp, 0, 1, 1.5f), Go(Kind::WalkTo, 0, {0.0f, 10.0f}, 0.5f),
	                 Act(Kind::PutDown, 0, 1.0f, true), Go(Kind::WalkTo, 0, {0.0f, 0.0f}, 0.5f),
	                 OnObject(Kind::PickUp, 0, 2, 1.5f), Go(Kind::WalkTo, 0, {6.0f, 10.0f}, 0.5f),
	                 Act(Kind::PutDown, 0, 1.0f, true), Go(Kind::WalkTo, 0, {0.0f, 0.0f}, 0.5f)},
	    .repeatFrom = 0,
	});

	all.push_back({
	    .id = "objects.held_world_place",
	    .name = "The world place of a thing in a creature's hand",
	    .facet = Facet::Objects,
	    .description = "A player's tiger picks up a rock, walks 20 with it and puts it down there.",
	    .expected = "The rock stays in the tiger's hand while the tiger walks, and the draw shows it in the hand. In the "
	                "inspector, the rock's Transform stays where the tiger took the rock until the next turn of the "
	                "tiger. Then at each turn of the tiger it is the tiger's own place, on the land below the tiger, "
	                "and its turn does not change. When the tiger lets go, the rock starts at the finger ends.",
	    .framing = {.shot = Shot::Follow, .distance = 1.2f},
	    .creatures = {CreatureSetup {.species = CreatureType::Tiger, .offset = {0.0f, 60.0f}}},
	    .objects = {{.type = MobileStaticInfo::RockChalk, .offset = {4.0f, 54.0f}}},
	    .commands = {OnObject(Kind::PickUp, 0, 0, 1.0f), Go(Kind::WalkTo, 0, {0.0f, 80.0f}, 0.5f),
	                 Act(Kind::PutDown, 0, 1.0f, true)},
	});

	all.push_back({
	    .id = "objects.pickup_head_look",
	    .name = "The head look after the hold of a pick-up, close up",
	    .facet = Facet::Objects,
	    .description = "A content tiger with a ball in front of it. A script tells it to hold the ball. Then it puts the "
	                   "ball down. This repeats, seen close up.",
	    .expected = "As it reaches, the head and the eyes look at the ball. From the moment that the hand takes hold of "
	                "the ball to the end of the pick-up, the head and the eyes look at no point: they turn to the "
	                "front. After the pick-up, they look at the things near the creature again.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Follow, .distance = 0.6f},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f})},
	    .objects = {{.type = MobileObjectInfo::Ball, .offset = {0.0f, -k_Reach}}},
	    .commands = {{.kind = Kind::DoAction, .delaySeconds = 1.0f, .object = 0, .action = "HoldObject"},
	                 Act(Kind::PutDown, 0, 6.0f, true)},
	    .repeatFrom = 0,
	});

	// A tiger a tenth of the size beside it, for whom a villager is too heavy
	auto small = Posed(CreatureType::Tiger, {30.0f, 0.0f});
	small.size = 0.1f;
	all.push_back({
	    .id = "objects.pickup_limits",
	    .name = "What is too big or heavy to lift, and putting down what can't be moved",
	    .facet = Facet::Objects,
	    .description = "A grown tiger, with a ball eight times the usual size to its front right and a villager to its "
	                   "front left whom a script has made immovable, though she may be picked up. A tiger a tenth of the "
	                   "size stands 30 m east with a villager in front of it. The small tiger tries to pick up its "
	                   "villager; the big one tries to pick up the giant ball, then picks up the villager and puts her "
	                   "down, then picks her up again and throws her to the west.",
	    .expected = "The small tiger gives up at once: the villager weighs more than 0.8 of its weight. The big tiger "
	                "gives up on the ball at once: its size and the ball's together are beyond its reach. It picks up "
	                "the villager; each time it lets her go, put down or thrown, she goes straight down onto the land "
	                "under its hand and stands there, never left in the air.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-10.0f, -15.0f}, {35.0f, -15.0f}, {-40.0f, 5.0f}}},
	    .creatures = {Posed(CreatureType::Tiger, {0.0f, 0.0f}), small},
	    .objects =
	        {{.type = VillagerInfo::NorseHousewifeFemale,
	          .offset = {-6.0f, -k_Reach},
	          .indestructible = true,
	          .inScript = true,
	          .immovable = true},
	         {.type = MobileObjectInfo::Ball, .offset = {12.0f, -k_Reach}, .scale = 8.0f},
	         {.type = VillagerInfo::NorseHousewifeFemale, .offset = {30.0f, -3.0f}, .indestructible = true, .inScript = true}},
	    .commands = {OnObject(Kind::PickUp, 1, 2, 1.0f), OnObject(Kind::PickUp, 0, 1, 1.0f), OnObject(Kind::PickUp, 0, 0, 1.0f),
	                 Act(Kind::PutDown, 0, 1.0f, true), OnObject(Kind::PickUp, 0, 0, 2.0f),
	                 Go(Kind::ThrowAt, 0, {-40.0f, -10.0f}, 1.0f), Go(Kind::WalkTo, 0, {0.0f, 0.0f}, 2.0f),
	                 Go(Kind::TurnToFace, 0, {0.0f, -60.0f}, 0.0f)},
	});

	all.push_back({
	    .id = "villagers.load_in_creature_hand",
	    .name = "A villager with a load in a creature's hand",
	    .facet = Facet::Objects,
	    .description = "A grown tiger with a man in front of it who carries a load of wood. The tiger picks him up, walks "
	                   "a few steps with him, holds him for three seconds and puts him down, and does it again.",
	    .expected = "Before the tiger picks him up, the man holds a log of wood in his hand. In the tiger's hand he holds "
	                "nothing: no log shows. When he is put down he still has the wood, and the log shows in his hand "
	                "again as he goes about.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-15.0f, -15.0f}, {15.0f, 15.0f}}},
	    .creatures = {Posed(CreatureType::Tiger, {0.0f, 0.0f})},
	    .objects = {{.type = VillagerInfo::CelticForesterMale, .offset = {0.0f, -k_Reach}, .woodHeld = 300}},
	    .commands = {OnObject(Kind::PickUp, 0, 0, 1.5f), Go(Kind::WalkTo, 0, {0.0f, 10.0f}, 0.5f),
	                 Act(Kind::PutDown, 0, 3.0f, true), Go(Kind::WalkTo, 0, {0.0f, 0.0f}, 2.0f)},
	    .repeatFrom = 0,
	});

	all.push_back({
	    .id = "objects.pickup_kinds",
	    .name = "The kinds of thing a creature picks up",
	    .facet = Facet::Objects,
	    .description = "A grown tiger with, in front of it from left to right, a toy ball, a rock, an idol, a birch and a "
	                   "magic mushroom. It picks up and throws the toy ball and then the rock to the west, tries to pick up "
	                   "the idol, pulls up the birch and puts it down, then picks up the mushroom and puts it down.",
	    .expected = "The toy ball and the rock are lifted and thrown. The idol is refused at once: of the statics only "
	                "toys are picked up. The birch creaks as it comes out of the ground and is planted again where it is "
	                "put down. The mushroom is lifted and put down.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-25.0f, -15.0f}, {25.0f, -15.0f}, {-60.0f, 0.0f}}},
	    .creatures = {Posed(CreatureType::Tiger, {0.0f, 0.0f})},
	    .objects = {{.type = MobileStaticInfo::ToyBall, .offset = {-12.0f, -k_Reach}},
	                {.type = MobileStaticInfo::RockChalk, .offset = {-6.0f, -k_Reach}},
	                {.type = MobileStaticInfo::Idol, .offset = {0.0f, -k_Reach}},
	                {.type = TreeInfo::Birch, .offset = {6.0f, -k_Reach}},
	                {.type = FeatureInfo::MagicMushroom, .offset = {12.0f, -k_Reach}}},
	    .commands = {OnObject(Kind::PickUp, 0, 0, 1.0f), Go(Kind::ThrowAt, 0, {-60.0f, -10.0f}, 0.5f),
	                 OnObject(Kind::PickUp, 0, 1, 1.0f), Go(Kind::ThrowAt, 0, {-60.0f, 10.0f}, 4.0f),
	                 OnObject(Kind::PickUp, 0, 2, 1.0f), OnObject(Kind::PickUp, 0, 3, 1.0f), Act(Kind::PutDown, 0, 1.0f, true),
	                 OnObject(Kind::PickUp, 0, 4, 1.0f), Act(Kind::PutDown, 0, 1.0f, true)},
	});

	all.push_back({
	    .id = "objects.pile_kinds",
	    .name = "The kinds of food and wood piles",
	    .facet = Facet::Objects,
	    .description = "From west to east: a store's food pile, the food miracle's pile, a small pile of food such as the "
	                   "hand drops, a pot of food, a pile of wood, the wood miracle's pile, a small pile of wood such as "
	                   "the hand drops and a pot of wood.",
	    .expected = "Each one is drawn where it stands, on the ground, and rises out of the ground for what it holds. The "
	                "store's food pile half full is a low heap of grain; the food miracle's pile full is a wide heap.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-50.0f, 25.0f}, {50.0f, 60.0f}}},
	    .objects = {{.type = PotInfo::StoragePitFoodPile, .offset = {-40.0f, 45.0f}, .amount = 7500},
	                {.type = PotInfo::MagicFood, .offset = {-10.0f, 40.0f}, .amount = 1000},
	                {.type = PotInfo::HandFood, .offset = {12.0f, 40.0f}},
	                {.type = PotInfo::FoodPot, .offset = {18.0f, 40.0f}},
	                {.type = PotInfo::WoodPile_1, .offset = {26.0f, 40.0f}, .amount = 2500},
	                {.type = PotInfo::MagicWood, .offset = {34.0f, 40.0f}, .amount = 1000},
	                {.type = PotInfo::HandWood, .offset = {40.0f, 40.0f}},
	                {.type = PotInfo::WoodPot, .offset = {46.0f, 40.0f}}},
	});

	// The hand holds a small pile of food over another on the land, and uses it on that pile
	constexpr glm::vec2 k_PotOnLand {0.0f, 40.0f};
	all.push_back({
	    .id = "hand.pot_onto_pot",
	    .name = "The hand puts a pot of food onto a pot of food",
	    .facet = Facet::Hand,
	    .description = "Two small piles of food such as the hand drops, 300 food each. The hand takes the west pile and "
	                   "holds it over the east pile. Then the hand uses the pile that it holds on the east pile, as the "
	                   "Action button pressed over it does.",
	    .expected = "The pile in the hand goes. The east pile takes all of its food and becomes larger. No new pile "
	                "comes on the land.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-20.0f, 30.0f}, {10.0f, 50.0f}}},
	    .objects = {{.type = PotInfo::HandFood, .offset = k_PotOnLand + glm::vec2(-15.0f, 0.0f), .amount = 300},
	                {.type = PotInfo::HandFood, .offset = k_PotOnLand, .amount = 300}},
	    .commands = {{.kind = Kind::HandTake, .delaySeconds = 2.0f, .object = 0},
	                 {.kind = Kind::HandApply, .delaySeconds = 3.0f, .object = 1}},
	    .hand = HandHold {.offset = k_PotOnLand, .height = 6.0f},
	});

	all.push_back({
	    .id = "objects.give_wood",
	    .name = "Giving a tree to a store as wood",
	    .facet = Facet::Objects,
	    .description = "A grown tiger, a birch to its left and its town's empty storage pit off to the east. A script tells "
	                   "it to give wood from the birch to the pit, as the creature does by itself out of compassion "
	                   "for a town short of wood.",
	    .expected = "It walks to the birch and pulls it up (the tree creaks), carries it towards the pit until it is "
	                "near enough to throw, turns to face the pit with a look of compassion and throws the tree in. The "
	                "pit's wood goes up.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-25.0f, -15.0f}, {90.0f, 0.0f}}},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f})},
	    .objects = {{.type = TreeInfo::Birch, .offset = {-20.0f, -15.0f}},
	                {.type = AbodeInfo::CelticStoragePit, .offset = {90.0f, 0.0f}, .storedFood = 0, .storedWood = 0},
	                {.type = VillagerInfo::CelticFarmerMale, .offset = {100.0f, -12.0f}, .joinTown = true}},
	    .commands =
	        {{.kind = Kind::DoAction, .delaySeconds = 1.0f, .object = 1, .action = "GiveWoodFromTreeToStoragePit", .with = 0}},
	});

	all.push_back({
	    .id = "objects.give_wood_site",
	    .name = "Giving a tree to a damaged building as wood",
	    .facet = Facet::Objects,
	    .description = "A grown tiger, a birch to its left and its town's hut off to the east, hurt to six tenths of its "
	                   "life so that its town wants it mended. A script tells it to give wood from the birch to the "
	                   "building's site.",
	    .expected = "It walks to the birch and pulls it up, carries it towards the hut until it is near enough to throw, "
	                "turns to face it with a look of compassion and throws the tree in: the tree is gone and the hut's "
	                "site has the wood.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-25.0f, -15.0f}, {90.0f, 0.0f}}},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f})},
	    .objects = {{.type = TreeInfo::Birch, .offset = {-20.0f, -15.0f}},
	                {.type = AbodeInfo::CelticHut, .offset = {90.0f, 0.0f}, .life = 0.6f}},
	    .commands = {{.kind = Kind::DoAction,
	                  .delaySeconds = 1.0f,
	                  .object = 1,
	                  .action = "GiveWoodFromTreeToBuildingSite",
	                  .with = 0}},
	});

	all.push_back({
	    .id = "objects.give_wood_workshop",
	    .name = "Giving a tree to a workshop as wood",
	    .facet = Facet::Objects,
	    .description = "A grown tiger, a birch to its left and its town's workshop off to the east, with no wood. A script "
	                   "tells it to give wood from the birch to the workshop, as its copy of the player's gift of wood "
	                   "to a workshop does.",
	    .expected = "It walks to the birch and pulls it up, carries it towards the workshop until it is near enough to "
	                "throw, turns to face the workshop with a look of compassion and throws the tree in. The tree is gone "
	                "and the workshop's pile of logs shows the wood.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-25.0f, -15.0f}, {90.0f, 0.0f}}},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f})},
	    .objects = {{.type = TreeInfo::Birch, .offset = {-20.0f, -15.0f}},
	                {.type = AbodeInfo::CelticWorkshop, .offset = {90.0f, 0.0f}, .storedFood = 0, .storedWood = 0}},
	    .commands =
	        {{.kind = Kind::DoAction, .delaySeconds = 1.0f, .object = 1, .action = "GiveWoodFromTreeToWorkshop", .with = 0}},
	});

	all.push_back({
	    .id = "objects.steal_scaffold_to_town",
	    .name = "Creature steals a scaffold and takes it to a town",
	    .facet = Facet::Objects,
	    .description = "A tiger with a scaffold of no workshop in front of it, and the storage pit of the player's town "
	                   "behind it. A script tells it to steal the scaffold and take it to the town.",
	    .expected = "It picks the scaffold up, looking at it. It goes near the storage pit of the town, to within three "
	                "times its height, looking at the pit, and puts the scaffold down there. The scaffold makes no "
	                "building site: only a scaffold that a hand puts down starts a building.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Follow, .distance = 1.5f},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f})},
	    .objects = {{.type = AbodeInfo::CelticStoragePit, .offset = {0.0f, 90.0f}, .storedFood = 0, .storedWood = 0},
	                {.type = ScaffoldInfo::Normal, .offset = {0.0f, -20.0f}, .scale = 0.7f}},
	    .commands = {{.kind = Kind::DoAction, .delaySeconds = 1.0f, .object = 1, .action = "StealObjectAndTakeToTown"}},
	    .townBeliefInPlayer = 1000.0f,
	});

	all.push_back({
	    .id = "objects.repair_house",
	    .name = "Helping repair a house",
	    .facet = Facet::Objects,
	    .description = "A grown tiger, a birch to its left and its town's hut off to the east, hurt to six tenths of its "
	                   "life. A script tells it to help repair the hut with the birch.",
	    .expected = "It walks to the birch and picks it up, walks right up to the hut, turns to it and puts the tree "
	                "down by it; the hut gets a fifth of its life back (to eight tenths) and the tree is gone.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-25.0f, -15.0f}, {90.0f, 0.0f}}},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f})},
	    .objects = {{.type = TreeInfo::Birch, .offset = {-20.0f, -15.0f}},
	                {.type = AbodeInfo::CelticHut, .offset = {90.0f, 0.0f}, .life = 0.6f}},
	    .commands = {{.kind = Kind::DoAction, .delaySeconds = 1.0f, .object = 1, .action = "HelpRepairHouse", .with = 0}},
	});

	all.push_back({
	    .id = "objects.plant_tree",
	    .name = "Planting a tree by something",
	    .facet = Facet::Objects,
	    .description = "A grown tiger, a birch in front of it and a rock off to the north-east. A script tells it to plant "
	                   "the birch by the rock, as the creature does by itself out of compassion for a town.",
	    .expected = "It pulls up the birch (the tree creaks), walks right up to the rock, turns to face it and puts the "
	                "birch down beside it, where it stands and takes root.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-15.0f, -15.0f}, {50.0f, 50.0f}}},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f})},
	    .objects = {{.type = TreeInfo::Birch, .offset = {0.0f, -k_Reach}},
	                {.type = MobileStaticInfo::RockChalk, .offset = {50.0f, 50.0f}}},
	    .commands = {{.kind = Kind::DoAction, .delaySeconds = 1.0f, .object = 1, .action = "PlantTree", .with = 0}},
	});

	all.push_back({
	    .id = "objects.kind_throws",
	    .name = "Throwing the kinds of thing about",
	    .facet = Facet::Objects,
	    .description = "A grown tiger with a villager, a rock and a toy ball in front of it, a hut off to the north and a "
	                   "birch to the north-east. A script tells it in turn to throw the villager about, to hurl the rock "
	                   "at the hut, and to throw the toy ball at the birch.",
	    .expected = "It picks up the villager and throws her off to one side. It picks up the rock, goes to where it can "
	                "throw at the hut and hurls the rock at it, angry. Half the time it jumps for joy, then picks up the "
	                "toy ball and throws it at the birch, angry, sometimes jumping for joy again.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-30.0f, -15.0f}, {0.0f, -90.0f}, {50.0f, -70.0f}}},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f})},
	    .objects = {{.type = VillagerInfo::CelticHousewifeFemale, .offset = {-12.0f, -k_Reach}},
	                {.type = MobileStaticInfo::RockChalk, .offset = {0.0f, -k_Reach}},
	                {.type = MobileStaticInfo::ToyBall, .offset = {12.0f, -k_Reach}},
	                {.type = AbodeInfo::CelticHut, .offset = {0.0f, -90.0f}},
	                {.type = TreeInfo::Birch, .offset = {50.0f, -70.0f}}},
	    .commands =
	        {{.kind = Kind::DoAction, .delaySeconds = 1.0f, .object = 0, .action = "ThrowAround"},
	         {.kind = Kind::DoAction, .delaySeconds = 6.0f, .waitUntilFree = true, .object = 3, .action = "Hurl", .with = 1},
	         {.kind = Kind::DoAction,
	          .delaySeconds = 6.0f,
	          .waitUntilFree = true,
	          .object = 4,
	          .action = "ThrowBallAtObject",
	          .with = 2}},
	});

	all.push_back({
	    .id = "objects.kick_ball",
	    .name = "Kicking a ball about",
	    .facet = Facet::Objects,
	    .description = "A grown tiger with a toy ball in front of it. A script tells it to kick the ball about.",
	    .expected = "It walks up to the ball, turns to it and kicks it: the ball flies off ahead of it, half upwards, and "
	                "bounces away. A quarter of the time it then points at the ball a second, else it waits up to a "
	                "second. It goes after the ball and kicks it again, two to four times in all, sometimes jumping for "
	                "joy at the end. The kicks make no sound.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-20.0f, 10.0f}, {20.0f, -150.0f}}},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f})},
	    .objects = {{.type = MobileStaticInfo::ToyBall, .offset = {0.0f, -k_Reach}}},
	    .commands = {{.kind = Kind::DoAction, .delaySeconds = 1.0f, .object = 0, .action = "KickBallAround"}},
	});

	all.push_back({
	    .id = "objects.break_rock",
	    .name = "Breaking a rock",
	    .facet = Facet::Objects,
	    .description = "A grown tiger with a rock in front of it. A script tells it to break the rock.",
	    .expected = "It picks the rock up and throws it off to one side; as the rock flies it breaks into two halves, "
	                "which fly on as it flew and come down apart.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-20.0f, 20.0f}, {80.0f, 80.0f}}},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f})},
	    .objects = {{.type = MobileStaticInfo::RockChalk, .offset = {0.0f, -k_Reach}}},
	    .commands = {{.kind = Kind::DoAction, .delaySeconds = 1.0f, .object = 0, .action = "BreakRock"}},
	});

	all.push_back({
	    .id = "objects.script_takes_rock_wood",
	    .name = "A script takes wood from a rock and gives wood to it",
	    .facet = Facet::Objects,
	    .description = "A rock of scale 1 holds 200 wood. After one second, REMOVE_RESOURCE takes 100 wood from it. After "
	                   "three seconds, ADD_RESOURCE gives it 300 wood. After five seconds, REMOVE_RESOURCE takes 1000 "
	                   "wood from it.",
	    .expected = "The rock becomes half its size and its life becomes half. Then it becomes twice its first size, or "
	                "its largest size if that is smaller. Then the rock gives all its wood and goes from the land.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-10.0f, -10.0f}, {10.0f, 10.0f}}},
	    .objects = {{.type = MobileStaticInfo::RockChalk, .offset = {0.0f, 0.0f}}},
	    .commands = {{.kind = Kind::CallNative,
	                  .delaySeconds = 1.0f,
	                  .script = "REMOVE_RESOURCE",
	                  .scriptArguments = {{.kind = ScriptArgument::Kind::Integer, .number = 1.0f},
	                                      {.kind = ScriptArgument::Kind::Number, .number = 100.0f},
	                                      {.kind = ScriptArgument::Kind::Object, .index = 0}}},
	                 {.kind = Kind::CallNative,
	                  .delaySeconds = 2.0f,
	                  .script = "ADD_RESOURCE",
	                  .scriptArguments = {{.kind = ScriptArgument::Kind::Integer, .number = 1.0f},
	                                      {.kind = ScriptArgument::Kind::Number, .number = 300.0f},
	                                      {.kind = ScriptArgument::Kind::Object, .index = 0}}},
	                 {.kind = Kind::CallNative,
	                  .delaySeconds = 2.0f,
	                  .script = "REMOVE_RESOURCE",
	                  .scriptArguments = {{.kind = ScriptArgument::Kind::Integer, .number = 1.0f},
	                                      {.kind = ScriptArgument::Kind::Number, .number = 1000.0f},
	                                      {.kind = ScriptArgument::Kind::Object, .index = 0}}}},
	});

	// A land's script moves its challenge scroll to another place, as The Sacrifice moves its scroll to the altar
	all.push_back({
	    .id = "scripts.scroll_moved_to_another_cell",
	    .name = "A script moves a challenge scroll to a place in another map cell",
	    .facet = Facet::Objects,
	    .description = "After one second, CREATE_HIGHLIGHT makes a silver challenge scroll on the land. After two more "
	                   "seconds, SET_POSITION puts the scroll 30 to the side, in another map cell.",
	    .expected = "The scroll shows where it was made, then it shows and turns at its new place, 30 to the side. "
	                "The cursor on the scroll at its new place picks it. Nothing stays at the old place.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {15.0f, 20.0f, -25.0f}, .look = {15.0f, 3.0f, 0.0f}},
	    .commands = {{.kind = Kind::CallNative,
	                  .delaySeconds = 1.0f,
	                  .script = "CREATE_HIGHLIGHT",
	                  .scriptArguments = {{.kind = ScriptArgument::Kind::Integer, .number = 2.0f},
	                                      {.kind = ScriptArgument::Kind::Position, .point = {0.0f, 0.0f}},
	                                      {.kind = ScriptArgument::Kind::Integer, .number = 28.0f}}},
	                 {.kind = Kind::CallNative,
	                  .delaySeconds = 2.0f,
	                  .script = "SET_POSITION",
	                  .scriptArguments = {{.kind = ScriptArgument::Kind::LastResult},
	                                      {.kind = ScriptArgument::Kind::Position, .point = {30.0f, 0.0f}}}}},
	});

	// A land's script aims a magic beam from one villager at another, as the healer of Land 2 calls the child
	all.push_back({
	    .id = "scripts.spot_beam_target",
	    .name = "A script aims a magic beam at a villager that walks",
	    .facet = Facet::Objects,
	    .description =
	        "A villager that a script holds stands still. A second villager walks to and fro 16 to the side. "
	        "After one second, SPECIAL_EFFECT_OBJECT starts a magic beam on the first villager for 60 "
	        "seconds, and ADD_SPOT_VISUAL_TARGET_OBJECT gives the beam the second villager as its target. "
	        "After two more seconds, SPECIAL_EFFECT_POSITION starts a magic beam 10 to the side of the first villager, "
	        "and ADD_SPOT_VISUAL_TARGET_POS gives it a point 12 above the land.",
	    .expected = "Wiggling ribbons of light go from the place of the first villager to the middle of the second "
	                "villager. The ribbons follow the second villager while it walks. The second beam goes from its "
	                "place up to the point above the land and stays there.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 5.0f, -10.0f}, .look = {0.0f, 2.0f, 6.0f}},
	    .objects = {{.type = VillagerInfo::CelticFarmerMale, .offset = {0.0f, 0.0f}, .inScript = true},
	                {.type = VillagerInfo::CelticHousewifeFemale,
	                 .offset = {-8.0f, 12.0f},
	                 .walkTo = glm::vec2 {8.0f, 12.0f},
	                 .walkAfterSeconds = 0.5f,
	                 .walkRepeatSeconds = 8.0f}},
	    .commands = {{.kind = Kind::CallNative,
	                  .delaySeconds = 1.0f,
	                  .script = "SPECIAL_EFFECT_OBJECT",
	                  .scriptArguments = {{.kind = ScriptArgument::Kind::Integer, .number = 20.0f},
	                                      {.kind = ScriptArgument::Kind::Object, .index = 0},
	                                      {.kind = ScriptArgument::Kind::Number, .number = 60.0f}}},
	                 {.kind = Kind::CallNative,
	                  .delaySeconds = 0.0f,
	                  .script = "ADD_SPOT_VISUAL_TARGET_OBJECT",
	                  .scriptArguments = {{.kind = ScriptArgument::Kind::LastResult},
	                                      {.kind = ScriptArgument::Kind::Object, .index = 1}}},
	                 {.kind = Kind::CallNative,
	                  .delaySeconds = 2.0f,
	                  .script = "SPECIAL_EFFECT_POSITION",
	                  .scriptArguments = {{.kind = ScriptArgument::Kind::Integer, .number = 20.0f},
	                                      {.kind = ScriptArgument::Kind::Position, .point = {-10.0f, 4.0f}},
	                                      {.kind = ScriptArgument::Kind::Number, .number = 60.0f}}},
	                 {.kind = Kind::CallNative,
	                  .delaySeconds = 0.0f,
	                  .script = "ADD_SPOT_VISUAL_TARGET_POS",
	                  .scriptArguments = {{.kind = ScriptArgument::Kind::LastResult},
	                                      {.kind = ScriptArgument::Kind::Position, .point = {-10.0f, 4.0f}, .height = 12.0f}}}},
	});

	all.push_back({
	    .id = "objects.smash_stone",
	    .name = "Smashing a stone in half",
	    .facet = Facet::Objects,
	    .description = "A grown tiger with a rock in front of it. A script tells it to smash the stone in half.",
	    .expected = "Angry, it reaches for the rock with a blow and stamps on it: the rock breaks into two halves with a "
	                "smashing sound.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Follow, .distance = 1.3f},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f})},
	    .objects = {{.type = MobileStaticInfo::RockChalk, .offset = {0.0f, -k_Reach}}},
	    .commands = {{.kind = Kind::DoAction, .delaySeconds = 1.0f, .object = 0, .action = "SmashStoneInHalf"}},
	});

	all.push_back({
	    .id = "objects.stomp_villager",
	    .name = "Stomping on a villager",
	    .facet = Facet::Objects,
	    .description = "A grown tiger with a villager in front of it. A script tells it to stomp on the villager.",
	    .expected = "Sometimes angry first, it reaches for the villager with a blow of its foot and an angry face. The "
	                "villager dies under the foot.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Follow, .distance = 1.3f},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f})},
	    .objects = {{.type = VillagerInfo::CelticFarmerMale, .offset = {0.0f, -k_Reach}}},
	    .commands = {{.kind = Kind::DoAction, .delaySeconds = 1.0f, .object = 0, .action = "Stomp"}},
	});

	all.push_back({
	    .id = "objects.stomp_lost_villager",
	    .name = "Stomping on a villager that the hand takes away",
	    .facet = Facet::Objects,
	    .description = "A grown tiger with a villager in front of it. A script tells it to stomp on the villager. As "
	                   "the foot starts to come down, the player's hand takes the villager.",
	    .expected = "The foot stops and goes back up along the same path, to where the stomp started. Then the tiger "
	                "stands, and the stomp fails. The villager stays alive in the hand.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Follow, .distance = 1.3f},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f})},
	    .objects = {{.type = VillagerInfo::CelticFarmerMale, .offset = {0.0f, -k_Reach}}},
	    .commands = {{.kind = Kind::DoAction, .delaySeconds = 1.0f, .object = 0, .action = "Stomp"},
	                 {.kind = Kind::HandTake, .delaySeconds = 3.7f, .object = 0}},
	});

	all.push_back({
	    .id = "objects.stomp_and_eat",
	    .name = "Stomping on a sheep and eating it",
	    .facet = Facet::Objects,
	    .description = "A grown tiger with a sheep in front of it. A script tells it to stomp on the sheep and eat it.",
	    .expected = "With an angry face it stomps on the sheep, which dies. Then it picks the dead sheep up and eats it.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Follow, .distance = 1.3f},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f})},
	    .objects = {{.type = AnimalInfo::Sheep, .offset = {0.0f, -k_Reach}, .living = true}},
	    .commands = {{.kind = Kind::DoAction, .delaySeconds = 1.0f, .object = 0, .action = "StompAndEat"}},
	});

	all.push_back({
	    .id = "objects.stone_and_eat",
	    .name = "Stoning a sheep and eating it",
	    .facet = Facet::Objects,
	    .description = "A grown tiger with a rock in front of it and a sheep further away. A script tells it to stone the "
	                   "sheep with the rock and eat it.",
	    .expected = "It picks up the rock and, with an angry face, walks to a place to throw from. As in the game, it "
	                "aims at the rock in its own hand, so the throw fails: it stops with the rock still in its hand and does "
	                "not eat the sheep.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{0.0f, 0.0f}, {0.0f, -80.0f}}},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f})},
	    .objects = {{.type = AnimalInfo::Sheep, .offset = {0.0f, -60.0f}, .living = true},
	                {.type = MobileStaticInfo::RockChalk, .offset = {6.0f, -k_Reach}}},
	    .commands = {{.kind = Kind::DoAction, .delaySeconds = 1.0f, .object = 0, .action = "StoneAndEat", .with = 1}},
	});

	all.push_back({
	    .id = "objects.sleep_at_home_without_temple",
	    .name = "A script tells a creature with no temple to sleep at home",
	    .facet = Facet::Objects,
	    .description = "A grown tiger and no temple. The tiger walks 100 south of the place where it was made. Then a "
	                   "script tells it to sleep at home, as the guide's scripts on Land 1 do.",
	    .expected = "It walks back to the place where it was made, faces down the slope and sleeps there until it is "
	                "rested.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{0.0f, 0.0f}, {0.0f, -100.0f}}},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f})},
	    .objects = {{.type = MobileStaticInfo::RockChalk, .offset = {40.0f, -50.0f}}},
	    .commands =
	        {Go(Kind::WalkTo, 0, {0.0f, -100.0f}, 0.5f, true),
	         {.kind = Kind::DoAction, .delaySeconds = 1.0f, .waitUntilFree = true, .object = 0, .action = "SleepAtHome"}},
	});

	all.push_back({
	    .id = "objects.kick_hut",
	    .name = "Kicking a hut",
	    .facet = Facet::Objects,
	    .description = "A grown tiger near a hut. A script tells it to kick the hut.",
	    .expected = "It walks up to the hut, faces it and kicks it: the kick makes a sound and breaks two pieces off the "
	                "hut, which fly off.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Follow, .distance = 1.3f},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 10.0f})},
	    .objects = {{.type = AbodeInfo::CelticHut, .offset = {0.0f, -25.0f}}},
	    .commands = {{.kind = Kind::DoAction, .delaySeconds = 1.0f, .object = 0, .action = "Kick"}},
	});

	all.push_back({
	    .id = "objects.throw_in_the_sea",
	    .name = "Throwing a ball into the sea",
	    .facet = Facet::Objects,
	    .description = "A grown tiger south of the lake with a ball beside it. A script tells it to throw the ball into "
	                   "the sea.",
	    .expected = "It picks up the ball, walks to the nearest coast with a playful face, stops two of its heights "
	                "short of it and throws the ball out into the nearest water.",
	    .framing = {.shot = Shot::Overview, .include = LakeInView()},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 120.0f})},
	    .objects = {{.type = MobileObjectInfo::Ball, .offset = {10.0f, 110.0f}}},
	    .commands = {{.kind = Kind::DoAction, .delaySeconds = 1.0f, .object = 0, .action = "ThrowInTheSea"}},
	});

	all.push_back({
	    .id = "objects.kick_tree",
	    .name = "Kicking a tree",
	    .facet = Facet::Objects,
	    .description = "A grown tiger near a birch. A script tells it to kick the tree.",
	    .expected = "It walks up to the tree, faces it and kicks it: the tree creaks and flies off ahead of the tiger.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Follow, .distance = 1.3f},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 10.0f})},
	    .objects = {{.type = TreeInfo::Birch, .offset = {0.0f, -20.0f}}},
	    .commands = {{.kind = Kind::DoAction, .delaySeconds = 1.0f, .object = 0, .action = "KickTree"}},
	});

	{
		// Villagers, sheep and carts close round a tiger that lies down to sleep
		std::vector<ObjectSetup> round;
		for (int i = 0; i < 8; ++i)
		{
			const auto angle = static_cast<float>(i) * std::numbers::pi_v<float> / 4.0f;
			const glm::vec2 at {portable_math::Sin(angle) * 6.0f, portable_math::Cos(angle) * 6.0f};
			if (i % 3 == 0)
			{
				round.push_back({.type = MobileObjectInfo::Cart, .offset = at});
			}
			else if (i % 3 == 1)
			{
				round.push_back({.type = VillagerInfo::CelticFarmerMale, .offset = at});
			}
			else
			{
				round.push_back({.type = AnimalInfo::Sheep, .offset = at, .living = true});
			}
		}
		all.push_back({
		    .id = "objects.trample_lying_down",
		    .name = "Knocking things about as it lies down",
		    .facet = Facet::Objects,
		    .description = "A grown tiger with villagers, sheep and carts close round it. A script tells it to sleep "
		                   "where it stands.",
		    .expected = "As it lies down, its hands, feet, head and body knock the villagers, sheep and carts that they "
		                "pass near through the air. Nothing is knocked while it sleeps. As it gets up, it knocks things "
		                "again.",
		    .environment = {.dispenserGrid = false},
		    .framing = {.shot = Shot::Follow, .distance = 1.3f},
		    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f})},
		    .objects = round,
		    .commands = {{.kind = Kind::DoAction, .delaySeconds = 1.0f, .object = 0, .action = "SleepOnTheSpot"}},
		});
	}

	all.push_back({
	    .id = "objects.trample_huts",
	    .name = "Knocking pieces off huts as it lies down",
	    .facet = Facet::Objects,
	    .description = "A grown tired tiger with four huts close round it. It sleeps where it stands.",
	    .expected = "As it lies down, its hands, feet, head and body knock pieces off the huts that they touch. The "
	                "speed of each piece is about four times the distance that the part of the body moved since its "
	                "last check: a part that moves slowly pushes a piece off, a part that moves quickly throws it far.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Follow, .distance = 1.6f},
	    .creatures = {CreatureSetup {.species = CreatureType::Tiger,
	                                 .offset = {0.0f, 0.0f},
	                                 .needs = {.energy = 1.0f, .exhaustion = 0.75f, .dehydration = 0.0f, .poo = 0.0f}}},
	    .objects = {{.type = AbodeInfo::CelticHut, .offset = {8.0f, 0.0f}},
	                {.type = AbodeInfo::CelticHut, .offset = {-8.0f, 0.0f}},
	                {.type = AbodeInfo::CelticHut, .offset = {0.0f, 8.0f}},
	                {.type = AbodeInfo::CelticHut, .offset = {0.0f, -8.0f}}},
	    .commands = {Act(Kind::Sleep, 0, 1.0f, false)},
	});

	all.push_back({
	    .id = "objects.stroke_toy",
	    .name = "Stroking a cuddly toy",
	    .facet = Facet::Objects,
	    .description = "A grown tiger with a cuddly toy in front of it. A script tells it to stroke the toy.",
	    .expected = "With a look of compassion it picks the toy up and strokes it in its hand, looking at it, then keeps "
	                "holding it.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Follow, .distance = 1.3f},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f})},
	    .objects = {{.type = MobileStaticInfo::ToyCuddly, .offset = {0.0f, -k_Reach}}},
	    .commands = {{.kind = Kind::DoAction, .delaySeconds = 1.0f, .object = 0, .action = "StrokToy"}},
	});

	all.push_back({
	    .id = "objects.throw_die",
	    .name = "Throwing a die",
	    .facet = Facet::Objects,
	    .description = "A grown tiger with a die in front of it. A script tells it to throw the die.",
	    .expected = "It picks the die up and throws it a little way off to one side, watches it fly, amazed, until it "
	                "lands (three to four seconds at most), then jumps for joy two times in three, else looks sad.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Follow, .distance = 1.3f},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f})},
	    .objects = {{.type = MobileStaticInfo::ToyDie, .offset = {0.0f, -k_Reach}}},
	    .commands = {{.kind = Kind::DoAction, .delaySeconds = 1.0f, .object = 0, .action = "ThrowDie"}},
	});

	{
		constexpr glm::vec2 k_ToyTemple {-300.0f, 100.0f};
		constexpr float k_ToyTempleTurn = 36.0f;
		all.push_back({
		    .id = "objects.take_toy_home",
		    .name = "Taking a toy home",
		    .facet = Facet::Objects,
		    .description = "A grown tiger of the player's, whose temple stands far to the west, with a toy ball in front "
		                   "of it. A script tells it to take the toy home.",
		    .expected = "With a look of compassion it picks the ball up, walks to its home, the pen before the temple, "
		                "and sets the ball down within five of it; now and then it tosses it away instead.",
		    .environment = {.dispenserGrid = false},
		    .framing = {.shot = Shot::Follow, .distance = 1.5f},
		    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f})},
		    .objects = {{.type = MobileStaticInfo::ToyBall, .offset = {0.0f, -k_Reach}}},
		    .temples = {{.offset = k_ToyTemple, .angle = k_ToyTempleTurn}},
		    .commands = {{.kind = Kind::DoAction, .delaySeconds = 1.0f, .object = 0, .action = "TakeToyHome"}},
		});
		all.push_back({
		    .id = "objects.kick_ball_in_pen",
		    .name = "Kicking a ball about in the pen",
		    .facet = Facet::Objects,
		    .description = "A grown tiger of the player's stands in its pen before the temple, shown at a newborn's size. A "
		                   "toy ball lies nine metres from it in the pen. A script tells it to kick the ball about.",
		    .expected = "It walks towards the ball and stops some metres short of it, because its walks keep clear of "
		                "things by its own size. It turns to the ball and kicks it: the ball flies off a short way, as a "
		                "small creature kicks. It goes after the ball and kicks it again, two to four times in all.",
		    .environment = {.dispenserGrid = false},
		    .framing = {.shot = Shot::Follow, .distance = 1.5f},
		    .creatures = {Content(CreatureType::Tiger, {-320.0f, 113.0f})},
		    .objects = {{.type = MobileStaticInfo::ToyBall, .offset = {-320.0f, 122.0f}}},
		    .temples = {{.offset = k_ToyTemple, .angle = k_ToyTempleTurn}},
		    .commands = {{.kind = Kind::DoAction, .delaySeconds = 2.0f, .object = 0, .action = "KickBallAround"}},
		});
		all.push_back({
		    .id = "objects.take_wheat_home",
		    .name = "Taking wheat from a field home",
		    .facet = Facet::Objects,
		    .description = "A grown tiger of the player's, whose temple stands far to the west, with a ripe wheat field in "
		                   "front of it. A script tells it to take food from the field to its home.",
		    .expected = "It walks to the field and faces it, pulls a handful of wheat from it, then walks home with the "
		                "wheat in its hand and looks ahead. At its home, the pen before the temple, it puts the wheat down.",
		    .environment = {.dispenserGrid = false},
		    .framing = {.shot = Shot::Follow, .distance = 1.5f},
		    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f})},
		    .objects = {{.type = FieldTypeInfo::Wheat, .offset = {0.0f, -40.0f}, .ripeCrop = true}},
		    .temples = {{.offset = k_ToyTemple, .angle = k_ToyTempleTurn}},
		    .commands = {{.kind = Kind::DoAction, .delaySeconds = 1.0f, .object = 0, .action = "TakeFoodFromFieldToHome"}},
		});
		all.push_back({
		    .id = "objects.bring_thing_home",
		    .name = "Bringing a thing home",
		    .facet = Facet::Objects,
		    .description = "A grown tiger of the player's, whose temple stands far to the west, with a rock in front of it. "
		                   "A script tells it to bring the rock home.",
		    .expected = "It picks the rock up, walks home and looks ahead, and puts the rock down within five metres of "
		                "its home, the pen before the temple.",
		    .environment = {.dispenserGrid = false},
		    .framing = {.shot = Shot::Follow, .distance = 1.5f},
		    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f})},
		    .objects = {{.type = MobileStaticInfo::Rock, .offset = {0.0f, -k_Reach}}},
		    .temples = {{.offset = k_ToyTemple, .angle = k_ToyTempleTurn}},
		    .commands = {{.kind = Kind::DoAction, .delaySeconds = 1.0f, .object = 0, .action = "BringHome"}},
		});
		all.push_back({
		    .id = "objects.steal_toy",
		    .name = "Stealing a toy",
		    .facet = Facet::Objects,
		    .description = "A grown tiger of the player's, whose temple stands far to the west, with a cuddly toy in front "
		                   "of it, out of the influence of the player. A script tells it to steal the toy and take it to the "
		                   "temple.",
		    .expected = "It picks the toy up and looks at it, walks home and looks ahead, and puts the toy down within two "
		                "times its height of its home, the pen before the temple.",
		    .environment = {.dispenserGrid = false},
		    .framing = {.shot = Shot::Follow, .distance = 1.5f},
		    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f})},
		    .objects = {{.type = MobileStaticInfo::ToyCuddly, .offset = {0.0f, -k_Reach}}},
		    .temples = {{.offset = k_ToyTemple, .angle = k_ToyTempleTurn}},
		    .commands = {{.kind = Kind::DoAction, .delaySeconds = 1.0f, .object = 0, .action = "StealObjectAndTakeToCitadel"}},
		});
		// The six practice globes of the guide's food lesson, made on the land in one place by the script
		std::vector<DispenserSetup> lessonGlobes(6,
		                                         {.type = MagicType::Food, .offset = {0.0f, -k_Reach}, .bubbleHeight = 0.0f});
		auto lessonTiger = Content(CreatureType::Tiger, {0.0f, 0.0f});
		lessonTiger.pauseMind = false;
		lessonTiger.phase = 9;
		all.push_back({
		    .id = "story.food_lesson_globes",
		    .name = "The globes of the guide's food lesson near the creature",
		    .facet = Facet::Story,
		    .description =
		        "A grown tiger of the player's, at the stage of growing up that the guide's second food lesson "
		        "gives it, stands near six food globes on the land, as the lesson's script makes them. Its "
		        "temple stands far to the west, and the globes are out of the influence of the player. Its "
		        "body wants nothing, and its mind runs. Tap the globes one at a time, and tap one while the hand still holds "
		        "food.",
		    .expected = "The globes stay where they are until the player takes them: the creature leaves them alone. A "
		                "tap takes a globe only when the hand holds nothing: while the hand holds a miracle, the globe "
		                "stays.",
		    .environment = {.dispenserGrid = false},
		    .framing = {.shot = Shot::Placed, .eye = {12.0f, 14.0f, -32.0f}, .look = {0.0f, 1.0f, -k_Reach}},
		    .creatures = {lessonTiger},
		    .temples = {{.offset = k_ToyTemple, .angle = k_ToyTempleTurn}},
		    .dispensers = lessonGlobes,
		});
		all.push_back({
		    .id = "objects.bring_villager_home",
		    .name = "Bringing a villager home",
		    .facet = Facet::Objects,
		    .description = "A grown tiger of the player's, whose temple stands far to the west, with a villager in front of "
		                   "it. A script tells it to bring the villager home.",
		    .expected = "It picks the villager up, walks home and puts the villager down near the pen. Then the villager "
		                "falls dead at once, becomes a skeleton, and goes when the turns that its state still had to count "
		                "are over. No death is counted, and the villager keeps its life.",
		    .environment = {.dispenserGrid = false},
		    .framing = {.shot = Shot::Follow, .distance = 1.5f},
		    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f})},
		    .objects = {{.type = VillagerInfo::CelticFarmerMale, .offset = {0.0f, -k_Reach}}},
		    .temples = {{.offset = k_ToyTemple, .angle = k_ToyTempleTurn}},
		    .commands = {{.kind = Kind::DoAction, .delaySeconds = 1.0f, .object = 0, .action = "BringHome"}},
		});
		all.push_back({
		    .id = "objects.bring_live_food_home",
		    .name = "Bringing a live pig home as food",
		    .facet = Facet::Objects,
		    .description = "A grown tiger of the player's, whose temple stands far to the west, with a live pig in front "
		                   "of it. A script tells it to bring the pig home as food.",
		    .expected = "It picks the pig up. In its hand the pig falls dead at once, and its body goes in a puff of smoke "
		                "when the turns that its state still had to count are over: for a pig that grazed nothing, on "
		                "the first turn that it lies dead. No death is counted.",
		    .environment = {.dispenserGrid = false},
		    .framing = {.shot = Shot::Follow, .distance = 1.5f},
		    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f})},
		    .objects = {{.type = AnimalInfo::Pig, .offset = {0.0f, -k_Reach}, .living = true}},
		    .temples = {{.offset = k_ToyTemple, .angle = k_ToyTempleTurn}},
		    .commands = {{.kind = Kind::DoAction, .delaySeconds = 1.0f, .object = 0, .action = "BringFoodHome"}},
		});
	}

	all.push_back({
	    .id = "objects.throw_to_player",
	    .name = "Throwing a toy to the player",
	    .facet = Facet::Objects,
	    .description = "A grown tiger of the player's with a toy ball in front of it. A script tells it to play a throwing "
	                   "game with the player; keep the hand in view.",
	    .expected = "It picks the ball up and throws it at the player's hand, where the hand was as it set about it, "
	                "looking at it; then it jumps for joy.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Follow, .distance = 1.3f},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f})},
	    .objects = {{.type = MobileStaticInfo::ToyBall, .offset = {0.0f, -k_Reach}}},
	    .commands = {{.kind = Kind::DoAction, .delaySeconds = 4.0f, .action = "PlayThrowingGameWithPlayer", .with = 0}},
	});

	// The actions with which a creature gets its player's attention, or tells the player what it thinks of the player
	all.push_back({
	    .id = "mind.go_to_hand",
	    .name = "Going to the player's hand",
	    .facet = Facet::Mind,
	    .description = "A player's tiger stands to one side of the land in view. Keep the hand still on the land far from "
	                   "it, more than an eighth of the view across. A script tells it to go to the hand after two seconds.",
	    .expected = "It walks to the hand, looking at it, and stops four metres past its own radius from the hand. It "
	                "follows the hand when the hand moves, for thirty seconds at most. One time in two it then jumps for "
	                "joy. With the hand near it the action can not start.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Follow, .distance = 1.3f},
	    .creatures = {Content(CreatureType::Tiger, {-60.0f, 0.0f}, 90.0f)},
	    // A script action needs a thing to act on, though these actions do not use it
	    .objects = {{.type = TreeInfo::Birch, .offset = {-15.0f, 25.0f}}},
	    .commands = {{.kind = Kind::DoAction, .delaySeconds = 2.0f, .action = "GoToHand"}},
	});
	all.push_back({
	    .id = "mind.point_at_hand",
	    .name = "Pointing at the player's hand",
	    .facet = Facet::Mind,
	    .description =
	        "A player's tiger stands side on to the camera. The cursor puts the hand on the land beside it. A script tells "
	        "it to point at the hand after two seconds and again after eight seconds.",
	    .expected = "Each time it turns to the camera, looking at it, then points for a second at where the hand was as it "
	                "began, looking at that place, amazed.",
	    .environment = {.dispenserGrid = false, .cursor = glm::vec2(0.7f, 0.75f)},
	    .framing = {.shot = Shot::Follow, .distance = 0.6f},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f}, 90.0f)},
	    // A script action needs a thing to act on, though these actions do not use it
	    .objects = {{.type = TreeInfo::Birch, .offset = {-15.0f, 25.0f}}},
	    .commands = {{.kind = Kind::DoAction, .delaySeconds = 2.0f, .action = "PointAtHand"},
	                 {.kind = Kind::DoAction, .delaySeconds = 6.0f, .action = "PointAtHand"}},
	});
	all.push_back({
	    .id = "mind.follow_player",
	    .name = "Following the player when the camera looks away",
	    .facet = Facet::Mind,
	    .description = "A player's tiger stands in the middle of the map. The camera stands to its north and looks further "
	                   "north, away from it. A script tells it to follow the player after two seconds.",
	    .expected = "It walks to where the camera looks, looking at the camera, until it is near that place or thirty "
	                "seconds go by. It turns to the camera and plays the clip that asks the player to look at it.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 40.0f, 150.0f}, .look = {0.0f, 0.0f, 400.0f}},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f}, 0.0f)},
	    // A script action needs a thing to act on, though these actions do not use it
	    .objects = {{.type = TreeInfo::Birch, .offset = {-15.0f, 25.0f}}},
	    .commands = {{.kind = Kind::DoAction, .delaySeconds = 2.0f, .action = "FollowPlayer"}},
	});
	all.push_back({
	    .id = "mind.go_to_middle_of_screen",
	    .name = "Going to the middle of the screen",
	    .facet = Facet::Mind,
	    .description = "A player's tiger stands near the edge of the view, the camera looking at the land beside it. A "
	                   "script tells it to go to the middle of the screen after two seconds.",
	    .expected = "It walks to where the camera looks, looking at the camera, then turns to the camera.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 60.0f, -120.0f}, .look = {0.0f, 0.0f, 0.0f}},
	    .creatures = {Content(CreatureType::Tiger, {35.0f, 10.0f}, 0.0f)},
	    // A script action needs a thing to act on, though these actions do not use it
	    .objects = {{.type = TreeInfo::Birch, .offset = {-15.0f, 25.0f}}},
	    .commands = {{.kind = Kind::DoAction, .delaySeconds = 2.0f, .action = "GoToMiddleOfScreen"}},
	});
	all.push_back({
	    .id = "mind.be_cross",
	    .name = "Being cross with the player",
	    .facet = Facet::Mind,
	    .description = "A player's tiger is slapped four times, so that its attitude to the player falls below minus a "
	                   "tenth. A script then tells it to be cross with the player.",
	    .expected = "It walks to where the camera looks, looking at the camera, turns to the camera, then one time in two "
	                "makes a rude sign, else taunts the player.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Follow, .distance = 0.6f},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f}, 180.0f)},
	    // A script action needs a thing to act on, though these actions do not use it
	    .objects = {{.type = TreeInfo::Birch, .offset = {-15.0f, 25.0f}}},
	    .commands = {{.kind = Kind::Slap, .delaySeconds = 0.5f},
	                 {.kind = Kind::Slap, .delaySeconds = 0.1f},
	                 {.kind = Kind::Slap, .delaySeconds = 0.1f},
	                 {.kind = Kind::Slap, .delaySeconds = 0.1f},
	                 {.kind = Kind::DoAction, .delaySeconds = 2.0f, .action = "BeCrossWithPlayer"}},
	});
	all.push_back({
	    .id = "mind.hold_keeps_plan",
	    .name = "A creature keeps its plan during a hold",
	    .facet = Facet::Mind,
	    .description = "The hand holds a player's tiger, strokes its head and lets go. Soon after, the hand holds it, "
	                   "strokes it and lets go again.",
	    .expected = "During each hold the log shows no new plan (\"sets about\"). On the first turn after each release, "
	                "the tiger starts the plan that it had during the hold again (\"starts N again after the hand let "
	                "go\"). A plan to show the player how it feels (23) also starts again, and the tiger turns to the "
	                "camera.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Follow, .distance = 1.2f},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f}, 180.0f)},
	    .commands = {{.kind = Kind::HandStroke,
	                  .delaySeconds = 0.5f,
	                  .bodyPart = static_cast<size_t>(creature_feedback::BodyPart::Head)},
	                 Act(Kind::HandLetGo, 0, 0.5f),
	                 {.kind = Kind::HandStroke,
	                  .delaySeconds = 0.4f,
	                  .bodyPart = static_cast<size_t>(creature_feedback::BodyPart::Head)},
	                 Act(Kind::HandLetGo, 0, 0.5f)},
	});
	all.push_back({
	    .id = "mind.restart_after_hold",
	    .name = "A plan starts again after a hold",
	    .facet = Facet::Mind,
	    .description = "The hand holds a player's tiger, strokes its head and lets go.",
	    .expected = "On the first turn after the release, the tiger starts the plan that it had during the hold again "
	                "from the first step. On the next turn, the feedback of the stroke arrives and stops that plan. The "
	                "log shows the restart one time only.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Follow, .distance = 1.2f},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f}, 180.0f)},
	    .commands = {Stroke(0, creature_feedback::BodyPart::Head, 1.0f), Act(Kind::HandLetGo, 0, 1.0f, true)},
	});
	all.push_back({
	    .id = "mind.stage_zero_body_desires",
	    .name = "The desires of the body before the first stage",
	    .facet = Facet::Mind,
	    .description = "A player's tiger goes back to the first stage of its growth, stage 0.",
	    .expected = "Its desires of fear, tiredness and rest stay at 0 or almost 0, and it does not act on them.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Follow, .distance = 1.2f},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f}, 180.0f)},
	    .commands = {{.kind = Kind::SetPhase, .creature = 0, .delaySeconds = 0.5f, .value = 0}},
	});
	all.push_back({
	    .id = "mind.show_how_nice",
	    .name = "Showing the player how nice it thinks the player is",
	    .facet = Facet::Mind,
	    .description = "A player's tiger is stroked, and a script tells it to show the player how nice it thinks the player "
	                   "is. Then it is slapped four times and told again.",
	    .expected = "The first time it turns to the camera and waves. The second time it turns to the camera and plays the "
	                "sad clip. It looks at the camera all the time.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Follow, .distance = 0.6f},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f}, 180.0f)},
	    // A script action needs a thing to act on, though these actions do not use it
	    .objects = {{.type = TreeInfo::Birch, .offset = {-15.0f, 25.0f}}},
	    .commands = {{.kind = Kind::Stroke, .delaySeconds = 0.5f},
	                 {.kind = Kind::DoAction, .delaySeconds = 1.5f, .action = "ShowPlayerHowNiceYouThinkHeIs"},
	                 {.kind = Kind::Slap, .delaySeconds = 8.0f},
	                 {.kind = Kind::Slap, .delaySeconds = 0.1f},
	                 {.kind = Kind::Slap, .delaySeconds = 0.1f},
	                 {.kind = Kind::Slap, .delaySeconds = 0.1f},
	                 {.kind = Kind::DoAction, .delaySeconds = 2.0f, .action = "ShowPlayerHowNiceYouThinkHeIs"}},
	});
	all.push_back({
	    .id = "mind.pray_to_player",
	    .name = "Praying to the player",
	    .facet = Facet::Mind,
	    .description = "A player's tiger stands facing away from the camera. A script tells it to pray to the player after "
	                   "two seconds.",
	    .expected = "It turns to the camera, looking at it, then kneels and prays.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Follow, .distance = 0.6f},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f}, 180.0f)},
	    // A script action needs a thing to act on, though these actions do not use it
	    .objects = {{.type = TreeInfo::Birch, .offset = {-15.0f, 25.0f}}},
	    .commands = {{.kind = Kind::DoAction, .delaySeconds = 2.0f, .action = "PrayToPlayer"}},
	});
	all.push_back({
	    .id = "mind.be_silly",
	    .name = "Being silly with the player",
	    .facet = Facet::Mind,
	    .description = "A player's tiger stands facing away from the camera. A script tells it to be silly with the player "
	                   "after two seconds.",
	    .expected = "Four times: it turns to the camera and talks, fluttering its eyelids; three times in four it shows that "
	                "it feels playful; one time in two it laughs and talks again; then it pulls a silly face for five "
	                "seconds.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Follow, .distance = 0.6f},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f}, 180.0f)},
	    // A script action needs a thing to act on, though these actions do not use it
	    .objects = {{.type = TreeInfo::Birch, .offset = {-15.0f, 25.0f}}},
	    .commands = {{.kind = Kind::DoAction, .delaySeconds = 2.0f, .action = "BeSillyWithPlayer"}},
	});
	all.push_back({
	    .id = "mind.throw_at_camera",
	    .name = "Throwing a thing at the camera",
	    .facet = Facet::Mind,
	    .description = "A player's tiger with a toy ball in front of it. A script tells it to throw the ball at the camera.",
	    .expected = "It picks the ball up, looking at it, then throws it at the camera, looking at the camera.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Follow, .distance = 1.3f},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f})},
	    .objects = {{.type = MobileStaticInfo::ToyBall, .offset = {0.0f, -k_Reach}}},
	    .commands = {{.kind = Kind::DoAction, .delaySeconds = 4.0f, .action = "ThrowAtCamera", .with = 0}},
	});
	all.push_back({
	    .id = "mind.show_player_object",
	    .name = "Showing the player a thing",
	    .facet = Facet::Mind,
	    .description = "A player's tiger with a toy ball in front of it, the camera looking away from it. A script tells it "
	                   "to show the player the ball.",
	    .expected = "It picks the ball up, walks to where the camera looks, looking at the camera, turns to the camera and "
	                "puts the ball down. Its face shows its attitude to the player as it turns and puts the ball down.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 40.0f, 150.0f}, .look = {0.0f, 0.0f, 400.0f}},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f})},
	    .objects = {{.type = MobileStaticInfo::ToyBall, .offset = {0.0f, -k_Reach}}},
	    .commands = {{.kind = Kind::DoAction, .delaySeconds = 4.0f, .action = "ShowPlayerAnObject", .with = 0}},
	});

	auto stoner = Content(CreatureType::Tiger, {0.0f, 0.0f});
	stoner.desires = {{.desire = creature_desires::Desire::GetHigh, .fraction = 1.0f}};
	all.push_back({
	    .id = "objects.magic_mushroom",
	    .name = "Eating a magic mushroom",
	    .facet = Facet::Objects,
	    .description = "A grown tiger with a magic mushroom in front of it, wanting to get high as much as it can. It "
	                   "picks the mushroom up and eats it by itself.",
	    .expected = "Once it has eaten it, it becomes high: amazed, it turns to the camera and talks; its eyes go wide and "
	                "dart about, and smiling it stares at the camera, plays dazed and confused actions, half the time "
	                "jumps for joy, and half the time walks to a clear place, sits there sadly ten to fifteen seconds and "
	                "starts up frightened. At last it turns to the camera amazed, talks and plays a dazed action again.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Follow, .distance = 1.3f},
	    .creatures = {stoner},
	    .objects = {{.type = FeatureInfo::MagicMushroom, .offset = {0.0f, -k_Reach}}},
	});

	auto hungry = Posed(CreatureType::Tiger, {0.0f, 0.0f});
	hungry.needs = {.energy = 0.3f};
	hungry.hold = false;
	all.push_back({
	    .id = "objects.eat_held",
	    .name = "Eating by picking food up",
	    .facet = Facet::Objects,
	    .description = "A hungry tiger with three piles of food about it picks each up and eats it from its hand.",
	    .expected = "It walks up to each, picks it up, brings it to its mouth and eats it; the food is gone and its "
	                "energy rises in the spawner.",
	    .framing = {.shot = Shot::Follow, .distance = 1.3f},
	    .creatures = {hungry},
	    .objects = {{.type = MobileObjectInfo::MagicFood, .offset = {0.0f, -k_Reach}},
	                {.type = MobileObjectInfo::MagicFood, .offset = {-15.0f, -5.0f}},
	                {.type = MobileObjectInfo::MagicFood, .offset = {15.0f, -5.0f}}},
	    .commands = {OnObject(Kind::PickUp, 0, 0, 1.0f), Act(Kind::EatHeld, 0, 0.5f, true), OnObject(Kind::PickUp, 0, 1, 1.0f),
	                 Act(Kind::EatHeld, 0, 0.5f, true), OnObject(Kind::PickUp, 0, 2, 1.0f), Act(Kind::EatHeld, 0, 0.5f, true)},
	});

	all.push_back({
	    .id = "objects.eat_poor_food",
	    .name = "Eating a handful of food, then one of wood",
	    .facet = Facet::Objects,
	    .description = "A hungry tiger picks up a handful of food and eats it, then picks up a handful of wood and eats it.",
	    .expected = "It keeps the food down and its energy rises. The wood has no food in it: the tiger is sick after it "
	                "eats the wood, is not hungry for twenty seconds, and does not eat a pot again.",
	    .framing = {.shot = Shot::Follow, .distance = 1.3f},
	    .creatures = {hungry},
	    .objects = {{.type = PotInfo::HandFood, .offset = {0.0f, -k_Reach}, .amount = 300},
	                {.type = PotInfo::HandWood, .offset = {-15.0f, -5.0f}, .amount = 300}},
	    .commands = {OnObject(Kind::PickUp, 0, 0, 1.0f), Act(Kind::EatHeld, 0, 0.5f, true), OnObject(Kind::PickUp, 0, 1, 1.0f),
	                 Act(Kind::EatHeld, 0, 0.5f, true)},
	});

	all.push_back({
	    .id = "objects.bring_back_up",
	    .name = "Bringing up what can't be eaten",
	    .facet = Facet::Objects,
	    .description = "A hungry tiger picks up a villager a script has made indestructible, as Sable is for her lessons, "
	                   "and tries to eat her.",
	    .expected = "It brings her to its mouth but doesn't swallow her: she falls from its hand unhurt, and it retches "
	                "and is sick, then isn't hungry for twenty seconds.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Follow, .distance = 1.3f},
	    .creatures = {hungry},
	    .objects = {{.type = VillagerInfo::CelticHousewifeFemale, .offset = {0.0f, -k_Reach}, .indestructible = true}},
	    .commands = {OnObject(Kind::PickUp, 0, 0, 1.0f), Act(Kind::EatHeld, 0, 0.5f, true)},
	});

	// Giving from the player's hand: the hand takes the thing, held in front of the creature, and gives it
	const std::vector<Command> giving {{.kind = Kind::HandTake, .delaySeconds = 1.0f, .object = 0},
	                                   {.kind = Kind::HandGive, .creature = 0, .delaySeconds = 1.5f}};
	const HandHold offering {.offset = {0.0f, -k_Reach}, .height = 12.0f};
	// Its mind runs, to take the thing and choose what to do with it
	auto hungryThinking = hungry;
	hungryThinking.pauseMind = false;
	all.push_back({
	    .id = "creature.give_food",
	    .name = "Giving food from the hand",
	    .facet = Facet::Objects,
	    .description = "The player's hand takes a pile of food and, held in front of a hungry tiger, gives it to it.",
	    .expected = "The hand turns its palm over to offer the food; the tiger turns to the camera looking curious, reaches "
	                "out, and the hand meets its hand as it takes the food; hungry, it then eats it.",
	    .environment = {.cursor = glm::vec2(0.5f, 0.6f), .temple = glm::vec2(0.0f, 80.0f)},
	    .framing = {.shot = Shot::Follow, .distance = 1.3f},
	    .creatures = {hungryThinking},
	    .objects = {{.type = MobileObjectInfo::MagicFood, .offset = {-15.0f, -15.0f}}},
	    .commands = giving,
	    .hand = offering,
	});

	// The hand gives the food, then moves far away while the creature reaches for it
	constexpr float k_CallOffSweepDelay = 0.3f;
	constexpr float k_CallOffSweepSeconds = 0.2f;
	auto callingOff = giving;
	callingOff.push_back({.kind = Kind::PointerSweep,
	                      .delaySeconds = k_CallOffSweepDelay,
	                      .point = {0.45f, 0.0f},
	                      .amount = k_CallOffSweepSeconds});
	all.push_back({
	    .id = "creature.give_called_off",
	    .name = "Calling off a gift by moving the hand away",
	    .facet = Facet::Objects,
	    .description = "The player's hand gives a pile of food to a hungry tiger. As the tiger reaches out for it, the "
	                   "pointer moves fast to the side, and the hand moves with it.",
	    .expected = "The tiger stops the gift: its hand closes on nothing, and the food stays in the player's hand.",
	    .environment = {.cursor = glm::vec2(0.5f, 0.6f), .temple = glm::vec2(0.0f, 80.0f)},
	    .framing = {.shot = Shot::Follow, .distance = 1.3f},
	    .creatures = {hungryThinking},
	    .objects = {{.type = MobileObjectInfo::MagicFood, .offset = {-15.0f, -15.0f}}},
	    .commands = callingOff,
	    .hand = offering,
	});

	// The hand gives the food, and then the select button and the Action button go down and up while the tiger reaches
	// for the food
	auto pressesInTheWait = giving;
	pressesInTheWait.push_back({.kind = Kind::PointerPress, .delaySeconds = 0.3f, .value = 1});
	pressesInTheWait.push_back({.kind = Kind::PointerRelease, .delaySeconds = 0.3f, .value = 1});
	pressesInTheWait.push_back({.kind = Kind::PointerPress, .delaySeconds = 0.3f, .value = 3});
	pressesInTheWait.push_back({.kind = Kind::PointerRelease, .delaySeconds = 0.3f, .value = 3});
	all.push_back({
	    .id = "hand.gift_wait_presses",
	    .name = "Presses while the hand waits after a gift",
	    .facet = Facet::Hand,
	    .description = "The player's hand gives a pile of food to a hungry tiger. While the tiger reaches for the food, "
	                   "the select button goes down and up, and then the Action button goes down and up.",
	    .expected = "From the gift until the tiger takes the food, the hand shows no tooltip. The select button grips "
	                "no land and makes no sound. The Action button gives nothing again and does not make the hand ready "
	                "to throw. The tiger takes the food and eats it.",
	    .environment = {.cursor = glm::vec2(0.5f, 0.6f), .temple = glm::vec2(0.0f, 80.0f)},
	    .framing = {.shot = Shot::Follow, .distance = 1.3f},
	    .creatures = {hungryThinking},
	    .objects = {{.type = MobileObjectInfo::MagicFood, .offset = {-15.0f, -15.0f}}},
	    .commands = pressesInTheWait,
	    .hand = offering,
	});

	all.push_back({
	    .id = "creature.give_animal_kept_at_home",
	    .name = "Giving an animal from afar to a creature kept at home",
	    .facet = Facet::Objects,
	    .description = "A hungry tiger is kept within ten metres of its home, as in the first land's lessons. The player's "
	                   "hand takes a pig from forty metres away and gives it to the tiger.",
	    .expected = "The tiger takes the pig from the hand and eats it at once. It does not look the pig over first: the pig "
	                "in the hand is where the hand holds it, not where the hand took it.",
	    .environment = {.dispenserGrid = false, .cursor = glm::vec2(0.5f, 0.6f)},
	    .framing = {.shot = Shot::Follow, .distance = 1.3f},
	    .creatures = {CreatureSetup {
	        .species = CreatureType::Tiger,
	        .needs = {.energy = 0.05f, .dehydration = 0.0f, .poo = 0.0f},
	        .desires = {{.desire = Desire::Hunger, .fraction = 1.0f}},
	    }},
	    .objects = {{.type = AnimalInfo::Pig, .offset = {0.0f, -40.0f}, .living = true}},
	    .commands = {{.kind = Kind::ConfineToHome, .delaySeconds = 0.1f, .radius = 10.0f},
	                 {.kind = Kind::HandTake, .delaySeconds = 1.0f, .object = 0},
	                 {.kind = Kind::HandGive, .creature = 0, .delaySeconds = 1.5f}},
	    .hand = offering,
	});

	auto curious = Posed(CreatureType::Tiger, {0.0f, 0.0f});
	curious.hold = false;
	curious.pauseMind = false;
	// A creature that moves far from the hand while the hand takes the villager cannot take her. Its mind starts a short
	// time before the hand gives her.
	constexpr float k_ThinksBeforeTheGive = 2.0f;
	auto curiousAtTheGive = curious;
	curiousAtTheGive.pauseMind = true;
	curiousAtTheGive.thinksAfterSeconds = k_ThinksBeforeTheGive;
	all.push_back({
	    .id = "creature.give_villager",
	    .name = "Giving a villager from the hand",
	    .facet = Facet::Objects,
	    .description = "The player's hand takes a villager and, held in front of a tiger that isn't hungry, gives her to it.",
	    .expected = "The hand turns the villager a quarter round to offer her; the tiger turns to the camera, reaches out and "
	                "takes her from the hand, then does with her what its desires say: looks her over the first time if it "
	                "wants nothing much, eats her if hungry, or throws her at the camera if it wants to play.",
	    .environment = {.dispenserGrid = false, .cursor = glm::vec2(0.5f, 0.6f), .temple = glm::vec2(0.0f, 80.0f)},
	    .framing = {.shot = Shot::Follow, .distance = 1.3f},
	    .creatures = {curiousAtTheGive},
	    .objects = {{.type = VillagerInfo::CelticHousewifeFemale, .offset = {-15.0f, -15.0f}}},
	    .commands = giving,
	    .hand = offering,
	});

	// It wants to play with the player and nothing else, so it throws what it is given at the camera
	auto playful = curiousAtTheGive;
	playful.desires = {{.desire = Desire::PlayWithPlayer, .fraction = 1.0f}};
	playful.hold = true;
	all.push_back({
	    .id = "creature.throws_villager",
	    .name = "A creature throws a villager it is given",
	    .facet = Facet::Objects,
	    .description = "The player's hand takes a villager and, held in front of a tiger that wants to play with the "
	                   "player, gives her to it.",
	    .expected = "The tiger takes her with its take clip and throws her at the camera with its throw clip, looking at "
	                "the camera. She leaves its hand at the moment of the throw and flies with her thrown clip, then "
	                "lands and gets up.",
	    .environment = {.dispenserGrid = false, .cursor = glm::vec2(0.5f, 0.6f), .temple = glm::vec2(0.0f, 80.0f)},
	    .framing = {.shot = Shot::Follow, .distance = 1.3f},
	    .creatures = {playful},
	    .objects = {{.type = VillagerInfo::CelticHousewifeFemale, .offset = {-15.0f, -15.0f}}},
	    .commands = giving,
	    .hand = offering,
	});

	all.push_back({
	    .id = "creature.picks_up_villager",
	    .name = "A creature picks up, puts down and throws a villager",
	    .facet = Facet::Objects,
	    .description = "A tiger picks up the villager in front of it and puts her down. Then it picks her up again and "
	                   "throws her to the west.",
	    .expected = "The tiger reaches down with its pick-up clips and takes her at the moment of the pick-up; in its "
	                "hand she plays her scared stiff clip. It puts her down with its put-down clip. Then it picks her up "
	                "and throws her with its throw clip: she leaves its hand at the moment of the throw, flies with her "
	                "thrown clip, lands and gets up.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Follow, .distance = 1.3f},
	    .creatures = {Posed(CreatureType::Tiger, {0.0f, 0.0f})},
	    .objects = {{.type = VillagerInfo::CelticHousewifeFemale, .offset = {0.0f, -k_Reach}}},
	    .commands = {OnObject(Kind::PickUp, 0, 0, 1.0f), Act(Kind::PutDown, 0, 1.5f, true), OnObject(Kind::PickUp, 0, 0, 1.5f),
	                 Go(Kind::ThrowAt, 0, {-40.0f, -10.0f}, 1.5f)},
	});

	// A leash order stops the plan while the tiger puts the villager down. The put-down continues and lets her go at its
	// moment.
	all.push_back({
	    .id = "creature.put_down_through_an_order",
	    .name = "A put-down that continues through a leash order",
	    .facet = Facet::Objects,
	    .description = "A tiger on the learning leash picks up the villager in front of it and starts to put her down. "
	                   "A fifth of a second later, the player tells it with the leash to go to a point to the north.",
	    .expected = "The order does not cut the put-down, and it does not hurry it, because the tiger holds her. The "
	                "tiger lets her go at the moment of the put-down, with no speed, and she stands on the land. When the "
	                "put-down ends, the tiger walks to the point. It does not carry her there.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Follow, .distance = 1.3f},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f})},
	    .objects = {{.type = VillagerInfo::CelticHousewifeFemale, .offset = {0.0f, -k_Reach}}},
	    .commands = {Leash(0, LeashType::Rope, 1.0f), OnObject(Kind::PickUp, 0, 0, 1.0f), Act(Kind::PutDown, 0, 1.5f, true),
	                 Go(Kind::LeashOrderAt, 0, {0.0f, 40.0f}, 0.2f, false)},
	});

	// The hand strokes the tiger and lets it go while the tiger puts the villager down. The feedback stops the plan, but
	// the put-down continues and lets her go at its moment.
	all.push_back({
	    .id = "creature.put_down_through_a_stroke",
	    .name = "A put-down that continues through a stroke",
	    .facet = Facet::Objects,
	    .description = "A tiger picks up the villager in front of it and starts to put her down. A tenth of a second later, "
	                   "the hand takes hold of the tiger to stroke its head. The put-down is too new for the stroke, so only "
	                   "the hold counts. A tenth of a second after that, the hand lets go.",
	    .expected = "The feedback of the hand stops what the tiger does, but not the put-down. The tiger lets the villager "
	                "go at the moment of the put-down, with no speed, and she stands on the land. The tiger does not keep "
	                "her in its hand.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Follow, .distance = 1.3f},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f})},
	    .objects = {{.type = VillagerInfo::CelticHousewifeFemale, .offset = {0.0f, -k_Reach}}},
	    .commands = {OnObject(Kind::PickUp, 0, 0, 1.0f),
	                 Act(Kind::PutDown, 0, 1.5f, true),
	                 {.kind = Kind::HandStroke,
	                  .delaySeconds = 0.1f,
	                  .bodyPart = static_cast<size_t>(creature_feedback::BodyPart::Head)},
	                 Act(Kind::HandLetGo, 0, 0.1f)},
	});

	// A script takes the tiger while it puts the villager down. The put-down continues, and the action of the script
	// waits.
	{
		constexpr uint32_t k_HappyAnimation = 55;
		all.push_back({
		    .id = "creature.put_down_through_a_script",
		    .name = "A put-down that continues when a script takes the creature",
		    .facet = Facet::Objects,
		    .description = "A tiger picks up the villager in front of it and starts to put her down. A fifth of a second "
		                   "later, a script takes the tiger and tells it to play the happy animation two times.",
		    .expected = "The script does not cut the put-down. The tiger lets the villager go at the moment of the "
		                "put-down, with no speed, and she stands on the land. When the put-down ends, the tiger plays the "
		                "happy animation.",
		    .environment = {.dispenserGrid = false},
		    .framing = {.shot = Shot::Follow, .distance = 1.3f},
		    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f})},
		    .objects = {{.type = VillagerInfo::CelticHousewifeFemale, .offset = {0.0f, -k_Reach}}},
		    .commands = {OnObject(Kind::PickUp, 0, 0, 1.0f),
		                 Act(Kind::PutDown, 0, 1.5f, true),
		                 {.kind = Kind::ScriptPlay, .delaySeconds = 0.2f, .value = k_HappyAnimation, .amount = 2.0f}},
		});
	}

	// A leash order stops the plan while the tiger reaches for the villager. The pick-up continues and takes her at its
	// moment.
	all.push_back({
	    .id = "creature.pick_up_through_an_order",
	    .name = "A pick-up that continues through a leash order",
	    .facet = Facet::Objects,
	    .description = "A tiger on the learning leash starts to pick up the villager in front of it. A fifth of a second "
	                   "later, the player tells it with the leash to go to a point to the north.",
	    .expected = "The order does not cut the pick-up. The tiger plays its reach to the end and takes the villager at "
	                "the moment of the pick-up; in its hand she plays her scared stiff clip. When the pick-up ends, the "
	                "tiger walks to the point.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Follow, .distance = 1.3f},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f})},
	    .objects = {{.type = VillagerInfo::CelticHousewifeFemale, .offset = {0.0f, -k_Reach}}},
	    .commands = {Leash(0, LeashType::Rope, 1.0f), OnObject(Kind::PickUp, 0, 0, 1.0f, false),
	                 Go(Kind::LeashOrderAt, 0, {0.0f, 40.0f}, 0.2f, false)},
	});

	// A leash order stops the plan while the tiger points. The arm does not hold its point: it plays the end of the point
	// at once.
	all.push_back({
	    .id = "creature.point_through_an_order",
	    .name = "A point that ends through a leash order",
	    .facet = Facet::Objects,
	    .description = "A tiger on the learning leash points at the land in front of it, for five seconds. One second "
	                   "later, the player tells it with the leash to go to a point to the north.",
	    .expected = "The arm does not stop at once and does not hold its point for the five seconds. At the order, the arm "
	                "plays the end of its point animations. When the point ends, the tiger walks to the point.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Follow, .distance = 1.3f},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f})},
	    .commands = {Leash(0, LeashType::Rope, 1.0f), Go(Kind::PointAt, 0, {0.0f, -40.0f}, 1.0f, false),
	                 Go(Kind::LeashOrderAt, 0, {0.0f, 40.0f}, 1.0f, false)},
	});

	auto hungryAtTheGive = hungryThinking;
	hungryAtTheGive.pauseMind = true;
	hungryAtTheGive.thinksAfterSeconds = k_ThinksBeforeTheGive;
	all.push_back({
	    .id = "creature.give_villager_eaten",
	    .name = "Giving a villager to a hungry creature",
	    .facet = Facet::Objects,
	    .description = "The player's hand takes a villager and, held in front of a hungry tiger, gives her to it.",
	    .expected = "The tiger reaches out with its take clip and takes her; in its hand she plays her scared stiff clip. "
	                "Its hunger is its strongest desire, so it plays its eat clip at once: at the moment of "
	                "the bite she is gone from its hand, and it swallows her as the clip ends. Then she dies and its "
	                "hunger falls.",
	    .environment = {.dispenserGrid = false, .cursor = glm::vec2(0.5f, 0.6f), .temple = glm::vec2(0.0f, 80.0f)},
	    .framing = {.shot = Shot::Follow, .distance = 1.3f},
	    .creatures = {hungryAtTheGive},
	    .objects = {{.type = VillagerInfo::CelticHousewifeFemale, .offset = {-15.0f, -15.0f}}},
	    .commands = giving,
	    .hand = offering,
	});

	all.push_back({
	    .id = "creature.give_villager_by_hand",
	    .name = "Giving a villager by hand",
	    .facet = Facet::Objects,
	    .description = "A tiger that isn't hungry stands with a villager in front of it. The player picks her up with the "
	                   "hand, holds her in front of the tiger and holds the Action button down to give her.",
	    .expected = "In the hand she plays her scared stiff clip. The hand turns her a quarter round to offer her; the "
	                "tiger turns to the camera, reaches out with its take clip and takes her at the moment of the take; "
	                "in its hand she keeps her scared stiff clip while it looks her over or does what its desires say.",
	    .environment = {.dispenserGrid = false, .temple = glm::vec2(0.0f, 80.0f)},
	    .framing = {.shot = Shot::Follow, .distance = 1.3f},
	    .creatures = {curious},
	    .objects = {{.type = VillagerInfo::CelticHousewifeFemale, .offset = {-15.0f, -15.0f}}},
	});

	// The pictures for the creature's web page: the camera looks at the tiger, and each action played once has its
	// moment halfway through. The first picture can come once more than ten seconds of the game went by.
	all.push_back({
	    .id = "creature.web_page",
	    .name = "The creature's web page",
	    .facet = Facet::Objects,
	    .description = "The camera looks at a tiger that plays the impress action after twelve seconds; then the game "
	                   "writes the tiger's web page from Scripts/creature_html.tmpl into the profile folder.",
	    .expected = "Halfway through the impress action the game takes a picture of the screen: creatureshot_0.jpg, 400 "
	                "by 300, in the player's profile folder. Then html/<the tiger's name>.html shows its values, the "
	                "picture and the decorations from the game folder, and creature.lhp holds the values and the "
	                "picture.",
	    .framing = {.shot = Shot::Follow, .distance = 1.3f},
	    .creatures = {Posed(CreatureType::Tiger, {0.0f, 0.0f})},
	    .commands = {Play(Kind::PlayAction, 0, animations::k_Impress, 12.0f, true),
	                 Command {.kind = Kind::WriteWebPage, .delaySeconds = 4.0f, .waitUntilFree = true}},
	});

	auto ogre = hungryThinking;
	ogre.species = CreatureType::Ogre;
	all.push_back({
	    .id = "creature.give_to_ogre",
	    .name = "Giving food to an ogre",
	    .facet = Facet::Objects,
	    .description = "The player's hand takes a pile of food and, held in front of a hungry ogre, tries to give it to it.",
	    .expected = "An ogre takes nothing from the hand: the hand doesn't offer the food, the ogre doesn't reach for it, "
	                "and the hand lets the food go as it would anywhere else.",
	    .environment = {.cursor = glm::vec2(0.5f, 0.6f), .temple = glm::vec2(0.0f, 80.0f)},
	    .framing = {.shot = Shot::Follow, .distance = 1.3f},
	    .creatures = {ogre},
	    .objects = {{.type = MobileObjectInfo::MagicFood, .offset = {-15.0f, -15.0f}}},
	    .commands = giving,
	    .hand = offering,
	});

	// A hungry grown-up tiger that thinks for itself, with villagers about it
	auto eater = Content(CreatureType::Tiger, {0.0f, 0.0f});
	eater.phase = 13;
	eater.hold = false;
	// It becomes hungry as the hand gives, so that it eats what it is given and not the villagers about it
	const Command hungryNow {.kind = Kind::SetDesire,
	                         .creature = 0,
	                         .delaySeconds = 1.4f,
	                         .value = static_cast<size_t>(Desire::Hunger),
	                         .amount = 0.9f};
	auto giveNow = giving[1];
	giveNow.delaySeconds = 0.1f;
	// The hand strokes it as soon as the meal lets it: a stroke too early in the meal does nothing
	std::vector<Command> eatAgain {giving[0], hungryNow, giveNow};
	for (int i = 0; i < 12; ++i)
	{
		auto stroke = Stroke(0, creature_feedback::BodyPart::Head, i == 0 ? 2.5f : 0.3f);
		stroke.waitUntilFree = false;
		eatAgain.push_back(stroke);
	}
	eatAgain.push_back(Act(Kind::HandLetGo, 0, 0.5f, false));
	all.push_back({
	    .id = "mind.stroked_eat_again",
	    .name = "Stroked while it eats from the hand",
	    .facet = Facet::Mind,
	    .description = "The player's hand gives a villager to a hungry grown-up tiger. As the tiger starts to eat her, the "
	                   "hand strokes it. Two more villagers of the same kind stand near.",
	    .expected = "The hand strokes the tiger while it eats. When the hand lets go, the tiger learns to eat what it is "
	                "given and eats the villager. When the meal is done, it tries to do the lesson again on a different "
	                "villager. The log shows the try. The villager that it ate is gone, so it does what it wants next.",
	    .environment = {.dispenserGrid = false, .cursor = glm::vec2(0.5f, 0.6f), .temple = glm::vec2(0.0f, 80.0f)},
	    .framing = {.shot = Shot::Follow, .distance = 1.5f},
	    .creatures = {eater},
	    .objects = {{.type = VillagerInfo::CelticHousewifeFemale, .offset = {-15.0f, -15.0f}},
	                {.type = VillagerInfo::CelticHousewifeFemale, .offset = {30.0f, 25.0f}},
	                {.type = VillagerInfo::CelticHousewifeFemale, .offset = {-35.0f, 30.0f}}},
	    .commands = eatAgain,
	    .hand = offering,
	});

	all.push_back({
	    .id = "objects.knock_down_trees",
	    .name = "Knocking down trees",
	    .facet = Facet::Objects,
	    .description = "A tiger walks up to each of three trees in a row and knocks it down.",
	    .expected = "It strikes each tree, which falls; its town's view of it shows in the spawner.",
	    .framing = {.shot = Shot::Overview},
	    .creatures = {Posed(CreatureType::Tiger, {0.0f, -20.0f})},
	    .objects = {{.type = TreeInfo::Oak, .offset = {-40.0f, 10.0f}},
	                {.type = TreeInfo::Beech, .offset = {0.0f, 10.0f}},
	                {.type = TreeInfo::Conifer, .offset = {40.0f, 10.0f}}},
	    .commands = {OnObject(Kind::KnockDown, 0, 0, 1.0f), OnObject(Kind::KnockDown, 0, 1, 1.0f),
	                 OnObject(Kind::KnockDown, 0, 2, 1.0f)},
	});

	all.push_back({
	    .id = "objects.point",
	    .name = "Pointing",
	    .facet = Facet::Objects,
	    .description = "A tiger facing the camera points in front of it, to either side, far off and behind it.",
	    .expected = "It turns as needed and points its arm at each point in turn.",
	    .framing = {.shot = Shot::Follow, .distance = 1.4f},
	    .creatures = {Posed(CreatureType::Tiger, {0.0f, 0.0f})},
	    .commands = {Go(Kind::PointAt, 0, {0.0f, -40.0f}, 1.0f), Go(Kind::PointAt, 0, {-40.0f, 0.0f}, 1.0f),
	                 Go(Kind::PointAt, 0, {40.0f, 0.0f}, 1.0f), Go(Kind::PointAt, 0, {0.0f, 200.0f}, 1.0f),
	                 Go(Kind::TurnToFace, 0, {0.0f, -60.0f}, 1.0f)},
	    .repeatFrom = 0,
	});

	// Left to the mind: one desire as strong as it gets, with things about to act on
	struct Mood
	{
		std::string_view id;
		std::string_view name;
		Desire desire;
		std::string_view description;
		std::string_view expected;
	};
	const std::array<Mood, 3> k_Moods {{
	    {"objects.curious", "Curious about things", Desire::Curiosity,
	     "A tiger as curious as it gets, wanting nothing else, among a ball, a pot and a barrel.",
	     "It walks up to something nearby, picks it up and looks it over, then mostly puts it down gently and sometimes "
	     "tosses it away, and goes on to the next."},
	    {"objects.playful", "Playing with things", Desire::Play,
	     "A tiger as playful as it gets, wanting nothing else, among a ball, a pot and a barrel.",
	     "It picks something up and throws it about, a good way off in a random direction, and goes after the next."},
	    {"objects.angry", "Angry with things", Desire::Anger,
	     "A tiger as angry as it gets, wanting nothing else, among a ball, a pot and a barrel, with trees nearby.",
	     "It picks something up, now and then shows its anger first, and hurls it at the nearest tree."},
	}};
	for (const auto& mood : k_Moods)
	{
		auto moody = Content(CreatureType::Tiger, {0.0f, 0.0f});
		moody.desires = OnlyDesire(mood.desire);
		all.push_back({
		    .id = mood.id,
		    .name = mood.name,
		    .facet = Facet::Objects,
		    .description = mood.description,
		    .expected = mood.expected,
		    .framing = {.shot = Shot::Overview, .distance = 1.2f},
		    .creatures = {moody},
		    .objects = {{.type = MobileObjectInfo::Ball, .offset = {-15.0f, -10.0f}},
		                {.type = MobileObjectInfo::EgyptPotA, .offset = {15.0f, -10.0f}},
		                {.type = MobileObjectInfo::EgyptBarrel, .offset = {0.0f, 15.0f}},
		                {.type = TreeInfo::Oak, .offset = {-50.0f, 30.0f}},
		                {.type = TreeInfo::Birch, .offset = {50.0f, 30.0f}}},
		});
	}

	// A street light is not a lantern: it has no model and no light
	all.push_back({
	    .id = "objects.street_light_has_no_model",
	    .name = "A street light of a land script beside a street lantern",
	    .facet = Facet::Objects,
	    .description = "Lines of a land make a street lantern 4 metres to one side of the middle and a street light 4 "
	                   "metres to the other side. The clock stands still at night.",
	    .expected = "The street lantern has its post and its light. Nothing shows at the other side: the street light "
	                "has no model, no light and no sound.",
	    .environment = {.hour = 21.0f, .clockRuns = false},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 6.0f, -14.0f}, .look = {0.0f, 2.0f, 0.0f}},
	    .landScript = R"(CREATE_STREET_LANTERN("-4.0,0.0", 7)
CREATE_STREET_LIGHT("4.0,0.0")
)",
	});
}
