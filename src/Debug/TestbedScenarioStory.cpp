/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Testbed scenarios for the story of the lands: the quests, the puzzles and the boat of the explorers.

#include "3D/FlatLand.h"
#include "Creature/CreatureCave.h"
#include "Creature/CreatureFight.h"
#include "ECS/IncomingMessageRules.h"
#include "ScriptHeaders/ScriptEnums.h"
#include "Sound/GameMusic.h"
#include "TestbedScenarioBuilders.h"
#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

namespace
{
/// The first land's Creature Breeder, run by the game's own script at the same places of the map as on the first land
/// (the testbed spans the whole map), each as high above the land as there; the kennels are a marker, as on the second
/// and fifth lands. The player's Cow stands by; the scenario clicks the kennels' scroll, and the breeder offers the
/// bonus creatures the debug option takes as unlocked
void AddBreederOffers(std::vector<Scenario>& all)
{
	using Argument = ScriptArgument;
	// The first land's breeder places, from the middle of the map, and how far above the land each is there
	constexpr glm::vec2 k_BreederStart {-661.839f, 850.803f};
	constexpr glm::vec2 k_SoapBox {-663.838f, 849.415f};
	constexpr glm::vec2 k_Kennels {-660.73f, 851.21f};
	constexpr glm::vec2 k_CreaturePlace {-667.115f, 843.713f};
	const auto marker = [](glm::vec2 point, float height = 0.0f) {
		return Argument {.kind = Argument::Kind::Marker, .point = point, .height = height};
	};
	const Command runBreeder {
	    .kind = Kind::RunScript,
	    .delaySeconds = 0.5f,
	    .script = "CreatureBreeder",
	    // Land, the breeder's start, the soap box, the kennels, where the creatures stand, then two camera shots
	    .scriptArguments = {{.kind = Argument::Kind::Number, .number = 1.0f},
	                        marker(k_BreederStart),
	                        marker(k_SoapBox),
	                        marker(k_Kennels),
	                        marker(k_CreaturePlace),
	                        marker({-671.651f, 844.151f}, 4.151f),
	                        marker({-653.575f, 856.752f}, -1.247f),
	                        marker({-696.66f, 839.907f}, 12.69f),
	                        marker({-674.738f, 851.945f}, 0.719f)},
	    // As the first land sets them up: the player's creature, and no swap at the breeder yet
	    .scriptGlobals = {{"MyCreature", {.kind = Argument::Kind::Creature, .index = 0}},
	                      {"GlobalCurrentCreature", {.number = -1.0f}},
	                      {"GlobalOldCreature", {.number = -1.0f}},
	                      {"BreederRunning", {.number = 0.0f}},
	                      {"NrOfChoosenCreature", {.number = 0.0f}},
	                      {"GlobalReset", {.number = 0.0f}}},
	};
	// The kennels' scroll goes up ten seconds in; the pointer at the middle of the screen, which looks at it, clicks it
	// with the Action button
	const std::vector<Command> commands {
	    runBreeder,
	    {.kind = Kind::PointerTo, .delaySeconds = 11.0f, .point = {0.5f, 0.5f}},
	    {.kind = Kind::PointerPress, .delaySeconds = 1.0f, .value = 3},
	    {.kind = Kind::PointerRelease, .delaySeconds = 0.1f, .value = 3},
	};
	const auto offer = [&](std::string_view id, std::string_view name, std::string_view description, std::string_view expected,
	                       creature_swap::Unlocked unlocked) {
		all.push_back({
		    .id = id,
		    .name = name,
		    .facet = Facet::Story,
		    .description = description,
		    .expected = expected,
		    .environment = {.dispenserGrid = false, .unlockedBonusCreatures = unlocked},
		    .framing = {.shot = Shot::Placed,
		                .eye = {k_Kennels.x - 20.0f, 10.0f, k_Kennels.y - 22.0f},
		                .look = {k_Kennels.x, 6.0f, k_Kennels.y}},
		    .creatures = {Content(CreatureType::Cow, k_CreaturePlace + glm::vec2 {10.0f, -12.0f}, 0.0f, "player's cow")},
		    .commands = commands,
		});
	};
	creature_swap::Unlocked leopard;
	leopard.set(static_cast<size_t>(creature_swap::Bonus::Leopard));
	auto leopardAndHorse = leopard;
	leopardAndHorse.set(static_cast<size_t>(creature_swap::Bonus::Horse));
	offer("creature.breeder_offer", "The Creature Breeder offers all five bonus creatures",
	      "All five bonus creatures taken as unlocked (the debug option). The first land's breeder script runs at its "
	      "places; the player's Cow stands by. Twelve seconds in the kennels' scroll is clicked.",
	      "The breeder walks to the soap box and the camera to him; sparkles and the Leopard, Horse, Mandrill, Gorilla and "
	      "Rhino appear one beside the next; between the first and the last the breeder says \"Look. I have these "
	      "wondrous Creatures you can choose from.\" A scroll stands over each. Click one with the Cow near to choose it; "
	      "click it again to swap. The offer ends after a swap or after 60 to 120 seconds, the others vanishing.",
	      creature_swap::Unlocked {}.set());
	offer("creature.breeder_offer_one", "The Creature Breeder offers one bonus creature",
	      "Only the Leopard taken as unlocked; otherwise as creature.breeder_offer.",
	      "The Leopard alone appears, with its scroll; the camera takes in it and the breeder, who says the line for one "
	      "creature.",
	      leopard);
	offer("creature.breeder_offer_two", "The Creature Breeder offers two bonus creatures",
	      "The Leopard and the Horse taken as unlocked; otherwise as creature.breeder_offer.",
	      "The Leopard and the Horse appear side by side, each with a scroll, and the breeder says \"Look. I have these "
	      "wondrous Creatures you can choose from.\"",
	      leopardAndHorse);

	// The swap a quest offers at its end, as The Lost Flock offers its sheep: the game's own SwapCreatures script over a
	// sheep of nobody's, the player's Cow nearby. The pointer looks at the sheep and clicks it twice with the Action
	// button: once to choose it, and once more, after the Cow has come over, to swap
	auto sheep = Content(CreatureType::Sheep, {15.0f, 0.0f}, 270.0f, "the flock's sheep");
	sheep.owner = PlayerNames::NEUTRAL;
	all.push_back({
	    .id = "creature.field_swap",
	    .name = "A quest's offer of its creature, as The Lost Flock's sheep",
	    .facet = Facet::Story,
	    .description = "The game's SwapCreatures script runs over a sheep of nobody's, the player's Cow 25 away. Three "
	                   "seconds in the sheep is clicked; twelve seconds later it is clicked again.",
	    .expected = "A scroll stands over the sheep, which looks at the camera. At the first click the scroll goes, the "
	                "Cow walks over and the two look each other over, the good advisor saying the reminder. At the second "
	                "click a cinema shows both: sparkles, both point at each other, the swap with its beams and the swap "
	                "sound, then the evil advisor says the sheep is now the player's. The offer starts again over the "
	                "Cow, so they can swap back.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {5.0f, 22.0f, -50.0f}, .look = {15.0f, 8.0f, 0.0f}},
	    .creatures = {Content(CreatureType::Cow, {-10.0f, 0.0f}, 90.0f, "player's cow"), sheep},
	    .commands = {{.kind = Kind::RunScript,
	                  .delaySeconds = 0.5f,
	                  .script = "SwapCreatures",
	                  .scriptArguments = {{.kind = Argument::Kind::Creature, .index = 1}},
	                  .scriptGlobals = {{"MyCreature", {.kind = Argument::Kind::Creature, .index = 0}},
	                                    {"GlobalReset", {.number = 0.0f}}}},
	                 {.kind = Kind::PointerTo, .delaySeconds = 2.0f, .point = {0.5f, 0.5f}},
	                 {.kind = Kind::PointerPress, .delaySeconds = 1.0f, .value = 3},
	                 {.kind = Kind::PointerRelease, .delaySeconds = 0.1f, .value = 3},
	                 {.kind = Kind::PointerPress, .delaySeconds = 12.0f, .value = 3},
	                 {.kind = Kind::PointerRelease, .delaySeconds = 0.1f, .value = 3}},
	});

	// The first land's breeder on the first land itself, played to the swap: the land's own launcher of the breeder,
	// the kennels' scroll, the Leopard's scroll with the Tiger near, then the Leopard itself. The places are the
	// script's, from the middle of the first land's extent.
	constexpr glm::vec2 k_LandOneMiddle {2240.0f, 2640.0f};
	const auto onLandOne = [k_LandOneMiddle](float x, float z) { return glm::vec2(x, z) - k_LandOneMiddle; };
	const auto landOneKennels = onLandOne(1899.27f, 3411.21f);
	const auto landOneShowPlace = onLandOne(1892.885f, 3403.713f);
	all.push_back({
	    .id = "story.lone_breeder_swap",
	    .name = "Story: a swap at the creature breeder of the first land",
	    .facet = Facet::Story,
	    .description = "The first land loads with its own scripts. The Leopard and the Horse are taken as unlocked. The "
	                   "land's own launcher of the breeder runs. Eleven seconds in, the kennels' scroll is clicked. Ten "
	                   "seconds later the Tiger walks to a point 20 metres south of the Leopard. Eight seconds later the "
	                   "Leopard's scroll is clicked, and eight seconds after that the Leopard itself.",
	    .expected = "At the click on the Leopard's scroll the Tiger walks up to the Leopard and the good advisor asks to "
	                "confirm. At the click on the Leopard a cinema starts. Both creatures walk up the hill to two spots "
	                "about 100 metres north of the kennels and about 20 metres higher. When the Leopard is within 10 "
	                "metres of its spot, or after 20 seconds, they face each other, sparkles show and they change bodies "
	                "with the swap sound, which takes about ten seconds. The Tiger then walks back to where the Leopard "
	                "stood, and the Leopard stays as the player's creature, on the ground.",
	    .environment =
	        {.land = Land::Story, .storyLand = "Land1.txt", .dispenserGrid = false, .unlockedBonusCreatures = leopardAndHorse},
	    .framing = {.shot = Shot::Placed,
	                .eye = {landOneKennels.x - 20.0f, 30.0f, landOneKennels.y - 22.0f},
	                .look = {landOneKennels.x, 10.0f, landOneKennels.y}},
	    .creatures = {{.label = "player's tiger",
	                   .species = CreatureType::Tiger,
	                   .offset = landOneShowPlace + glm::vec2 {10.0f, -12.0f},
	                   .phase = 3,
	                   .needs = {.energy = 1.0f, .exhaustion = 0.0f, .dehydration = 0.0f, .poo = 0.0f, .life = 1.0f}}},
	    .commands =
	        {{.kind = Kind::RunScript,
	          .delaySeconds = 0.5f,
	          .script = "RunCreatureBreederLand",
	          .scriptArguments = {{.kind = Argument::Kind::Number, .number = 1.0f}},
	          .scriptGlobals = runBreeder.scriptGlobals},
	         {.kind = Kind::ClickScroll, .delaySeconds = 11.0f, .point = landOneKennels},
	         {.kind = Kind::WalkTo, .creature = 0, .delaySeconds = 10.0f, .point = landOneShowPlace + glm::vec2 {0.0f, -20.0f}},
	         {.kind = Kind::ClickScroll, .delaySeconds = 8.0f, .point = landOneShowPlace},
	         {.kind = Kind::ClickCreatureAt, .delaySeconds = 8.0f, .point = landOneShowPlace}},
	});

	// The end of a cinema, as the films of the creature trainer end: the script sends the camera up and away, and gives
	// the camera back before it arrives
	const auto move = [](std::string_view native, glm::vec2 point, float height, float seconds, float delay) {
		return Command {
		    .kind = Kind::CallNative,
		    .delaySeconds = delay,
		    .script = native,
		    .scriptArguments = {{.kind = Argument::Kind::Position, .point = point, .height = height}, {.number = seconds}}};
	};
	all.push_back({
	    .id = "story.cinema_end_glide",
	    .name = "The camera at the end of a cinema",
	    .facet = Facet::Story,
	    .description = "A script takes the camera. It sends the camera up over 2 seconds, looking north. After 1.2 "
	                   "seconds it sends the camera north over 3 seconds and gives the camera back at once.",
	    .expected = "The camera rises and turns to look north. It does not stop when the script gives it back. It "
	                "continues smoothly to the last place that the script chose and stops there about 1.5 seconds "
	                "later.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 8.0f, -40.0f}, .look = {0.0f, 4.0f, 0.0f}},
	    .commands = {{.kind = Kind::CallNative, .delaySeconds = 1.0f, .script = "START_CAMERA_CONTROL"},
	                 move("MOVE_CAMERA_POSITION", {0.0f, -40.0f}, 45.0f, 2.0f, 0.5f),
	                 move("MOVE_CAMERA_FOCUS", {0.0f, 60.0f}, 0.0f, 2.0f, 0.0f),
	                 move("MOVE_CAMERA_POSITION", {5.0f, -20.0f}, 45.0f, 3.0f, 1.2f),
	                 {.kind = Kind::CallNative, .delaySeconds = 0.0f, .script = "END_CAMERA_CONTROL"}},
	});
}

/// The computer gods' creatures of the second, third and fifth lands: the game's own scripts load each from its mind
/// file for player two, as the lands do. The player's Cow stands near, for the scripts that wait for it.
void AddRivalCreatureLoads(std::vector<Scenario>& all)
{
	using Argument = ScriptArgument;
	static constexpr glm::vec2 k_Place {20.0f, 0.0f};
	const auto load = [](std::string_view script, float scale) {
		return Command {.kind = Kind::RunScript,
		                .delaySeconds = 0.5f,
		                .script = script,
		                // The player, the place and the scale of the creature
		                .scriptArguments = {{.kind = Argument::Kind::Number, .number = 2.0f},
		                                    {.kind = Argument::Kind::Marker, .point = k_Place},
		                                    {.kind = Argument::Kind::Number, .number = scale}}};
	};
	const auto cow = Content(CreatureType::Cow, {-15.0f, 0.0f}, 90.0f, "player's cow");
	all.push_back({
	    .id = "creature.load_lethys",
	    .name = "The third land loads the creature of Lethys from his mind file",
	    .facet = Facet::Story,
	    .description = "The game's SetupLethysCreature script runs for player two with the scale 1.5, as the third land "
	                   "runs it.",
	    .expected = "A Wolf of player two appears on the ground at the place, fully grown. It takes the body, the "
	                "alignment and the tattoos of the LethysCreature mind file. It knows every leash, and it is the "
	                "creature of player two. The script sets its strength, warmth, fatness, energy, itchiness, poo, "
	                "exhaustion, thirst and fight health to 0.2 in the turn that it loads, and these values stay: the "
	                "Creature and CreatureNeeds components of the wolf show them after its first turns. Only the "
	                "warmth then moves to the temperature of the land.",
	    .environment = {.dispenserGrid = false, .players = {PlayerNames::PLAYER_TWO}},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 25.0f, -55.0f}, .look = {10.0f, 8.0f, 0.0f}},
	    .creatures = {cow},
	    .commands = {load("SetupLethysCreature", 1.5f)},
	});
	// As in the big fight of the fifth land, player two has a creature already, which the new creature replaces
	auto old = Content(CreatureType::Lion, {20.0f, 25.0f}, 180.0f, "player two's old lion");
	old.owner = PlayerNames::PLAYER_TWO;
	all.push_back({
	    .id = "creature.load_nemesis",
	    .name = "The fifth land loads the creature of Nemesis from his mind file over his old creature",
	    .facet = Facet::Story,
	    .description = "Player two has a Lion. The game's SetupNemesisCreature script runs for player two with the scale "
	                   "2, as the fifth land runs it.",
	    .expected = "A second Lion of player two appears on the ground at the place, fully grown, from the "
	                "NemesisCreature mind file. The script log says that the player has a creature already and that the "
	                "game overwrites it. The new Lion is now the creature of player two.",
	    .environment = {.dispenserGrid = false, .players = {PlayerNames::PLAYER_TWO}},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 30.0f, -60.0f}, .look = {10.0f, 8.0f, 10.0f}},
	    .creatures = {cow, old},
	    .commands = {load("SetupNemesisCreature", 2.0f)},
	});
	all.push_back({
	    .id = "creature.load_khazar",
	    .name = "The second land loads the creature of Khazar from his mind file",
	    .facet = Facet::Story,
	    .description = "The game's SetupKhazarCreature script runs for player two with the scale 1.2, as the second land "
	                   "runs it.",
	    .expected = "A Tortoise of player two appears on the ground at the place, fully grown, from the KhazarCreature "
	                "mind file. It knows every leash: the CreatureLeash component shows four known leashes, and each "
	                "leash word of its mind is 1. It is the creature of player two. The script makes it a friend of the "
	                "player's Cow and gives it the alignment of the Cow.",
	    .environment = {.dispenserGrid = false, .players = {PlayerNames::PLAYER_TWO}},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 25.0f, -55.0f}, .look = {10.0f, 8.0f, 0.0f}},
	    .creatures = {cow},
	    .commands = {load("SetupKhazarCreature", 1.2f)},
	});
}

