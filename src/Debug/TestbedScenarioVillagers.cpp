/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Testbed scenarios for the villagers: their size and reach, sway, fright, stores and poison.

#include "Creature/CreatureCave.h"
#include "Creature/CreatureIdleMind.h"
#include "Sound/GameMusic.h"
#include "TestbedScenarioBuilders.h"
#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

void testbed_scenarios::AddVillagerSizeAndReachScenarios(std::vector<Scenario>& all)
{
	// Children of each age from 1 to 12 in a row, then a grown up, all held still by a script
	std::vector<ObjectSetup> ages;
	constexpr uint32_t k_OldestChild = 12;
	for (uint32_t age = 1; age <= k_OldestChild + 1; ++age)
	{
		ages.push_back({.type = VillagerInfo::CelticHousewifeFemale,
		                .offset = {static_cast<float>(age) * 4.0f - 28.0f, 0.0f},
		                .villagerAge = age <= k_OldestChild ? age : 30u,
		                .inScript = true});
	}
	all.push_back({
	    .id = "growth.villager_size_by_age",
	    .name = "Villagers of each age side by side",
	    .facet = Facet::Growth,
	    .description = "Twelve children of the ages 1 to 12 and a grown up stand in a row, from the west, held still by a "
	                   "script.",
	    .expected = "Each child is a little larger than the one to its west. A child starts at the size of the year before "
	                "its age and grows towards the size of the year after it. The grown up is between 0.95 and 1.05 of "
	                "full size.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-30.0f, -6.0f}, {30.0f, 6.0f}}},
	    .objects = ages,
	});

	// Villagers of the player's town walking there and back
	std::vector<ObjectSetup> walkers;
	for (int i = 0; i < 3; ++i)
	{
		const float y = static_cast<float>(i) * 6.0f - 6.0f;
		walkers.push_back({.type = VillagerInfo::CelticHousewifeFemale,
		                   .offset = {-20.0f, y},
		                   .walkTo = glm::vec2(20.0f, y),
		                   .walkAfterSeconds = 1.0f,
		                   .walkRepeatSeconds = 20.0f,
		                   .joinTown = true});
	}
	all.push_back({
	    .id = "movement.villager_speed_town_belief",
	    .name = "The walk speed of villagers in a player's town",
	    .facet = Facet::Movement,
	    .description = "Three villagers of the player's town walk 40 metres east and back every twenty seconds. Their speed "
	                   "comes from the player's most Indian tribal power and from the town's belief in the player against "
	                   "the town's own neutral belief.",
	    .expected = "The villagers walk at the speed of their state, scaled by the town's belief in the player beyond the "
	                "town's own neutral belief. The inspector shows the speed of each walker's WallHug.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-24.0f, -10.0f}, {24.0f, 10.0f}}},
	    .objects = walkers,
	});
	// A villager that sits at the end of each walk stands up before the next walk starts
	all.push_back({
	    .id = "movement.villager_stands_up_then_walks",
	    .name = "A villager stands up before it walks",
	    .facet = Facet::Movement,
	    .description = "A villager walks 20 metres east and sits down there. Every twenty seconds it walks to the other "
	                   "point and sits down again.",
	    .expected = "At the start of each walk, the villager plays its clip to stand up where it sits. It does not move "
	                "until the clip ends. Then it walks. The villager walks in its own turn, and it does not walk while "
	                "a clip into or out of a state plays.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {10.0f, 2.5f, -7.0f}, .look = {9.0f, 1.0f, 0.0f}},
	    .objects = {{.type = VillagerInfo::CelticHousewifeFemale,
	                 .offset = {-10.0f, 0.0f},
	                 .walkTo = glm::vec2(10.0f, 0.0f),
	                 .walkAfterSeconds = 1.0f,
	                 .walkRepeatSeconds = 20.0f,
	                 .walkFinal = VillagerStates::SitAndChillout}},
	});
	// Dancers that walk to their places in a dance, each step at its point in the dance's state
	all.push_back({
	    .id = "movement.dancers_step_in_their_dance",
	    .name = "Dancers take their steps in the dance",
	    .facet = Facet::Movement,
	    .description = "A leader stands in the middle with four villagers round him, 8 metres away. After one second, a "
	                   "script makes the dance about a person about the leader, and the four villagers join it.",
	    .expected = "Each dancer does the move of the dance first. In the same turn, it starts its walk to its place and "
	                "takes the first step. When the group moves, each dancer takes its step to the new place in the "
	                "turn that the group moves. A dancer on its place takes no step.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 14.0f, -18.0f}, .look = {0.0f, 0.0f, 0.0f}},
	    .objects = {{.type = VillagerInfo::CelticLeaderMale, .offset = {0.0f, 0.0f}},
	                {.type = VillagerInfo::CelticHousewifeFemale, .offset = {8.0f, 0.0f}},
	                {.type = VillagerInfo::CelticFarmerMale, .offset = {-8.0f, 0.0f}},
	                {.type = VillagerInfo::CelticHousewifeFemale, .offset = {0.0f, 8.0f}},
	                {.type = VillagerInfo::CelticForesterMale, .offset = {0.0f, -8.0f}}},
	    .commands = {{.kind = Kind::MakeDance,
	                  .delaySeconds = 1.0f,
	                  .value = static_cast<size_t>(DanceInfo::DanceAroundPerson),
	                  .object = 0}},
	});
	// A dancer that must go round a hut to get to its place, while the dance turns its place away
	all.push_back({
	    .id = "movement.dancer_leaves_circle_when_goal_moves",
	    .name = "A dancer round a hut leaves it when the dance moves its place",
	    .facet = Facet::Movement,
	    .description = "A leader stands in the middle. A Celtic hut stands 9 metres west of him, and a villager stands "
	                   "behind the hut, 16 metres west of the leader. After one second, a script makes the dance about "
	                   "a person about the leader, and the villager joins it.",
	    .expected = "The villager walks round the hut to its place. Each time the dance moves its place, it notes a new "
	                "distance at which it can leave the hut. It leaves the hut when it is nearer its place than at "
	                "the last move, and it gets to the dance. It does not go round the hut for ever.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-22.0f, -12.0f}, {12.0f, 12.0f}}},
	    .objects = {{.type = VillagerInfo::CelticLeaderMale, .offset = {0.0f, 0.0f}},
	                {.type = AbodeInfo::CelticHut, .offset = {-9.0f, 0.0f}},
	                {.type = VillagerInfo::CelticFarmerMale, .offset = {-16.0f, 0.0f}}},
	    .commands = {{.kind = Kind::MakeDance,
	                  .delaySeconds = 1.0f,
	                  .value = static_cast<size_t>(DanceInfo::DanceAroundPerson),
	                  .object = 0}},
	});
	// Lethys's worship site on the second land dances among the spokes of his temple
	all.push_back({
	    .id = "movement.lethys_dancers_leave_temple_spokes",
	    .name = "Lethys's worshippers do not go round a spoke of his temple for ever",
	    .facet = Facet::Movement,
	    .description = "The second land of the story, as the story plays it, with the camera on Lethys's worship site. "
	                   "The dance of the site stands next to the spokes of his temple, and the dance moves the places of "
	                   "its dancers. Best watched at a high game speed, or run with --sim-fast to turn 60000.",
	    .expected = "A worshipper that goes round a temple spoke to get to its place notes a new distance to leave the "
	                "spoke each time the dance moves its place. It leaves the spoke and dances again. No worshipper "
	                "of the site goes round a spoke for ever, and none dies of hunger because of it.",
	    .environment = {.land = Land::Story, .storyLand = "Land2.txt", .dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-1609.0f, 1086.0f}, {-1549.0f, 1146.0f}}},
	});
	// Two creatures, each with its own turn in the list of the living
	all.push_back({
	    .id = "mind.two_creatures_own_turns",
	    .name = "Two creatures each take their turn at their own place",
	    .facet = Facet::Mind,
	    .description = "A tiger and a cow stand 30 metres apart with nothing to do, and a villager walks between them. "
	                   "With the game's log at the debug level, each creature writes the synced random numbers of its "
	                   "turn.",
	    .expected = "Each creature thinks, walks and acts as it does alone. In the log, the numbers that each creature "
	                "draws come together in its own turn, the newer creature first. No numbers of the other creature come "
	                "between them.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview},
	    .creatures = {Content(CreatureType::Tiger, {-15.0f, 0.0f}, 90.0f, "older"),
	                  Content(CreatureType::Cow, {15.0f, 0.0f}, 270.0f, "newer")},
	    .objects = {{.type = VillagerInfo::CelticHousewifeFemale,
	                 .offset = {0.0f, -10.0f},
	                 .walkTo = glm::vec2(0.0f, 10.0f),
	                 .walkAfterSeconds = 1.0f,
	                 .walkRepeatSeconds = 20.0f}},
	});

	// The tutorial's interface levels, as a lesson's script steps through them
	std::vector<Command> levels;
	for (const size_t level : {1, 2, 1, 5, 1, 11, 1, 15, 8})
	{
		levels.push_back({.kind = Kind::SetInterfaceLevel, .delaySeconds = 5.0f, .value = level});
	}
	all.push_back({
	    .id = "tutorial.interface_level_reach",
	    .name = "The hand's reach at each interface level",
	    .facet = Facet::Tutorial,
	    .description = "The interface level changes every five seconds, as a lesson's script changes it: level 1, then 2, 5, "
	                   "11 and 15, each after level 1 again, and at the end level 8. The scenario log gives the hand's reach "
	                   "at each level.",
	    .expected = "Level 1 gives the hand a short reach of 75 metres. Levels 2, 5, 11 and 15 give the full reach of "
	                "1800 metres. Level 8 keeps the reach that level 15 gave.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Testbed},
	    .commands = levels,
	});
	// A lesson's interface level limits the Action button, and the normal level removes the limits
	all.push_back({
	    .id = "tutorial.interface_level_hand_limits",
	    .name = "The limits of the Action button at a lesson's interface level",
	    .facet = Facet::Tutorial,
	    .description = "The interface level becomes 1, the level of the opening's lesson. After 40 seconds it becomes "
	                   "the normal level 0. A villager and a scroll stand in front of the camera.",
	    .expected = "At level 1 the Action button does not pick up the villager, and a press on the land does nothing. "
	                "A click on the scroll still reaches the scripts. At level 0 the hand picks up the villager again.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 12.0f, -20.0f}, .look = {0.0f, 2.0f, 6.0f}},
	    .objects = {{.type = VillagerInfo::CelticFarmerMale, .offset = {-4.0f, 6.0f}}},
	    .highlights = {{.kind = 1, .offset = {4.0f, 6.0f}, .challenge = 0}},
	    .commands = {{.kind = Kind::SetInterfaceLevel, .delaySeconds = 0.5f, .value = 1},
	                 {.kind = Kind::SetInterfaceLevel, .delaySeconds = 40.0f, .value = 0}},
	});
	all.push_back({
	    .id = "tutorial.bookmark_flight",
	    .name = "A long flight to a bookmark",
	    .facet = Facet::Tutorial,
	    .description = "A bookmark is put on the land 900 metres from the middle of the map, and the camera flies to it, as "
	                   "the number key 1 does. Five seconds later a second bookmark is put back at the middle, and the "
	                   "camera flies back to it.",
	    .expected = "Each flight goes by a high point half way. The high point is 0.3 metres higher for each metre that the "
	                "flight goes across, so these flights go up by about 270 metres. The camera makes a woosh at the start "
	                "of each flight.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Testbed},
	    .commands = {{.kind = Kind::FlyToBookmark, .delaySeconds = 2.0f, .point = {900.0f, 0.0f}, .value = 0},
	                 {.kind = Kind::FlyToBookmark, .delaySeconds = 5.0f, .point = {0.0f, 0.0f}, .value = 1}},
	});
	all.push_back({
	    .id = "tutorial.tab_town_flights",
	    .name = "The Tab key flies to the player's towns",
	    .facet = Facet::Tutorial,
	    .description = "Lines of a land script make three towns of the player in this order: a town with a storage pit to "
	                   "the west, a town with a village centre to the north, and a town with no building to the east. A "
	                   "neutral town with a storage pit stands to the south. The player presses the Tab key four times, "
	                   "eight seconds apart.",
	    .expected = "The first press flies the camera to the village centre to the north. The second press flies it to the "
	                "storage pit to the west. The town to the east has no place to fly to, so the third press flies to "
	                "the north again, and the fourth to the west. The camera never goes to the neutral town. Each flight "
	                "ends 24 metres west, 28 metres up and 47 metres north of the place, looking at it.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview},
	    .landScript = R"(CREATE_TOWN(60, "-70.0,0.0", "PLAYER_ONE", 0, "CELTIC")
SET_TOWN_BELIEF(60, "PLAYER_ONE", 10.0)
CREATE_ABODE(60, "-70.0,10.0", "CELTIC_ABODE_STORAGE_PIT", 0, 1000, 100, 100)
CREATE_TOWN(61, "0.0,80.0", "PLAYER_ONE", 0, "NORSE")
SET_TOWN_BELIEF(61, "PLAYER_ONE", 10.0)
CREATE_TOWN_CENTRE(61, "0.0,90.0", "NORSE_ABODE_TOWN_CENTRE", 0, 990, 0)
CREATE_TOWN(62, "70.0,0.0", "PLAYER_ONE", 0, "CELTIC")
SET_TOWN_BELIEF(62, "PLAYER_ONE", 10.0)
CREATE_TOWN(63, "0.0,-80.0", "NEUTRAL", 0, "CELTIC")
CREATE_ABODE(63, "0.0,-70.0", "CELTIC_ABODE_STORAGE_PIT", 0, 1000, 100, 100)
)",
	    .commands = {{.kind = Kind::TabToTown, .delaySeconds = 3.0f},
	                 {.kind = Kind::TabToTown, .delaySeconds = 8.0f},
	                 {.kind = Kind::TabToTown, .delaySeconds = 8.0f},
	                 {.kind = Kind::TabToTown, .delaySeconds = 8.0f}},
	});
	all.push_back({
	    .id = "tutorial.bookmark_markers_follow",
	    .name = "Bookmark markers and a followed creature",
	    .facet = Facet::Tutorial,
	    .description = "Three bookmarks go on the land near the middle of the map, as Ctrl and the keys 1, 2 and 3 do. Then "
	                   "the player keeps the tiger in bookmark 4, as Ctrl and the key 4 do with the hand over it, and the "
	                   "camera follows it, as Shift and the key 4 do. The tiger walks away to the north.",
	    .expected = "Each bookmark shows its yellow number in a ring, two metres above its place. The marker of bookmark 4 "
	                "stays two metres above the tiger and moves with it. All the markers throb together: the width and "
	                "the height each swing once a second, the height a fifth of a second behind. The camera flies after "
	                "the tiger each turn, until the player moves the camera.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview},
	    .creatures = {Posed(CreatureType::Tiger, {0.0f, -10.0f}, 0.0f)},
	    .commands = {{.kind = Kind::FlyToBookmark, .delaySeconds = 0.5f, .point = {25.0f, 0.0f}, .value = 0},
	                 {.kind = Kind::FlyToBookmark, .delaySeconds = 0.5f, .point = {-25.0f, 0.0f}, .value = 1},
	                 {.kind = Kind::FlyToBookmark, .delaySeconds = 0.5f, .point = {0.0f, 25.0f}, .value = 2},
	                 {.kind = Kind::BookmarkCreature, .creature = 0, .delaySeconds = 1.0f, .value = 3},
	                 Go(Kind::WalkTo, 0, {0.0f, 160.0f}, 0.5f, false)},
	});
	{
		// The first land's turning lesson, as the game's own script runs it after the turning demonstration. The lesson
		// keeps the camera near the temple, and the temple's place is where the camera goes back to.
		using Argument = ScriptArgument;
		constexpr glm::vec2 k_LessonTemple {0.0f, 40.0f};
		constexpr glm::vec2 k_ViewPlace {0.0f, 0.0f};
		all.push_back({
		    .id = "tutorial.teach_rotate",
		    .name = "The turning lesson and the hand's turning state",
		    .facet = Facet::Tutorial,
		    .description = "The player's temple stands north of the middle. Half a second in, the game's TeachRotate "
		                   "script runs, as the first land's lesson starts it. Hold the land with both buttons and move "
		                   "the mouse across to turn the camera, or turn it with the keys while both buttons hold the land.",
		    .expected = "The advisors tell the player to turn the camera. While both buttons hold the land and the camera "
		                "turns, the hand's state that the scripts read is the turning state (21). When the lesson's minute "
		                "runs out while the player turns the land in this way, the turning demonstration does not play "
		                "again. With no turn, it plays again after each minute. Turning enough ends the lesson.",
		    .environment = {.dispenserGrid = false},
		    .framing = {.shot = Shot::Placed,
		                .eye = {k_ViewPlace.x, 30.0f, k_ViewPlace.y - 40.0f},
		                .look = {k_LessonTemple.x, 0.0f, k_LessonTemple.y}},
		    .temples = {{.offset = k_LessonTemple}},
		    .commands = {{.kind = Kind::RunScript,
		                  .delaySeconds = 0.5f,
		                  .script = "TeachRotate",
		                  .scriptArguments = {{.kind = Argument::Kind::Temple, .index = 0},
		                                      {.kind = Argument::Kind::Marker, .point = k_ViewPlace, .height = 30.0f}}}},
		});
	}
}

