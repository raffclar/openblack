/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The testbed's scenarios of creature actions that Land 1 can start and that no other scenario stages. A script tells
// a tiger to do each action, so that the sweep of the creature's clips sees every action

#include <string_view>
#include <vector>

#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

namespace
{
using Kind = Command::Kind;

/// A grown tiger whose needs stay full, so that only the told action shows
CreatureSetup Actor()
{
	return {.label = "actor",
	        .species = CreatureType::Tiger,
	        .facingDegrees = 0.0f,
	        .phase = 3,
	        .needs = {.energy = 1.0f, .exhaustion = 0.0f, .dehydration = 0.0f, .poo = 0.0f, .life = 1.0f}};
}

Command Do(std::string_view action, float delay, size_t object = 0)
{
	return {.kind = Kind::DoAction, .delaySeconds = delay, .object = object, .action = action};
}

/// The framing that keeps the tiger and the things near it in view
Framing Near()
{
	return {.shot = Shot::Overview, .include = {{-40.0f, -60.0f}, {40.0f, 20.0f}}};
}

/// The framing that keeps the tiger and things up to 100 metres in front of it in view
Framing Far()
{
	return {.shot = Shot::Overview, .include = {{-40.0f, -110.0f}, {40.0f, 20.0f}}};
}

/// The tiger knows the miracle, once its mind has set up what it knows, on its first turns
Command Know(MagicType type)
{
	return {.kind = Kind::KnowMiracle, .delaySeconds = 0.5f, .value = static_cast<size_t>(type)};
}

/// A script tells the tiger to do the action to another creature of the scenario
Command DoToCreature(std::string_view action, float delay, size_t creature)
{
	return {.kind = Kind::DoAction, .delaySeconds = delay, .action = action, .atCreature = creature};
}

/// A script tells the tiger to do the action to an object of the scenario, with another object
Command DoWith(std::string_view action, float delay, size_t object, size_t with)
{
	return {.kind = Kind::DoAction, .delaySeconds = delay, .object = object, .action = action, .with = with};
}

/// A script tells the tiger to pick up the one-shot globe of the scenario and to do the action with it to an object
Command DoWithGlobe(std::string_view action, float delay, size_t object)
{
	return {.kind = Kind::DoAction, .delaySeconds = delay, .object = object, .action = action, .globe = 0};
}

/// A script tells the tiger to hold the one-shot globe of the scenario
Command HoldGlobe(float delay)
{
	return {.kind = Kind::DoAction, .delaySeconds = delay, .action = "HoldObject", .globe = 0, .onGlobe = true};
}

/// A cow whose mind leaves it where it stands, for the tiger to cast at
CreatureSetup StillCow(glm::vec2 offset)
{
	return {.label = "target", .species = CreatureType::Cow, .offset = offset, .facingDegrees = 180.0f, .pauseMind = true};
}

/// A villager hurt to a third of its life, for the tiger to heal
ObjectSetup HurtVillager(glm::vec2 offset)
{
	return {.type = VillagerInfo::CelticFarmerMale, .offset = offset, .life = 0.33f};
}

/// A one-shot globe of the miracle on the land, with no dispenser
DispenserSetup Globe(MagicType type, glm::vec2 offset)
{
	return {.type = type, .offset = offset, .bubbleHeight = 0.0f};
}
} // namespace