/// The end of the third land's prison: the land's own set-up and prison scripts run with the developer switch that
/// makes every prison statue count as stopped, so the creature is freed at once
void AddThirdLandPrison(std::vector<Scenario>& all)
{
	using Argument = ScriptArgument;
	all.push_back({
	    .id = "story.l3_creature_freed",
	    .name = "Story: the creature is freed from the prison of the third land",
	    .facet = Facet::Story,
	    .description = "The third land loads with its own scripts and sky, and the player's Tiger stands on it. Its set-up "
	                   "script runs, then its prison script with the developer switch on, so the three statues count as "
	                   "stopped at their first check and the scene of the freed creature plays.",
	    .expected = "The Tiger is put on the prison spot. The statues do not sink (the switch skips their scenes). Lethys "
	                "speaks, both advisors speak about the freed creature, and the Tiger looks at the camera, walks down "
	                "the hill and waves. After each statue's scene the help forgets every message about the creature, so "
	                "no creature message from before the scene comes up when the scene ends.",
	    .environment = {.land = Land::Story, .storyLand = "Land3.txt", .dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-20.0f, -20.0f}, {20.0f, 20.0f}}},
	    .creatures = {{.label = "player's tiger",
	                   .species = CreatureType::Tiger,
	                   .phase = 3,
	                   .needs = {.energy = 1.0f, .exhaustion = 0.0f, .dehydration = 0.0f, .poo = 0.0f, .life = 1.0f}}},
	    .commands = {{.kind = Kind::RunScript,
	                  .delaySeconds = 0.5f,
	                  .script = "SetupLand3",
	                  .scriptGlobals = {{"MyCreature", {.kind = Argument::Kind::Creature, .index = 0}}}},
	                 {.kind = Kind::RunScript,
	                  .delaySeconds = 1.0f,
	                  .script = "FreeTheCreature",
	                  .scriptGlobals = {{"GlobalDebug", {.number = 1.0f}}}}},
	});
	all.push_back({
	    .id = "story.l3_pillars_turn_away",
	    .name = "Story: the prison pillars of the third land turn away from the freed creature",
	    .facet = Facet::Story,
	    .description = "The third land loads with its own scripts and sky, and the player's Tiger stands on it. Its set-up "
	                   "script runs, then its prison script with the developer switch on, so the three statues count as "
	                   "stopped at their first check. When the scene of the freed creature ends, each statue script ends.",
	    .expected = "At the end of each statue script, its pillar turns about the upright to face away from the prison "
	                "spot: it faces a point 10 metres past the pillar on the line from the prison spot. The freeze beams "
	                "and the plasma sound stop.",
	    .environment = {.land = Land::Story, .storyLand = "Land3.txt", .dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-40.0f, -40.0f}, {40.0f, 40.0f}}},
	    .creatures = {{.label = "player's tiger",
	                   .species = CreatureType::Tiger,
	                   .phase = 3,
	                   .needs = {.energy = 1.0f, .exhaustion = 0.0f, .dehydration = 0.0f, .poo = 0.0f, .life = 1.0f}}},
	    .commands = {{.kind = Kind::RunScript,
	                  .delaySeconds = 0.5f,
	                  .script = "SetupLand3",
	                  .scriptGlobals = {{"MyCreature", {.kind = Argument::Kind::Creature, .index = 0}}}},
	                 {.kind = Kind::RunScript,
	                  .delaySeconds = 1.0f,
	                  .script = "FreeTheCreature",
	                  .scriptGlobals = {{"GlobalDebug", {.number = 1.0f}}}}},
	});
	// The prison spot of the third land and a point far from it, from the middle of the map
	static constexpr glm::vec2 k_PrisonSpot {-497.79f, -300.15f};
	static constexpr glm::vec2 k_FarFromPrison {-575.0f, -385.0f};
	all.push_back({
	    .id = "story.l3_laetes_walks_back",
	    .name = "Story: Lethys's creature walks back to the prison of the third land",
	    .facet = Facet::Story,
	    .description = "The third land loads with its own scripts and sky, and the player's Tiger stands on it. Its set-up "
	                   "script and its prison script run. Four seconds later, Lethys's creature is put 115 metres from "
	                   "the prison spot, which is more than 75 metres.",
	    .expected = "If Lethys's creature does a miracle at the time, it does the miracle to its end. Then it walks back "
	                "to its start point near the prison. When it is within 75 metres of the prisoner again, it does its "
	                "miracles at the prisoner again.",
	    .environment = {.land = Land::Story, .storyLand = "Land3.txt", .dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_FarFromPrison - glm::vec2 {15.0f}, k_PrisonSpot + glm::vec2 {15.0f}}},
	    .creatures = {{.label = "player's tiger",
	                   .species = CreatureType::Tiger,
	                   .phase = 3,
	                   .needs = {.energy = 1.0f, .exhaustion = 0.0f, .dehydration = 0.0f, .poo = 0.0f, .life = 1.0f}}},
	    .commands = {{.kind = Kind::RunScript,
	                  .delaySeconds = 0.5f,
	                  .script = "SetupLand3",
	                  .scriptGlobals = {{"MyCreature", {.kind = Argument::Kind::Creature, .index = 0}}}},
	                 {.kind = Kind::RunScript, .delaySeconds = 1.0f, .script = "FreeTheCreature"},
	                 {.kind = Kind::CallNative,
	                  .delaySeconds = 4.0f,
	                  .script = "CALL_PLAYER_CREATURE",
	                  .scriptArguments = {{.kind = Argument::Kind::Number, .number = 2.0f}}},
	                 {.kind = Kind::CallNative,
	                  .delaySeconds = 0.0f,
	                  .script = "SET_POSITION",
	                  .scriptArguments = {{.kind = Argument::Kind::Result},
	                                      {.kind = Argument::Kind::Position, .point = k_FarFromPrison}}}},
	});
	// The numbers that the land's script gave the villages of the three pillars
	static constexpr size_t k_EgyptianVillage = 2;
	static constexpr size_t k_IndianVillage = 3;
	static constexpr size_t k_JapaneseVillage = 4;
	all.push_back({
	    .id = "story.l3_third_village_in_second_scene",
	    .name = "Story: the third village of the prison goes to the player while the second pillar's scene plays",
	    .facet = Facet::Story,
	    .description = "The third land loads with its own scripts and sky, and the player's Tiger stands on it. Its set-up "
	                   "script and its prison script run, and the pillars freeze the Tiger every 3 seconds. The Japanese "
	                   "village goes to the player, then the Indian village after the first pillar's scene. Ten seconds "
	                   "later, while the second pillar's scene plays, the Egyptian village goes to the player.",
	    .expected = "The first and second pillars sink. The second scene sees three pillars down and plays the release. "
	                "The third pillar froze the Tiger again just before, and every freeze added its time to the ice. So the "
	                "Tiger stays in the ice and does not play its release animation until the added time runs out. Then "
	                "the third scene plays the release again. This is the land's script as it is written.",
	    .environment = {.land = Land::Story, .storyLand = "Land3.txt", .dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-40.0f, -40.0f}, {40.0f, 40.0f}}},
	    .creatures = {{.label = "player's tiger",
	                   .species = CreatureType::Tiger,
	                   .phase = 3,
	                   .needs = {.energy = 1.0f, .exhaustion = 0.0f, .dehydration = 0.0f, .poo = 0.0f, .life = 1.0f}}},
	    .commands = {{.kind = Kind::RunScript,
	                  .delaySeconds = 0.5f,
	                  .script = "SetupLand3",
	                  .scriptGlobals = {{"MyCreature", {.kind = Argument::Kind::Creature, .index = 0}}}},
	                 {.kind = Kind::RunScript, .delaySeconds = 1.0f, .script = "FreeTheCreature"},
	                 {.kind = Kind::GiveLandTown, .delaySeconds = 10.0f, .value = k_JapaneseVillage, .amount = 5.0f},
	                 {.kind = Kind::GiveLandTown, .delaySeconds = 90.0f, .value = k_IndianVillage, .amount = 5.0f},
	                 {.kind = Kind::GiveLandTown, .delaySeconds = 10.0f, .value = k_EgyptianVillage, .amount = 5.0f}},
	});

	// A script makes a pack of wolves, sends it off and then lets it go, as the attack of the possessed wolves does
	// when the first wolf reaches the village
	const auto wolf = [](glm::vec2 point, float delay) {
		return Command {
		    .kind = Kind::CallNative,
		    .delaySeconds = delay,
		    .script = "CREATE",
		    .scriptArguments = {{.kind = Argument::Kind::Integer, .number = static_cast<float>(script::ObjectType::Animal)},
		                        {.kind = Argument::Kind::Integer, .number = static_cast<float>(AnimalInfo::Wolf)},
		                        {.kind = Argument::Kind::Position, .point = point}}};
	};
	const auto attach = [](size_t made) {
		return Command {.kind = Kind::CallNative,
		                .delaySeconds = 0.0f,
		                .script = "FLOCK_ATTACH",
		                .scriptArguments = {{.kind = Argument::Kind::Made, .index = made},
		                                    {.kind = Argument::Kind::Made, .index = 3},
		                                    {.kind = Argument::Kind::Integer, .number = 0.0f}}};
	};
	all.push_back({
	    .id = "story.l3_wolf_pack_released",
	    .name = "Story: a script lets its pack of wolves go",
	    .facet = Facet::Story,
	    .description = "After one second, a script makes three wolves south of the middle and a flock, and attaches each "
	                   "wolf to the flock, as the attack of the possessed wolves on the third land does. The flock gets "
	                   "the distances 5 and 40, SET_PROPERTY gives it half speed, and MOVE_GAME_THING sends it 100 "
	                   "metres to the north. After eight seconds, RELEASE_FROM_SCRIPT lets the flock go.",
	    .expected = "The three wolves walk north together. When the script lets the flock go, each wolf also goes out "
	                "of the control of the script at once: the inspector shows that no wolf is under script control. "
	                "Each wolf then starts to wander as a wild wolf does: it walks on the land to a point about the place "
	                "of its flock. No wolf stands still.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-25.0f, -40.0f}, {25.0f, 75.0f}}},
	    .commands = {wolf({-6.0f, -30.0f}, 1.0f),
	                 wolf({0.0f, -30.0f}, 0.0f),
	                 wolf({6.0f, -30.0f}, 0.0f),
	                 {.kind = Kind::CallNative,
	                  .delaySeconds = 0.0f,
	                  .script = "FLOCK_CREATE",
	                  .scriptArguments = {{.kind = Argument::Kind::Position, .point = {0.0f, -30.0f}}}},
	                 {.kind = Kind::CallNative,
	                  .delaySeconds = 0.0f,
	                  .script = "CHANGE_INNER_OUTER_PROPERTIES",
	                  .scriptArguments = {{.kind = Argument::Kind::Made, .index = 3},
	                                      {.kind = Argument::Kind::Number, .number = 5.0f},
	                                      {.kind = Argument::Kind::Number, .number = 40.0f},
	                                      {.kind = Argument::Kind::Number, .number = 0.0f}}},
	                 attach(0),
	                 attach(1),
	                 attach(2),
	                 {.kind = Kind::CallNative,
	                  .delaySeconds = 0.0f,
	                  .script = "SET_PROPERTY",
	                  .scriptArguments = {{.kind = Argument::Kind::Integer, .number = 11.0f},
	                                      {.kind = Argument::Kind::Made, .index = 3, .kept = true},
	                                      {.kind = Argument::Kind::Number, .number = 0.5f}}},
	                 {.kind = Kind::CallNative,
	                  .delaySeconds = 0.0f,
	                  .script = "MOVE_GAME_THING",
	                  .scriptArguments = {{.kind = Argument::Kind::Made, .index = 3},
	                                      {.kind = Argument::Kind::Position, .point = {0.0f, 70.0f}},
	                                      {.kind = Argument::Kind::Number, .number = 0.0f}}},
	                 {.kind = Kind::CallNative,
	                  .delaySeconds = 8.0f,
	                  .script = "RELEASE_FROM_SCRIPT",
	                  .scriptArguments = {{.kind = Argument::Kind::Made, .index = 3}}}},
	});

	// The campfire of the burning fishermen on the beach of the Japanese village, from the middle of the map
	static constexpr glm::vec2 k_Campfire {376.11f, 36.22f};
	all.push_back({
	    .id = "story.l3_fishermen_in_reach",
	    .name = "Story: the burning fishermen of the third land in reach of the hand",
	    .facet = Facet::Story,
	    .description = "The third land loads with its own scripts and sky, and the player's Tiger stands on it. Its set-up "
	                   "script runs. Then a script ring of the player's influence with a radius of 80 metres goes on the "
	                   "fishermen's campfire on the beach, and the script of Lethys's fireballs runs with no wait. When "
	                   "his scene ends, the hand can take a burning fisherman during its first short runs.",
	    .expected = "Lethys speaks three lines, throws his fireballs and sets 16 fishermen alight. The script makes 18 "
	                "fishermen: it lets the first two of fishermen seven and eight go at the end of the turn that makes "
	                "them, and they then live as homeless villagers. A fisherman that the hand takes during its short "
	                "runs stays alight in the hand. When the hand puts it down, it lands and goes back to its script "
	                "walk: it runs to the last place the script gave it, then does its other runs and runs to the "
	                "village.",
	    .environment = {.land = Land::Story, .storyLand = "Land3.txt", .dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_Campfire - glm::vec2 {30.0f}, k_Campfire + glm::vec2 {30.0f}}},
	    .creatures = {{.label = "player's tiger",
	                   .species = CreatureType::Tiger,
	                   .phase = 3,
	                   .needs = {.energy = 1.0f, .exhaustion = 0.0f, .dehydration = 0.0f, .poo = 0.0f, .life = 1.0f}}},
	    .commands = {{.kind = Kind::RunScript,
	                  .delaySeconds = 0.5f,
	                  .script = "SetupLand3",
	                  .scriptGlobals = {{"MyCreature", {.kind = Argument::Kind::Creature, .index = 0}}}},
	                 {.kind = Kind::CallNative,
	                  .delaySeconds = 1.0f,
	                  .script = "INFLUENCE_POSITION",
	                  .scriptArguments = {{.kind = Argument::Kind::Position, .point = k_Campfire},
	                                      {.kind = Argument::Kind::Number, .number = 80.0f},
	                                      {.kind = Argument::Kind::Integer, .number = 0.0f},
	                                      {.kind = Argument::Kind::Integer, .number = 0.0f}}},
	                 {.kind = Kind::RunScript,
	                  .delaySeconds = 0.5f,
	                  .script = "FanaticAttack",
	                  .scriptArguments = {{.kind = Argument::Kind::Number, .number = 1.0f}}}},
	});
}

