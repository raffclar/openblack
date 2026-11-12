/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Testbed scenarios for the mind of the creature: what it learns, wants and does.

#include <numbers>

#include <glm/gtc/constants.hpp>

#include "3D/FlatLand.h"
#include "3D/TempleInteriorInterface.h"
#include "Common/PortableMath.h"
#include "Creature/CreatureCave.h"
#include "Creature/CreatureFeedback.h"
#include "Creature/CreatureFight.h"
#include "Creature/CreatureIdleMind.h"
#include "Creature/CreatureLayers.h"
#include "Sound/GameMusic.h"
#include "Sound/SoundId.h"
#include "TestbedScenarioBuilders.h"
#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

void testbed_scenarios::AddMind(std::vector<Scenario>& all)
{
	// The game's belief type for villagers, the deed of playing with a toy, and the first skill of the game's table
	constexpr size_t k_VillagerBelief = 6;
	constexpr size_t k_PlayWithToy = 41;
	constexpr size_t k_FirstSkill = 0;
	const auto setDesire = [](Desire desire, float amount, float delay) {
		return Command {.kind = Kind::SetDesire,
		                .creature = 0,
		                .delaySeconds = delay,
		                .value = static_cast<size_t>(desire),
		                .amount = amount};
	};

	// Facing away from the camera, their minds free, while a script's cinema bars come and go
	const auto awayOwned = Content(CreatureType::Tiger, {-20.0f, 0.0f}, 180.0f, "player's");
	auto awayNeutral = Content(CreatureType::Cow, {20.0f, 0.0f}, 180.0f, "no player's");
	awayNeutral.owner = PlayerNames::NEUTRAL;
	all.push_back({
	    .id = "mind.cut_scene_look",
	    .name = "Creatures looking at the camera in a cut scene",
	    .facet = Facet::Mind,
	    .description = "A player's tiger and a cow with no player stand facing away from the camera, their minds free. "
	                   "A script's cinema bars slide in at once (before any help script can take the dialogue, whose "
	                   "release would take the bars away), and twenty-five seconds later out again.",
	    .expected = "As the bars come in both stop what they were doing. The tiger turns round to the camera and looks at "
	                "it, curious; now and then it first points at the camera, waves, talks, or looks confused and talks, "
	                "and keeps on looking until the bars go. The cow doesn't turn but may point, wave and talk. Once the "
	                "bars are out each finishes the look it is in and goes back to its own life.",
	    .framing = {.shot = Shot::Testbed},
	    .creatures = {awayOwned, awayNeutral},
	    .commands = {{.kind = Kind::WideScreen, .delaySeconds = 0.0f, .value = 1},
	                 {.kind = Kind::WideScreen, .delaySeconds = 25.0f, .value = 0}},
	});

	// The camera looks away from the creature, so the player does not watch it
	all.push_back({
	    .id = "mind.lonely_unwatched",
	    .name = "Lonely when the player does not watch it",
	    .facet = Facet::Mind,
	    .description = "A player's cow stands in the middle of the map, its mind free. The camera stands to its north and "
	                   "looks further north, away from it.",
	    .expected = "The player does not watch it, so it gets lonely: its loneliness source goes up by a sixtieth each "
	                "second and is full after one minute, and its desire to attract the player's attention grows. Turn "
	                "the camera to it and its loneliness source goes to zero at once. Its desire to get high stays at "
	                "zero, because it has not eaten a hundred magic mushrooms.",
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 40.0f, 150.0f}, .look = {0.0f, 0.0f, 400.0f}},
	    .creatures = {Content(CreatureType::Cow, {0.0f, 0.0f}, 0.0f)},
	});

	all.push_back({
	    .id = "mind.look_at_camera_still_hand",
	    .name = "Looking at the camera stops when the hand stays still",
	    .facet = Facet::Mind,
	    .description = "A player's tiger stands facing away from the camera, a birch beside it. A script tells it to look "
	                   "at the camera after one second, and again after twelve seconds.",
	    .expected = "Each time it turns round to the camera and watches it, curious. If the hand moves all the time, each "
	                "look lasts six seconds. If the hand stays in one place, the first look stops five seconds after "
	                "the hand last moved, and the second look stops at once.",
	    .framing = {.shot = Shot::Testbed},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f}, 180.0f)},
	    .objects = {{.type = TreeInfo::Birch, .offset = {-15.0f, 0.0f}}},
	    .commands = {{.kind = Kind::DoAction, .delaySeconds = 1.0f, .object = 0, .action = "LookAtCamera"},
	                 {.kind = Kind::DoAction, .delaySeconds = 12.0f, .object = 0, .action = "LookAtCamera"}},
	});

	all.push_back({
	    .id = "mind.gesture_action",
	    .name = "A script forces a gesture action",
	    .facet = Facet::Mind,
	    .description = "A player's tiger stands facing away from the camera, a birch beside it. A script forces the gesture "
	                   "actions on it: the star after one second, the moon after twelve seconds. No shipped script "
	                   "does this, and no desire starts these actions.",
	    .expected = "Each time it turns round to the camera and looks at it. Then it raises an arm and draws the circle "
	                "in the air in front of its head, with a glowing chain in the player's colour. The star and the moon "
	                "both draw the circle. The arm holds the end of the circle a moment, then comes down.",
	    .framing = {.shot = Shot::Testbed},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f}, 180.0f)},
	    .objects = {{.type = TreeInfo::Birch, .offset = {-15.0f, 0.0f}}},
	    .commands = {{.kind = Kind::DoAction, .delaySeconds = 1.0f, .object = 0, .action = "GestureStar"},
	                 {.kind = Kind::DoAction, .delaySeconds = 12.0f, .object = 0, .action = "GestureMoon"}},
	});

	all.push_back({
	    .id = "mind.slight_feedback_looks_at_camera",
	    .name = "Feedback too slight to count: it looks at the camera",
	    .facet = Facet::Mind,
	    .description = "A player's tiger stands facing away from the camera, a birch beside it. The hand strokes its head "
	                   "once and gives it one gentle slap, so that the sum is nothing. Then the hand lets go.",
	    .expected = "When the hand lets go, the tiger does not turn at once. It plans to look at the camera for its desire "
	                "to idle with the player, and turns round to the camera, curious. The look ends by the rules of the "
	                "look that a script orders. For example, it ends when the hand stays still for five seconds.",
	    .framing = {.shot = Shot::Testbed},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f}, 180.0f)},
	    .objects = {{.type = TreeInfo::Birch, .offset = {-15.0f, 0.0f}}},
	    .commands = {Stroke(0, creature_feedback::BodyPart::Head, 2.0f), Slap(0, 0.5f, true, true, 1.0f),
	                 Act(Kind::HandLetGo, 0, 0.5f, false)},
	});

	all.push_back({
	    .id = "mind.head_look_hand",
	    .name = "An idle lion watches the hand",
	    .facet = Facet::Mind,
	    .description = "A player's lion stands side on to the camera, a birch far from it. A script tells it to look at "
	                   "the hand every six seconds. Move the hand from side to side over the land in front of it.",
	    .expected = "Its head turns to follow the hand, quickly at first and then easing to a stop on it. The head does "
	                "not turn further than half a turn to either side or a quarter turn up or down.",
	    .framing = {.shot = Shot::Testbed},
	    .creatures = {Content(CreatureType::Lion, {0.0f, 0.0f}, 90.0f)},
	    // The script needs a thing to tell the creature about. The look itself is at the hand.
	    .objects = {{.type = TreeInfo::Birch, .offset = {-100.0f, 0.0f}}},
	    .commands = {{.kind = Kind::DoAction, .delaySeconds = 1.0f, .object = 0, .action = "LookAtHand"},
	                 {.kind = Kind::DoAction, .delaySeconds = 6.0f, .object = 0, .action = "LookAtHand"}},
	    .repeatFrom = 1,
	});

	all.push_back({
	    .id = "creatures.reaction_cut_short",
	    .name = "A forced action stops a reaction",
	    .facet = Facet::Mind,
	    .description = "A creature stands facing south, towards the camera. A cart 20 metres in front of it makes the "
	                   "reaction of a thing crushed by the player. After four seconds, a script tells the creature to "
	                   "look at the camera.",
	    .expected = "The creature goes to look at the cart. When the script tells it to look at the camera, it stops the "
	                "look at the cart at once and turns its face to the camera. It does not go back to the cart.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {35.0f, 12.0f, -10.0f}, .look = {0.0f, 6.0f, -10.0f}},
	    .creatures = {{.offset = {0.0f, 0.0f}, .facingDegrees = 0.0f}},
	    .objects = {{.type = MobileObjectInfo::Cart, .offset = {0.0f, -20.0f}, .reaction = Reaction::ReactToObjectCrushed}},
	    .commands = {{.kind = Kind::DoAction, .delaySeconds = 4.0f, .object = 0, .action = "LookAtCamera"}},
	});

	all.push_back({
	    .id = "mind.look_at_land",
	    .name = "An idle lion looks at the hills and the sea",
	    .facet = Facet::Mind,
	    .description = "A player's lion stands alone, with only a birch far from it, facing the nearest hill. A script "
	                   "tells it to be idle every fifteen seconds.",
	    .expected = "While it waits, nothing else catches its eye, so its head turns to the nearest hill or sea that it "
	                "did not go to yet, at the ground there. When that place is no longer in front of it, it looks down "
	                "at the ground where it stands, the place where it started, until the hill or the sea comes back "
	                "into view.",
	    .framing = {.shot = Shot::Testbed},
	    .creatures = {Content(CreatureType::Lion, {0.0f, 0.0f}, 180.0f)},
	    .objects = {{.type = TreeInfo::Birch, .offset = {100.0f, 0.0f}}},
	    .commands = {{.kind = Kind::DoAction, .delaySeconds = 1.0f, .object = 0, .action = "BeIdle"},
	                 {.kind = Kind::DoAction, .delaySeconds = 15.0f, .object = 0, .action = "BeIdle"}},
	    .repeatFrom = 1,
	});

	all.push_back({
	    .id = "mind.look_at_destination",
	    .name = "With nothing to watch, a lion looks at the place it walked to",
	    .facet = Facet::Mind,
	    .description = "A player's lion stands alone, with only a birch far behind it. It walks forty metres to the "
	                   "east, then a script tells it to be idle every fifteen seconds. Then it walks back.",
	    .expected = "As it walks, its head looks ahead along the walk. When it is idle with nothing in view, its head "
	                "turns down to the ground at the place that it walked to, where it now stands. It does not look "
	                "straight ahead at the height of its head.",
	    .framing = {.shot = Shot::Testbed},
	    .creatures = {Content(CreatureType::Lion, {0.0f, 0.0f}, 90.0f)},
	    .objects = {{.type = TreeInfo::Birch, .offset = {-100.0f, 0.0f}}},
	    .commands = {Go(Kind::WalkTo, 0, {40.0f, 0.0f}, 1.0f),
	                 {.kind = Kind::DoAction, .delaySeconds = 1.0f, .object = 0, .action = "BeIdle"},
	                 {.kind = Kind::DoAction, .delaySeconds = 15.0f, .object = 0, .action = "BeIdle"},
	                 Go(Kind::WalkTo, 0, {0.0f, 0.0f}, 15.0f),
	                 Go(Kind::WalkTo, 0, {40.0f, 0.0f}, 1.0f),
	                 {.kind = Kind::DoAction, .delaySeconds = 1.0f, .object = 0, .action = "BeIdle"}},
	    .repeatFrom = 3,
	});

	all.push_back({
	    .id = "mind.woken_eyes",
	    .name = "Yawning droops the lids until the lion stops",
	    .facet = Facet::Mind,
	    .description = "A player's lion stands facing the camera. A script tells it to be idle every fifteen seconds: it "
	                   "waits and yawns two times.",
	    .expected = "With the first yawn its lids droop and it blinks slowly. The lids stay down through the wait and the "
	                "second yawn. When it stops being idle, its lids open by how rested it is: nearly wide when it is "
	                "fresh.",
	    .framing = {.shot = Shot::Testbed},
	    .creatures = {Content(CreatureType::Lion, {0.0f, 0.0f}, 0.0f)},
	    .objects = {{.type = TreeInfo::Birch, .offset = {100.0f, 0.0f}}},
	    .commands = {{.kind = Kind::DoAction, .delaySeconds = 1.0f, .object = 0, .action = "BeIdle"},
	                 {.kind = Kind::DoAction, .delaySeconds = 15.0f, .object = 0, .action = "BeIdle"}},
	    .repeatFrom = 1,
	});

	// The emotes: a script tells a player's lion to do one action again and again, with a villager beside it as the thing
	// that the action is done to
	const auto emote = [&all](std::string_view id, std::string_view name, std::string_view description,
	                          std::string_view expected, std::string_view action, float every, Environment environment,
	                          std::optional<float> villagerLife) {
		all.push_back({
		    .id = id,
		    .name = name,
		    .facet = Facet::Mind,
		    .description = description,
		    .expected = expected,
		    .environment = environment,
		    .framing = {.shot = Shot::Testbed},
		    .creatures = {Content(CreatureType::Lion, {0.0f, 0.0f}, 90.0f)},
		    .objects = {{.type = VillagerInfo::CelticFarmerMale, .offset = {-40.0f, 10.0f}, .life = villagerLife}},
		    .commands = {{.kind = Kind::DoAction, .delaySeconds = 1.0f, .object = 0, .action = action},
		                 {.kind = Kind::DoAction, .delaySeconds = every, .object = 0, .action = action}},
		    .repeatFrom = 1,
		});
	};
	emote("emote.howl", "The creature howls at the player",
	      "A script tells a player's lion to howl at the player every eight seconds.",
	      "It turns to face the camera, its head on the camera, then plays its howl with its sound. It pulls no face.",
	      "HowlAtPlayer", 8.0f, {}, std::nullopt);
	emote("emote.sad", "The creature is sad", "A script tells a player's lion to be sad every ten seconds.",
	      "It looks down at its feet with a sad face for two to five seconds, then it plays its sad clip or it cries, in "
	      "equal shares.",
	      "BeSad", 10.0f, {}, std::nullopt);
	emote("emote.silly_faces", "The creature pulls silly faces at a villager",
	      "A script tells a player's lion to pull silly faces at a villager beside it every twelve seconds.",
	      "It goes near the villager and turns to it. Two times in three it plays its playful clip. Then it pulls three "
	      "random faces in a row, one each turn, and holds the last for five seconds, its head on the villager.",
	      "PullSillyFaces", 12.0f, {}, std::nullopt);
	emote("emote.pathetic", "The creature is pathetic to the player",
	      "A script tells a player's lion to be pathetic to the player every fifteen seconds. Move the camera's focus "
	      "away from it.",
	      "It walks to the point that the camera looks at, for up to thirty seconds, its head on the camera. Then it "
	      "turns to the camera and plays its pick-me clip.",
	      "BePatheticToPlayer", 15.0f, {}, std::nullopt);
	emote("emote.stroke_heal", "The creature strokes a hurt villager and heals it",
	      "A villager at three tenths of its life stands near a player's lion. A script tells the lion to stroke it "
	      "every twenty seconds.",
	      "The lion picks the villager up, strokes it with a face of compassion, heals it to full life, then puts it "
	      "down. The villager then goes home.",
	      "Stroke", 20.0f, {}, 0.3f);
	emote("emote.look_at_sun", "The creature looks at the sun",
	      "At noon, a script tells a player's lion to look at the sun every six seconds.",
	      "It turns to face the far south-west of the land, its head on that point. One time in two it points there for "
	      "three seconds.",
	      "LookAtSun", 6.0f, {}, std::nullopt);
	emote("emote.look_at_moon", "The creature looks at the moon",
	      "At midnight, a script tells a player's lion to look at the moon every six seconds.",
	      "It turns to face the moon's place, its head on it. One time in two it points there for three seconds.", "LookAtMoon",
	      6.0f, {.hour = 0.0f}, std::nullopt);
	emote("emote.sit_down", "The creature sits down by a villager",
	      "A script tells a player's lion to sit down by a villager every twenty-five seconds.",
	      "It walks to the villager, turns to face it, faces down the slope, then sits for ten to fifteen seconds and "
	      "looks about.",
	      "SitDown", 25.0f, {}, std::nullopt);
	emote("emote.sleep_on_the_spot", "The creature sleeps at a clear place near it",
	      "A script tells a player's lion to sleep on the spot every forty seconds.",
	      "One time in two it yawns. It walks to a clear place near it, sleeps until it is rested, then gets up dazed "
	      "with a grimace.",
	      "SleepOnTheSpot", 40.0f, {}, std::nullopt);

	all.push_back({
	    .id = "mind.look_at_hand_turn",
	    .name = "Looking at the hand turns round once and then stops",
	    .facet = Facet::Mind,
	    .description = "A player's tiger stands facing away from the camera, a birch beside it. A script tells it to look "
	                   "at the hand after one second, and again after twelve seconds. Keep the hand over the land in "
	                   "front of the camera.",
	    .expected = "Each time it turns round to the hand in one smooth turn, then watches it, curious, and stops after "
	                "about two seconds. It does not freeze at the start of its turn.",
	    .framing = {.shot = Shot::Testbed},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f}, 180.0f)},
	    .objects = {{.type = TreeInfo::Birch, .offset = {-15.0f, 0.0f}}},
	    .commands = {{.kind = Kind::DoAction, .delaySeconds = 1.0f, .object = 0, .action = "LookAtHand"},
	                 {.kind = Kind::DoAction, .delaySeconds = 12.0f, .object = 0, .action = "LookAtHand"}},
	});

	all.push_back({
	    .id = "mind.reward_villagers",
	    .name = "Learning: reward eating villagers, punish eating food",
	    .facet = Facet::Mind,
	    .description = "A hungry tiger with a villager right by it, lots of magic food close by and more villagers further "
	                   "off. Every 18 seconds its "
	                   "hunger is set to 40% of its most, enough for one meal. Whatever it eats, as soon as it has eaten, "
	                   "it is stroked if it was a villager and slapped if it was anything else.",
	    .expected = "First it eats the villager beside it and is stroked: with one example, its hunger tree finds "
	                "everything good. Then it eats the nearest food and is slapped: now the tree tells them apart. From then "
	                "on it walks past the food to eat villagers, the "
	                "planner's goal usefulness for villagers near 0.8 and for food near 0. The spawner's Mind panel shows "
	                "the tree and the thoughts.",
	    .framing = {.shot = Shot::Overview},
	    .creatures = {CreatureSetup {
	        .species = CreatureType::Tiger,
	        .needs = {.energy = 0.3f, .exhaustion = 0.0f, .dehydration = 0.0f, .poo = 0.0f},
	        .desires = OnlyDesire(Desire::Hunger),
	    }},
	    .objects = {{.type = VillagerInfo::CelticLeaderMale, .offset = {12.0f, -12.0f}},
	                {.type = MobileObjectInfo::MagicFood, .offset = {-25.0f, -25.0f}},
	                {.type = MobileObjectInfo::MagicFood, .offset = {25.0f, -30.0f}},
	                {.type = MobileObjectInfo::MagicFood, .offset = {0.0f, -40.0f}},
	                {.type = MobileObjectInfo::MagicFood, .offset = {-35.0f, 15.0f}},
	                {.type = MobileObjectInfo::MagicFood, .offset = {35.0f, 20.0f}},
	                {.type = VillagerInfo::CelticFarmerMale, .offset = {70.0f, -60.0f}},
	                {.type = VillagerInfo::CelticForesterMale, .offset = {-70.0f, -60.0f}},
	                {.type = VillagerInfo::CelticHousewifeFemale, .offset = {80.0f, 30.0f}},
	                {.type = VillagerInfo::CelticFishermanMale, .offset = {-80.0f, 40.0f}},
	                {.type = VillagerInfo::CelticShepherdMale, .offset = {0.0f, 85.0f}},
	                {.type = VillagerInfo::CelticTraderMale, .offset = {60.0f, 75.0f}}},
	    .commands = {{.kind = Kind::RewardIf, .creature = 0, .delaySeconds = 1.0f, .value = k_VillagerBelief},
	                 setDesire(Desire::Hunger, 0.4f, 1.0f),
	                 Act(Kind::Stop, 0, 17.0f)},
	    .repeatFrom = 1,
	});

	all.push_back({
	    .id = "mind.community",
	    .name = "Load a chosen creature mind",
	    .facet = Facet::Mind,
	    .description = "A wolf that takes up the mind file last opened with the spawner's Open mind file button (a "
	                   "community-made creature, say), or else the game's own Khazar creature mind, with villagers, "
	                   "food, trees and a ball about it.",
	    .expected = "Its name, desires, opinions and the examples of its decision trees come from the file (the spawner's "
	                "Mind panel lists them, and its thoughts). The planner weighs what it has learnt: it goes for the "
	                "things its trees rate well. If neither file can be read the wolf keeps a fresh mind and the log "
	                "says why.",
	    .framing = {.shot = Shot::Overview},
	    .creatures = {CreatureSetup {.species = CreatureType::Wolf, .mindFile = "chosen:KhazarCreature"}},
	    .objects = {{.type = MobileObjectInfo::MagicFood, .offset = {-30.0f, -30.0f}},
	                {.type = MobileObjectInfo::Ball, .offset = {30.0f, -20.0f}},
	                {.type = TreeInfo::Beech, .offset = {-50.0f, 40.0f}},
	                {.type = TreeInfo::Beech, .offset = {55.0f, 45.0f}},
	                {.type = VillagerInfo::CelticFarmerMale, .offset = {60.0f, -50.0f}},
	                {.type = VillagerInfo::CelticHousewifeFemale, .offset = {-60.0f, -55.0f}}},
	});
	all.push_back({
	    .id = "mind.made_from_file_by_land",
	    .name = "A land makes a creature from a mind file",
	    .facet = Facet::Mind,
	    .description = "The land's script line CREATE_CREATURE_FROM_FILE makes the second player's creature from the "
	                   "game's mind file ComputerControlledCreature, 20 m east of the middle, as the later lands do. Save "
	                   "the game and read the creature's mind file.",
	    .expected = "The creature takes up the file. Then it knows all leashes and it is fully grown (stage 13, its time "
	                "at the stage 0). In the saved mind file, the four leash words are 1 and the stage is 13.",
	    .framing = {.shot = Shot::Overview},
	    .objects = {{.type = MobileObjectInfo::Ball, .offset = {-20.0f, 0.0f}}},
	    .landScript = R"(CREATE_CREATURE_FROM_FILE("PLAYER_TWO", 6, "ComputerControlledCreature", "20.0,0.0"))",
	});

	std::vector<Command> phases;
	for (size_t phase = 1; phase <= k_LastPhase; ++phase)
	{
		phases.push_back({.kind = Kind::SetPhase, .creature = 0, .delaySeconds = 8.0f, .value = phase});
	}
	all.push_back({
	    .id = "mind.phases",
	    .name = "Stages of growing up unlock desires",
	    .facet = Facet::Mind,
	    .description = "A cow that starts at the first stage of growing up and moves on a stage every eight seconds, "
	                   "through all fourteen.",
	    .expected = "At first only fear, tiredness, attention, showing how it is and the like are active; hunger, "
	                "curiosity and play come at the second stage, water at the fourth, anger at the eleventh, compassion "
	                "at the twelfth. The Mind panel's desires list grows, and the planner starts weighing what each new "
	                "desire can do.",
	    .framing = {.shot = Shot::Follow},
	    .creatures = {CreatureSetup {.species = CreatureType::Cow, .phase = 0}},
	    .objects = {{.type = MobileObjectInfo::MagicFood, .offset = {-30.0f, -30.0f}},
	                {.type = MobileObjectInfo::Ball, .offset = {30.0f, -20.0f}}},
	    .commands = phases,
	});

	all.push_back({
	    .id = "mind.slap_reaction",
	    .name = "What a slap makes it show",
	    .facet = Facet::Mind,
	    .description = "The hand slaps a grown-up tiger facing the camera hard on the head and lets go. The scenario "
	                   "repeats, so each run can show another reaction.",
	    .expected = "After the slap's reeling animation, the tiger shows one of four reactions, each one time in four: "
	                "frightened when it feels no more anger than kindness, else angry or a rude gesture; sad; it cries; "
	                "or it looks at the player for one to two seconds with darting eyes and a sad face.",
	    .framing = {.shot = Shot::Follow, .distance = 1.2f},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f})},
	    .commands = {Slap(0, 0.85f, false, false, 1.0f), Act(Kind::HandLetGo, 0, 1.0f, true), Act(Kind::Stop, 0, 6.0f)},
	    .repeatFrom = 0,
	});

	{
		// Villagers and sheep close round a tiger that the hand slaps hard on the head, the waist and the feet
		std::vector<ObjectSetup> round;
		for (int i = 0; i < 8; ++i)
		{
			const auto angle = static_cast<float>(i) * std::numbers::pi_v<float> / 4.0f;
			const glm::vec2 at {portable_math::Sin(angle) * 5.0f, portable_math::Cos(angle) * 5.0f};
			if (i % 2 == 0)
			{
				round.push_back({.type = VillagerInfo::CelticFarmerMale, .offset = at});
			}
			else
			{
				round.push_back({.type = AnimalInfo::Sheep, .offset = at, .living = true});
			}
		}
		all.push_back({
		    .id = "hand.slap_knocks_things",
		    .name = "A slap in a game alone knocks things about",
		    .facet = Facet::Hand,
		    .description = "A grown tiger faces the camera with villagers and sheep close round it. The hand slaps it hard "
		                   "on the head, the waist and the feet, then lets go.",
		    .expected = "Each slap plays its reeling animation. In a game alone, the hands, feet, head and body of the "
		                "tiger knock the villagers and sheep that they pass near through the air while it reels. When it "
		                "stands again, it knocks nothing. In a network game with two or more humans, a slap knocks nothing.",
		    .environment = {.dispenserGrid = false},
		    .framing = {.shot = Shot::Follow, .distance = 1.3f},
		    .creatures = {Posed(CreatureType::Tiger, {0.0f, 0.0f})},
		    .objects = round,
		    .commands = {Slap(0, 0.85f, false, false, 1.0f), Slap(0, 0.55f, false, true, 2.0f),
		                 Slap(0, 0.2f, false, false, 2.0f), Act(Kind::HandLetGo, 0, 2.0f, true)},
		});
	}

	auto young = Content(CreatureType::Tiger, {0.0f, 0.0f});
	young.phase = 1;
	all.push_back({
	    .id = "mind.young_feedback",
	    .name = "A young creature stroked and slapped",
	    .facet = Facet::Mind,
	    .description = "A tiger at the second stage of growing up, too young to learn from the hand, is stroked and let "
	                   "go, then slapped hard and let go. The scenario repeats.",
	    .expected = "One stroke in two, it then plays its happy animation. One slap in two, it is then frightened of the "
	                "player: it runs away one time in three, else it cowers where it stands. Each other slap shows one of "
	                "the four reactions of a grown-up creature. The advisor says nothing: the help needs a grown-up "
	                "creature.",
	    .framing = {.shot = Shot::Follow, .distance = 1.5f},
	    .creatures = {young},
	    .commands = {Stroke(0, creature_feedback::BodyPart::Head, 1.0f), Act(Kind::HandLetGo, 0, 1.0f, true),
	                 Act(Kind::Stop, 0, 6.0f), Slap(0, 0.85f, false, false, 1.0f), Act(Kind::HandLetGo, 0, 1.0f, true),
	                 Act(Kind::Stop, 0, 8.0f)},
	    .repeatFrom = 0,
	});

	auto grown = Content(CreatureType::Tiger, {0.0f, 20.0f});
	grown.phase = 13;
	all.push_back({
	    .id = "mind.lost_thing",
	    .name = "The creature loses what it goes to",
	    .facet = Facet::Mind,
	    .description = "A grown-up tiger on the learning leash is told to act on a cow. When the advisor has said what "
	                   "the tiger wants, and before the tiger is done with the cow, the cow is removed from the world.",
	    .expected = "The tiger stops what it does and decides again. The good advisor says: \"Your Creature lost the "
	                "object he was looking for.\"",
	    .framing = {.shot = Shot::Follow, .distance = 2.0f},
	    .creatures = {grown},
	    .objects = {{.type = AnimalInfo::Cow, .offset = {30.0f, 60.0f}}},
	    .commands = {Leash(0, LeashType::Rope, 1.0f), OnObject(Kind::LeashOrderOn, 0, 0, 2.0f, false),
	                 OnObject(Kind::RemoveObject, 0, 0, 9.0f, false), Act(Kind::TakeOffLeash, 0, 8.0f)},
	});

	// The help explains why an order fails, says what the creature wants, and tells what a stroke taught it
	all.push_back({
	    .id = "mind.help_explanations",
	    .name = "The help explains what the creature does and learns",
	    .facet = Facet::Mind,
	    .description = "The pointer goes high in the sky, away from the tiger. A grown-up tiger on the learning leash is "
	                   "told with the Action button to go to a place off the land, then to act on a cow. The leash comes "
	                   "off, and the player strokes it twice.",
	    .expected = "The order off the land gets no acknowledge sound and no marker, and the help says: \"Your "
	                "Creature wasn't able to get to his destination because it is inaccessible.\" Told to act on the "
	                "cow, the help says what the tiger wants: \"Your Creature is feeling curious.\" After the strokes, "
	                "the good advisor comes out, looks at the tiger and says what the stroke taught it about the source of "
	                "what it did last, such as \"From now on, your Creature will want to look around more.\" or \"From "
	                "now on, your Creature will pay more attention to you.\" Each line has its voice.",
	    .framing = {.shot = Shot::Follow, .distance = 2.0f},
	    .creatures = {grown},
	    .objects = {{.type = AnimalInfo::Cow, .offset = {30.0f, 60.0f}}},
	    .commands = {{.kind = Kind::PointerTo, .delaySeconds = 0.5f, .point = {0.9f, 0.1f}},
	                 Leash(0, LeashType::Rope, 0.5f),
	                 Go(Kind::LeashOrderAt, 0, {4800.0f, 4800.0f}, 2.0f, false),
	                 OnObject(Kind::LeashOrderOn, 0, 0, 10.0f, false),
	                 Act(Kind::TakeOffLeash, 0, 8.0f),
	                 Act(Kind::Stroke, 0, 9.5f),
	                 Act(Kind::Stroke, 0, 4.0f)},
	});

	// The help is held back while the plan of the creature is to sleep at home, but not while it sleeps on the spot
	CreatureSetup lowOnEnergy {.species = CreatureType::Tiger,
	                           .offset = {0.0f, 20.0f},
	                           .phase = 13,
	                           .needs = {.energy = 0.3f, .exhaustion = 0.95f, .dehydration = 0.0f, .poo = 0.0f, .life = 1.0f},
	                           .desires = OnlyDesire(Desire::Tiredness),
	                           .hold = true};
	// Not so tired that it cannot walk home
	auto lowOnEnergyCanWalk = lowOnEnergy;
	lowOnEnergyCanWalk.needs.exhaustion = 0.6f;
	all.push_back({
	    .id = "mind.help_sleep_on_spot",
	    .name = "The help speaks while the creature sleeps on the spot",
	    .facet = Facet::Mind,
	    .description = "A grown-up tiger with low energy and a strong desire to sleep has no temple. After two seconds it "
	                   "lies down and sleeps where it stands. Its body stays as it is.",
	    .expected = "The good advisor says that the tiger has low energy (help text 868), also while it sleeps on the "
	                "spot. The advisor says it again every two minutes. Before, the help said nothing while it slept.",
	    .framing = {.shot = Shot::Follow, .distance = 2.0f},
	    .creatures = {lowOnEnergy},
	    .commands = {Act(Kind::Sleep, 0, 2.0f, false)},
	});
	all.push_back({
	    .id = "mind.sleep_eyes_close_in_the_loop",
	    .name = "The eyes close when the sleep loop starts",
	    .facet = Facet::Mind,
	    .description = "A grown-up tiger with low energy and a strong desire to sleep has no temple. After two seconds it "
	                   "lies down and sleeps where it stands. The camera looks at its face.",
	    .expected = "The eyes stay open while the tiger lies down. They close on the first turn of the sleep loop, when "
	                "the body lies still. Before, the eyes closed as the tiger started to lie down.",
	    .framing = {.shot = Shot::Head, .distance = 1.0f},
	    .creatures = {lowOnEnergy},
	    .commands = {Act(Kind::Sleep, 0, 2.0f, false)},
	});
	all.push_back({
	    .id = "mind.help_sleep_at_home",
	    .name = "The help holds back while the creature goes home to sleep",
	    .facet = Facet::Mind,
	    .description = "A grown-up tiger with low energy and a strong desire to sleep is about 110 metres from its player's "
	                   "temple. That is near enough that it does not sleep on the spot. It is less tired than the tiger of "
	                   "mind.help_sleep_on_spot, so that it can walk. Its body stays as it is.",
	    .expected = "The tiger goes home to sleep. While its plan is to sleep at home, on the way and as it sleeps, the good "
	                "advisor says nothing about its low energy. Compare with mind.help_sleep_on_spot.",
	    .framing = {.shot = Shot::Follow, .distance = 2.0f},
	    .creatures = {lowOnEnergyCanWalk},
	    .temples = {{.offset = {-100.0f, 60.0f}, .angle = 36.0f}},
	});

	auto curious = Content(CreatureType::Tiger, {0.0f, 20.0f});
	curious.phase = 13;
	all.push_back({
	    .id = "learning.forgotten_order",
	    .name = "The creature forgets the oldest of its orders",
	    .facet = Facet::Mind,
	    .description = "A grown-up tiger on the learning leash is told with the Action button to act on six cows, one "
	                   "after the other, 90 seconds apart.",
	    .expected = "The tiger picks up and holds each cow for about 44 seconds. The inspector shows each remembered action "
	                "marked as ordered, and as done when the hold ends. At the sixth order the log shows that the tiger "
	                "forgets the first order. The hold serves its curiosity, which learns nothing from orders, so the "
	                "log shows no approval.",
	    .framing = {.shot = Shot::Follow, .distance = 2.0f},
	    .creatures = {curious},
	    .objects = {{.type = AnimalInfo::Cow, .offset = {20.0f, 40.0f}},
	                {.type = AnimalInfo::Cow, .offset = {-20.0f, 40.0f}},
	                {.type = AnimalInfo::Cow, .offset = {20.0f, 60.0f}},
	                {.type = AnimalInfo::Cow, .offset = {-20.0f, 60.0f}},
	                {.type = AnimalInfo::Cow, .offset = {20.0f, 80.0f}},
	                {.type = AnimalInfo::Cow, .offset = {-20.0f, 80.0f}}},
	    .commands = {Leash(0, LeashType::Rope, 1.0f), OnObject(Kind::LeashOrderOn, 0, 0, 2.0f, false),
	                 OnObject(Kind::LeashOrderOn, 0, 1, 90.0f, false), OnObject(Kind::LeashOrderOn, 0, 2, 90.0f, false),
	                 OnObject(Kind::LeashOrderOn, 0, 3, 90.0f, false), OnObject(Kind::LeashOrderOn, 0, 4, 90.0f, false),
	                 OnObject(Kind::LeashOrderOn, 0, 5, 90.0f, false), Act(Kind::TakeOffLeash, 0, 90.0f)},
	});

	auto watched = Content(CreatureType::Tiger, {0.0f, 20.0f});
	watched.phase = 13;
	all.push_back({
	    .id = "learning.seen_action",
	    .name = "The player sees the action that it orders",
	    .facet = Facet::Mind,
	    .description = "A grown-up tiger on the learning leash is told with the Action button to act on a cow. The camera "
	                   "follows the tiger the whole time.",
	    .expected = "The tiger picks up and holds the cow. The inspector shows the newest remembered action marked as "
	                "seen once the main step of its agenda moves on, and as done when the hold ends. Feedback then goes "
	                "to it, less and less until its learning window closes.",
	    .framing = {.shot = Shot::Follow, .distance = 2.0f},
	    .creatures = {watched},
	    .objects = {{.type = AnimalInfo::Cow, .offset = {20.0f, 40.0f}}},
	    .commands = {Leash(0, LeashType::Rope, 1.0f), OnObject(Kind::LeashOrderOn, 0, 0, 2.0f, false),
	                 Act(Kind::TakeOffLeash, 0, 60.0f)},
	});

	auto iconWatcher = Content(CreatureType::Tiger, {0.0f, 0.0f});
	iconWatcher.phase = 13;
	auto healed = Content(CreatureType::Cow, {0.0f, 30.0f}, 180.0f, "hurt");
	healed.needs.life = 0.3f;
	all.push_back({
	    .id = "learning.icon_miracle_without_seed",
	    .name = "A miracle that an icon made, and its seed",
	    .facet = Facet::Mind,
	    .description = "A grown-up tiger stands near a hurt cow. A heal seed from the player's worship (made at an icon) "
	                   "is pressed on the cow. The seed leaves the hand but stays linked to the heal until the heal goes.",
	    .expected = "If the tiger takes up its reaction at the moment of the cast, the log tells that it ignores the "
	                "miracle: the seed is not linked yet. When it takes up the reaction later, while the heal lasts, it "
	                "is not ignored (no such log line) and the heal counts towards learning the miracle.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{0.0f, 0.0f}, {0.0f, 30.0f}}, .distance = 0.6f},
	    .creatures = {iconWatcher, healed},
	    .miracles = {{.type = MagicType::Heal,
	                  .target = MiracleCast::Target::Creature,
	                  .creature = 1,
	                  .handOffset = {0.0f, 30.0f},
	                  .delaySeconds = 2.0f,
	                  .byHand = true,
	                  .fromWorship = true}},
	});

	all.push_back({
	    .id = "learning.miracle_meter",
	    .name = "The learning meter after a miracle that the creature sees",
	    .facet = Facet::Mind,
	    .description = "A grown-up tiger stands near a hurt cow. After 12 seconds, the hand casts a heal on the cow. The "
	                   "tiger takes up its reaction to the heal and looks at it. The meter shows at most once in 100 "
	                   "turns, counted from the start, so the heal comes after the first 10 seconds.",
	    .expected = "When the tiger takes up the reaction, the heal counts as one sighting. The learning meter shows over "
	                "the head of the tiger: a light bulb and a small yellow percentage that rise and fade in about six "
	                "seconds.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{0.0f, 0.0f}, {0.0f, 30.0f}}, .distance = 0.6f},
	    .creatures = {iconWatcher, healed},
	    .miracles = {{.type = MagicType::Heal,
	                  .target = MiracleCast::Target::Creature,
	                  .creature = 1,
	                  .handOffset = {0.0f, 30.0f},
	                  .delaySeconds = 12.0f,
	                  .byHand = true}},
	});

	{
		// A ring of huts shuts the tiger in, with a cow outside it
		auto shutIn = Content(CreatureType::Tiger, {0.0f, 0.0f});
		shutIn.phase = 13;
		std::vector<ObjectSetup> objects {{.type = AnimalInfo::Cow, .offset = {0.0f, 90.0f}}};
		constexpr int k_Huts = 24;
		constexpr float k_RingRadius = 24.0f;
		for (int i = 0; i < k_Huts; ++i)
		{
			const float angle = glm::two_pi<float>() * static_cast<float>(i) / static_cast<float>(k_Huts);
			objects.push_back({.type = AbodeInfo::CelticHut,
			                   .offset = {k_RingRadius * portable_math::Sin(angle), k_RingRadius * portable_math::Cos(angle)},
			                   .yawDegrees = glm::degrees(angle)});
		}
		all.push_back({
		    .id = "mind.clear_way",
		    .name = "The creature clears its way out",
		    .facet = Facet::Mind,
		    .description = "A grown-up tiger on the learning leash stands in a ring of huts. It is told to act on a cow "
		                   "outside the ring.",
		    .expected = "The tiger finds no way out. It kicks one of the huts that the search met, or throws it when it "
		                "can pick it up. Then it walks on to where it went. A tiger with no way and nothing to clear "
		                "makes a sound of refusal and gives up.",
		    .framing = {.shot = Shot::Overview},
		    .creatures = {shutIn},
		    .objects = std::move(objects),
		    .commands = {Leash(0, LeashType::Rope, 1.0f), OnObject(Kind::LeashOrderOn, 0, 0, 2.0f, false),
		                 Act(Kind::TakeOffLeash, 0, 3.0f)},
		});
	}

	{
		// A tight ring of huts shuts a rock in, so that the tiger finds no way to it. A second rock lies in the open.
		auto tiger = Content(CreatureType::Tiger, {0.0f, 0.0f});
		tiger.phase = 13;
		std::vector<ObjectSetup> objects {{.type = MobileStaticInfo::Rock, .offset = {0.0f, -60.0f}},
		                                  {.type = MobileStaticInfo::Rock, .offset = {40.0f, 30.0f}}};
		constexpr int k_Huts = 12;
		constexpr float k_RingRadius = 9.0f;
		for (int i = 0; i < k_Huts; ++i)
		{
			const float angle = glm::two_pi<float>() * static_cast<float>(i) / static_cast<float>(k_Huts);
			objects.push_back(
			    {.type = AbodeInfo::CelticHut,
			     .offset = {k_RingRadius * portable_math::Sin(angle), -60.0f + k_RingRadius * portable_math::Cos(angle)},
			     .yawDegrees = glm::degrees(angle)});
		}
		all.push_back({
		    .id = "mind.pick_up_no_way",
		    .name = "The creature stops when it finds no way to a thing",
		    .facet = Facet::Mind,
		    .description = "A grown-up tiger on the learning leash. A ring of huts shuts a rock in, and a second rock lies "
		                   "in the open. The player tells it to act on the shut-in rock. Then the scenario removes the leash.",
		    .expected = "The tiger walks to pick up the shut-in rock. When the search finds no route, it stops at once and "
		                "makes a sound of refusal. Then it thinks for itself and does something different. For ten seconds "
		                "it does not choose the shut-in rock again.",
		    .framing = {.shot = Shot::Follow, .distance = 2.0f},
		    .creatures = {tiger},
		    .objects = std::move(objects),
		    .commands = {Leash(0, LeashType::Rope, 1.0f), OnObject(Kind::LeashOrderOn, 0, 0, 2.0f, false),
		                 Act(Kind::TakeOffLeash, 0, 3.0f)},
		});
	}

	{
		// A tight ring of huts shuts a rock in, so that the tiger finds no way to it to stomp on it
		auto tiger = Content(CreatureType::Tiger, {0.0f, 0.0f});
		tiger.phase = 13;
		std::vector<ObjectSetup> objects {{.type = MobileStaticInfo::Rock, .offset = {0.0f, -60.0f}}};
		constexpr int k_Huts = 12;
		constexpr float k_RingRadius = 9.0f;
		for (int i = 0; i < k_Huts; ++i)
		{
			const float angle = glm::two_pi<float>() * static_cast<float>(i) / static_cast<float>(k_Huts);
			objects.push_back(
			    {.type = AbodeInfo::CelticHut,
			     .offset = {k_RingRadius * portable_math::Sin(angle), -60.0f + k_RingRadius * portable_math::Cos(angle)},
			     .yawDegrees = glm::degrees(angle)});
		}
		all.push_back({
		    .id = "mind.stomp_no_way",
		    .name = "The creature stops when it finds no way to a thing to stomp on",
		    .facet = Facet::Mind,
		    .description = "A grown-up tiger. A ring of huts shuts a rock in. The script tells the tiger to smash the rock.",
		    .expected = "The tiger walks to stomp on the rock. When the search finds no route, it stops at once and makes "
		                "a sound of refusal. For ten seconds it does not choose the rock again.",
		    .framing = {.shot = Shot::Follow, .distance = 2.0f},
		    .creatures = {tiger},
		    .objects = std::move(objects),
		    .commands = {{.kind = Kind::DoAction, .delaySeconds = 2.0f, .object = 0, .action = "SmashStoneInHalf"}},
		});
	}

	{
		// A ring of huts shuts the tiger in, with a rock outside it
		auto shutIn = Content(CreatureType::Tiger, {0.0f, 0.0f});
		shutIn.phase = 13;
		std::vector<ObjectSetup> objects {{.type = MobileStaticInfo::Rock, .offset = {0.0f, 60.0f}}};
		constexpr int k_Huts = 24;
		constexpr float k_RingRadius = 24.0f;
		for (int i = 0; i < k_Huts; ++i)
		{
			const float angle = glm::two_pi<float>() * static_cast<float>(i) / static_cast<float>(k_Huts);
			objects.push_back({.type = AbodeInfo::CelticHut,
			                   .offset = {k_RingRadius * portable_math::Sin(angle), k_RingRadius * portable_math::Cos(angle)},
			                   .yawDegrees = glm::degrees(angle)});
		}
		all.push_back({
		    .id = "mind.pick_up_shut_in",
		    .name = "The creature clears its way out to pick up a thing",
		    .facet = Facet::Mind,
		    .description = "A grown-up tiger on the learning leash stands in a ring of huts. The player tells it to act on "
		                   "a rock outside the ring.",
		    .expected = "The tiger walks to pick up the rock and finds no way out. It stops the pick-up. It kicks one of "
		                "the huts that the search met, or throws it when it can pick it up. Then it walks on to where it "
		                "went.",
		    .framing = {.shot = Shot::Overview},
		    .creatures = {shutIn},
		    .objects = std::move(objects),
		    .commands = {Leash(0, LeashType::Rope, 1.0f), OnObject(Kind::LeashOrderOn, 0, 0, 2.0f, false),
		                 Act(Kind::TakeOffLeash, 0, 3.0f)},
		});
	}

	{
		// A far rock to pick up, and a near ball to look at
		auto tiger = Content(CreatureType::Tiger, {0.0f, 0.0f});
		tiger.phase = 13;
		all.push_back({
		    .id = "mind.script_action_stops_walk",
		    .name = "A script action stops a walk to a thing",
		    .facet = Facet::Mind,
		    .description = "A grown-up tiger. The script tells it to throw a rock 150 m away. After four seconds, while "
		                   "the tiger walks to the rock, the script tells it to look at a ball near it.",
		    .expected = "The tiger walks to the rock. When the second order comes, it stops at once and turns to look at "
		                "the ball. It does not walk on to the rock.",
		    .framing = {.shot = Shot::Follow, .distance = 2.0f},
		    .creatures = {tiger},
		    .objects = {{.type = MobileStaticInfo::Rock, .offset = {0.0f, -150.0f}},
		                {.type = MobileStaticInfo::ToyBall, .offset = {25.0f, 10.0f}}},
		    .commands = {{.kind = Kind::DoAction, .delaySeconds = 2.0f, .object = 0, .action = "ThrowAround"},
		                 {.kind = Kind::DoAction, .delaySeconds = 4.0f, .object = 1, .action = "LookButDontApproach"}},
		});
	}

	{
		// A small rock between a fence and a hut in a village, 20 m from a young tiger of the size it has on the first land
		auto smallTiger = Content(CreatureType::Tiger, {0.0f, 0.0f});
		smallTiger.size = 0.26f;
		all.push_back({
		    .id = "mind.pick_up_small_rock_by_hut",
		    .name = "The creature picks up a small rock between a fence and a hut",
		    .facet = Facet::Mind,
		    .description = "A young tiger. A small rock lies 20 m from it, between a short fence, a second rock and a hut. "
		                   "The script tells the tiger to pick up the small rock and look at it.",
		    .expected = "The tiger walks round to the small rock and stops at the length of its arm from it. It picks the "
		                "rock up at once. It does not walk again and again beside the rock.",
		    .framing = {.shot = Shot::Follow, .distance = 2.0f},
		    .creatures = {smallTiger},
		    .objects = {{.type = MobileStaticInfo::Rock, .offset = {20.0f, 0.0f}, .scale = 0.267f},
		                {.type = MobileStaticInfo::CeltFenceShort, .offset = {22.5f, 3.0f}, .yawDegrees = 45.0f},
		                {.type = MobileStaticInfo::Rock, .offset = {23.3f, -6.3f}, .scale = 0.267f},
		                {.type = AbodeInfo::CelticShackX, .offset = {28.0f, 2.7f}}},
		    .commands = {{.kind = Kind::DoAction, .delaySeconds = 2.0f, .object = 0, .action = "ExamineByPickingUp"}},
		});
	}

	all.push_back({
	    .id = "mind.pick_up_running_sheep",
	    .name = "The creature walks to a sheep that runs and picks it up",
	    .facet = Facet::Mind,
	    .description = "A tiger and a sheep 15 m in front of it. After one second the sheep runs away from the tiger. "
	                   "Then the script tells the tiger to pick up the sheep and look at it.",
	    .expected = "On each turn the tiger aims at the place where the sheep will be when it gets there, not at the "
	                "place where the sheep is. When the sheep stops, the tiger walks up to it, reaches to where the "
	                "sheep will be at the moment of the hold, and picks it up.",
	    .framing = {.shot = Shot::Follow, .distance = 3.0f},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f})},
	    .objects = {{.type = AnimalInfo::Sheep, .offset = {15.0f, 0.0f}, .living = true}},
	    .commands = {{.kind = Kind::AnimalFlees, .delaySeconds = 1.0f, .object = 0},
	                 {.kind = Kind::DoAction, .delaySeconds = 0.5f, .object = 0, .action = "ExamineByPickingUp"}},
	});

	all.push_back({
	    .id = "mind.pick_up_walking_villager",
	    .name = "The creature walks to a villager that walks past and picks it up",
	    .facet = Facet::Mind,
	    .description = "A tiger. A villager 30 m in front of it and 20 m to the side walks across in front of the tiger, "
	                   "to 40 m on the other side. The script tells the tiger to pick up the villager and look at it.",
	    .expected = "The tiger walks to a place ahead of the villager on the villager's way, and aims again on each "
	                "turn. It reaches to where the villager will be at the moment of the hold, and picks it up.",
	    .framing = {.shot = Shot::Follow, .distance = 3.0f},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f})},
	    .objects = {{.type = VillagerInfo::CelticHousewifeFemale,
	                 .offset = {30.0f, 20.0f},
	                 .walkTo = glm::vec2 {30.0f, -40.0f},
	                 .walkAfterSeconds = 0.5f}},
	    .commands = {{.kind = Kind::DoAction, .delaySeconds = 1.0f, .object = 0, .action = "ExamineByPickingUp"}},
	});

	all.push_back({
	    .id = "mind.mimic",
	    .name = "Mimic the player",
	    .facet = Facet::Mind,
	    .description = "A grown-up tiger with two toy balls about it. Every 20 seconds the player plays with a toy near it, "
	                   "the one deed creatures copy without the learning leash.",
	    .expected = "Every time, unless it is still copying the action of the last time, it picks up a ball and throws "
	                "it about, copying the player; then for a while it wants to play. The deed is on no thing, so the "
	                "creature does not show that it saw it. The Mind panel shows the stage of copying.",
	    .framing = {.shot = Shot::Overview},
	    .creatures = {CreatureSetup {.species = CreatureType::Tiger, .needs = Content()}},
	    // A loose ball from the mobile object table is not a toy: the toys are statics
	    .objects = {{.type = MobileStaticInfo::ToyBall, .offset = {25.0f, -25.0f}},
	                {.type = MobileStaticInfo::ToyBall, .offset = {-30.0f, -20.0f}}},
	    .commands =
	        {{.kind = Kind::PlayerDid, .creature = 0, .delaySeconds = 3.0f, .point = {30.0f, -30.0f}, .value = k_PlayWithToy},
	         Act(Kind::Stop, 0, 20.0f)},
	    .repeatFrom = 0,
	});

	all.push_back({
	    .id = "mind.mimic_same_kind",
	    .name = "Mimic the player on a thing of the same kind",
	    .facet = Facet::Mind,
	    .description = "A grown-up tiger. The player plays with a loose ball that is not a toy. A toy die lies near the "
	                   "tiger and a cuddly toy lies far from it. Every 20 seconds the player plays again.",
	    .expected = "The tiger cannot play with the player's ball, so it plays with the nearest thing of the same kind: "
	                "the toy die. The log shows the pass and the thing for each copy.",
	    .framing = {.shot = Shot::Overview},
	    .creatures = {CreatureSetup {.species = CreatureType::Tiger, .needs = Content()}},
	    .objects = {{.type = MobileObjectInfo::Ball, .offset = {30.0f, -30.0f}},
	                {.type = MobileStaticInfo::ToyDie, .offset = {12.0f, 8.0f}},
	                {.type = MobileStaticInfo::ToyCuddly, .offset = {-35.0f, 25.0f}}},
	    .commands = {{.kind = Kind::PlayerDidOn, .creature = 0, .delaySeconds = 3.0f, .value = k_PlayWithToy, .object = 0},
	                 Act(Kind::Stop, 0, 20.0f)},
	    .repeatFrom = 0,
	});

	// The deeds of the player that the five notices follow, from the least likely to be copied to the most likely: a
	// creature that copies a deed only starts to copy a deed that is as likely or more likely
	constexpr size_t k_PlantTree = 11;
	constexpr size_t k_PutFoodInWorshipSite = 0;
	constexpr size_t k_DamageByThrowing = 15;
	constexpr size_t k_StealObjectToTown = 35;
	const auto deedOn = [](size_t deed, float delay) {
		return Command {.kind = Kind::PlayerDidOn, .creature = 0, .delaySeconds = delay, .value = deed, .object = 0};
	};
	all.push_back({
	    .id = "mind.notice_deed",
	    .name = "Show that it saw a deed",
	    .facet = Facet::Mind,
	    .description = "A grown-up tiger on the learning leash, near a rock. Every 20 seconds the player does a deed on "
	                   "the rock: plant a tree, put food in a worship site, damage by a throw, steal a thing, play with "
	                   "a toy.",
	    .expected = "After each deed the tiger points at the rock, amazed, turns to the camera and watches it. Then it "
	                "plays an action: aha after the tree, happy or aha after the food, angry (and one time in three a "
	                "laugh first) after the damage, a scratch after the theft, happy after the toy. The aggressive "
	                "notice points for a shorter time and watches the camera one time, angry.",
	    .framing = {.shot = Shot::Follow},
	    .creatures = {CreatureSetup {.species = CreatureType::Tiger, .needs = Content()}},
	    .objects = {{.type = MobileStaticInfo::Rock, .offset = {25.0f, -25.0f}}},
	    .commands = {Leash(0, LeashType::Rope, 1.0f), deedOn(k_PlantTree, 2.0f), deedOn(k_PutFoodInWorshipSite, 20.0f),
	                 deedOn(k_DamageByThrowing, 20.0f), deedOn(k_StealObjectToTown, 20.0f), deedOn(k_PlayWithToy, 20.0f)},
	});

	constexpr size_t k_PutWoodInWorkshop = 9;
	all.push_back({
	    .id = "mind.copy_wood_to_workshop",
	    .name = "Copy the player's gift of wood to a workshop",
	    .facet = Facet::Mind,
	    .description = "A grown-up tiger on the learning leash, with its town's workshop near it and in its view, and a birch "
	                   "behind it. Every 30 seconds the player gives wood to the workshop, four times.",
	    .expected = "After a gift the tiger points at the workshop and watches the camera. When it copies the gift, it "
	                "walks to the birch, pulls it up, carries it to the workshop and throws it in: the workshop's pile of "
	                "logs shows the wood. The log shows the copy of the gift as the action GiveWoodFromTreeToWorkshop.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-30.0f, -45.0f}, {40.0f, 25.0f}}},
	    .creatures = {CreatureSetup {.species = CreatureType::Tiger, .needs = Content()}},
	    .objects = {{.type = AbodeInfo::CelticWorkshop, .offset = {30.0f, -35.0f}, .storedFood = 0, .storedWood = 0},
	                {.type = TreeInfo::Birch, .offset = {-25.0f, 20.0f}}},
	    .commands = {Leash(0, LeashType::Rope, 1.0f), deedOn(k_PutWoodInWorkshop, 2.0f), deedOn(k_PutWoodInWorkshop, 30.0f),
	                 deedOn(k_PutWoodInWorkshop, 30.0f), deedOn(k_PutWoodInWorkshop, 30.0f)},
	});

	all.push_back({
	    .id = "mind.deed_sight",
	    .name = "Copy only a deed it can see",
	    .facet = Facet::Mind,
	    .description = "A grown-up tiger faces a rock 40 m ahead. Another rock is 40 m behind it "
	                   "and a third 400 m ahead. The player plays with a toy at the rock behind it, then at the far "
	                   "rock, then at the near rock ahead.",
	    .expected = "The tiger does not copy the deeds behind it and far away. It copies only the deed at the near rock: "
	                "the log shows \"I saw you\" one time. The Desires panel shows the first source of the desire to "
	                "play 0.1 higher and the wish to follow the player 0.3 higher after that deed.",
	    .framing = {.shot = Shot::Follow},
	    .creatures = {CreatureSetup {.species = CreatureType::Tiger, .needs = Content()}},
	    .objects = {{.type = MobileStaticInfo::Rock, .offset = {0.0f, 40.0f}},
	                {.type = MobileStaticInfo::Rock, .offset = {0.0f, -400.0f}},
	                {.type = MobileStaticInfo::Rock, .offset = {0.0f, -40.0f}}},
	    .commands = {{.kind = Kind::PlayerDidOn, .creature = 0, .delaySeconds = 2.0f, .value = k_PlayWithToy, .object = 0},
	                 {.kind = Kind::PlayerDidOn, .creature = 0, .delaySeconds = 4.0f, .value = k_PlayWithToy, .object = 1},
	                 {.kind = Kind::PlayerDidOn, .creature = 0, .delaySeconds = 4.0f, .value = k_PlayWithToy, .object = 2}},
	});

	all.push_back({
	    .id = "mind.copy_broken_rock",
	    .name = "Copy a broken rock",
	    .facet = Facet::Mind,
	    .description = "A grown-up tiger on the learning leash faces a chalk rock 30 m ahead. Two more chalk rocks are to "
	                   "its left and right. The hand taps the rock ahead and breaks it. Then the leash comes off.",
	    .expected = "The tiger sees the deed: the log shows \"I saw you: Break rocks\" and it learns that rocks are good to "
	                "play with. The broken rock is gone, so it does not walk to it. When its wish to follow the player wins, "
	                "or after 3 minutes, it copies the deed: it picks up a rock of the same kind and breaks it. The log "
	                "shows \"I'm copying you: BreakRock\".",
	    .framing = {.shot = Shot::Follow},
	    .creatures = {CreatureSetup {.species = CreatureType::Tiger, .needs = Content()}},
	    .objects = {{.type = MobileStaticInfo::RockChalk, .offset = {0.0f, -30.0f}},
	                {.type = MobileStaticInfo::RockChalk, .offset = {-15.0f, -20.0f}},
	                {.type = MobileStaticInfo::RockChalk, .offset = {15.0f, -20.0f}}},
	    .commands = {Leash(0, LeashType::Rope, 1.0f),
	                 {.kind = Kind::HandTapObject, .delaySeconds = 2.0f, .object = 0},
	                 Act(Kind::TakeOffLeash, 0, 2.0f)},
	});

	// The creature's happy action, played once as a script asks
	constexpr uint32_t k_HappyAction = 55;
	all.push_back({
	    .id = "mind.hand_free_action",
	    .name = "Let go of a thing before an action",
	    .facet = Facet::Mind,
	    .description = "A content tiger. A script tells it to hold a rock. 8 seconds later, while it holds the rock, a "
	                   "script tells it to play its happy action two times. This repeats every 20 seconds.",
	    .expected = "Before the first happy action, the tiger lets go of the rock. It puts the rock down when it finds a "
	                "plan for the rock with Impress, Compassion or Curiosity. Else it tosses the rock away. Then it plays "
	                "the action two times. The log shows how it let go.",
	    .framing = {.shot = Shot::Follow},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f})},
	    .objects = {{.type = MobileStaticInfo::Rock, .offset = {15.0f, -15.0f}}},
	    .commands = {{.kind = Kind::DoAction, .delaySeconds = 1.0f, .object = 0, .action = "HoldObject"},
	                 {.kind = Kind::ScriptPlay, .delaySeconds = 8.0f, .value = k_HappyAction, .amount = 2.0f},
	                 Act(Kind::Stop, 0, 11.0f)},
	    .repeatFrom = 0,
	});

	all.push_back({
	    .id = "saves.creature_resumes",
	    .name = "Save and load: the creature does its plan again",
	    .facet = Facet::Mind,
	    .description = "A content tiger of the second player, so a load gives it the mind of the saved game's mind "
	                   "file. A script tells it to hold a rock 250 metres away, so the tiger walks to the rock. 4 "
	                   "seconds later the game saves into slot 0. 6 seconds after that, before the tiger gets to the "
	                   "rock, the game loads slot 0.",
	    .expected = "At the load the tiger is back where it was at the save, facing the same way. A script controls "
	                "it, so the save kept its plan: it walks to the rock again from the start of its step and holds "
	                "the rock. The log shows the save and the load.",
	    .framing = {.shot = Shot::Overview, .include = {{100.0f, -15.0f}, {260.0f, 15.0f}}},
	    .creatures = {[] {
		    auto tiger = Content(CreatureType::Tiger, {100.0f, 0.0f});
		    tiger.owner = PlayerNames::PLAYER_TWO;
		    return tiger;
	    }()},
	    .objects = {{.type = MobileStaticInfo::Rock, .offset = {350.0f, 0.0f}}},
	    .commands = {{.kind = Kind::DoAction, .delaySeconds = 1.0f, .object = 0, .action = "HoldObject"},
	                 {.kind = Kind::SaveGame, .delaySeconds = 4.0f, .value = 0},
	                 {.kind = Kind::LoadGame, .delaySeconds = 6.0f, .value = 0}},
	});

	all.push_back({
	    .id = "saves.creed_glow",
	    .name = "Save and load: the load restores the creed glows",
	    .facet = Facet::Mind,
	    .description = "A tiger of the second player stands on the land. After a second, a script gives it a full creed "
	                   "power above its body and a half power on its left hand. 2 seconds later the game saves into "
	                   "slot 0. 1 second after that, the script removes the two powers. 3 seconds later the game loads "
	                   "slot 0.",
	    .expected = "The tiger gets a glowing shell above its head and one on its left hand, and it starts to hum. The "
	                "shells go and the hum stops. At the load the two shells appear again with the same size, and the "
	                "tiger hums again. The log shows the save and the load.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{80.0f, -15.0f}, {120.0f, 15.0f}}},
	    .creatures = {[] {
		    auto tiger = Content(CreatureType::Tiger, {100.0f, 0.0f});
		    tiger.owner = PlayerNames::PLAYER_TWO;
		    tiger.hold = true;
		    tiger.pauseMind = true;
		    return tiger;
	    }()},
	    .commands = {{.kind = Kind::GiveCreedPower, .delaySeconds = 1.0f, .value = 3, .amount = 1.0f},
	                 {.kind = Kind::GiveCreedPower, .delaySeconds = 0.0f, .value = 0, .amount = 0.5f},
	                 {.kind = Kind::SaveGame, .delaySeconds = 2.0f, .value = 0},
	                 {.kind = Kind::GiveCreedPower, .delaySeconds = 1.0f, .value = 3, .amount = 0.0f},
	                 {.kind = Kind::GiveCreedPower, .delaySeconds = 0.0f, .value = 0, .amount = 0.0f},
	                 {.kind = Kind::LoadGame, .delaySeconds = 3.0f, .value = 0}},
	});

	all.push_back({
	    .id = "saves.world_back",
	    .name = "Save and load: the whole world comes back",
	    .facet = Facet::Mind,
	    .description = "A content tiger of the second player. A script tells it to hold a rock 250 metres away, so the "
	                   "tiger walks to the rock. 3 seconds later the game saves into slot 0. The tiger walks on. 5 "
	                   "seconds after the save the game loads slot 0: the testbed's flat land is built again and the "
	                   "saved world takes its place.",
	    .expected = "At the load the whole testbed is as it was at the save: the tiger, the rock, the dispensers and "
	                "the camera are where they were, and the clock shows the turn of the save. The tiger walks to the "
	                "rock again. The log shows the saved world with its count of entities, then the load of the whole "
	                "world of slot 0.",
	    .framing = {.shot = Shot::Overview, .include = {{100.0f, -15.0f}, {260.0f, 15.0f}}},
	    .creatures = {[] {
		    auto tiger = Content(CreatureType::Tiger, {100.0f, 0.0f});
		    tiger.owner = PlayerNames::PLAYER_TWO;
		    return tiger;
	    }()},
	    .objects = {{.type = MobileStaticInfo::Rock, .offset = {350.0f, 0.0f}}},
	    .commands = {{.kind = Kind::DoAction, .delaySeconds = 1.0f, .object = 0, .action = "HoldObject"},
	                 {.kind = Kind::SaveGame, .delaySeconds = 3.0f, .value = 0},
	                 {.kind = Kind::LoadGame, .delaySeconds = 5.0f, .value = 0}},
	});

	all.push_back({
	    .id = "saves.fire_and_walk_back",
	    .name = "Save and load: the fires burn on and the walk continues",
	    .facet = Facet::Mind,
	    .description = "Three oaks burn. A content tiger walks to a rock 250 metres away. 3 seconds later the game saves "
	                   "into slot 0. 5 seconds after the save the game loads slot 0.",
	    .expected = "At the load the oaks burn as they did at the save, with the same flames, and their fire continues "
	                "to grow and spread. The tiger continues on the same route to the rock. The log shows the running "
	                "state of the systems in the save and no section that does not read.",
	    .framing = {.shot = Shot::Overview, .include = {{-60.0f, -40.0f}, {260.0f, 15.0f}}},
	    .creatures = {Content(CreatureType::Tiger, {100.0f, 0.0f})},
	    .objects = {{.type = MobileStaticInfo::Rock, .offset = {350.0f, 0.0f}},
	                {.type = TreeInfo::Oak, .offset = {-40.0f, -30.0f}, .alight = true},
	                {.type = TreeInfo::Oak, .offset = {-50.0f, -20.0f}, .alight = true},
	                {.type = TreeInfo::Oak, .offset = {-30.0f, -40.0f}, .alight = true}},
	    .commands = {{.kind = Kind::DoAction, .delaySeconds = 1.0f, .object = 0, .action = "HoldObject"},
	                 {.kind = Kind::SaveGame, .delaySeconds = 3.0f, .value = 0},
	                 {.kind = Kind::LoadGame, .delaySeconds = 5.0f, .value = 0}},
	});

	all.push_back({
	    .id = "saves.miracles_back",
	    .name = "Save and load: a shield and a fireball in flight come back",
	    .facet = Facet::Miracles,
	    .description = "A shield over a content tiger. 4 seconds after the start a hand throws a fireball at the shield. "
	                   "4.6 seconds after the start the game saves into slot 0. 3 seconds after the save the game loads "
	                   "slot 0.",
	    .expected = "At the save the box shows \"Saving...\" on a grey band across the middle of a darker screen. At the "
	                "load the box shows \"Loading...\". After the load the shield stands with its vapour. The fireball "
	                "continues its flight from where it was at the save, bounces off the shield with a spark and burns "
	                "out. The log shows no section that does not read and no value that the save does not keep.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{0.0f, -5.0f}, {0.0f, 70.0f}}},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 40.0f})},
	    .miracles = {{.type = MagicType::Shield, .point = {0.0f, 40.0f}, .delaySeconds = 0.5f},
	                 {.type = MagicType::Fireball,
	                  .point = {0.0f, 40.0f},
	                  .handOffset = {0.0f, -5.0f},
	                  .handHeight = 10.0f,
	                  .throwVelocity = {0.0f, 4.0f, 40.0f},
	                  .delaySeconds = 4.0f}},
	    .commands = {{.kind = Kind::SaveGame, .delaySeconds = 4.6f, .value = 0},
	                 {.kind = Kind::LoadGame, .delaySeconds = 3.0f, .value = 0}},
	});

	// A script casts two fireballs and doubles the strength of the second, as the fourth land's fireballs do in their
	// aggressive waves
	all.push_back({
	    .id = "miracles.script_fireball_strength",
	    .name = "A script doubles the strength of the fireball that it casts",
	    .facet = Facet::Miracles,
	    .description = "One second after the start, a script casts two level one fireballs from 30 metres above two "
	                   "points 30 metres apart, as no player's miracle with radius 1 and time 30. Then SET_PROPERTY sets "
	                   "the strength of the second fireball to 2.",
	    .expected = "Both fireballs fall onto their points. The left fireball has the strength of a plain script "
	                "fireball. The right fireball has two times that strength each turn. A script that reads the "
	                "strength of the right fireball gets 2. The log shows no \"Cannot set strength of this\" message.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-25.0f, -10.0f}, {25.0f, 10.0f}}},
	    .commands = {{.kind = Kind::CallNative,
	                  .delaySeconds = 1.0f,
	                  .script = "SPELL_AT_POS",
	                  .scriptArguments = {{.kind = ScriptArgument::Kind::Integer, .number = 1.0f},
	                                      {.kind = ScriptArgument::Kind::Position, .point = {-15.0f, 0.0f}},
	                                      {.kind = ScriptArgument::Kind::Position, .point = {-15.0f, 0.0f}, .height = 30.0f},
	                                      {.kind = ScriptArgument::Kind::Number, .number = 1.0f},
	                                      {.kind = ScriptArgument::Kind::Number, .number = 30.0f},
	                                      {.kind = ScriptArgument::Kind::Number, .number = 0.0f}}},
	                 {.kind = Kind::CallNative,
	                  .delaySeconds = 0.1f,
	                  .script = "SPELL_AT_POS",
	                  .scriptArguments = {{.kind = ScriptArgument::Kind::Integer, .number = 1.0f},
	                                      {.kind = ScriptArgument::Kind::Position, .point = {15.0f, 0.0f}},
	                                      {.kind = ScriptArgument::Kind::Position, .point = {15.0f, 0.0f}, .height = 30.0f},
	                                      {.kind = ScriptArgument::Kind::Number, .number = 1.0f},
	                                      {.kind = ScriptArgument::Kind::Number, .number = 30.0f},
	                                      {.kind = ScriptArgument::Kind::Number, .number = 0.0f}}},
	                 {.kind = Kind::CallNative,
	                  .delaySeconds = 0.1f,
	                  .script = "SET_PROPERTY",
	                  .scriptArguments = {{.kind = ScriptArgument::Kind::Integer, .number = 17.0f},
	                                      {.kind = ScriptArgument::Kind::Made, .index = 1, .kept = true},
	                                      {.kind = ScriptArgument::Kind::Number, .number = 2.0f}}}},
	});

	// A physical shield stands in the physics as a solid dome from the turn that it goes up
	all.push_back({
	    .id = "miracles.physical_shield_stops_boulders",
	    .name = "A physical shield stops thrown boulders",
	    .facet = Facet::Miracles,
	    .description = "A hand casts a physical shield over a point 40 metres north. 3 seconds after the start a hand "
	                   "throws a boulder north at the shield from 50 metres south of it. The throw occurs again "
	                   "every 6 seconds.",
	    .expected = "The dome rises and spins. Each boulder hits the side of the dome and bounces back or rolls off it. "
	                "No boulder goes through the dome or lands in it.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{0.0f, -10.0f}, {0.0f, 70.0f}}},
	    .objects = {{.type = MobileStaticInfo::Boulder1Chalk, .offset = {0.0f, -10.0f}}},
	    .miracles = {{.type = MagicType::PhysicalShield, .point = {0.0f, 40.0f}, .delaySeconds = 0.5f}},
	    .throws = {{.object = 0,
	                .from = glm::vec2 {0.0f, -10.0f},
	                .height = 6.0f,
	                .velocity = {0.0f, 6.0f, 25.0f},
	                .delaySeconds = 3.0f,
	                .repeatSeconds = 6.0f}},
	});

	// Two rocks fall fast on a small physical shield, as the rocks of Khazar's Shield Challenge fall on the shield over
	// his hut: the dome is light and soft, so it holds the smaller rock and lets the larger rock through
	all.push_back({
	    .id = "miracles.small_shield_and_falling_rocks",
	    .name = "A small physical shield and two falling rocks",
	    .facet = Facet::Miracles,
	    .description = "A script casts a physical shield of radius 15.6 over a point 40 metres north. 3 seconds after "
	                   "the start a rock of scale 0.6 falls from 50 metres above a point 22 metres east and 21 metres "
	                   "south of the shield's middle. It falls toward the middle and arrives there after 2 seconds, as "
	                   "the challenge's rocks do. 6 seconds later a rock of scale 0.7 falls in the same way. The falls "
	                   "occur again every 12 seconds.",
	    .expected = "The dome rises and spins. Each rock hits the dome at about 30 metres a second. The rock of scale "
	                "0.6 sinks a little into the dome, then rolls off it to the land outside. The rock of scale 0.7 "
	                "sinks into the dome, goes through it and lands near its middle, as in the original game: the "
	                "small dome is too soft for a rock of more than 5 tonnes.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-10.0f, 20.0f}, {30.0f, 60.0f}}},
	    .objects = {{.type = MobileStaticInfo::Rock, .offset = {22.0f, 19.0f}, .scale = 0.6f},
	                {.type = MobileStaticInfo::Rock, .offset = {28.0f, 19.0f}, .scale = 0.7f}},
	    .miracles = {{.type = MagicType::PhysicalShield,
	                  .point = {0.0f, 40.0f},
	                  .delaySeconds = 0.5f,
	                  .script = ScriptSpell {.radius = 15.6f, .seconds = 600.0f, .curl = 0.0f}}},
	    .throws = {{.object = 0,
	                .from = glm::vec2 {22.0f, 19.0f},
	                .height = 50.0f,
	                .velocity = {-11.0f, -15.19f, 10.5f},
	                .delaySeconds = 3.0f,
	                .repeatSeconds = 12.0f},
	               {.object = 1,
	                .from = glm::vec2 {22.0f, 19.0f},
	                .height = 50.0f,
	                .velocity = {-11.0f, -15.19f, 10.5f},
	                .delaySeconds = 9.0f,
	                .repeatSeconds = 12.0f}},
	});

	// A save while sounds, script music and an advisor's line play: a load brings back only what the game keeps
	all.push_back({
	    .id = "saves.sounds_music_help_back",
	    .name = "Save and load: the sounds, the music and the help text",
	    .facet = Facet::Audio,
	    .description = "The player's giant ape gets the full creed power and hums. A script attaches the village bell "
	                   "sound to a ball. A script starts the music of the chosen creature. A script shows a help text "
	                   "that the good advisor says. While the advisor talks, the game saves into slot 0. 3 seconds "
	                   "after the save the game loads slot 0.",
	    .expected = "After the load no sound that played at the save continues. The hum of the ape starts again. The "
	                "bell of the ball does not start again. The help text shows again with its full reading time, and "
	                "the advisor does not say it again. No music plays: the script's music counts as playing, as in "
	                "the game, until a script stops it or starts other music.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-40.0f, 0.0f}, {40.0f, 0.0f}}},
	    .creatures = {Posed(CreatureType::GiantApe, {-20.0f, 10.0f}, 90.0f)},
	    .objects = {{.type = MobileStaticInfo::ToyBall, .offset = {10.0f, -10.0f}}},
	    .commands = {Command {.kind = Kind::GiveCreedPower, .delaySeconds = 1.0f, .value = 3, .amount = 1.0f},
	                 Command {.kind = Kind::AttachObjectScriptSound,
	                          .delaySeconds = 1.0f,
	                          .value = static_cast<size_t>(audio::SoundId::G_VillageBell)},
	                 Command {.kind = Kind::StartScriptMusic,
	                          .delaySeconds = 0.5f,
	                          .value = static_cast<size_t>(audio::MusicType::ScriptCreatureChosen)},
	                 Command {.kind = Kind::RunText, .delaySeconds = 1.0f, .value = 1009},
	                 Command {.kind = Kind::SaveGame, .delaySeconds = 1.5f, .value = 0},
	                 Command {.kind = Kind::LoadGame, .delaySeconds = 3.0f, .value = 0}},
	});

	// A save while a vortex out of the land writes the crossing file: the load puts the file back as it was at the save
	all.push_back({
	    .id = "saves.vortex_crossing_back",
	    .name = "Save and load: a vortex that leads away",
	    .facet = Facet::Story,
	    .description = "A vortex that leads away from the land opens among six oaks. It takes the oaks one by one and "
	                   "writes each into the crossing file. 4 seconds after the start the game saves into slot 0. 6 "
	                   "seconds after the save the game loads slot 0.",
	    .expected = "At the save the vortex has taken the six oaks. After the load the vortex is open at the same "
	                "place, and the crossing file (vortex.txt in the player's saved games) holds the six lines that "
	                "it held at the save. An oak moved into the vortex after the load adds its line after them.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-30.0f, -30.0f}, {30.0f, 30.0f}}},
	    .objects = {{.type = TreeInfo::Oak, .offset = {6.0f, 0.0f}},
	                {.type = TreeInfo::Oak, .offset = {-8.0f, 3.0f}},
	                {.type = TreeInfo::Oak, .offset = {2.0f, -11.0f}},
	                {.type = TreeInfo::Oak, .offset = {14.0f, 9.0f}},
	                {.type = TreeInfo::Oak, .offset = {-15.0f, -12.0f}},
	                {.type = TreeInfo::Oak, .offset = {20.0f, -18.0f}}},
	    .vortices = {{.type = VortexType::In, .offset = {0.0f, 0.0f}, .delaySeconds = 0.5f}},
	    .commands = {Command {.kind = Kind::SaveGame, .delaySeconds = 4.0f, .value = 0},
	                 Command {.kind = Kind::LoadGame, .delaySeconds = 6.0f, .value = 0}},
	});

	// Both kinds of land vortex, open at a save: after the load each looks as it did before the save
	all.push_back({
	    .id = "saves.vortex_drawn_after_load",
	    .name = "Save and load: two open vortices",
	    .facet = Facet::Story,
	    .description = "A vortex that leads away from the land opens on the left. A vortex that leads into the land "
	                   "opens on the right. 6 seconds after the start the game saves into slot 0. 4 seconds after the "
	                   "save the game loads slot 0.",
	    .expected = "Before and after the load, each vortex shows its swirl in the hole in the land, with the stars "
	                "that turn over it. The land under each vortex stays level. After the load nothing of the land "
	                "or the sea shows in the holes.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-60.0f, -30.0f}, {60.0f, 30.0f}}},
	    .vortices = {{.type = VortexType::In, .offset = {-35.0f, 0.0f}, .delaySeconds = 0.5f},
	                 {.type = VortexType::Out, .offset = {35.0f, 0.0f}, .delaySeconds = 0.5f}},
	    .commands = {Command {.kind = Kind::SaveGame, .delaySeconds = 6.0f, .value = 0},
	                 Command {.kind = Kind::LoadGame, .delaySeconds = 4.0f, .value = 0}},
	});

	// A vortex levels the pond and then goes. The saved game keeps the levelled land, and the load gives it back.
	all.push_back({
	    .id = "saves.levelled_land_back",
	    .name = "Save and load: the land that a vortex levelled",
	    .facet = Facet::Story,
	    .description = "A vortex that leads away from the land opens over the pond north-west of the middle. It levels "
	                   "the pond and its banks. 4 seconds later it starts to fade out, and 7 seconds after that it is "
	                   "gone. 14 seconds after the start the game saves into slot 0. 4 seconds after the save the game "
	                   "loads slot 0.",
	    .expected = "When the vortex has gone, a level square of land is where the pond and its banks were. After the "
	                "load the square is level, as it was at the save: the pond and its banks do not come back.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview,
	                .include = {flat_land::k_PondCentre - flat_land::k_MapMiddle - glm::vec2(70.0f),
	                            flat_land::k_PondCentre - flat_land::k_MapMiddle + glm::vec2(70.0f)}},
	    .vortices = {{.type = VortexType::Out,
	                  .offset = flat_land::k_PondCentre - flat_land::k_MapMiddle,
	                  .delaySeconds = 0.5f,
	                  .fadeOutAfterSeconds = 4.0f}},
	    .commands = {Command {.kind = Kind::SaveGame, .delaySeconds = 14.0f, .value = 0},
	                 Command {.kind = Kind::LoadGame, .delaySeconds = 4.0f, .value = 0}},
	});

	all.push_back({
	    .id = "frontend.continue_with_menu",
	    .name = "Start-up: continue from the save on exit",
	    .facet = Facet::Story,
	    .description = "Two oaks stand on the testbed. 2 seconds after the start the game saves into the slot of the "
	                   "save on exit (slot 20). 1 second later the game continues as a plain start of the program "
	                   "does.",
	    .expected = "The tips screen shows while slot 20 loads. Then the menu fades in over the testbed as it was "
	                "saved, with the profile's name above its buttons, and the game is paused under it. The hand is "
	                "hidden and the menu's pointer shows. Continue Game closes the menu, and the game runs again.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-20.0f, -20.0f}, {20.0f, 20.0f}}},
	    .objects = {{.type = TreeInfo::Oak, .offset = {6.0f, 0.0f}}, {.type = TreeInfo::Oak, .offset = {-8.0f, 3.0f}}},
	    .commands = {Command {.kind = Kind::SaveGame, .delaySeconds = 2.0f, .value = 20},
	                 Command {.kind = Kind::ContinueGame, .delaySeconds = 1.0f}},
	});

	all.push_back({
	    .id = "frontend.logo_effect",
	    .name = "Start-up: the logo effect",
	    .facet = Facet::Story,
	    .description = "One second after the start the logo effect of the first start plays, with its sound, as before "
	                   "the first game of the program.",
	    .expected = "The screen fades from black to a sky sphere. Dots float at random depths and the camera turns round "
	                "them. The camera comes back, and the dots make the Lionhead logo, with TM at its top right. The "
	                "dots leave trails. Then the camera flies through the logo and the screen goes black after about 33 "
	                "seconds. A key ends it at once. On a processor of family 15 with SSE2 (all recent AMD processors) "
	                "the sponsor's logo shows at the bottom left.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-20.0f, -20.0f}, {20.0f, 20.0f}}},
	    .commands = {Command {.kind = Kind::PlayLogoEffect, .delaySeconds = 1.0f}},
	});

	all.push_back({
	    .id = "mind.hand_free_pick_up",
	    .name = "Put down a thing before a pick-up",
	    .facet = Facet::Mind,
	    .description = "A content tiger. A script tells it to hold the first rock. 8 seconds later a script tells it to "
	                   "examine the second rock. 20 seconds later a script tells it to hold the second rock, and 8 "
	                   "seconds after that to examine the second rock again.",
	    .expected = "For the first order to examine, the tiger puts the first rock down (it does not toss it). Then it "
	                "picks up the second rock and examines it. For the second order to examine, it does not pick the "
	                "rock up again: it examines the rock that it holds at once.",
	    .framing = {.shot = Shot::Follow},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f})},
	    .objects = {{.type = MobileStaticInfo::Rock, .offset = {15.0f, -15.0f}},
	                {.type = MobileStaticInfo::Rock, .offset = {-15.0f, -15.0f}}},
	    .commands = {{.kind = Kind::DoAction, .delaySeconds = 1.0f, .object = 0, .action = "HoldObject"},
	                 {.kind = Kind::DoAction, .delaySeconds = 8.0f, .object = 1, .action = "ExamineByPickingUp"},
	                 {.kind = Kind::DoAction, .delaySeconds = 20.0f, .object = 1, .action = "HoldObject"},
	                 {.kind = Kind::DoAction, .delaySeconds = 8.0f, .object = 1, .action = "ExamineByPickingUp"}},
	});

	// An angry tiger with two tigers of another player about it: its friend, and one that a fight knocked out
	auto furious = Content(CreatureType::Tiger, {0.0f, 0.0f}, 0.0f, "angry tiger");
	furious.desires = OnlyDesire(Desire::Anger);
	const auto otherTiger = [](glm::vec2 offset, float facing, std::string_view label) {
		auto tiger = Content(CreatureType::Tiger, offset, facing, label);
		tiger.owner = PlayerNames::PLAYER_TWO;
		return tiger;
	};
	all.push_back({
	    .id = "mind.friends_and_dead",
	    .name = "Not angry with friends or dead creatures",
	    .facet = Facet::Mind,
	    .description = "An angry tiger stands between two tigers of another player. The tiger to its left becomes its "
	                   "friend, as a script makes friends. A fight knocks out the tiger to its right.",
	    .expected = "The angry tiger makes no plan of anger about its friend, and none about the tiger that lies "
	                "knocked out. Both tigers have no worth for its anger. The inspector shows the friends of each "
	                "tiger and the knock-out. When the knocked out tiger is up again, the angry tiger can plan a fight "
	                "with it.",
	    .framing = {.shot = Shot::Overview},
	    .creatures = {furious, otherTiger({-40.0f, 0.0f}, 90.0f, "friend"), otherTiger({40.0f, 0.0f}, 270.0f, "knocked out")},
	    // The friends and the knock-out are made before the angry tiger can plan anything
	    .commands = {{.kind = Kind::MakeFriends, .creature = 0, .delaySeconds = 0.0f, .value = 1},
	                 Fight(Kind::KnockOut, 2, 0, 0.0f)},
	});
	all.push_back({
	    .id = "combat.anger_picks_foe",
	    .name = "An angry creature fights neither a friend nor a dead creature",
	    .facet = Facet::Combat,
	    .description = "An angry tiger stands between two tigers of another player, as in mind.friends_and_dead: its "
	                   "friend to its left and a knocked out tiger to its right. A third tiger of that player stands "
	                   "further away, in front of it.",
	    .expected = "The angry tiger does not fight its friend or the knocked out tiger. Its anger makes a plan to fight "
	                "the tiger in front of it, and the two duel. The fight transcript gives the reason anger.",
	    .framing = {.shot = Shot::Overview},
	    .creatures = {furious, otherTiger({-40.0f, 0.0f}, 90.0f, "friend"), otherTiger({40.0f, 0.0f}, 270.0f, "knocked out"),
	                  otherTiger({0.0f, 70.0f}, 180.0f, "foe")},
	    // The friends and the knock-out are made before the angry tiger can plan anything
	    .commands = {{.kind = Kind::MakeFriends, .creature = 0, .delaySeconds = 0.0f, .value = 1},
	                 Fight(Kind::KnockOut, 2, 0, 0.0f)},
	});

	// A tiger of the player and a lion of no player, as the guide is on the first land
	const auto guideLion = [](glm::vec2 offset, float facing) {
		auto lion = Content(CreatureType::Lion, offset, facing, "guide");
		lion.owner = PlayerNames::NEUTRAL;
		return lion;
	};
	auto befriender = Content(CreatureType::Tiger, {-25.0f, 0.0f}, 90.0f, "befriends");
	befriender.desires = OnlyDesire(Desire::BeFriends);
	all.push_back({
	    .id = "friends.wave_with_guide",
	    .name = "Make friends with the guide",
	    .facet = Facet::Mind,
	    .description = "A tiger and a lion of no player stand 50 apart and face each other. As a script of the first land "
	                   "does, both want only to be friends.",
	    .expected = "The tiger smiles, howls or waves at the lion. To wave, it asks the lion to play first; the lion agrees "
	                "when it wants to be friends with the tiger more than it fears it or is angry with it. Then the lion "
	                "stands still in its mind, turns to the tiger and waves back as the tiger waves, from where they "
	                "stand. The tiger grimaces as it tells the lion to turn. The lion shows no face of the order. The "
	                "inspector shows the partners while they wave.",
	    .framing = {.shot = Shot::Overview},
	    .creatures = {befriender, guideLion({25.0f, 0.0f}, 270.0f)},
	    .commands = {{.kind = Kind::OnlyDesire,
	                  .creature = 0,
	                  .delaySeconds = 0.0f,
	                  .value = static_cast<uint32_t>(Desire::BeFriends),
	                  .amount = 600.0f},
	                 {.kind = Kind::OnlyDesire,
	                  .creature = 1,
	                  .delaySeconds = 0.0f,
	                  .value = static_cast<uint32_t>(Desire::BeFriends),
	                  .amount = 600.0f}},
	});
	// The lion of no player asks the player's tiger, which wants only to sleep, as the guide does after the fight lesson
	auto askingLion = guideLion({25.0f, 0.0f}, 270.0f);
	askingLion.desires = OnlyDesire(Desire::BeFriends);
	auto sleepyTiger = Content(CreatureType::Tiger, {-25.0f, 0.0f}, 90.0f, "refuses");
	sleepyTiger.desires = OnlyDesire(Desire::Tiredness);
	all.push_back({
	    .id = "friends.refused",
	    .name = "Another creature refuses to be friends",
	    .facet = Facet::Mind,
	    .description = "A lion of no player and your tiger stand 50 apart and face each other. The lion wants only to be "
	                   "friends. Your tiger wants only to sleep. No script has made a creature do an action.",
	    .expected = "The lion asks your tiger to play or to be friends. Your tiger wants neither more than it fears the "
	                "lion or is angry with it, so it refuses. The lion stops that action, and your tiger tells you that "
	                "it refused to play or to be friends. The log says that the lion stops because the tiger refused. "
	                "When your tiger looks the lion over, its wish to be friends can grow. Then it can ask the lion, which "
	                "agrees.",
	    .framing = {.shot = Shot::Overview},
	    .creatures = {sleepyTiger, askingLion},
	    .commands = {{.kind = Kind::OnlyDesire,
	                  .creature = 0,
	                  .delaySeconds = 0.0f,
	                  .value = static_cast<uint32_t>(Desire::Tiredness),
	                  .amount = 600.0f},
	                 {.kind = Kind::OnlyDesire,
	                  .creature = 1,
	                  .delaySeconds = 0.0f,
	                  .value = static_cast<uint32_t>(Desire::BeFriends),
	                  .amount = 600.0f}},
	});
	all.push_back({
	    .id = "friends.side_by_side",
	    .name = "Do things side by side with a friend",
	    .facet = Facet::Mind,
	    .description = "A tiger and a lion of no player stand 50 apart and face each other. Both want only to be friends.",
	    .expected = "Some of the time the tiger asks the lion to be its friend and does a joint action with it. To be happy "
	                "together, both go to places side by side, the tiger waits until the lion stops, both turn to face "
	                "each other and both play the happy action. To sit together, both go to the two sides of a clear "
	                "place, turn to face each other and sit for 10 to 15 seconds; then the tiger plays the happy action. "
	                "The inspector shows the partners while they do it.",
	    .framing = {.shot = Shot::Overview},
	    .creatures = {befriender, guideLion({25.0f, 0.0f}, 270.0f)},
	    .commands = {{.kind = Kind::OnlyDesire,
	                  .creature = 0,
	                  .delaySeconds = 0.0f,
	                  .value = static_cast<uint32_t>(Desire::BeFriends),
	                  .amount = 600.0f},
	                 {.kind = Kind::OnlyDesire,
	                  .creature = 1,
	                  .delaySeconds = 0.0f,
	                  .value = static_cast<uint32_t>(Desire::BeFriends),
	                  .amount = 600.0f}},
	});
	// One joint action with the lion, which a script tells the tiger to do. The lion first smiles at the tiger by a
	// script too, so that a script controls both and the lion agrees.
	struct JointScene
	{
		std::string_view id;
		std::string_view name;
		std::string_view action;
		bool town {false};
		std::optional<Desire> desire;
		std::string_view expected;
	};
	for (const auto& scene : {
	         JointScene {.id = "friends.happy_together",
	                     .name = "Be happy with a friend",
	                     .action = "BeHappyWithFriend",
	                     .expected = "Both go to places side by side and the tiger waits until the lion stops. Both turn "
	                                 "to face each other and play the happy action."},
	         JointScene {.id = "friends.sit_together",
	                     .name = "Sit with a friend",
	                     .action = "SitWithFriend",
	                     .expected = "Both go to the two sides of a clear place, turn to face each other and sit for 10 "
	                                 "to 15 seconds. Then the tiger plays the happy action."},
	         JointScene {.id = "friends.sleep_together",
	                     .name = "Sleep with a friend",
	                     .action = "SleepWithFriend",
	                     .desire = Desire::Tiredness,
	                     .expected = "Both go to the two sides of a clear place, turn to face each other and sleep for 17 "
	                                 "seconds. Then the tiger wakes with a confused look."},
	         JointScene {.id = "friends.drink_together",
	                     .name = "Drink with a friend",
	                     .action = "DrinkWithFriend",
	                     .desire = Desire::Water,
	                     .expected = "Both go to the nearest coast, side by side, turn to face the sea and drink. Then "
	                                 "the tiger turns to the lion with a smile."},
	         JointScene {.id = "friends.poo_together",
	                     .name = "Have a poo with a friend",
	                     .action = "PooWithFriend",
	                     .desire = Desire::Poo,
	                     .expected = "Both go to places side by side, squat for four seconds and have a poo. Then the "
	                                 "tiger turns to the lion with a smile and plays the happy action."},
	         JointScene {.id = "friends.help_town_together",
	                     .name = "Help a town with a friend",
	                     .action = "HelpTownWithFriend",
	                     .town = true,
	                     .expected = "The tiger turns to the lion, plays the happy action and points at the town. Both go "
	                                 "near the storage pit and turn to face each other. Then the first of the two to take "
	                                 "up the wish to help the town does so, and the other one stops."},
	         JointScene {.id = "friends.attack_town_together",
	                     .name = "Attack a town with a friend",
	                     .action = "AttackWithFriend",
	                     .town = true,
	                     .desire = Desire::Anger,
	                     .expected = "The script's action fails: a creature that has learnt nothing finds the town more "
	                                 "useful to its compassion than to its anger, so it cannot attack the town. A creature "
	                                 "taught to be angry with towns attacks it as helping a town with a friend."},
	     })
	{
		auto tiger = Content(CreatureType::Tiger, {-25.0f, 0.0f}, 90.0f, "asks");
		if (scene.desire.has_value())
		{
			tiger.desires = {{.desire = *scene.desire, .fraction = 0.6f}};
			tiger.hold = true;
		}
		Scenario scenario {
		    .id = scene.id,
		    .name = scene.name,
		    .facet = Facet::Mind,
		    .description = "A tiger and a lion of no player stand 50 apart and face each other. A script tells the lion "
		                   "to smile at the tiger, then tells the tiger to do a joint action with the lion.",
		    .expected = scene.expected,
		    .environment = {.dispenserGrid = false},
		    .framing = {.shot = Shot::Overview},
		    .creatures = {tiger, guideLion({25.0f, 0.0f}, 270.0f)},
		    .commands = {{.kind = Kind::DoAction,
		                  .creature = 1,
		                  .delaySeconds = 0.5f,
		                  .action = "SmileAtFriend",
		                  .atCreature = size_t {0}},
		                 {.kind = Kind::DoAction,
		                  .creature = 0,
		                  .delaySeconds = 4.0f,
		                  .action = scene.action,
		                  .atCreature = size_t {1}}},
		};
		if (scene.town)
		{
			scenario.objects = {
			    {.type = AbodeInfo::CelticStoragePit, .offset = {0.0f, -70.0f}, .storedFood = 2000, .storedWood = 0}};
		}
		all.push_back(std::move(scenario));
	}
	auto missing = Content(CreatureType::Tiger, {-60.0f, 0.0f}, 90.0f, "misses its friend");
	missing.desires = OnlyDesire(Desire::MissFriend);
	all.push_back({
	    .id = "friends.miss_friend",
	    .name = "Miss a friend",
	    .facet = Facet::Mind,
	    .description = "A tiger that wants only to see a friend it misses stands 120 from a lion of no player.",
	    .expected = "The tiger pines for the lion: with a sad face it turns to it and points at it, then plays its sad "
	                "action. Or it looks for the lion: it points at it and plays its sad action, then walks near it, to "
	                "three times its own height.",
	    .framing = {.shot = Shot::Overview},
	    .creatures = {missing, guideLion({60.0f, 0.0f}, 270.0f)},
	    .commands = {{.kind = Kind::OnlyDesire,
	                  .creature = 0,
	                  .delaySeconds = 0.0f,
	                  .value = static_cast<uint32_t>(Desire::MissFriend),
	                  .amount = 600.0f}},
	});

	all.push_back({
	    .id = "mind.watch_skill",
	    .name = "Learn a skill by watching",
	    .facet = Facet::Mind,
	    .description = "A grown-up lion watches villagers build: it sees the skill now, again 4 seconds later and again "
	                   "4 seconds after that.",
	    .expected = "Once more than the skill takes to learn has gone by since it first saw it (7 seconds for building, by "
	                "the game's table) it "
	                "knows it: its thoughts say it has learnt to build, and the Mind panel marks the skill known.",
	    .framing = {.shot = Shot::Follow},
	    .creatures = {CreatureSetup {.species = CreatureType::Lion, .needs = Content()}},
	    .commands = {{.kind = Kind::SeeSkill, .creature = 0, .delaySeconds = 1.0f, .value = k_FirstSkill},
	                 {.kind = Kind::SeeSkill, .creature = 0, .delaySeconds = 4.0f, .value = k_FirstSkill},
	                 {.kind = Kind::SeeSkill, .creature = 0, .delaySeconds = 4.0f, .value = k_FirstSkill}},
	});

	// A content tiger: it only lies down to sleep when told to, and gets up again soon after, being rested
	const auto rested = Content(CreatureType::Tiger, {0.0f, 0.0f});
	all.push_back({
	    .id = "mind.replan_mid_action",
	    .name = "Changing its plan in the middle of an action",
	    .facet = Facet::Mind,
	    .description = "A tiger starts showing it is hungry (an action played once); half a second in it is told to "
	                   "sleep, which changes its plan.",
	    .expected = "It shows it is hungry to the end of the animation, then lies down to sleep. It doesn't snap out of "
	                "the action the moment its plan changes.",
	    .framing = {.shot = Shot::Follow, .distance = 1.2f},
	    .creatures = {rested},
	    .commands = {Play(Kind::PlayAction, 0, animations::k_Hungry, 1.0f, true), Act(Kind::Sleep, 0, 0.5f, false)},
	});

	all.push_back({
	    .id = "mind.replan_from_sitting",
	    .name = "Changing its plan while sitting",
	    .facet = Facet::Mind,
	    .description = "A tiger sits down; four seconds later, sitting, it is told to sleep, which changes its plan.",
	    .expected = "It gets up through the end of sitting, then lies down to sleep. It doesn't jump from sitting "
	                "straight into lying down.",
	    .framing = {.shot = Shot::Follow, .distance = 1.2f},
	    .creatures = {rested},
	    .commands = {Act(Kind::SitDown, 0, 1.0f, true), Act(Kind::Sleep, 0, 4.0f, false)},
	});

	// Worn out, held so, but not yet wanting to sleep when the hand takes hold
	auto worn = Content(CreatureType::Tiger, {0.0f, 0.0f});
	worn.needs.exhaustion = 0.75f;
	worn.desires = {{.desire = Desire::Tiredness, .fraction = 0.0f}};
	all.push_back({
	    .id = "mind.held_while_tired",
	    .name = "Tired while held by the hand",
	    .facet = Facet::Mind,
	    .description = "The hand takes hold of a worn out tiger by stroking it; once held, its desire to sleep is set as "
	                   "strong as it gets. The hand strokes it twice more over twelve seconds, then lets go.",
	    .expected = "While the hand holds it, its plan never changes and it never lies down to sleep: a held creature's "
	                "mind stands still, choosing nothing (the Mind panel's activity stays as it was when taken hold of). "
	                "Once let go it stops what it was doing and chooses afresh.",
	    .framing = {.shot = Shot::Follow, .distance = 1.2f},
	    .creatures = {worn},
	    .commands = {Stroke(0, creature_feedback::BodyPart::Head, 1.0f), setDesire(Desire::Tiredness, 1.0f, 0.5f),
	                 Stroke(0, creature_feedback::BodyPart::Belly, 6.0f), Stroke(0, creature_feedback::BodyPart::Head, 6.0f),
	                 Act(Kind::HandLetGo, 0, 1.0f, true)},
	});

	// The player's tiger looks north over two small forests and two flocks of birds; its player's temple stands to
	// the west and another god's far off to the north east, out of its sight
	all.push_back({
	    .id = "mind.object_beliefs",
	    .name = "Beliefs about the temples, flocks and forests it has met",
	    .facet = Facet::Mind,
	    .description = "The player's tiger, its mind paused, stands looking north at two forests of three trees and two "
	                   "flocks of doves. Its player's temple is to the west; another player's is far to the north east. "
	                   "After six seconds the Creature Cave opens on its Mind scroll.",
	    .expected = "Its eyes sweep the cells about it in a couple of seconds. The Mind scroll then says it knows of 1 "
	                "temple (its player's, the nearest; the far one it never sees), 2 flocks and 2 forests.",
	    .framing = {.shot = Shot::Overview},
	    .creatures = {Posed(CreatureType::Tiger, {0.0f, -20.0f}, 180.0f, "yours")},
	    .objects = {{.type = TreeInfo::Oak, .offset = {30.0f, 25.0f}, .forest = 1},
	                {.type = TreeInfo::Oak, .offset = {38.0f, 32.0f}, .forest = 1},
	                {.type = TreeInfo::Oak, .offset = {26.0f, 36.0f}, .forest = 1},
	                {.type = TreeInfo::Conifer, .offset = {-30.0f, 25.0f}, .forest = 2},
	                {.type = TreeInfo::Conifer, .offset = {-38.0f, 32.0f}, .forest = 2},
	                {.type = TreeInfo::Conifer, .offset = {-26.0f, 36.0f}, .forest = 2}},
	    .birdFlocks = {{.kind = AnimalInfo::Dove, .offset = {-10.0f, 30.0f}, .count = 5, .reach = 10.0f},
	                   {.kind = AnimalInfo::Dove, .offset = {10.0f, 40.0f}, .count = 5, .reach = 10.0f}},
	    .temples = {{.offset = {-90.0f, 0.0f}, .angle = 0.0f},
	                {.offset = {300.0f, 300.0f}, .owner = PlayerNames::PLAYER_TWO, .angle = 0.0f}},
	    .commands = {{.kind = Kind::EnterTemple, .delaySeconds = 5.5f, .value = static_cast<size_t>(TempleRoom::Main)},
	                 {.kind = Kind::OpenCreatureCave,
	                  .creature = 0,
	                  .delaySeconds = 0.5f,
	                  .value = static_cast<size_t>(creature_cave::Page::Mind)}},
	});
	// The player's tiger looks north at another god's ogre, a hut and a villager, to read its beliefs in the inspector
	auto rival = Posed(CreatureType::Ogre, {0.0f, 25.0f}, 0.0f, "rival");
	rival.owner = PlayerNames::PLAYER_TWO;
	all.push_back({
	    .id = "mind.beliefs_rest",
	    .name = "What it believes about another creature, a building and a villager",
	    .facet = Facet::Mind,
	    .description = "The player's tiger, its mind paused, looks north at another player's ogre, a hut and a villager. "
	                   "Read its beliefs with the inspector query creature.beliefs.",
	    .expected = "After its eyes sweep about it, it has a belief about each of them. The ogre's belief shows the desire "
	                "of the ogre's last action, how many miracles the ogre knows, and that the ogre carries nothing (29). "
	                "The hut shows high life and that it is not being built (1). The villager shows high life.",
	    .framing = {.shot = Shot::Overview},
	    .creatures = {Posed(CreatureType::Tiger, {0.0f, -20.0f}, 180.0f, "yours"), rival},
	    .objects = {{.type = AbodeInfo::CelticHut, .offset = {-20.0f, 20.0f}},
	                {.type = VillagerInfo::CelticHousewifeFemale, .offset = {15.0f, 15.0f}}},
	});
	// The player's tiger looks north at a villager and a hut, which both catch fire after three seconds
	all.push_back({
	    .id = "mind.belief_attributes",
	    .name = "A belief keeps what the creature took in, and a building keeps its first sight",
	    .facet = Facet::Mind,
	    .description = "The player's tiger, its mind paused, looks north at a villager and a hut. After three seconds both "
	                   "catch fire. Read its beliefs with the inspector query creature.beliefs.",
	    .expected = "Before the fire, both beliefs show OnFire 1 (not on fire). After the fire, the villager's belief shows "
	                "OnFire 0 (on fire) once the eyes sweep over it again. The hut's belief still shows OnFire 1: a belief "
	                "about a building keeps the attributes of the first sight.",
	    .framing = {.shot = Shot::Overview},
	    .creatures = {Posed(CreatureType::Tiger, {0.0f, -20.0f}, 180.0f, "yours")},
	    .objects = {{.type = VillagerInfo::CelticHousewifeFemale, .offset = {15.0f, 15.0f}},
	                {.type = AbodeInfo::CelticHut, .offset = {-20.0f, 20.0f}}},
	    .commands = {{.kind = Kind::SetAlight, .creature = 0, .delaySeconds = 3.0f, .value = 1, .object = 0},
	                 {.kind = Kind::SetAlight, .creature = 0, .delaySeconds = 0.0f, .value = 1, .object = 1}},
	});
	// A hungry tiger, its mind running, looks north at a villager some way off; a nearer one stands behind it, out of sight
	auto peckish = Posed(CreatureType::Tiger, {0.0f, -20.0f}, 180.0f, "yours");
	peckish.needs = {.energy = 0.3f};
	peckish.hold = false;
	peckish.pauseMind = false;
	all.push_back({
	    .id = "mind.goal_it_knows",
	    .name = "Choosing only among the things it has seen",
	    .facet = Facet::Mind,
	    .description = "A hungry tiger looks north at a villager 45 m away. Another stands 14 m behind it, where it can't "
	                   "see. Its mind runs; after two seconds it is made as hungry as can be.",
	    .expected = "It goes for the villager ahead, the one it has seen, though the other is nearer: a creature weighs only "
	                "the things it knows of, each by what it has learnt of acting on such things and by how near it is.",
	    .framing = {.shot = Shot::Overview},
	    .creatures = {peckish},
	    .objects = {{.type = VillagerInfo::CelticHousewifeFemale, .offset = {0.0f, 25.0f}},
	                {.type = VillagerInfo::CelticHousewifeFemale, .offset = {0.0f, -34.0f}}},
	    .commands = {setDesire(Desire::Hunger, 1.0f, 2.0f)},
	});
	// A tiger with its mind on, alone, that wants to show its state and is very hungry
	auto communicative = Posed(CreatureType::Tiger, {0.0f, -20.0f}, 90.0f, "yours");
	communicative.hold = false;
	communicative.pauseMind = false;
	all.push_back({
	    .id = "mind.communicate_state",
	    .name = "Showing its state to the player",
	    .facet = Facet::Mind,
	    .description = "A tiger stands alone with its mind on, side on to the camera. At once it wants to show its "
	                   "state as much as possible, and it is very hungry.",
	    .expected = "It turns to face the camera, with its head on the camera, then plays its happy animation once: its mind "
	                "is less than ten seconds old, which counts as feedback from the player. Then it does not want to show "
	                "its state for a minute. After that minute it shows its state again with its hungry animation. When no "
	                "plan is ready, it stands and does nothing.",
	    .framing = {.shot = Shot::Overview},
	    .creatures = {communicative},
	    // The desires are set as soon as it has them, so that its first communication shows them
	    .commands = {setDesire(Desire::ManifestState, 1.0f, 0.1f), setDesire(Desire::Hunger, 1.0f, 0.1f)},
	});
	// A tiger of another player looks north at a village of the player: a hut and three of its villagers
	auto stranger = Posed(CreatureType::Tiger, {0.0f, -20.0f}, 180.0f, "another player's");
	stranger.owner = PlayerNames::PLAYER_TWO;
	stranger.hold = false;
	stranger.pauseMind = false;
	const std::vector<ObjectSetup> village {
	    {.type = AbodeInfo::CelticHut, .offset = {0.0f, 40.0f}},
	    {.type = VillagerInfo::CelticFarmerMale, .offset = {-8.0f, 25.0f}, .joinTown = true},
	    {.type = VillagerInfo::CelticHousewifeFemale, .offset = {0.0f, 22.0f}, .joinTown = true},
	    {.type = VillagerInfo::CelticFarmerMale, .offset = {8.0f, 25.0f}, .joinTown = true},
	};
	// A rock by the tiger, which it can hurl
	auto angryVillage = village;
	angryVillage.push_back({.type = MobileStaticInfo::RockChalk, .offset = {6.0f, -14.0f}});
	all.push_back({
	    .id = "mind.angry_at_village",
	    .name = "Anger at a village acts on one of its people",
	    .facet = Facet::Mind,
	    .description = "A tiger of another player looks north at a village of the player: a hut and three villagers. A rock "
	                   "lies by it. Its mind runs; after two seconds it is made as angry as can be.",
	    .expected = "Its anger is about the village (the Mind panel's plan shows the town as what the plan is about), and it "
	                "acts on one of the villagers that it has seen, not on the town itself.",
	    .framing = {.shot = Shot::Overview},
	    .creatures = {stranger},
	    .objects = angryVillage,
	    .commands = {setDesire(Desire::Anger, 1.0f, 2.0f)},
	});
	all.push_back({
	    .id = "mind.angry_at_temple",
	    .name = "Anger at a temple acts on its heart",
	    .facet = Facet::Mind,
	    .description = "A tiger of another player looks north at a temple of the player. A rock lies by the tiger. Its mind "
	                   "runs; after two seconds it is made as angry as can be.",
	    .expected = "Its anger is about the temple, and it acts on the heart of the temple: the temple itself, as the "
	                "temple here is one thing. It does not act on a worship site.",
	    .framing = {.shot = Shot::Overview},
	    .creatures = {stranger},
	    .objects = {{.type = MobileStaticInfo::RockChalk, .offset = {6.0f, -14.0f}}},
	    .temples = {{.offset = {0.0f, 70.0f}}},
	    .commands = {setDesire(Desire::Anger, 1.0f, 2.0f)},
	});
	all.push_back({
	    .id = "mind.impress_village",
	    .name = "To impress a village it acts on one of its people",
	    .facet = Facet::Mind,
	    .description = "A tiger of another player looks north at a village of the player: a hut and three villagers. Its "
	                   "mind runs; after two seconds its desire to impress is made as strong as can be.",
	    .expected = "Its desire to impress is about the village, and it acts on one of the villagers that it has seen and "
	                "that does not dance, never on the hut.",
	    .framing = {.shot = Shot::Overview},
	    .creatures = {stranger},
	    .objects = village,
	    .commands = {setDesire(Desire::Impress, 1.0f, 2.0f)},
	});
	// The player's tiger looks north at a village of the player, on the learning leash
	auto ownTiger = Posed(CreatureType::Tiger, {0.0f, -20.0f}, 180.0f, "player's");
	ownTiger.hold = false;
	ownTiger.pauseMind = false;
	all.push_back({
	    .id = "leash.shown_hut",
	    .name = "A hut shown on the leash",
	    .facet = Facet::Leash,
	    .description = "The player's tiger on the learning leash looks north at a village of the player: a hut and three "
	                   "villagers. Its compassion is made as strong as can be. Then the Action button of the leash "
	                   "orders it onto the hut.",
	    .expected = "First it tries the desire it wants most for the village of the hut: compassion. When compassion "
	                "has no plan, it tries the next desire, up to 40 times. The log shows the desire that it uses and "
	                "how many desires it tried.",
	    .framing = {.shot = Shot::Overview},
	    .creatures = {ownTiger},
	    .objects = village,
	    .commands = {Leash(0, LeashType::Rope, 1.0f), setDesire(Desire::Compassion, 1.0f, 1.0f),
	                 OnObject(Kind::LeashOrderOn, 0, 0, 0.5f, false), Act(Kind::TakeOffLeash, 0, 20.0f)},
	    .repeatFrom = 0,
	});
	all.push_back({
	    .id = "leash.aggression_lesson_saved",
	    .name = "A hut shown on the aggression leash, then saved",
	    .facet = Facet::Leash,
	    .description = "The player's tiger on the aggression leash looks north at a village of the player: a hut and three "
	                   "villagers. The Action button of the leash orders it onto the hut, then the leash comes off. Save "
	                   "the game and read the creature's mind file.",
	    .expected = "The tiger learns anger towards the village of the hut and less compassion for it. In the saved mind "
	                "file, the activity trees of those desires (and of the desires that depend on them) hold a new example "
	                "of lesson kind 1: the village, with the place of the village, action 0 and the desire of its tree.",
	    .framing = {.shot = Shot::Overview},
	    .creatures = {ownTiger},
	    .objects = village,
	    .commands = {Leash(0, LeashType::Evil, 1.0f), OnObject(Kind::LeashOrderOn, 0, 0, 0.5f, false),
	                 Act(Kind::TakeOffLeash, 0, 5.0f)},
	});
	// The player's evil, big tiger swaps minds with a small, neutral sheep that belongs to nobody, as The Lost Flock's
	// sheep offers; then back again
	auto swapper = Content(CreatureType::Tiger, {-15.0f, 0.0f}, 90.0f, "player's tiger");
	swapper.alignment = -0.9f;
	swapper.size = 1.5f;
	swapper.tattoos.push_back({.design = 3, .site = 0, .colour = {220, 40, 40}});
	swapper.tattoos.push_back({.design = 7, .site = 2, .colour = {40, 90, 230}});
	auto sheep = Content(CreatureType::Sheep, {15.0f, 0.0f}, 270.0f, "sheep");
	sheep.owner = PlayerNames::NEUTRAL;
	sheep.alignment = 0.0f;
	sheep.size = 0.6f;
	all.push_back({
	    .id = "mind.creature_swap",
	    .name = "Swapping creatures",
	    .facet = Facet::Mind,
	    .description = "The player's evil tiger (size 1.5, two tattoos) and a neutral sheep of nobody's (size 0.6) face "
	                   "each other. Three seconds in, a script swaps their minds, as The Lost Flock's sheep lets the "
	                   "player; twenty seconds later it swaps them back.",
	    .expected = "At the swap both play their creation action and the beam starts; the tattoos move to the sheep at once "
	                "while each keeps its name. Over about ten seconds the sheep grows to the tiger's size and turns "
	                "evil, while the tiger shrinks to the sheep's size and turns neutral. The sheep is now the player's "
	                "creature, which the leash and the spawner show. Swapped back, it all goes the other way.",
	    .framing = {.shot = Shot::Overview},
	    .creatures = {swapper, sheep},
	    .commands = {{.kind = Kind::SwapMinds, .creature = 0, .delaySeconds = 3.0f, .value = 1},
	                 {.kind = Kind::SwapMinds, .creature = 1, .delaySeconds = 20.0f, .value = 0}},
	});

	// The player's evil tiger swaps minds with a good gorilla of the second player. Each load paints the skin and the hair
	// with the alignment of the mind that comes.
	auto evilTiger = Content(CreatureType::Tiger, {-15.0f, 0.0f}, 90.0f, "player's evil tiger");
	evilTiger.alignment = -0.9f;
	auto goodGorilla = Content(CreatureType::Gorilla, {15.0f, 0.0f}, 270.0f, "second player's good gorilla");
	goodGorilla.owner = PlayerNames::PLAYER_TWO;
	goodGorilla.alignment = 0.9f;
	all.push_back({
	    .id = "mind.creature_swap_paint",
	    .name = "The skins at a swap of minds",
	    .facet = Facet::Mind,
	    .description = "The player's evil tiger and the good gorilla of the second player face each other. Three seconds in, "
	                   "a script swaps their minds.",
	    .expected = "At the swap the tiger gets the good skin and hair at once, and the gorilla gets the evil skin and hair. "
	                "Each body then turns from its own alignment towards the other's. The skin follows again when the "
	                "alignment has moved by 0.03.",
	    .framing = {.shot = Shot::Overview},
	    .creatures = {evilTiger, goodGorilla},
	    .commands = {{.kind = Kind::SwapMinds, .creature = 0, .delaySeconds = 3.0f, .value = 1}},
	});

	// The developer view of the mind opens on the tiger. A script makes it hungry and playful.
	all.push_back({
	    .id = "mind.debug_view",
	    .name = "The developer view of a creature's mind",
	    .facet = Facet::Mind,
	    .description = "A player's tiger stands in the testbed, its mind free, a birch beside it. The Creature Mind "
	                   "window opens on it. One second in, a script sets its hunger to 0.8 and its desire to play to 0.5.",
	    .expected = "The window shows the active desires of the tiger, the highest first, with hunger marked as the "
	                "winner. Open a desire to see its sources: the value, the threshold and the part of each. The plans "
	                "part shows the plan, the best plan of each desire and the planner steps with the reasons. The "
	                "timeline shows each desire over the last turns. A label over the tiger shows its top desire and its "
	                "plan. The view changes nothing in the world.",
	    .framing = {.shot = Shot::Testbed},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f}, 180.0f)},
	    .objects = {{.type = TreeInfo::Birch, .offset = {-15.0f, 0.0f}}},
	    .commands = {setDesire(Desire::Hunger, 0.8f, 1.0f), setDesire(Desire::Play, 0.5f, 1.0f)},
	    .debugWindow = "Creature Mind",
	});

	// The tree view of the developer view of the mind. The tiger takes up the game's Khazar mind, whose trees have grown
	// from many examples. The hand strokes it and then slaps it again and again, so that the trees of what it does then
	// grow again while the view is open.
	all.push_back({
	    .id = "mind.debug_trees",
	    .name = "The decision trees of a creature's mind",
	    .facet = Facet::Mind,
	    .description = "A player's tiger takes up the game's Khazar creature mind. A villager, a sheep, a birch and a pot "
	                   "of food stand near it. The Creature Mind window opens on it, on its Decision trees tab. One "
	                   "second in, a script sets its hunger to 0.9. Then, every ten seconds, the hand strokes it and lets "
	                   "it go, and ten seconds later slaps it and lets it go.",
	    .expected = "The tab lists the active desires first, with the count of examples of each tree. A desire shows its "
	                "activity tree and its act-on tree as a graph of boxes and lines, which you can drag and zoom, and "
	                "as a text tree. Inner boxes show the attribute that they test, and each line shows its value. Each "
	                "leaf shows its value from -1 to 1, red through grey to green, and its count of examples. The path "
	                "of the thing of the planner's best plan is yellow. Use Under the hand or Follow the hand to see the "
	                "path of another thing in blue. After each stroke or slap, the tree of the desire that the tiger "
	                "served grows again: the desire gets a star, the changed nodes blink orange, the turn shows, and the "
	                "new example shows in the examples list with its turn. The view changes nothing in the world.",
	    .framing = {.shot = Shot::Testbed},
	    .creatures = {CreatureSetup {
	        .species = CreatureType::Tiger, .facingDegrees = 180.0f, .mindFile = "game:KhazarCreature"}},
	    .objects = {{.type = VillagerInfo::CelticFarmerMale, .offset = {-25.0f, 10.0f}},
	                {.type = AnimalInfo::Sheep, .offset = {25.0f, 10.0f}, .living = true},
	                {.type = TreeInfo::Birch, .offset = {-15.0f, -20.0f}},
	                {.type = PotInfo::FoodPot, .offset = {15.0f, -20.0f}}},
	    .commands = {setDesire(Desire::Hunger, 0.9f, 1.0f), Stroke(0, creature_feedback::BodyPart::Head, 10.0f),
	                 Act(Kind::HandLetGo, 0, 1.0f, true), Slap(0, 0.5f, false, true, 10.0f),
	                 Act(Kind::HandLetGo, 0, 1.0f, true)},
	    .repeatFrom = 1,
	    .debugWindow = "Creature Mind/Decision trees",
	});
}