void testbed_scenarios::AddCreatureActionScenarios(std::vector<Scenario>& all)
{
	const ObjectSetup rock {.type = MobileStaticInfo::Rock, .offset = {6.0f, 0.0f}};
	const ObjectSetup villager {.type = VillagerInfo::CelticFarmerMale, .offset = {0.0f, -40.0f}};

	all.push_back({
	    .id = "actions.go_to_hill_and_look",
	    .name = "Goes to a hill and looks about",
	    .facet = Facet::Movement,
	    .description = "A grown tiger is told to go to a hill and look about from it. The testbed is flat, so every "
	                   "region of 80 metres is a hill.",
	    .expected = "It walks to the highest cell of a region near it and looks about from there.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-160.0f, -120.0f}, {160.0f, 120.0f}}},
	    .creatures = {Actor()},
	    .objects = {rock},
	    .commands = {Do("GoToHillAndLook", 1.0f)},
	});
	all.push_back({
	    .id = "actions.go_to_hill_and_sit",
	    .name = "Goes to a hill to sit",
	    .facet = Facet::Movement,
	    .description = "A grown tiger is told to go to a hill and sit there.",
	    .expected = "It walks to the highest cell of a region near it. It does not sit down there.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-160.0f, -120.0f}, {160.0f, 120.0f}}},
	    .creatures = {Actor()},
	    .objects = {rock},
	    .commands = {Do("GoToHillAndSit", 1.0f)},
	});
	all.push_back({
	    .id = "actions.explore_coastline",
	    .name = "Explores the coast",
	    .facet = Facet::Movement,
	    .description = "A grown tiger is told to explore the coast. The testbed's lake gives it a coast to find.",
	    .expected = "It walks to a coast that its map of the land gives and looks about there.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-200.0f, -200.0f}, {200.0f, 200.0f}}},
	    .creatures = {Actor()},
	    .objects = {rock},
	    .commands = {Do("ExploreCoastline", 1.0f)},
	});
	all.push_back({
	    .id = "actions.explore_towns",
	    .name = "Explores the towns",
	    .facet = Facet::Movement,
	    .description = "A grown tiger is told to explore the towns. Two huts stand 120 metres north of it.",
	    .expected = "It walks to a town that its map of the land gives and looks about there.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-80.0f, -160.0f}, {80.0f, 20.0f}}},
	    .creatures = {Actor()},
	    .objects = {rock,
	                {.type = AbodeInfo::CelticHut, .offset = {-10.0f, -120.0f}},
	                {.type = AbodeInfo::CelticHut, .offset = {12.0f, -126.0f}}},
	    .commands = {Do("ExploreTowns", 1.0f)},
	});
	all.push_back({
	    .id = "actions.put_down",
	    .name = "Puts down what it holds",
	    .facet = Facet::Objects,
	    .description = "A grown tiger is told to hold a rock, and eight seconds later to put down what it holds.",
	    .expected = "It reaches for the rock and picks it up. Then it puts the rock down in front of it.",
	    .environment = {.dispenserGrid = false},
	    .framing = Near(),
	    .creatures = {Actor()},
	    .objects = {{.type = MobileStaticInfo::Rock, .offset = {0.0f, -12.0f}}},
	    .commands = {Do("HoldObject", 1.0f), Do("PutDown", 8.0f)},
	});
	all.push_back({
	    .id = "actions.run_away_from_player",
	    .name = "Runs away from the player",
	    .facet = Facet::Mind,
	    .description = "A grown tiger is told to run away from the player.",
	    .expected = "One time in two it starts, frightened, with wide eyes. It runs with a frightened face and looks back "
	                "in turns. One time in two it starts again at the end of the run.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Follow, .distance = 1.5f},
	    .creatures = {Actor()},
	    .objects = {rock},
	    .commands = {Do("RunAwayFromPlayer", 1.0f)},
	});
	all.push_back({
	    .id = "actions.scratch",
	    .name = "Scratches",
	    .facet = Facet::Expressions,
	    .description = "A grown tiger is told to scratch, as an itch makes it.",
	    .expected = "It plays the scratch clip once.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Follow, .distance = 1.2f},
	    .creatures = {Actor()},
	    .objects = {rock},
	    .commands = {Do("Scratch", 1.0f)},
	});
	all.push_back({
	    .id = "actions.sleep_by_tree",
	    .name = "Sleeps by a tree",
	    .facet = Facet::Needs,
	    .description = "A grown tiger is told to sleep by an oak 30 metres in front of it.",
	    .expected = "It walks to a clear place by the oak, yawns half the time, faces down the slope and sleeps until "
	                "rested. It gets up dazed, with a grimace.",
	    .environment = {.dispenserGrid = false},
	    .framing = Near(),
	    .creatures = {Actor()},
	    .objects = {{.type = TreeInfo::Oak, .offset = {0.0f, -30.0f}}},
	    .commands = {Do("SleepByObject", 1.0f)},
	});
	all.push_back({
	    .id = "actions.point_at_thing",
	    .name = "Points at a thing",
	    .facet = Facet::Expressions,
	    .description = "A grown tiger is told to point at a cow 40 metres in front of it.",
	    .expected = "It points at the cow for five seconds, amazed, and turns as the cow moves.",
	    .environment = {.dispenserGrid = false},
	    .framing = Near(),
	    .creatures = {Actor()},
	    .objects = {{.type = AnimalInfo::Cow, .offset = {0.0f, -40.0f}}},
	    .commands = {Do("PointAtObject", 1.0f)},
	});
	auto pooer = Actor();
	pooer.needs.poo = 0.9f;
	all.push_back({
	    .id = "actions.poo_at_home",
	    .name = "Poos at home",
	    .facet = Facet::Needs,
	    .description = "A grown tiger full of poo, with no temple, stands where it was made, which is its home. It is "
	                   "told to poo at home. A creature starts this action only within 20 metres of its home.",
	    .expected = "It walks until it is within two times its height of its home. Then it squats and poos there.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Follow, .distance = 1.5f},
	    .creatures = {pooer},
	    .objects = {rock},
	    .commands = {Do("PooAtHome", 1.0f)},
	});
	all.push_back({
	    .id = "actions.rest_on_the_spot",
	    .name = "Rests on the spot",
	    .facet = Facet::Needs,
	    .description = "A grown tiger is told to rest where it stands.",
	    .expected = "It waits a while where it stands, exhausted, and looks at its own feet.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Follow, .distance = 1.2f},
	    .creatures = {Actor()},
	    .objects = {rock},
	    .commands = {Do("RestOnTheSpot", 1.0f)},
	});
	all.push_back({
	    .id = "actions.look_forever",
	    .name = "Watches a thing",
	    .facet = Facet::Senses,
	    .description = "A grown tiger is told to watch a villager 40 metres in front of it.",
	    .expected = "It turns to face the villager and watches it.",
	    .environment = {.dispenserGrid = false},
	    .framing = Near(),
	    .creatures = {Actor()},
	    .objects = {villager},
	    .commands = {Do("LookForever", 1.0f)},
	});
	all.push_back({
	    .id = "actions.watch_player",
	    .name = "Watches the player",
	    .facet = Facet::Senses,
	    .description = "A grown tiger is told to watch the player.",
	    .expected = "It stands where it is and watches the player. The action plays no clip of its own.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Follow, .distance = 1.2f},
	    .creatures = {Actor()},
	    .objects = {rock},
	    .commands = {Do("WatchPlayer", 1.0f)},
	});
	all.push_back({
	    .id = "actions.frighten_villagers",
	    .name = "Frightens villagers in play",
	    .facet = Facet::Mind,
	    .description = "A grown tiger is told to frighten a villager in play.",
	    .expected = "It goes to three times its height from the villager, faces it for two seconds and plays the "
	                "frightened clip with wide eyes.",
	    .environment = {.dispenserGrid = false},
	    .framing = Near(),
	    .creatures = {Actor()},
	    .objects = {villager},
	    .commands = {Do("PlayfullyFrightenVillagers", 1.0f)},
	});
	all.push_back({
	    .id = "actions.point_out_highlight",
	    .name = "Points out a highlight",
	    .facet = Facet::Expressions,
	    .description = "A grown tiger is told to point out a villager to the player, as a lesson's highlight makes it.",
	    .expected = "When the camera is more than 30 metres away, it first looks at the camera, curious, and plays its "
	                "look-at-me. Then it points at the villager a second, amazed, and looks at the camera two seconds "
	                "more.",
	    .environment = {.dispenserGrid = false},
	    .framing = Near(),
	    .creatures = {Actor()},
	    .objects = {villager},
	    .commands = {Do("PointOutHighlight", 1.0f)},
	});
	all.push_back({
	    .id = "actions.make_breeder",
	    .name = "Makes a villager a breeder",
	    .facet = Facet::Mind,
	    .description = "A grown tiger is told to make a villager 40 metres in front of it a breeder.",
	    .expected = "It picks the villager up, strokes it with a look of compassion, makes it a breeder and puts it "
	                "down.",
	    .environment = {.dispenserGrid = false},
	    .framing = Near(),
	    .creatures = {Actor()},
	    .objects = {villager},
	    .commands = {Do("MakeBreederDisciple", 1.0f)},
	});
	all.push_back({
	    .id = "actions.eat_from_food_pile",
	    .name = "Eats from a pile of food",
	    .facet = Facet::Needs,
	    .description = "A grown tiger is told to eat from a pile of food that the food miracle made, 30 metres in front "
	                   "of it.",
	    .expected = "It walks to the pile, turns to face it, takes a handful of food from it and eats the handful.",
	    .environment = {.dispenserGrid = false},
	    .framing = Near(),
	    .creatures = {Actor()},
	    .objects = {{.type = MobileObjectInfo::MagicFood, .offset = {0.0f, -30.0f}}},
	    .commands = {Do("EatFromFoodPile", 1.0f)},
	});
	all.push_back({
	    .id = "actions.give_magic_food_to_store",
	    .name = "Gives a pile of miracle food to the store",
	    .facet = Facet::Objects,
	    .description = "A grown tiger with an empty storage pit 60 metres in front of it and a pile of food that the "
	                   "food miracle made to one side. It is told to give the food to the store.",
	    .expected = "It takes a handful from the pile, goes to where it can throw into the store and throws the food "
	                "in. The store gets the food.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-25.0f, -80.0f}, {60.0f, 20.0f}}},
	    .creatures = {Actor()},
	    .objects = {{.type = AbodeInfo::CelticStoragePit, .offset = {0.0f, -60.0f}, .storedFood = 0, .storedWood = 0},
	                {.type = MobileObjectInfo::MagicFood, .offset = {40.0f, -20.0f}}},
	    .commands =
	        {{.kind = Kind::DoAction, .delaySeconds = 1.0f, .object = 0, .action = "GiveMagicFoodToStoragePit", .with = 1}},
	});

	AddCreatureActionCastScenarios(all);
	AddCreatureActionTownScenarios(all);
}