/// The man of the third land who wants to be thrown: the land's own script makes him at his campfire, with the player's
/// creature near him, so that the hand or the creature can take him
void AddThirdLandThrowBloke(std::vector<Scenario>& all)
{
	using Argument = ScriptArgument;
	// His place on the beach of the third land, from the middle of the map
	constexpr glm::vec2 k_Man = glm::vec2(2796.28f, 3146.40f) - flat_land::k_MapMiddle;
	all.push_back({
	    .id = "story.l3_throw_bloke",
	    .name = "Story: the man of the third land who wants to be thrown",
	    .facet = Facet::Story,
	    .description = "The third land loads with its own scripts and sky. The land's script of the man who wants to be "
	                   "thrown starts. It makes the man and his campfire on the beach, and the player's Tiger stands 15 "
	                   "metres from him. Take the man with the hand and throw him, throw him into the sea to the north "
	                   "west, set him alight, or let the Tiger take him and eat him.",
	    .expected = "The man stands at his campfire, looks at the waterfall and plays his clip. While the camera is near, "
	                "he asks to be thrown every 20 seconds. In the hand he says a line. When he flies, the game slows to "
	                "0.32 of its speed under wide screen bars, and the camera follows him from 10 metres. After 3 "
	                "seconds the speed comes back to normal over 3 seconds; when he lands the speed is normal again. On "
	                "land he walks back to his place and boasts. In the sea he stays until the camera looks away, then "
	                "a new man comes at his place. Fire does not hurt him. When the Tiger eats him, the Tiger plays its "
	                "clip of a creature that must poo and then poos. A new man comes out at its back, flies to it and "
	                "walks back to his place.",
	    .environment = {.land = Land::Story, .storyLand = "Land3.txt", .dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {k_Man.x + 20.0f, 25.0f, k_Man.y - 25.0f}, .look = {k_Man.x, 2.0f, k_Man.y}},
	    .creatures = {Content(CreatureType::Tiger, k_Man + glm::vec2(12.0f, -9.0f), 300.0f, "player's tiger")},
	    .commands = {{.kind = Kind::RunScript,
	                  .delaySeconds = 0.5f,
	                  .script = "ThrowBlokeMain",
	                  .scriptGlobals = {{"MyCreature", {.kind = Argument::Kind::Creature, .index = 0}}}}},
	});
}

/// The waterfall of the third land, which the draw of the land makes with no script
void AddThirdLandWaterfall(std::vector<Scenario>& all)
{
	// The camera stands south-west of the waterfall, near enough to hear its sound, and looks at its foot. Both points
	// are from the middle of the map.
	constexpr glm::vec2 k_Eye = glm::vec2(2985.0f, 3082.0f) - flat_land::k_MapMiddle;
	constexpr glm::vec2 k_Look = glm::vec2(3030.0f, 3135.0f) - flat_land::k_MapMiddle;
	all.push_back({
	    .id = "story.l3_waterfall",
	    .name = "Story: the waterfall of the third land",
	    .facet = Facet::Story,
	    .description = "The third land loads with its own scripts and sky. The camera looks north-east at the waterfall "
	                   "on the coast, which no script makes: the draw of the land makes it on its first frame.",
	    .expected = "The water slides down the waterfall at half the height of its texture each second. The rock "
	                "around it stays still. Every 0.7 seconds a ring of spray starts at its foot, grows to 60 metres "
	                "across and fades. The sound of water that flows comes from the waterfall.",
	    .environment = {.land = Land::Story, .storyLand = "Land3.txt", .dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {k_Eye.x, 32.0f, k_Eye.y}, .look = {k_Look.x, 7.0f, k_Look.y}},
	});
}