/// A blow that sways the creature, and a wolf that frightens it
void testbed_scenarios::AddSwayAndFrightScenarios(std::vector<Scenario>& all)
{
	all.push_back({
	    .id = "objects.sway_from_a_thrown_rock",
	    .name = "A thrown rock sways the creature",
	    .facet = Facet::Objects,
	    .description = "A tiger stands still, and a rock flies at its legs from the west every five seconds.",
	    .expected = "Each rock that hits the tiger sways its body. The body swings back on its spring once a game "
	                "turn and settles. A body that turns on the spot or sits does not sway.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-25.0f, 0.0f}, {10.0f, 0.0f}}},
	    .creatures = {Posed(CreatureType::Tiger, {0.0f, 0.0f}, 90.0f)},
	    .objects = {{.type = MobileStaticInfo::Rock, .offset = {-25.0f, 0.0f}}},
	    .throws =
	        {{.object = 0, .height = 14.0f, .velocity = {14.0f, 4.0f, 0.0f}, .delaySeconds = 2.0f, .repeatSeconds = 5.0f}},
	});

	// Rocks that a script makes go into the physics
	const auto create = [](float delay, int32_t type, glm::vec2 point, float height) {
		return Command {
		    .kind = Kind::CallNative,
		    .delaySeconds = delay,
		    .script = "CREATE",
		    .scriptArguments = {{.kind = ScriptArgument::Kind::Integer, .number = static_cast<float>(type)},
		                        {.kind = ScriptArgument::Kind::Integer, .number = static_cast<float>(MobileStaticInfo::Rock)},
		                        {.kind = ScriptArgument::Kind::Position, .point = point, .height = height}}};
	};
	all.push_back({
	    .id = "objects.script_rock_falls",
	    .name = "A rock that a script makes falls onto the land",
	    .facet = Facet::Objects,
	    .description = "After one second, CREATE makes a rock as a mobile static 10 metres above the land, 10 metres "
	                   "west of the middle. Two seconds later, CREATE makes a rock by the rock type at the same height, "
	                   "10 metres east of the middle.",
	    .expected = "The first rock falls from the height that the script gave and comes to rest on the land. The "
	                "second rock shows on the land at once, turned to the slope, and stays there.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-15.0f, 0.0f}, {15.0f, 0.0f}}},
	    .commands = {create(1.0f, 8, {-10.0f, 0.0f}, 10.0f), create(2.0f, 33, {10.0f, 0.0f}, 10.0f)},
	});

	all.push_back({
	    .id = "senses.creature_runs_from_a_wolf",
	    .name = "A wolf frightens the creature",
	    .facet = Facet::Senses,
	    .description = "A tiger with its fear as strong as it gets, and a wolf fifteen metres east of it.",
	    .expected = "The tiger runs away from the wolf. Tigers, leopards, wolves and the wolves of a spell frighten a "
	                "creature as lions do.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-30.0f, 0.0f}, {20.0f, 0.0f}}},
	    .creatures = {CreatureSetup {
	        .species = CreatureType::Tiger,
	        .offset = {0.0f, 0.0f},
	        .needs = Content(),
	        .desires = {{.desire = Desire::Fear, .fraction = 1.0f}},
	    }},
	    .objects = {{.type = AnimalInfo::Wolf, .offset = {15.0f, 0.0f}, .living = true}},
	});
}