void testbed_scenarios::AddCreatureActionCastScenarios(std::vector<Scenario>& all)
{
	const ObjectSetup pillar {.type = FeatureInfo::FatPilarChalk, .offset = {0.0f, -90.0f}, .scale = 0.5f};

	all.push_back({
	    .id = "actions.cast_fireball",
	    .name = "Casts a fireball by its desire's action",
	    .facet = Facet::Miracles,
	    .description = "A grown tiger that knows the fireball is told to do the fireball action of its desires at a "
	                   "pillar of rock 90 metres in front of it.",
	    .expected = "It goes to a clear place, turns to the rock, raises its arms and a fireball flies from between its "
	                "hands to the rock.",
	    .environment = {.dispenserGrid = false},
	    .framing = Far(),
	    .creatures = {Actor()},
	    .objects = {pillar},
	    .commands = {Know(MagicType::Fireball), Do("CastFireball", 1.0f)},
	});
	all.push_back({
	    .id = "actions.cast_explosion",
	    .name = "Casts an explosion by its desire's action",
	    .facet = Facet::Miracles,
	    .description = "A grown tiger that knows the explosion is told to do the explosion action of its desires at a "
	                   "pillar of rock 90 metres in front of it.",
	    .expected = "It walks near enough to the rock, turns to it, raises its arms and casts the explosion at the rock.",
	    .environment = {.dispenserGrid = false},
	    .framing = Far(),
	    .creatures = {Actor()},
	    .objects = {pillar},
	    .commands = {Know(MagicType::ExplosionOne), Do("CastExplosion", 1.0f)},
	});
	all.push_back({
	    .id = "actions.cast_heal",
	    .name = "Casts a heal by its desire's action",
	    .facet = Facet::Miracles,
	    .description = "A grown tiger that knows the heal is told to do the heal action of its desires at a villager hurt "
	                   "to a third of its life, 60 metres in front of it.",
	    .expected = "It walks up to the villager, faces it and casts the heal. The life of the villager fills up.",
	    .environment = {.dispenserGrid = false},
	    .framing = Near(),
	    .creatures = {Actor()},
	    .objects = {HurtVillager({0.0f, -60.0f})},
	    .commands = {Know(MagicType::Heal), Do("CastHealSpell", 1.0f)},
	});
	all.push_back({
	    .id = "actions.cast_make_creature_strong",
	    .name = "Casts the strength spell on another creature",
	    .facet = Facet::Miracles,
	    .description = "A grown tiger that knows the strength spell is told to make a cow 60 metres in front of it strong.",
	    .expected = "It walks near to the cow, faces it, raises its arms and casts the strength spell at the cow.",
	    .environment = {.dispenserGrid = false},
	    .framing = Near(),
	    .creatures = {Actor(), StillCow({0.0f, -60.0f})},
	    .commands = {Know(MagicType::CreatureSpellStrong), DoToCreature("CastMakeCreatureStrong", 1.0f, 1)},
	});
	all.push_back({
	    .id = "actions.cast_make_creature_nice",
	    .name = "Casts the compassion spell on another creature",
	    .facet = Facet::Miracles,
	    .description = "A grown tiger that knows the compassion spell is told to make a cow 60 metres in front of it nice.",
	    .expected = "It walks near to the cow, faces it, raises its arms and casts the compassion spell at the cow.",
	    .environment = {.dispenserGrid = false},
	    .framing = Near(),
	    .creatures = {Actor(), StillCow({0.0f, -60.0f})},
	    .commands = {Know(MagicType::CreatureSpellCompassion), DoToCreature("CastMakeCreatureNice", 1.0f, 1)},
	});
	all.push_back({
	    .id = "actions.cast_one_shot_in_hand_kind",
	    .name = "Casts a kind one-shot miracle that it holds",
	    .facet = Facet::Miracles,
	    .description = "A grown tiger that knows the heal is told to hold a one-shot heal globe at its feet. Then it is told "
	                   "to cast the kind one-shot miracle in its hand at a villager hurt to a third of its life.",
	    .expected = "It walks to the villager and turns to it. The globe goes from its hand, it holds the casting pose and "
	                "the heal lights over the villager.",
	    .environment = {.dispenserGrid = false},
	    .framing = Far(),
	    .creatures = {Actor()},
	    .objects = {HurtVillager({0.0f, -90.0f})},
	    .dispensers = {Globe(MagicType::Heal, {0.0f, -12.0f})},
	    .commands = {Know(MagicType::Heal), HoldGlobe(1.0f), Do("CastOneOffSpellInHandCompassiona", 10.0f)},
	});
	all.push_back({
	    .id = "actions.pick_up_and_cast_angry_one_shot",
	    .name = "Picks up an angry one-shot miracle and casts it",
	    .facet = Facet::Miracles,
	    .description = "A grown tiger that knows the lightning bolt is told to pick up a one-shot lightning globe 25 metres "
	                   "in front of it and cast it at a pillar of rock 90 metres in front of it.",
	    .expected = "It walks to the globe and picks it up. It walks near to the rock and turns to it. The globe goes from "
	                "its hand, it holds the casting pose and the bolt hits the rock.",
	    .environment = {.dispenserGrid = false},
	    .framing = Far(),
	    .creatures = {Actor()},
	    .objects = {pillar},
	    .dispensers = {Globe(MagicType::LightningBolt, {0.0f, -25.0f})},
	    .commands = {Know(MagicType::LightningBolt), DoWithGlobe("PickUpAndCastOneOffSpellAggressi", 1.0f, 0)},
	});
	all.push_back({
	    .id = "actions.sprinkle_water_on_crops",
	    .name = "Sprinkles miracle water on a field",
	    .facet = Facet::Miracles,
	    .description = "A grown tiger that knows the water miracle is told to water a field of wheat that is not sown, 50 "
	                   "metres in front of it.",
	    .expected = "It walks to the field, faces it and casts the water miracle on it.",
	    .environment = {.dispenserGrid = false},
	    .framing = Near(),
	    .creatures = {Actor()},
	    .objects = {{.type = FieldTypeInfo::Wheat, .offset = {0.0f, -50.0f}}},
	    .commands = {Know(MagicType::Water), Do("SprinkleMagicWaterOnCrops", 1.0f)},
	});
	all.push_back({
	    .id = "actions.water_tree",
	    .name = "Waters a tree with miracle water",
	    .facet = Facet::Miracles,
	    .description = "A grown tiger that knows the water miracle is told to water an oak 40 metres in front of it.",
	    .expected = "It walks to the oak, faces it and casts the water miracle on it.",
	    .environment = {.dispenserGrid = false},
	    .framing = Near(),
	    .creatures = {Actor()},
	    .objects = {{.type = TreeInfo::Oak, .offset = {0.0f, -40.0f}}},
	    .commands = {Know(MagicType::Water), Do("WaterTree", 1.0f)},
	});
	all.push_back({
	    .id = "actions.cast_wood_by_building_site",
	    .name = "Casts miracle wood by a building site",
	    .facet = Facet::Miracles,
	    .description = "A grown tiger that knows the wood miracle and a Celtic hut of the player's town that stands a third "
	                   "built, 50 metres in front of the tiger. The tiger is told to cast wood by the site.",
	    .expected = "It walks to the site, faces it and casts the wood miracle by it.",
	    .environment = {.dispenserGrid = false},
	    .framing = Near(),
	    .creatures = {Actor()},
	    .objects = {{.type = AbodeInfo::CelticHut, .offset = {0.0f, -50.0f}, .built = 0.3f}},
	    .commands = {Know(MagicType::Wood), Do("CastMagicWoodByBuildingSite", 1.0f)},
	});
}