/// The Pied Piper of the first land. The game's script runs at the same map positions as on the first land, with the
/// Norse town, the creche and the piper's cave made there. The scenario clicks the creche, and the round starts.
void AddPiedPiperRound(std::vector<Scenario>& all)
{
	using Argument = ScriptArgument;
	// The first land's Norse town, as the lines of the first land make it, from the middle of the map. The town has the
	// number of the scenario's town, thus the scenario gives this town to the script.
	constexpr std::string_view k_NorseTown = R"(CREATE_TOWN(900, "-694.39,81.24", "PLAYER_ONE", 7, "NORSE")
SET_TOWN_BELIEF(900, "PLAYER_ONE", 9.238012)
CREATE_PLANNED_ABODE(900, "-637.52,157.17", "NORSE_ABODE_WONDER", 916, 250, 0, 0)
CREATE_ABODE(900, "-804.91,132.52", "NORSE_ABODE_C", 4710, 990, 0, 0)
CREATE_VILLAGER_POS("-797.15,132.48", "-804.91,132.52", "NORSE_FORESTER", 21)
CREATE_VILLAGER_POS("-797.15,132.48", "-804.91,132.52", "NORSE_HOUSEWIFE", 24)
CREATE_VILLAGER_POS("-797.15,132.48", "-811.55,119.58", "NORSE_FORESTER", 6)
CREATE_VILLAGER_POS("-797.15,132.48", "-811.55,119.58", "NORSE_HOUSEWIFE", 6)
CREATE_ABODE(900, "-779.62,153.34", "NORSE_ABODE_E", 6282, 1000, 0, 0)
CREATE_VILLAGER_POS("-779.69,141.47", "-782.63,152.23", "NORSE_HOUSEWIFE", 3)
CREATE_VILLAGER_POS("-779.69,141.47", "-782.63,152.23", "NORSE_HOUSEWIFE", 36)
CREATE_VILLAGER_POS("-779.69,141.47", "-767.31,151.49", "NORSE_FORESTER", 13)
CREATE_VILLAGER_POS("-779.69,141.47", "-767.31,151.49", "NORSE_HOUSEWIFE", 18)
CREATE_VILLAGER_POS("-779.69,141.47", "-704.62,122.31", "NORSE_HOUSEWIFE", 8)
CREATE_ABODE(900, "-702.20,105.53", "NORSE_ABODE_F", 1177, 888, 0, 0)
CREATE_VILLAGER_POS("-705.45,106.99", "-702.20,105.53", "NORSE_FORESTER", 13)
CREATE_VILLAGER_POS("-705.45,106.99", "-702.20,105.53", "NORSE_HOUSEWIFE", 48)
CREATE_VILLAGER_POS("-705.45,106.99", "-702.20,105.53", "NORSE_HOUSEWIFE", 1)
CREATE_ABODE(900, "-755.27,90.97", "NORSE_ABODE_A", 6217, 1000, 0, 0)
CREATE_VILLAGER_POS("-754.97,93.93", "-755.27,90.97", "NORSE_FORESTER", 1)
CREATE_VILLAGER_POS("-754.97,93.93", "-765.56,86.56", "NORSE_HOUSEWIFE", 24)
CREATE_ABODE(900, "-774.79,92.58", "NORSE_ABODE_F", 5756, 888, 0, 0)
CREATE_VILLAGER_POS("-772.92,95.61", "-811.55,119.58", "NORSE_HOUSEWIFE", 6)
CREATE_VILLAGER_POS("-772.92,95.61", "-765.56,86.56", "NORSE_FORESTER", 13)
CREATE_ABODE(900, "-906.86,94.09", "NORSE_ABODE_E", 0, 1000, 0, 0)
CREATE_VILLAGER_POS("-906.91,82.21", "-782.63,152.23", "NORSE_FORESTER", 5)
CREATE_VILLAGER_POS("-906.91,82.21", "-782.63,152.23", "NORSE_FORESTER", 5)
CREATE_VILLAGER_POS("-906.91,82.21", "-906.86,94.09", "NORSE_FORESTER", 13)
CREATE_VILLAGER_POS("-906.91,82.21", "-906.86,94.09", "NORSE_HOUSEWIFE", 3)
CREATE_VILLAGER_POS("-906.91,82.21", "-782.57,97.55", "NORSE_FORESTER", 20)
CREATE_VILLAGER_POS("-906.91,82.21", "-709.75,113.09", "NORSE_HOUSEWIFE", 30)
CREATE_ABODE(900, "-881.77,176.70", "NORSE_ABODE_GRAVEYARD", 0, 1000, 0, 0)
CREATE_ABODE(900, "-846.17,32.59", "NORSE_ABODE_C", 1588, 1000, 0, 0)
CREATE_VILLAGER_POS("-854.01,32.48", "-755.27,90.97", "NORSE_FORESTER", 13)
CREATE_VILLAGER_POS("-854.01,32.48", "-846.17,32.59", "NORSE_FORESTER", 5)
CREATE_VILLAGER_POS("-854.01,32.48", "-846.17,32.59", "NORSE_HOUSEWIFE", 8)
CREATE_VILLAGER_POS("-854.01,32.48", "-788.12,103.35", "NORSE_FORESTER", 35)
CREATE_ABODE(900, "-695.01,209.27", "NORSE_ABODE_C", 2486, 1000, 0, 0)
CREATE_VILLAGER_POS("-699.81,203.07", "-755.27,90.97", "NORSE_HOUSEWIFE", 6)
CREATE_VILLAGER_POS("-699.81,203.07", "-695.01,209.27", "NORSE_FORESTER", 24)
CREATE_VILLAGER_POS("-699.81,203.07", "-695.01,209.27", "NORSE_HOUSEWIFE", 30)
CREATE_ABODE(900, "-733.21,81.36", "NORSE_ABODE_STORAGE_PIT", 3730, 1000, 20000, 30000)
CREATE_ABODE(900, "-802.99,107.32", "NORSE_ABODE_D", 5039, 1000, 0, 0)
CREATE_VILLAGER_POS("-796.25,104.55", "-802.99,107.32", "NORSE_HOUSEWIFE", 8)
CREATE_VILLAGER_POS("-796.25,104.55", "-802.99,107.32", "NORSE_HOUSEWIFE", 17)
CREATE_VILLAGER_POS("-796.25,104.55", "-713.05,105.14", "NORSE_FORESTER", 13)
CREATE_VILLAGER_POS("-796.25,104.55", "-713.05,105.14", "NORSE_HOUSEWIFE", 8)
CREATE_VILLAGER_POS("-796.25,104.55", "-704.62,122.31", "NORSE_FORESTER", 30)
CREATE_TOWN_CENTRE(900, "-754.78,131.31", "NORSE_ABODE_TOWN_CENTRE", 6282, 990, 0)
CREATE_ABODE(900, "-795.14,92.64", "NORSE_ABODE_A", 5366, 1000, 0, 0)
CREATE_VILLAGER_POS("-792.71,94.37", "-802.99,107.32", "NORSE_FORESTER", 13)
CREATE_VILLAGER_POS("-792.71,94.37", "-795.14,92.64", "NORSE_FORESTER", 28)
CREATE_VILLAGER_POS("-792.71,94.37", "-795.14,92.64", "NORSE_HOUSEWIFE", 8)
CREATE_ABODE(900, "-709.16,133.52", "NORSE_ABODE_F", 1963, 1000, 0, 0)
CREATE_VILLAGER_POS("-712.91,132.09", "-709.16,133.52", "NORSE_FORESTER", 13)
CREATE_VILLAGER_POS("-712.91,132.09", "-709.16,133.52", "NORSE_HOUSEWIFE", 8)
CREATE_ABODE(900, "-709.75,113.09", "NORSE_ABODE_A", 1439, 1000, 0, 0)
CREATE_VILLAGER_POS("-712.69,113.59", "-774.79,92.58", "NORSE_HOUSEWIFE", 8)
CREATE_VILLAGER_POS("-712.69,113.59", "-709.75,113.09", "NORSE_FORESTER", 13)
CREATE_ABODE(900, "-691.99,170.12", "NORSE_ABODE_CRECHE", 4907, 1000, 0, 0)
CREATE_ABODE(900, "-707.14,145.21", "NORSE_ABODE_B", 1700, 1000, 0, 0)
CREATE_VILLAGER_POS("-711.13,144.80", "-774.79,92.58", "NORSE_FORESTER", 5)
CREATE_VILLAGER_POS("-711.13,144.80", "-707.48,145.18", "NORSE_FORESTER", 27)
CREATE_VILLAGER_POS("-711.13,144.80", "-707.14,145.21", "NORSE_HOUSEWIFE", 50)
CREATE_NEW_TOWN_FIELD(900, "-825.00,135.00", 2, 0.000000)
CREATE_NEW_TOWN_FIELD(900, "-735.00,195.00", 0, 0.000000)
CREATE_NEW_TOWN_FIELD(900, "-725.00,195.00", 0, 0.000000)
CREATE_NEW_TOWN_FIELD(900, "-725.00,185.00", 0, 0.000000)
CREATE_NEW_TOWN_FIELD(900, "-735.00,185.00", 0, 0.000000)
CREATE_NEW_TOWN_FIELD(900, "-825.00,145.00", 0, 0.000000)
CREATE_NEW_TOWN_FIELD(900, "-815.00,145.00", 0, 0.000000)
CREATE_NEW_TOWN_FIELD(900, "-805.00,145.00", 0, 0.000000)
CREATE_NEW_TOWN_FIELD(900, "-935.00,95.00", 0, 0.000000)
CREATE_NEW_TOWN_FIELD(900, "-925.00,95.00", 0, 0.000000)
CREATE_NEW_TOWN_FIELD(900, "-925.00,105.00", 0, 0.000000)
CREATE_NEW_TOWN_FIELD(900, "-935.00,105.00", 0, 0.000000)
CREATE_NEW_TOWN_FIELD(900, "-855.00,55.00", 0, 0.000000)
CREATE_NEW_TOWN_FIELD(900, "-855.00,45.00", 0, 0.000000)
CREATE_NEW_TOWN_FIELD(900, "-845.00,55.00", 0, 0.000000)
CREATE_NEW_TOWN_FIELD(900, "-835.00,55.00", 0, 0.000000)
CREATE_NEW_TOWN_FIELD(900, "-835.00,45.00", 0, 0.000000)
)";
	// The positions on the first land, from the middle of the map: the creche, the piper's cave and where the player's
	// creature stands, far from the town and the cave
	constexpr glm::vec2 k_Creche {-691.99f, 170.12f};
	constexpr glm::vec2 k_Cave {-589.2f, 252.12f};
	constexpr glm::vec2 k_CreaturePlace {-645.0f, -451.0f};
	const Command runRound {
	    .kind = Kind::RunScript,
	    .delaySeconds = 0.5f,
	    .script = "PiedPiper",
	    // As the first land sets them up: the piper's cave, the Norse town, the player's creature, and no round yet
	    .scriptGlobals = {{"PiperCave", {.kind = Argument::Kind::Object, .index = 0}},
	                      {"NorseTown", {.kind = Argument::Kind::Town}},
	                      {"MyCreature", {.kind = Argument::Kind::Creature, .index = 0}},
	                      {"PiedPiperInitiated", {.number = 0.0f}},
	                      {"PiedPiperFinished", {.number = 0.0f}},
	                      {"PiedPiperCompletedNow", {.number = 0.0f}},
	                      {"PiedPiperChildrenInCave", {.number = 0.0f}},
	                      {"PiedPiperDead", {.number = 0.0f}},
	                      {"CaveOpen", {.number = 0.0f}}},
	};
	all.push_back({
	    .id = "story.pied_piper_round",
	    .name = "The Pied Piper takes the children of the Norse town",
	    .facet = Facet::Story,
	    .description = "The first land's Norse town, its creche and the piper's cave stand at their places on the first "
	                   "land. The player's Cow stands 500 metres south of the town. The game's own PiedPiper script runs, "
	                   "and three seconds in the creche is clicked.",
	    .expected = "A scroll stands over the creche. On the click a film plays: the creche leader walks to the soap box "
	                "and asks for help, the camera shows the piper at his cave and the cave's sounds. After the film the "
	                "piper dances near the town, children join his flock, and he takes them to the cave. He leaves the "
	                "dance after ten minutes or with five children, and the round starts again. When the Cow comes "
	                "near the piper, he runs from it to his cave, and then the round starts again.",
	    .environment = {.dispenserGrid = false, .landNumber = 1},
	    .framing = {.shot = Shot::Placed,
	                .eye = {k_Creche.x - 20.0f, 12.0f, k_Creche.y - 22.0f},
	                .look = {k_Creche.x, 4.0f, k_Creche.y}},
	    .creatures = {Content(CreatureType::Cow, k_CreaturePlace, 0.0f, "player's cow")},
	    .objects = {{.type = AnimatedStaticInfo::PiperCaveEntrance, .offset = k_Cave, .yawDegrees = -131.895f}},
	    .landScript = k_NorseTown,
	    .commands = {runRound,
	                 {.kind = Kind::PointerTo, .delaySeconds = 2.0f, .point = {0.5f, 0.5f}},
	                 {.kind = Kind::PointerPress, .delaySeconds = 1.0f, .value = 3},
	                 {.kind = Kind::PointerRelease, .delaySeconds = 0.1f, .value = 3}},
	    .objectsAfterLandScript = true,
	});
}

/// The Ogre quest of the first land. The game's script runs at the same map positions as on the first land. The
/// player's Cow is in the valley below the lair, where the script sends it. The scenario sets the flag of the fight
/// lesson, thus the script makes the Ogre at the lair immediately. The scenario clicks the scroll after ten seconds.
void AddOgreLair(std::vector<Scenario>& all)
{
	using Argument = ScriptArgument;
	// The positions on the first land, from the middle of the map: the lair, the scroll and the position where the
	// script sends the player's creature
	constexpr glm::vec2 k_Lair {-21.307f, 600.6f};
	constexpr glm::vec2 k_Scroll {-6.428f, 624.607f};
	constexpr glm::vec2 k_SentBackTo {-28.15f, 484.55f};
	const Command runQuest {
	    .kind = Kind::RunScript,
	    .delaySeconds = 0.5f,
	    .script = "CreatureGuardian",
	    .scriptGlobals = {{"MyCreature", {.kind = Argument::Kind::Creature, .index = 0}},
	                      {"CreatureDevelopment_Fight", {.number = 1.0f}},
	                      {"CreatureGuardianFinished", {.number = 0.0f}},
	                      {"EndSendMyCreatureAwayFromGuardianFlag", {.number = 0.0f}}},
	};
	// The scroll appears when the script makes the Ogre. The pointer clicks the scroll at the middle of the screen with
	// the Action button.
	const std::vector<Command> introduction {
	    runQuest,
	    {.kind = Kind::PointerTo, .delaySeconds = 10.0f, .point = {0.5f, 0.5f}},
	    {.kind = Kind::PointerPress, .delaySeconds = 1.0f, .value = 3},
	    {.kind = Kind::PointerRelease, .delaySeconds = 0.1f, .value = 3},
	};
	const Framing framing {
	    .shot = Shot::Placed, .eye = {k_Scroll.x - 15.0f, 30.0f, k_Scroll.y - 45.0f}, .look = {k_Scroll.x, 0.0f, k_Scroll.y}};
	// The player's creature. Its mind is free, thus the script can control it and release it.
	auto cow = Content(CreatureType::Cow, k_SentBackTo, 0.0f, "player's cow");
	cow.hold = false;
	all.push_back({
	    .id = "story.ogre_lair",
	    .name = "The Ogre at his lair, and the film that brings him in",
	    .facet = Facet::Story,
	    .description = "The first land's Ogre quest: the player's Cow stands down the valley from the lair and the game's "
	                   "own script runs as after the fight lesson. Ten seconds in, the Ogre's scroll is clicked.",
	    .expected = "An ogre 1.2 times the Cow's size, named Sleg, sleeps at the lair beside a silver scroll. On the click "
	                "a film plays: music, the Ogre scratches himself, the advisors and the Ogre talk, the Cow points at "
	                "the Ogre, who looks it over, plays his angry animation and goes back to sleep. Then the Cow is the "
	                "player's again.",
	    .environment = {.dispenserGrid = false},
	    .framing = framing,
	    .creatures = {cow},
	    .commands = introduction,
	});
	all.push_back({
	    .id = "story.ogre_fed_twice",
	    .name = "Feeding the Ogre twice",
	    .facet = Facet::Story,
	    .description = "As story.ogre_lair, with two piles of food such as the hand drops lying 22 metres from the lair, "
	                   "far from the Cow.",
	    .expected = "After the film the script finds the nearer pile. A film takes the camera to it: the Ogre walks to it, "
	                "picks up a handful and eats it, and the rest of the pile goes. The good advisor asks whether he is "
	                "getting drowsy and he walks back to the lair. Then the same for the other pile, after which he falls "
	                "asleep, the evil advisor calls him thick, and the camera flies through the pass to the reward.",
	    .environment = {.dispenserGrid = false},
	    .framing = framing,
	    .creatures = {cow},
	    .objects = {{.type = PotInfo::MagicFood, .offset = k_Lair + glm::vec2 {10.0f, -20.0f}, .amount = 1000},
	                {.type = PotInfo::MagicFood, .offset = k_Lair + glm::vec2 {-12.0f, -20.0f}, .amount = 1000}},
	    .commands = introduction,
	});
	auto fight = introduction;
	fight.push_back(Go(Kind::WalkTo, 0, k_Lair + glm::vec2 {0.0f, -20.0f}, 70.0f, false));
	all.push_back({
	    .id = "story.ogre_fight",
	    .name = "The player's creature fights the Ogre",
	    .facet = Facet::Story,
	    .description = "As story.ogre_lair; 70 seconds after the click the Cow walks to 20 metres from the lair.",
	    .expected = "The evil advisor tells the player to prepare for battle, the Cow's leash comes off and the Ogre picks "
	                "a fight with it in an arena. If the Cow wins, a film shows the Ogre fall dead and flies through the "
	                "pass to the reward. If the Ogre wins, he says so and walks back to his lair, and another fight starts "
	                "30 seconds later while the Cow stays near.",
	    .environment = {.dispenserGrid = false},
	    .framing = framing,
	    .creatures = {cow},
	    .commands = fight,
	});
}