void testbed_scenarios::AddStoreScenarios(std::vector<Scenario>& all)
{
	std::vector<ObjectSetup> objects {
	    {.type = AbodeInfo::CelticTownCentre, .offset = {0.0f, 50.0f}},
	    {.type = AbodeInfo::CelticStoragePit, .offset = {-30.0f, 30.0f}, .life = 0.1f, .storedFood = 2000, .storedWood = 1000},
	};
	for (size_t i = 0; i < 3; ++i)
	{
		const float x = (static_cast<float>(i) - 1.5f) * 3.0f;
		objects.push_back({.type = VillagerInfo::CelticForesterMale, .offset = {x, 20.0f}, .joinTown = true});
		objects.push_back({.type = VillagerInfo::CelticHousewifeFemale, .offset = {x, 17.0f}, .joinTown = true});
	}
	all.push_back({
	    .id = "objects.store_stops_working",
	    .name = "A broken storage pit calls people to its piles",
	    .facet = Facet::Objects,
	    .description = "A Celtic town of the player's with a village centre and a storage pit with food and wood. The "
	                   "storage pit has damage down to a tenth of its life. Six villagers of the town stand near it.",
	    .expected = "The storage pit stops working. Its food pile and each wood pile with wood in it call the people "
	                "near them, as a pile on its own does (the inspector shows the reactions of the piles). A repair "
	                "of the pit above its limit stops the calls. A pile that becomes empty stops its call and keeps its "
	                "place.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-30.0f, 30.0f}, {0.0f, 50.0f}, {0.0f, 17.0f}}},
	    .objects = std::move(objects),
	});

	// A toadstool, as Land 1's script puts nine on the land, let go above a storage pit
	constexpr glm::vec2 k_Pit {-30.0f, 30.0f};
	all.push_back({
	    .id = "objects.toadstool_poisons_store",
	    .name = "A toadstool poisons the food of a storage pit",
	    .facet = Facet::Objects,
	    .description = "A Celtic town of the player's with a village centre and a storage pit with food and wood, and "
	                   "a toadstool beside them. After one second the toadstool is lifted and let go above the "
	                   "storage pit, as the hand drops one there.",
	    .expected = "The storage pit takes the toadstool as 150 food. Its food pile becomes poisoned and shows a pale "
	                "green tint. Its wood piles keep their usual colour. Villagers who take food or wood from the "
	                "storage pit become poisoned.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_Pit - glm::vec2 {12.0f}, k_Pit + glm::vec2 {12.0f}}},
	    .objects = {{.type = AbodeInfo::CelticTownCentre, .offset = {0.0f, 50.0f}},
	                {.type = AbodeInfo::CelticStoragePit, .offset = k_Pit, .storedFood = 2000, .storedWood = 1000},
	                {.type = MobileObjectInfo::Toadstool, .offset = k_Pit + glm::vec2 {15.0f, -15.0f}}},
	    .throws = {{.object = 2, .from = k_Pit, .height = 6.0f, .velocity = {0.0f, -1.0f, 0.0f}, .delaySeconds = 1.0f}},
	});
}