void testbed_scenarios::AddCreatureActionTownScenarios(std::vector<Scenario>& all)
{
	const ObjectSetup emptyPit {
	    .type = AbodeInfo::CelticStoragePit, .offset = {0.0f, -60.0f}, .storedFood = 0, .storedWood = 0};

	all.push_back({
	    .id = "actions.give_fruit_to_store",
	    .name = "Gives the fruit of a tree to the store",
	    .facet = Facet::Objects,
	    .description = "A grown tiger with an empty storage pit 60 metres in front of it and an oak to one side. It is told "
	                   "to give the fruit of the tree to the store.",
	    .expected = "It goes to the oak and shakes fruit from it, takes it up, goes to where it can throw into the store "
	                "and throws the fruit in. The store gets the food.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-25.0f, -80.0f}, {60.0f, 20.0f}}},
	    .creatures = {Actor()},
	    .objects = {emptyPit, {.type = TreeInfo::Oak, .offset = {40.0f, -20.0f}}},
	    .commands = {DoWith("GiveFruitFromTreeToStoragePit", 1.0f, 0, 1)},
	});
	all.push_back({
	    .id = "actions.give_magic_wood_to_store",
	    .name = "Gives a pile of miracle wood to the store",
	    .facet = Facet::Objects,
	    .description = "A grown tiger with an empty storage pit 60 metres in front of it and a pile of wood that the wood "
	                   "miracle made to one side. It is told to give the wood to the store.",
	    .expected = "It takes up the wood, goes to where it can throw into the store and throws the wood in. The store "
	                "gets the wood.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-25.0f, -80.0f}, {60.0f, 20.0f}}},
	    .creatures = {Actor()},
	    .objects = {emptyPit, {.type = MobileObjectInfo::MagicWood, .offset = {40.0f, -20.0f}}},
	    .commands = {DoWith("GiveMagicWoodToStoragePit", 1.0f, 0, 1)},
	});
	all.push_back({
	    .id = "actions.steal_food_from_farm",
	    .name = "Takes food from a field to a storage pit",
	    .facet = Facet::Objects,
	    .description = "A grown tiger with an empty storage pit in front of it and a ripe field of wheat to one side. A "
	                   "script tells it to steal food from the farm for the pit, as a copy of the player's theft makes it.",
	    .expected = "It goes to the field and takes a handful of wheat, as for a gift of food from a field. It goes to where "
	                "it can throw into the pit, faces the pit and throws the wheat in. The pit gets the food.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-25.0f, -80.0f}, {65.0f, 45.0f}}},
	    .creatures = {Actor()},
	    .objects = {emptyPit, {.type = FieldTypeInfo::Wheat, .offset = {50.0f, 35.0f}, .ripeCrop = true}},
	    .commands = {DoWith("StealFoodFromFarm", 1.0f, 0, 1)},
	});
	all.push_back({
	    .id = "actions.practice_throw",
	    .name = "Practises a throw with a rock",
	    .facet = Facet::Objects,
	    .description = "A grown tiger with a rock 12 metres in front of it is told to practise a throw with it.",
	    .expected = "It picks the rock up and throws it.",
	    .environment = {.dispenserGrid = false},
	    .framing = Far(),
	    .creatures = {Actor()},
	    .objects = {{.type = MobileStaticInfo::Rock, .offset = {0.0f, -12.0f}}},
	    .commands = {Do("PracticeThrow", 1.0f)},
	});
	all.push_back({
	    .id = "actions.throw_to_impress",
	    .name = "Throws a rock to impress",
	    .facet = Facet::Objects,
	    .description = "A grown tiger with a rock 12 metres in front of it and a villager 60 metres away is told to throw "
	                   "the rock to impress the villager.",
	    .expected = "It picks the rock up, turns towards the villager and throws the rock hard.",
	    .environment = {.dispenserGrid = false},
	    .framing = Far(),
	    .creatures = {Actor()},
	    .objects = {{.type = VillagerInfo::CelticFarmerMale, .offset = {0.0f, -60.0f}},
	                {.type = MobileStaticInfo::Rock, .offset = {0.0f, -12.0f}}},
	    .commands = {DoWith("ThrowToImpress", 1.0f, 0, 1)},
	});
	all.push_back({
	    .id = "actions.notice_stealing",
	    .name = "Shows that it saw the player steal",
	    .facet = Facet::Mind,
	    .description = "A grown tiger is told to show that it saw a theft of the player, at a rock beside it.",
	    .expected = "It looks at the rock, then turns to the camera to show the player that it noticed the deed.",
	    .environment = {.dispenserGrid = false},
	    .framing = Near(),
	    .creatures = {Actor()},
	    .objects = {{.type = MobileStaticInfo::Rock, .offset = {6.0f, -10.0f}}},
	    .commands = {Do("NoticeStealingAction", 1.0f)},
	});
	all.push_back({
	    .id = "actions.take_villager_home",
	    .name = "Takes a villager home to sleep",
	    .facet = Facet::Objects,
	    .description = "A grown tiger with a villager of the player's town 20 metres in front of it, whose home is a "
	                   "Celtic hut 90 metres away. It is told to take the villager home to sleep.",
	    .expected = "It picks the villager up, carries it to its hut and puts it down there.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-40.0f, -30.0f}, {100.0f, 20.0f}}},
	    .creatures = {Actor()},
	    .objects = {{.type = AbodeInfo::CelticHut, .offset = {90.0f, 0.0f}},
	                {.type = VillagerInfo::CelticFarmerMale, .offset = {0.0f, -20.0f}, .joinTown = true, .homeIn = 0}},
	    .commands = {Do("TakeVillagerHomeToSleep", 1.0f, 1)},
	});
}