/// The last lesson of the first land's guide. The game's own script runs at the same map positions as on the first land.
/// The player's Tiger waits where the script makes it watch the guide. The scenario sets the flag of the Ogre quest, thus
/// the script does not show the Ogre's place.
void AddGuideStorm(std::vector<Scenario>& all)
{
	// The positions on the first land, from the middle of the map: the place where the guide wakes, the place where the
	// creature watches it, and the top of the mountain
	constexpr glm::vec2 k_GuideWakes {-271.88f, 251.481f};
	constexpr glm::vec2 k_CreatureWatches {-245.011f, 266.323f};
	constexpr glm::vec2 k_MountainTop {-241.71f, -46.942f};
	// Near enough to the top for the storm to start, then near enough for the last words of the guide
	constexpr glm::vec2 k_StormStarts = k_MountainTop + glm::vec2 {0.0f, 60.0f};
	constexpr glm::vec2 k_LastWords = k_MountainTop + glm::vec2 {0.0f, 25.0f};
	const Command runStorm {
	    .kind = Kind::RunScript,
	    .delaySeconds = 0.5f,
	    .script = "TheStorm",
	    .scriptGlobals = {{"CreatureGuardianInitiated", {.number = 1.0f}}},
	};
	// The guide sleeps for five minutes of game time, then walks to the framed place. The pointer clicks the middle of
	// the screen with the Action button three times, a minute apart, because the five minutes take longer in a slow
	// build. Later the Tiger walks up the mountain, first for the storm and then for the last words. The script moves
	// the Tiger back during the storm's film, thus it walks up again.
	const Command click {.kind = Kind::PointerPress, .delaySeconds = 1.0f, .value = 3};
	const Command letGo {.kind = Kind::PointerRelease, .delaySeconds = 0.1f, .value = 3};
	const std::vector<Command> commands {
	    runStorm,
	    {.kind = Kind::PointerTo, .delaySeconds = 330.0f, .point = {0.5f, 0.5f}},
	    click,
	    letGo,
	    {.kind = Kind::PointerTo, .delaySeconds = 60.0f, .point = {0.5f, 0.5f}},
	    click,
	    letGo,
	    {.kind = Kind::PointerTo, .delaySeconds = 60.0f, .point = {0.5f, 0.5f}},
	    click,
	    letGo,
	    Go(Kind::WalkTo, 0, k_StormStarts, 60.0f, false),
	    Go(Kind::WalkTo, 0, k_LastWords, 120.0f, false),
	    Go(Kind::WalkTo, 0, k_LastWords, 60.0f, false),
	    Go(Kind::WalkTo, 0, k_LastWords, 120.0f, false),
	};
	auto tiger = Content(CreatureType::Tiger, k_CreatureWatches, 0.0f, "player's tiger");
	tiger.hold = false;
	all.push_back({
	    .id = "story.guide_storm",
	    .name = "The guide's last lesson, Nemesis's storm and the death of the guide",
	    .facet = Facet::Story,
	    .description = "The first land's TheStorm script runs on the flat testbed. The script makes the guide (a lion for "
	                   "the Tiger). The guide walks to the fight clearing and stays there for five minutes under a gold "
	                   "scroll. Then it walks to the framed place. From 330 seconds the pointer clicks the middle of the "
	                   "screen three times, a minute apart. Later the Tiger walks up to the mountain top. Click the lines "
	                   "that wait for a click.",
	    .expected = "The guide's music plays while the guide talks and walks to the mountain top. A rain storm forms and "
	                "comes to the top with Nemesis's music, Nemesis talks and the guide answers. Six lightning bolts and "
	                "an explosion from the storm hit the guide, the evil advisor holds on to the screen edge, and the "
	                "guide falls dead and stays down. Then the storm moves away towards the player's village. On the flat "
	                "land, the camera places of the script and the mountain top are above the ground.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed,
	                .eye = {k_GuideWakes.x + 30.0f, 40.0f, k_GuideWakes.y + 50.0f},
	                .look = {k_GuideWakes.x, 0.0f, k_GuideWakes.y}},
	    .creatures = {tiger},
	    .commands = commands,
	});
}

/// The stone circle of The Singing Stones, as the first land makes it at its places
void AddLandHaze(std::vector<Scenario>& all)
{
	// The camera looks from above the kennels across the first land, so the far land is in the distance haze
	constexpr glm::vec2 k_LandOneMiddle {2240.0f, 2640.0f};
	constexpr glm::vec2 k_Kennels = glm::vec2(1899.27f, 3411.21f) - k_LandOneMiddle;
	constexpr glm::vec2 k_Far = glm::vec2(2600.0f, 2400.0f) - k_LandOneMiddle;
	all.push_back({
	    .id = "light.land_distance_haze",
	    .name = "The distance haze on the land",
	    .facet = Facet::Light,
	    .description = "The first land starts at midday with a clear sky. The camera is high above the kennels and looks "
	                   "south east across the island. Use --detail-level 3 or more, which has the haze.",
	    .expected = "The land near the camera shows its own colours. From about 400 units away the land and the "
	                "things on it fade into the haze, and at 900 units and more they have the full haze. The colours "
	                "change smoothly with the distance, with no bands at the edges of the land blocks.",
	    .environment = {.land = Land::Story, .storyLand = "Land1.txt", .dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {k_Kennels.x, 250.0f, k_Kennels.y}, .look = {k_Far.x, 0.0f, k_Far.y}},
	});
}

void AddSingingStones(std::vector<Scenario>& all)
{
	// The circle's centre on the map, from the middle of the map
	constexpr glm::vec2 k_Circle {2496.6653f - 2560.0f, 2246.3257f - 2560.0f};
	all.push_back({
	    .id = "story.singing_stones_circle",
	    .name = "The stone circle of The Singing Stones",
	    .facet = Facet::Story,
	    .description = "The game's SingingStoneCircle script runs as the first land starts it. It makes the circle south "
	                   "of the middle: eight holders on a ring, three stones in them and the other stones far away.",
	    .expected = "Eight prints of the holders show on the ground, on a ring 17.5 from the centre, one every eighth of "
	                "a turn. The three fixed stones stand on the land in the first, second and sixth holders. The "
	                "holders' models are under the land and do not show. Two seconds in, the circle plays the notes of "
	                "its three stones in turn, one holder every half second.",
	    .environment = {.dispenserGrid = false, .landNumber = 1},
	    .framing = {.shot = Shot::Placed,
	                .eye = {k_Circle.x + 10.0f, 30.0f, k_Circle.y - 40.0f},
	                .look = {k_Circle.x, 0.0f, k_Circle.y}},
	    .commands = {{.kind = Kind::RunScript, .delaySeconds = 0.5f, .script = "SingingStoneCircle"}},
	});

	// The funeral march of the second land's singing stones brings back the dead in the ring. The script finds the dead
	// about a fixed place of the second land, from the middle of the map here.
	constexpr glm::vec2 k_Ring {2103.497f - 2560.0f, 3000.377f - 2560.0f};
	all.push_back({
	    .id = "story.singing_stones_raise_dead",
	    .name = "The singing stones bring back the dead in their ring",
	    .facet = Facet::Story,
	    .description = "A cow 3 east of the ring's centre and a villager 6 west of it are killed. Four seconds in, the "
	                   "game's BringBackTheDead script runs, as the funeral march on the second land starts it.",
	    .expected = "A healing glow 13.5 times its usual size covers the ring. The nearest dead thing comes back first: "
	                "the cow puffs steam and comes back alive at the ring's centre, not where it lay, as in the game. "
	                "About 3 seconds later the villager puffs steam and comes back where it lay, of the same kind and "
	                "age, as a skeleton. Each revived thing is let go 3 seconds after it comes back.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed,
	                .eye = {k_Ring.x + 4.0f, 14.0f, k_Ring.y - 22.0f},
	                .look = {k_Ring.x, 0.0f, k_Ring.y}},
	    .objects = {{.type = AnimalInfo::Cow, .offset = k_Ring + glm::vec2 {3.0f, 0.0f}, .life = 0.0f},
	                {.type = VillagerInfo::CelticFarmerMale, .offset = k_Ring + glm::vec2 {-6.0f, 0.0f}, .life = 0.0f}},
	    .commands = {{.kind = Kind::RunScript,
	                  .delaySeconds = 4.0f,
	                  .script = "BringBackTheDead",
	                  .scriptGlobals = {{"SSBringBackToLifeActive", {.number = 0.0f}}}}},
	});

	// The first tune of the second land's singing stones, Twinkle Twinkle Little Star, as the stones start it when the
	// player taps the tune. The script makes its mist and its bats at a fixed place of the second land.
	all.push_back({
	    .id = "story.singing_stones_twinkle",
	    .name = "The singing stones play Twinkle Twinkle Little Star",
	    .facet = Facet::Story,
	    .description = "Two seconds in, the game's TwinkleSequence script runs, as a tap of the tune on the second "
	                   "land's stones starts it. It is a cut scene with its own music and camera.",
	    .expected = "A flock of 10 bats comes about the ring, and a faint white mist shows at the ring's centre. The "
	                "camera moves in, then up, then back, and the game's time goes to 23:00 over 10 seconds. At the end "
	                "the mist grows from size 1 to 2 and fades away over 10 seconds, and the music stops.",
	    .environment = {.dispenserGrid = false, .landNumber = 2},
	    .framing = {.shot = Shot::Placed,
	                .eye = {k_Ring.x - 7.0f, 14.0f, k_Ring.y + 3.0f},
	                .look = {k_Ring.x + 9.0f, 10.0f, k_Ring.y - 1.0f}},
	    .commands = {{.kind = Kind::RunScript,
	                  .delaySeconds = 2.0f,
	                  .script = "TwinkleSequence",
	                  .scriptGlobals = {{"SSDoneBats", {.number = 0.0f}}}}},
	});
}