void testbed_scenarios::AddPoisonScenarios(std::vector<Scenario>& all)
{
	// What the Plague's script does first: it enables the poison of the town and then of its storage pit, and later cures
	// the town. The scenario runs those calls.
	constexpr glm::vec2 k_Pit {-30.0f, 30.0f};
	using Argument = ScriptArgument;
	const auto poison = [](float delay, Argument thing, int flag) {
		return Command {.kind = Kind::CallNative,
		                .delaySeconds = delay,
		                .script = "SET_POISONED",
		                .scriptArguments = {{.kind = Argument::Kind::Integer, .number = static_cast<float>(flag)}, thing}};
	};
	all.push_back({
	    .id = "objects.script_poisons_town_and_store",
	    .name = "A script poisons a town and its storage pit",
	    .facet = Facet::Objects,
	    .description = "A Celtic town of the player's with a village centre, a storage pit with 2000 food and 1000 wood, "
	                   "and four villagers of the town. After one second a script poisons the town. After three seconds it "
	                   "poisons the storage pit. After ten seconds it cures the town.",
	    .expected = "At one second each of the four villagers is poisoned, as the inspector shows. At three seconds the "
	                "food pile of the storage pit is poisoned and shows a pale green tint. Its wood piles keep their usual "
	                "colour. At ten seconds the villagers are cured, but the food pile stays poisoned. Villagers who take "
	                "food from the pit become poisoned again.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_Pit - glm::vec2 {12.0f}, k_Pit + glm::vec2 {12.0f}}},
	    .objects = {{.type = AbodeInfo::CelticTownCentre, .offset = {0.0f, 50.0f}},
	                {.type = AbodeInfo::CelticStoragePit, .offset = k_Pit, .storedFood = 2000, .storedWood = 1000},
	                {.type = VillagerInfo::CelticFarmerMale, .offset = k_Pit + glm::vec2 {-6.0f, -8.0f}, .joinTown = true},
	                {.type = VillagerInfo::CelticFarmerMale, .offset = k_Pit + glm::vec2 {-3.0f, -9.0f}, .joinTown = true},
	                {.type = VillagerInfo::CelticHousewifeFemale, .offset = k_Pit + glm::vec2 {3.0f, -9.0f}, .joinTown = true},
	                {.type = VillagerInfo::CelticHousewifeFemale, .offset = k_Pit + glm::vec2 {6.0f, -8.0f}, .joinTown = true}},
	    .commands = {poison(1.0f, {.kind = Argument::Kind::Town}, 1),
	                 poison(2.0f, {.kind = Argument::Kind::Object, .index = 1}, 1),
	                 poison(7.0f, {.kind = Argument::Kind::Town}, 0)},
	});

	// The end of the Plague's script cures the storage pit and then the town. A script can cure the storage pit
	// (deviation 179).
	all.push_back({
	    .id = "objects.script_cures_store",
	    .name = "A script cures a storage pit",
	    .facet = Facet::Objects,
	    .description = "A Celtic town of the player's with a village centre and a storage pit with 2000 food and 1000 wood. "
	                   "After one second a script poisons the storage pit. After six seconds it cures the storage pit.",
	    .expected = "At one second the food pile of the storage pit is poisoned and shows a pale green tint. At six seconds "
	                "the food pile is cured, and the tint goes. The food stays in the pile.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_Pit - glm::vec2 {12.0f}, k_Pit + glm::vec2 {12.0f}}},
	    .objects = {{.type = AbodeInfo::CelticTownCentre, .offset = {0.0f, 50.0f}},
	                {.type = AbodeInfo::CelticStoragePit, .offset = k_Pit, .storedFood = 2000, .storedWood = 1000}},
	    .commands = {poison(1.0f, {.kind = Argument::Kind::Object, .index = 1}, 1),
	                 poison(5.0f, {.kind = Argument::Kind::Object, .index = 1}, 0)},
	});
}