/// The beach temple puzzle of the second land, as its script makes it: turned a half turn, at its normal size
void AddBeachTemplePuzzle(std::vector<Scenario>& all)
{
	// The puzzle's place, from the middle of the map. The half turn puts its three bases in a row to the south.
	constexpr glm::vec2 k_Puzzle {0.0f, -40.0f};
	constexpr float k_PuzzleObjectType = 32.0f;
	constexpr float k_HanoiKind = 3.0f;
	all.push_back({
	    .id = "story.beach_temple_puzzle",
	    .name = "The rings of The Beach Temple Puzzle",
	    .facet = Facet::Story,
	    .description = "A script makes the second land's beach temple puzzle south of the middle, turned a half turn, as "
	                   "its quest makes it on the beach.",
	    .expected = "Three bases stand in a row from north to south, 20 apart. The four rings are stacked on the north "
	                "base, the largest at the bottom. The hand can take only the top ring. A ring that the hand drops "
	                "near a base or a larger ring flickers out and stands on that stack. A ring dropped far from a stack "
	                "flickers back to its place after six turns.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed,
	                .eye = {k_Puzzle.x + 35.0f, 25.0f, k_Puzzle.y - 20.0f},
	                .look = {k_Puzzle.x, 3.0f, k_Puzzle.y - 20.0f}},
	    .commands = {{.kind = Kind::CallNative,
	                  .delaySeconds = 0.5f,
	                  .script = "CREATE_WITH_ANGLE_AND_SCALE",
	                  .scriptArguments = {{.number = 180.0f},
	                                      {.number = 1.0f},
	                                      {.kind = ScriptArgument::Kind::Integer, .number = k_PuzzleObjectType},
	                                      {.kind = ScriptArgument::Kind::Integer, .number = k_HanoiKind},
	                                      {.kind = ScriptArgument::Kind::Position, .point = k_Puzzle}}}},
	});
	// A tiger stands by the puzzle for a check of a ring in a creature's hand
	all.push_back({
	    .id = "story.beach_temple_ring_in_a_creature_hand",
	    .name = "A ring of The Beach Temple Puzzle in a creature's hand",
	    .facet = Facet::Story,
	    .description = "A script makes the second land's beach temple puzzle south of the middle, turned a half turn. A "
	                   "content tiger stands 12 m east of the stack. Give the tiger the order creature.pick_up on the "
	                   "smallest ring.",
	    .expected = "The tiger reaches out and takes the top ring in its hand. From its next turn the puzzle sees the "
	                "ring where the tiger is. No base and no larger ring is within 10 of that place, so after a short "
	                "calm the ring goes out of the hand and back to the top of its stack.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed,
	                .eye = {k_Puzzle.x + 40.0f, 25.0f, k_Puzzle.y - 20.0f},
	                .look = {k_Puzzle.x + 6.0f, 3.0f, k_Puzzle.y + 2.0f}},
	    .creatures = {Content(CreatureType::Tiger, {k_Puzzle.x + 12.0f, k_Puzzle.y + 4.0f}, 270.0f)},
	    .commands = {{.kind = Kind::CallNative,
	                  .delaySeconds = 0.5f,
	                  .script = "CREATE_WITH_ANGLE_AND_SCALE",
	                  .scriptArguments = {{.number = 180.0f},
	                                      {.number = 1.0f},
	                                      {.kind = ScriptArgument::Kind::Integer, .number = k_PuzzleObjectType},
	                                      {.kind = ScriptArgument::Kind::Integer, .number = k_HanoiKind},
	                                      {.kind = ScriptArgument::Kind::Position, .point = k_Puzzle}}}},
	});
	// A script takes the smallest ring off the stack and puts it on open ground, then the game stops
	constexpr float k_MobileObjectType = 20.0f;
	constexpr float k_SmallestRing = 30.0f;
	constexpr glm::vec2 k_OpenGround {k_Puzzle.x + 14.0f, k_Puzzle.y};
	all.push_back({
	    .id = "story.beach_temple_ring_shadows",
	    .name = "The land shadows of The Beach Temple Puzzle",
	    .facet = Facet::Story,
	    .description = "A script makes the second land's beach temple puzzle, puts its smallest ring on open ground to "
	                   "the east of the stack, and stops the game.",
	    .expected = "The smallest ring lies on open ground and casts no shadow into the land. The three bases and the "
	                "largest ring cast their shadows. A ring that the puzzle puts back on a stack casts a shadow again.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed,
	                .eye = {k_Puzzle.x + 8.0f, 40.0f, k_Puzzle.y - 25.0f},
	                .look = {k_Puzzle.x + 8.0f, 0.0f, k_Puzzle.y}},
	    .commands = {{.kind = Kind::CallNative,
	                  .delaySeconds = 0.5f,
	                  .script = "CREATE_WITH_ANGLE_AND_SCALE",
	                  .scriptArguments = {{.number = 180.0f},
	                                      {.number = 1.0f},
	                                      {.kind = ScriptArgument::Kind::Integer, .number = k_PuzzleObjectType},
	                                      {.kind = ScriptArgument::Kind::Integer, .number = k_HanoiKind},
	                                      {.kind = ScriptArgument::Kind::Position, .point = k_Puzzle}}},
	                 {.kind = Kind::CallNative,
	                  .delaySeconds = 1.0f,
	                  .script = "CALL_NEAR",
	                  .scriptArguments = {{.kind = ScriptArgument::Kind::Integer, .number = k_MobileObjectType},
	                                      {.kind = ScriptArgument::Kind::Integer, .number = k_SmallestRing},
	                                      {.kind = ScriptArgument::Kind::Position, .point = k_Puzzle},
	                                      {.number = 5.0f},
	                                      {.kind = ScriptArgument::Kind::Integer, .number = 0.0f}}},
	                 {.kind = Kind::CallNative,
	                  .delaySeconds = 0.5f,
	                  .script = "SET_POSITION",
	                  .scriptArguments = {{.kind = ScriptArgument::Kind::LastResult},
	                                      {.kind = ScriptArgument::Kind::Position, .point = k_OpenGround}}},
	                 {.kind = Kind::PauseGame, .delaySeconds = 0.0f, .value = 1}},
	});
}

/// The tree puzzle of the second land, as a script makes it: not turned, at its normal size
void AddTreePuzzle(std::vector<Scenario>& all)
{
	// The puzzle's place, from the middle of the map. The spots of the grid are about ten metres apart.
	constexpr glm::vec2 k_Puzzle {0.0f, -40.0f};
	constexpr float k_PuzzleObjectType = 32.0f;
	constexpr float k_TreeKind = 1.0f;
	all.push_back({
	    .id = "story.tree_puzzle",
	    .name = "The trees of the second land's tree puzzle",
	    .facet = Facet::Story,
	    .description = "A script makes the second land's tree puzzle south of the middle: nine trees on a grid of three "
	                   "by three spots, with a lantern at each corner, mist over the spots and bats over the middle.",
	    .expected = "Nine small trees, conifers and oaks, grow to full size in about two seconds. A lantern stands one "
	                "step out from each corner. Bats fly over the middle spot. When the hand pulls a tree, that tree and "
	                "the trees of the spots next to it change kind and grow again from small.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed,
	                .eye = {k_Puzzle.x + 45.0f, 30.0f, k_Puzzle.y - 25.0f},
	                .look = {k_Puzzle.x + 10.0f, 3.0f, k_Puzzle.y + 10.0f}},
	    .commands = {{.kind = Kind::CallNative,
	                  .delaySeconds = 0.5f,
	                  .script = "CREATE_WITH_ANGLE_AND_SCALE",
	                  .scriptArguments = {{.number = 0.0f},
	                                      {.number = 1.0f},
	                                      {.kind = ScriptArgument::Kind::Integer, .number = k_PuzzleObjectType},
	                                      {.kind = ScriptArgument::Kind::Integer, .number = k_TreeKind},
	                                      {.kind = ScriptArgument::Kind::Position, .point = k_Puzzle}}}},
	});
	all.push_back({
	    .id = "story.tree_puzzle_deleted",
	    .name = "A script deletes the second land's tree puzzle",
	    .facet = Facet::Story,
	    .description = "A script makes the second land's tree puzzle south of the middle. Four seconds later the script "
	                   "deletes the puzzle game.",
	    .expected = "The nine trees grow to full size and a lantern stands at each corner. At the delete, the nine trees "
	                "and the four lanterns go at once. The mist over the spots and the bats over the middle stay.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed,
	                .eye = {k_Puzzle.x + 45.0f, 30.0f, k_Puzzle.y - 25.0f},
	                .look = {k_Puzzle.x + 10.0f, 3.0f, k_Puzzle.y + 10.0f}},
	    .commands = {{.kind = Kind::CallNative,
	                  .delaySeconds = 0.5f,
	                  .script = "CREATE_WITH_ANGLE_AND_SCALE",
	                  .scriptArguments = {{.number = 0.0f},
	                                      {.number = 1.0f},
	                                      {.kind = ScriptArgument::Kind::Integer, .number = k_PuzzleObjectType},
	                                      {.kind = ScriptArgument::Kind::Integer, .number = k_TreeKind},
	                                      {.kind = ScriptArgument::Kind::Position, .point = k_Puzzle}}},
	                 {.kind = Kind::CallNative,
	                  .delaySeconds = 4.0f,
	                  .script = "OBJECT_DELETE",
	                  .scriptArguments = {{.kind = ScriptArgument::Kind::LastResult},
	                                      {.kind = ScriptArgument::Kind::Integer, .number = 0.0f}}}},
	});

	// The third land's puzzle has three states. After its prize, its script finds each lantern of the puzzle by the
	// lantern row of the mobile statics, and deletes the lantern with a fade.
	constexpr float k_ThirdLandTreeKind = 2.0f;
	constexpr float k_MobileStaticType = 8.0f;
	constexpr float k_LanternRow = 7.0f;
	constexpr float k_LanternReach = 50.0f;
	constexpr float k_DeleteWithFade = 1.0f;
	constexpr glm::vec2 k_PuzzleMiddle {k_Puzzle.x + 10.0f, k_Puzzle.y + 10.0f};
	std::vector<Command> commands {{.kind = Kind::CallNative,
	                                .delaySeconds = 0.5f,
	                                .script = "CREATE_WITH_ANGLE_AND_SCALE",
	                                .scriptArguments = {{.number = 0.0f},
	                                                    {.number = 1.0f},
	                                                    {.kind = ScriptArgument::Kind::Integer, .number = k_PuzzleObjectType},
	                                                    {.kind = ScriptArgument::Kind::Integer, .number = k_ThirdLandTreeKind},
	                                                    {.kind = ScriptArgument::Kind::Position, .point = k_Puzzle}}}};
	constexpr int k_Lanterns = 4;
	// The first search comes four seconds after the start, and each next search one second after the delete before it
	for (int lantern = 0; lantern < k_Lanterns; ++lantern)
	{
		commands.push_back({.kind = Kind::CallNative,
		                    .delaySeconds = lantern == 0 ? 3.5f : 1.0f,
		                    .script = "CALL_NEAR",
		                    .scriptArguments = {{.kind = ScriptArgument::Kind::Integer, .number = k_MobileStaticType},
		                                        {.kind = ScriptArgument::Kind::Integer, .number = k_LanternRow},
		                                        {.kind = ScriptArgument::Kind::Position, .point = k_PuzzleMiddle},
		                                        {.number = k_LanternReach},
		                                        {.kind = ScriptArgument::Kind::Integer, .number = 0.0f}}});
		commands.push_back({.kind = Kind::CallNative,
		                    .delaySeconds = 0.0f,
		                    .script = "OBJECT_DELETE",
		                    .scriptArguments = {{.kind = ScriptArgument::Kind::LastResult},
		                                        {.kind = ScriptArgument::Kind::Integer, .number = k_DeleteWithFade}}});
	}
	all.push_back({
	    .id = "story.l3_tree_puzzle_lanterns",
	    .name = "The third land's tree puzzle loses its lanterns",
	    .facet = Facet::Story,
	    .description = "A script makes the third land's tree puzzle south of the middle. From four seconds, the script "
	                   "finds a lantern of the puzzle by its row each second and deletes it with a fade.",
	    .expected = "Nine small trees of three kinds grow to full size. A lantern stands one step out from each corner. "
	                "From four seconds, one lantern each second fades away, until no lantern stays.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed,
	                .eye = {k_Puzzle.x + 45.0f, 30.0f, k_Puzzle.y - 25.0f},
	                .look = {k_Puzzle.x + 10.0f, 3.0f, k_Puzzle.y + 10.0f}},
	    .commands = std::move(commands),
	});
}

/// The mazes of the fourth and fifth lands, as their scripts make them: not turned, at their normal size
void AddMazePuzzles(std::vector<Scenario>& all)
{
	// The place of each maze, from the middle of the map. A cell is four metres across, so the first Theseus maze and
	// the lion maze are 28 metres to a side.
	constexpr glm::vec2 k_Puzzle {-14.0f, -54.0f};
	constexpr glm::vec2 k_Middle {k_Puzzle.x + 14.0f, k_Puzzle.y + 14.0f};
	// The player's temple stands north of the maze, so that the hand reaches the maze in its influence
	constexpr glm::vec2 k_Temple {0.0f, 20.0f};
	constexpr float k_PuzzleObjectType = 32.0f;
	constexpr float k_FirstTheseusKind = 10.0f;
	constexpr float k_SecondTheseusKind = 11.0f;
	constexpr float k_LionMazeKind = 12.0f;
	const auto makeMaze = [&](float kind) {
		return Command {.kind = Kind::CallNative,
		                .delaySeconds = 0.5f,
		                .script = "CREATE_WITH_ANGLE_AND_SCALE",
		                .scriptArguments = {{.number = 0.0f},
		                                    {.number = 1.0f},
		                                    {.kind = ScriptArgument::Kind::Integer, .number = k_PuzzleObjectType},
		                                    {.kind = ScriptArgument::Kind::Integer, .number = kind},
		                                    {.kind = ScriptArgument::Kind::Position, .point = k_Puzzle}}};
	};
	const Framing above {
	    .shot = Shot::Placed, .eye = {k_Middle.x, 48.0f, k_Middle.y - 22.0f}, .look = {k_Middle.x, 0.0f, k_Middle.y}};
	all.push_back({
	    .id = "story.theseus_maze",
	    .name = "The first Theseus maze of the fourth land",
	    .facet = Facet::Story,
	    .description = "A script makes the fourth land's first Theseus maze south of the middle: fences on a grid of seven "
	                   "by seven cells, a palm tree at each corner, a lantern at the middle of each edge, a villager and "
	                   "a lion.",
	    .expected = "Fences stand on the walls of the maze. A sparkle shows the villager and the lion for one second. A tap "
	                "on the land in line with the villager moves the villager one cell that way, and then the lion goes "
	                "up to two cells towards the villager. A sparkle marks the tapped cell. When the lion catches the "
	                "villager, the two animals start again on their first cells. The hand can not take a fence or an "
	                "animal.",
	    .environment = {.dispenserGrid = false, .temple = k_Temple},
	    .framing = above,
	    .commands = {makeMaze(k_FirstTheseusKind)},
	});
	all.push_back({
	    .id = "story.theseus_maze_two",
	    .name = "The second Theseus maze of the fourth land",
	    .facet = Facet::Story,
	    .description = "A script makes the fourth land's second Theseus maze south of the middle: fences on a grid of "
	                   "eight by eight cells, with its villager and its lion.",
	    .expected = "Fences stand on the walls of the larger maze, which has its exit at the far end along. The villager "
	                "starts in the third column, the lion in the second. Taps move them as in the first maze.",
	    .environment = {.dispenserGrid = false, .temple = k_Temple},
	    .framing = above,
	    .commands = {makeMaze(k_SecondTheseusKind)},
	});
	all.push_back({
	    .id = "story.lion_maze",
	    .name = "The lion maze of the fifth land",
	    .facet = Facet::Story,
	    .description = "A script makes the fifth land's lion maze south of the middle: fences on a grid of seven by seven "
	                   "cells, a large sheep, a large wolf and five small bell towers around the maze.",
	    .expected = "A small bell tower stands at the middle of each edge and one at a corner. A tap on a bell at an edge "
	                "rings it with a sparkle, and the wolf turns and slides that way, cell after cell, until a fence "
	                "stops it. A tap on the bell at the corner puts the wolf and the sheep back on their first cells. "
	                "When the wolf stops on the cell of the sheep, the two animals go in a sparkle.",
	    .environment = {.dispenserGrid = false, .temple = k_Temple},
	    .framing = above,
	    .commands = {makeMaze(k_LionMazeKind)},
	});
	// The bell towers of the fourth land's Totem Puzzle are puzzle totems that a script makes
	constexpr float k_TotemObjectType = 34.0f;
	constexpr float k_BellTowerRow = 146.0f;
	all.push_back({
	    .id = "story.script_puzzle_totem",
	    .name = "A bell tower that a script makes as a puzzle totem",
	    .facet = Facet::Story,
	    .description = "A script makes a puzzle totem of the bell tower row south of the middle, turned a quarter turn, "
	                   "as the fourth land's Totem Puzzle makes its five towers.",
	    .expected = "A bell tower of full size stands on the land. It belongs to no town. A click of the hand on it counts "
	                "as a click for the scripts, but a tap does not ring it.",
	    .environment = {.dispenserGrid = false, .temple = k_Temple},
	    .framing = {.shot = Shot::Placed,
	                .eye = {k_Middle.x + 20.0f, 18.0f, k_Middle.y - 20.0f},
	                .look = {k_Middle.x, 4.0f, k_Middle.y}},
	    .commands = {{.kind = Kind::CallNative,
	                  .delaySeconds = 0.5f,
	                  .script = "CREATE_WITH_ANGLE_AND_SCALE",
	                  .scriptArguments = {{.number = 90.0f},
	                                      {.number = 1.0f},
	                                      {.kind = ScriptArgument::Kind::Integer, .number = k_TotemObjectType},
	                                      {.kind = ScriptArgument::Kind::Integer, .number = k_BellTowerRow},
	                                      {.kind = ScriptArgument::Kind::Position, .point = k_Middle}}}},
	});
}

void AddFourthLandArrival(std::vector<Scenario>& all)
{
	using Argument = ScriptArgument;
	// The fourth land's home village, from the middle of the map
	static constexpr glm::vec2 k_Land4Village {-314.42f, 102.99f};
	all.push_back({
	    .id = "story.l4_arrival",
	    .name = "Story: the player comes through the vortex to the fourth land",
	    .facet = Facet::Story,
	    .description = "The fourth land loads with its own scripts and sky, and the player's Tiger stands on it. Its set-up "
	                   "script runs, then its arrival script with the whole scene.",
	    .expected = "A vortex opens beside the home village and closes 23 seconds later; until it closes the Tiger points "
	                "at it again and again. With Nemesis's music the camera starts in the vortex looking down, rises 65 "
	                "metres in 10 seconds while the picture fades in over 6 seconds, then sweeps to the village. Five "
	                "storms with lightning stand near the village, and the sky moves to evening. The advisors speak "
	                "about the ruined land along a camera path, and both point when they speak about the gates and the "
	                "temple.",
	    .environment = {.land = Land::Story, .storyLand = "Land4.txt", .dispenserGrid = false},
	    .framing = {.shot = Shot::Overview,
	                .include = {{k_Land4Village.x - 60.0f, k_Land4Village.y - 60.0f},
	                            {k_Land4Village.x + 60.0f, k_Land4Village.y + 100.0f}}},
	    .creatures = {{.label = "player's tiger",
	                   .species = CreatureType::Tiger,
	                   .phase = 3,
	                   .needs = {.energy = 1.0f, .exhaustion = 0.0f, .dehydration = 0.0f, .poo = 0.0f, .life = 1.0f}}},
	    .commands = {{.kind = Kind::RunScript,
	                  .delaySeconds = 0.5f,
	                  .script = "SetupLand4",
	                  .scriptGlobals = {{"MyCreature", {.kind = Argument::Kind::Creature, .index = 0}}}},
	                 {.kind = Kind::RunScript,
	                  .delaySeconds = 0.5f,
	                  .script = "BeginLand4",
	                  .scriptArguments = {{.kind = ScriptArgument::Kind::Number, .number = 1.0f}}}},
	});
	all.push_back({
	    .id = "story.l4_aggressive_fireballs",
	    .name = "Story: the fourth land's fireballs come in an aggressive wave",
	    .facet = Facet::Story,
	    .description = "The fourth land loads with its own scripts and sky, and the player's Tiger stands on it. Its set-up "
	                   "script runs, then its arrival script without the scene, with the switch of the aggressive waves "
	                   "already on.",
	    .expected = "Five fireball throwers high in the sky to the north throw level one fireballs, one every 1 to 20 "
	                "seconds each. Two throwers aim at the edge of the home village, and three at their own points west "
	                "of it. Each fireball has two times the strength of a fireball of a plain wave. The log shows no "
	                "\"Cannot set strength of this\" message.",
	    .environment = {.land = Land::Story, .storyLand = "Land4.txt", .dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-720.0f, -130.0f}, {-300.0f, 350.0f}}},
	    .creatures = {{.label = "player's tiger",
	                   .species = CreatureType::Tiger,
	                   .phase = 3,
	                   .needs = {.energy = 1.0f, .exhaustion = 0.0f, .dehydration = 0.0f, .poo = 0.0f, .life = 1.0f}}},
	    .commands = {{.kind = Kind::RunScript,
	                  .delaySeconds = 0.5f,
	                  .script = "SetupLand4",
	                  .scriptGlobals = {{"MyCreature", {.kind = Argument::Kind::Creature, .index = 0}}}},
	                 {.kind = Kind::RunScript,
	                  .delaySeconds = 0.5f,
	                  .script = "BeginLand4",
	                  .scriptArguments = {{.kind = ScriptArgument::Kind::Number, .number = 0.0f}},
	                  .scriptGlobals = {{"FireMeteorWaveAggressive", {.number = 1.0f}}}}},
	});
}
} // namespace

void testbed_scenarios::AddStory(std::vector<Scenario>& all)
{
	// The player's temple west of the middle, turned as Land 1's
	constexpr glm::vec2 k_Temple {-300.0f, 100.0f};
	constexpr float k_TempleTurn = 36.0f;
	all.push_back({
	    .id = "story.game_over",
	    .name = "The game's end as the player's temple falls",
	    .facet = Facet::Story,
	    .description = "The player's temple stands west of the middle; two seconds in it starts being destroyed, as when "
	                   "its heart loses the last of its life, and the game's GameOver script plays.",
	    .expected = "The temple is destroyed and the script takes the camera round and up from it while the advisors leave. "
	                "The screen fades to black over 12 seconds, and \"Game Over\" fades in white over 3 seconds, centred in a "
	                "box from a quarter to three quarters across the screen and 45% to 55% down, its line 7.5% of the "
	                "screen high. Once the advisor's line is read the game's time stops, the words fade out over 5 "
	                "seconds, and the player is taken into the temple's save game room, kept there: leaving it ends the "
	                "game.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview,
	                .include = {k_Temple - glm::vec2 {40.0f}, k_Temple + glm::vec2 {40.0f}},
	                .distance = 1.0f},
	    .temples = {{.offset = k_Temple, .angle = k_TempleTurn}},
	    .commands = {{.kind = Kind::DestroyTemple, .delaySeconds = 2.0f, .value = 0}},
	});
	all.push_back({
	    .id = "temple.game_over_new_game_question",
	    .name = "Escape asks about a new game once the game is over",
	    .facet = Facet::Story,
	    .description = "The player's temple stands west of the middle; two seconds in it starts being destroyed, and the "
	                   "game's GameOver script plays. Once the script has taken the player into the save game room, press "
	                   "Escape, then Shift and Escape. Then click the door of the room.",
	    .expected = "Each Escape, with Shift, Ctrl or Alt too, shows \"Are you sure you want to start a completely new "
	                "game?\" in the box with a Yes and a No. Escape does not answer it and does not close it, and the "
	                "player stays in the room. No closes the box. A click on the door of the room shows the same box, and "
	                "the camera does not go back through the door. While the box shows, the room takes no press. Yes ends "
	                "the game, and a new game starts on the first land. Under a script's cinema bars, Escape does nothing.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview,
	                .include = {k_Temple - glm::vec2 {40.0f}, k_Temple + glm::vec2 {40.0f}},
	                .distance = 1.0f},
	    .temples = {{.offset = k_Temple, .angle = k_TempleTurn}},
	    .commands = {{.kind = Kind::DestroyTemple, .delaySeconds = 2.0f, .value = 0}},
	});
	all.push_back({
	    .id = "temple.heart_fade",
	    .name = "The temple's heart glows and fades out in its destruction",
	    .facet = Facet::Story,
	    .description = "The player's temple stands west of the middle, near the camera. Two seconds in, its destruction "
	                   "starts, as when its heart loses the last of its life.",
	    .expected = "From 5.5 seconds the temple glows in red, the player's colour, and the glow beats with the heartbeat. "
	                "From 16 seconds the temple fades out over four seconds. A little into the fade it writes no depth: "
	                "the land and the things behind it show through it, and its cut-out parts go first. From 16 seconds a "
	                "faint shell of light in red covers the heart, reaching out with the heartbeat and growing at the "
	                "end. Where the shell touches the heart it adds a faint grey. It is not drawn after 23.8 seconds, "
	                "just before it goes.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview,
	                .include = {k_Temple - glm::vec2 {25.0f}, k_Temple + glm::vec2 {25.0f}},
	                .distance = 1.0f},
	    .temples = {{.offset = k_Temple, .angle = k_TempleTurn}},
	    .commands = {{.kind = Kind::DestroyTemple, .delaySeconds = 2.0f, .value = 0}},
	});
	{
		// The scripts' delete with explode and with temple explode, as the later lands' scripts use them
		using Argument = ScriptArgument;
		const auto remove = [](float delay, Argument thing, float mode) {
			return Command {.kind = Kind::CallNative,
			                .delaySeconds = delay,
			                .script = "OBJECT_DELETE",
			                .scriptArguments = {thing, {.kind = Argument::Kind::Integer, .number = mode}}};
		};
		const Argument hut {.kind = Argument::Kind::Object, .index = 0};
		const Argument temple {.kind = Argument::Kind::Temple, .index = 0};
		all.push_back({
		    .id = "temple.script_breaks_up",
		    .name = "A script breaks up a hut and a rival's temple",
		    .facet = Facet::Story,
		    .description = "A rival's temple stands west of the middle with a hut in front of it. One second in, a script "
		                   "deletes the temple with temple explode. Two seconds later it deletes the hut with explode. 20 "
		                   "seconds after the first delete, it deletes the temple with temple explode again, as the last "
		                   "land's fight does.",
		    .expected = "The temple starts its destruction: the loop sound, the beams across it and the glow. The hut "
		                "breaks into pieces of its model that fly out from where it stood, and the hut is gone. At the second "
		                "delete, the model of the temple breaks into many pieces that fly out fast, while the temple "
		                "itself stays. Two seconds later the temple explodes and goes, as its destruction ends.",
		    .environment = {.dispenserGrid = false},
		    .framing = {.shot = Shot::Overview,
		                .include = {k_Temple - glm::vec2 {60.0f}, k_Temple + glm::vec2 {60.0f}},
		                .distance = 1.0f},
		    .objects = {{.type = AbodeInfo::CelticHut, .offset = k_Temple + glm::vec2 {0.0f, -45.0f}}},
		    .temples = {{.offset = k_Temple, .owner = PlayerNames::PLAYER_TWO, .angle = k_TempleTurn}},
		    .commands = {remove(1.0f, temple, 3.0f), remove(2.0f, hut, 2.0f), remove(18.0f, temple, 3.0f)},
		});
	}
	all.push_back({
	    .id = "temple.intro_movement",
	    .name = "Looking about the temple while the advisors introduce it",
	    .facet = Facet::Story,
	    .description = "The player's temple stands west of the middle; after a second the player goes into it for the "
	                   "first time, and the advisors' introduction of its main room plays. Turn and tilt the room with the "
	                   "camera's keys, move the mouse, and click and drag while they talk.",
	    .expected = "The camera flies in from the entrance to the main room by itself while the good and evil advisors "
	                "come out and talk through their nine lines. The hand is away under the cinema bars, "
	                "and the camera's keys turn and tilt the room as they do after the scene. Clicks and drags do nothing "
	                "until the scene is over: the flight isn't cut short and the room isn't dragged round.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview,
	                .include = {k_Temple - glm::vec2 {40.0f}, k_Temple + glm::vec2 {40.0f}},
	                .distance = 1.0f},
	    .temples = {{.offset = k_Temple, .angle = k_TempleTurn}},
	    // Room 3 is the main room, the one the entrance leads to
	    .commands = {{.kind = Kind::EnterTemple, .delaySeconds = 1.0f, .value = 3}},
	});
	all.push_back({
	    .id = "temple.entrance_pick",
	    .name = "The hand over the player's temple entrance",
	    .facet = Facet::Story,
	    .description = "The player's temple stands west of the middle, fully built. The pointer rests on the land beside "
	                   "it, then over the temple's walls, then over its entrance for a few seconds, and then the right "
	                   "mouse button is clicked there.",
	    .expected = "Over the land and the walls the hand offers nothing about the temple but its health. Over the "
	                "entrance it picks the entrance (not the temple behind it) and offers \"Enter Temple\" with the "
	                "right mouse button. The click takes the player inside, the camera flying in from the entrance.",
	    .environment = {.dispenserGrid = false},
	    // From the west, where its entrance faces, a little above the land
	    .framing = {.shot = Shot::Placed,
	                .eye = {k_Temple.x - 90.0f, 40.0f, k_Temple.y},
	                .look = {k_Temple.x, 10.0f, k_Temple.y}},
	    .temples = {{.offset = k_Temple, .angle = k_TempleTurn}},
	    .commands = {{.kind = Kind::PointerTo, .delaySeconds = 2.0f, .point = {0.2f, 0.8f}},
	                 {.kind = Kind::PointerTo, .delaySeconds = 3.0f, .point = {0.5f, 0.36f}},
	                 {.kind = Kind::PointerTo, .delaySeconds = 3.0f, .point = {0.447f, 0.604f}},
	                 {.kind = Kind::PointerPress, .delaySeconds = 4.0f, .value = 3},
	                 {.kind = Kind::PointerRelease, .delaySeconds = 0.1f, .value = 3}},
	});
	all.push_back({
	    .id = "temple.damage_beam",
	    .name = "A hurt temple's heart beaming across itself",
	    .facet = Facet::Story,
	    .description = "The player's temple stands west of the middle with its heart at 30% of its life and no town to "
	                   "pass blows on to.",
	    .expected = "Every 1.2 seconds (12 turns) a plasma beam leaps between two points of the heart's model, leaving "
	                "and arriving along the model's surface, lasting 1.18 seconds and fairly faint (alpha 76 of 255). "
	                "Each beam sounds one of the five heart beam sounds in turn, from the heart.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview,
	                .include = {k_Temple - glm::vec2 {40.0f}, k_Temple + glm::vec2 {40.0f}},
	                .distance = 1.0f},
	    .temples = {{.offset = k_Temple, .angle = k_TempleTurn, .life = 0.3f}},
	});
	all.push_back({
	    .id = "temple.low_detail_skin",
	    .name = "The temple's skin at the lowest detail levels",
	    .facet = Facet::Story,
	    .description = "Start the game with --detail-level 1 (Low) or 0 (Minimum). The player's temple stands west of "
	                   "the middle, with the player a little evil, so that its skin is a blend of the evil and the neutral "
	                   "pictures. Start it again with --detail-level 4 to compare.",
	    .expected = "At Low and Minimum the temple's skin is at half size and looks a little sharper and more broken "
	                "than a smooth half picture: each texel is one texel of each picture, not the mean of four. The "
	                "patterns and the colours are at the same places as at the higher levels.",
	    .environment = {.dispenserGrid = false, .playerAlignment = -0.4f},
	    .framing = {.shot = Shot::Overview,
	                .include = {k_Temple - glm::vec2 {30.0f}, k_Temple + glm::vec2 {30.0f}},
	                .distance = 1.0f},
	    .temples = {{.offset = k_Temple, .angle = k_TempleTurn}},
	});
	all.push_back({
	    .id = "audio.temple_heartbeat",
	    .name = "The heartbeat heard from the player's temple",
	    .facet = Facet::Story,
	    .description = "The player's temple stands west of the middle, fully built, with no danger to the player's "
	                   "people. The camera looks at it from less than 140 m.",
	    .expected = "From the first turn a heartbeat sounds from the temple without a stop, heard to 500 m and fully to a "
	                "third of that. It plays at 30% of its rate, as the heart is calm, and the temple's column of light "
	                "throbs with the same beat. It starts only once: it is not heard twice over. It starts only with the "
	                "camera within 140 m of the temple.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview,
	                .include = {k_Temple - glm::vec2 {25.0f}, k_Temple + glm::vec2 {25.0f}},
	                .distance = 1.0f},
	    .temples = {{.offset = k_Temple, .angle = k_TempleTurn}},
	});
	// The player's temple of the second land with its Norse town, an Indian town that the player won and the Indian town
	// nearest the temple, at the same distances and directions from the temple as on the second land
	constexpr std::string_view k_SecondLandSites = R"(CREATE_TOWN(950, "-129.73,154.60", "PLAYER_ONE", 7, "NORSE")
SET_TOWN_BELIEF(950, "PLAYER_ONE", 1.0)
CREATE_ABODE(950, "-139.73,164.60", "NORSE_ABODE_B", 0, 1000, 0, 0)
CREATE_VILLAGER_POS("-135.73,160.60", "-139.73,164.60", "NORSE_FORESTER", 20)
CREATE_VILLAGER_POS("-135.73,160.60", "-139.73,164.60", "NORSE_HOUSEWIFE", 20)
CREATE_TOWN(951, "-599.77,-138.91", "PLAYER_ONE", 4, "INDIAN")
SET_TOWN_BELIEF(951, "PLAYER_ONE", 1.0)
CREATE_ABODE(951, "-609.77,-128.91", "INDIAN_ABODE_A", 0, 1000, 0, 0)
CREATE_VILLAGER_POS("-605.77,-132.91", "-609.77,-128.91", "INDIAN_FORESTER", 20)
CREATE_VILLAGER_POS("-605.77,-132.91", "-609.77,-128.91", "INDIAN_HOUSEWIFE", 20)
CREATE_TOWN(952, "-498.68,206.42", "NEUTRAL", 4, "INDIAN")
CREATE_ABODE(952, "-508.68,216.42", "INDIAN_ABODE_A", 0, 1000, 0, 0)
CREATE_VILLAGER_POS("-504.68,212.42", "-508.68,216.42", "INDIAN_FORESTER", 20)
CREATE_CITADEL("0.0,0.0", 0, "PLAYER_ONE", 4254, 1000)
)";
	all.push_back({
	    .id = "worship.site_places_second_land",
	    .name = "The places of the player's worship sites around the temple of the second land",
	    .facet = Facet::Story,
	    .description = "Lines of a land put the player's temple, turned as on the second land, in the middle. The player "
	                   "has a Norse town to the north-west and an Indian town far to the west. A neutral Indian town is "
	                   "the Indian town nearest the temple. Each town of the player gets its site as the land is laid "
	                   "out, and the sites are built at once.",
	    .expected = "The Norse site takes the first place around the temple, with its altar about 28 m west of the "
	                "temple. The Indian site takes the next place to the south, with its altar 18 m west and 22 m south "
	                "of the temple: there the scripts of the second land look for the altar of the site.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview,
	                .include = {glm::vec2 {-60.0f, -50.0f}, glm::vec2 {30.0f, 40.0f}},
	                .distance = 1.0f},
	    .landScript = k_SecondLandSites,
	    .commands = {{.kind = Kind::BuildWorshipSites, .delaySeconds = 0.5f}},
	});
	AddBreederOffers(all);
	AddPiedPiperRound(all);
	AddOgreLair(all);
	AddGuideStorm(all);
	AddRivalCreatureLoads(all);
	AddThirdLandPrison(all);
	AddThirdLandThrowBloke(all);
	AddThirdLandWaterfall(all);
	AddSingingStones(all);
	AddLandHaze(all);
	AddBeachTemplePuzzle(all);
	AddTreePuzzle(all);
	AddMazePuzzles(all);
	AddFourthLandArrival(all);
}

void testbed_scenarios::AddExplorersBoat(std::vector<Scenario>& all)
{
	// The dry dock on the testbed lake's east bank, so that the boat is pushed west into the water
	constexpr glm::vec2 k_Dock {58.0f, 220.0f};
	all.push_back({
	    .id = "story.explorers_boat",
	    .name = "The explorers' boat pushed into the sea and sailing away",
	    .facet = Facet::Story,
	    .description = "The film the Land 1 silver scroll of the explorers ends with, played on the testbed after a second: "
	                   "its dry dock stands on the lake's east bank and its sailing starts in the lake's north-east corner "
	                   "(on Land 1 the boat sails out to sea, 425 metres west of the beach).",
	    .expected = "Five sailors push the boat for three seconds, then it slides west down the bank into the lake with a "
	                "creak, a splash and a big splash, throwing up brown sand and then white water; the sailors stop "
	                "pushing as it goes, three worn out and two cheering. About eighteen seconds in it is at sea: two cows "
	                "eat beside a pile of grain, a man and a woman stand arms out at the bow, two sailors sit swinging "
	                "their legs, one scrubs the deck, five wake sprites run back from it, and it sails south west at five "
	                "metres a second for a minute, then is gone. Out of the lake it sails under the testbed's flat land.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview,
	                .include = {k_Dock - glm::vec2 {110.0f, 70.0f}, k_Dock + glm::vec2 {10.0f, 60.0f}},
	                .distance = 1.0f},
	    .commands = {{.kind = Kind::PlayBoatFilm, .delaySeconds = 1.0f, .point = k_Dock}},
	});
	all.push_back({
	    .id = "story.explorers_boat_deck_light",
	    .name = "The deck of the explorers' boat in the light of the boat's place",
	    .facet = Facet::Story,
	    .description = "The same film as story.explorers_boat, framed on the lake that the boat crosses at sea. Each thing "
	                   "on the deck takes the land's light of the place of the boat, and no colour of the land.",
	    .expected = "About eighteen seconds in, the boat sails across the lake. The cows, the grain and the six people "
	                "on its deck show the same light as the boat at its middle, also where they stand over the bank. "
	                "No colour of the land under a part shows on it.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview,
	                .include = {k_Dock - glm::vec2 {170.0f, 150.0f}, k_Dock - glm::vec2 {40.0f, 20.0f}},
	                .distance = 1.0f},
	    .commands = {{.kind = Kind::PlayBoatFilm, .delaySeconds = 1.0f, .point = k_Dock}},
	});
}
