/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TestbedScenarioRegistry.h"

#include <cmath>

#include <algorithm>
#include <array>
#include <iterator>
#include <numbers>

#include <SDL_scancode.h>
#include <fmt/format.h>
#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>

#include "3D/FlatLand.h"
#include "3D/TempleInteriorInterface.h"
#include "Common/PortableMath.h"
#include "Creature/CreatureCave.h"
#include "Creature/CreatureFace.h"
#include "Creature/CreatureFeedback.h"
#include "Creature/CreatureFight.h"
#include "Creature/CreatureIdleMind.h"
#include "Creature/CreatureLayers.h"
#include "Creature/CreatureObjectActions.h"
#include "ECS/IncomingMessageRules.h"
#include "Input/BindableActions.h"
#include "Input/PlatformKeys.h"
#include "Particles/ParticleTypes.h"
#include "ScriptHeaders/ScriptEnums.h"
#include "Sound/GameMusic.h"
#include "Sound/SoundId.h"
#include "TestbedDispenserGrid.h"
#include "TestbedScenarioBuilders.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

// How to add a scenario: append one to the list Build() returns, in the section of its facet. Give it a new id
// ("facet.what", never reused or renamed, as tests and the command line pick scenarios by it), a name, its facet, a
// description of what it sets up and what to look for, and then its data: the environment (land, hour, weather, body
// time), the framing, the creatures with the bodies, needs and desires they start with, the objects on the land, and
// the commands, which play in turn (commands on objects name them by their place in the scenario's objects). Offsets are from
// the middle of the map, x east and y north; the testbed's camera looks north from 120 units south of the middle, and the lake
// lies beyond the middle (k_Lake, its open water 100 units a side, ringed by 20 units of shallows and a bank). Keep other
// scenarios clear of it. A new facet goes into the Facet enum and its Name. test_testbed_scenarios
// checks the data of every scenario; anything a scenario needs that the runner can't yet do goes into these types and
// TestbedScenarioRunner together.

namespace
{
/// The hand of the player and the creature of a rival god: the hand holds it only after a script makes the two gods
/// allies
void AddAlliedCreatureHold(std::vector<Scenario>& all)
{
	auto rival = Content(CreatureType::Tiger, {15.0f, 0.0f}, 270.0f, "ally's");
	rival.owner = PlayerNames::PLAYER_TWO;
	all.push_back({
	    .id = "hand.allied_creature",
	    .name = "The hand holds the creature of an allied god",
	    .facet = Facet::Hand,
	    .description = "The player's cow and the tiger of a rival god stand 30 apart. The hand tries to hold the tiger "
	                   "and stroke its head. Then a script makes the two gods allies, and the hand holds the tiger and "
	                   "strokes its head again.",
	    .expected = "At the first try the hand does not hold the tiger, and the log shows that the hand can not hold "
	                "it. After the alliance the hand holds the tiger, the camera turns to it, and the hand strokes its "
	                "head. Then the hand lets go and the camera goes back.",
	    .environment = {.dispenserGrid = false, .players = {PlayerNames::PLAYER_TWO}},
	    .framing = {.shot = Shot::Overview, .distance = 0.5f},
	    .creatures = {Content(CreatureType::Cow, {-15.0f, 0.0f}, 90.0f, "player's"), rival},
	    .commands = {Stroke(1, creature_feedback::BodyPart::Head, 1.0f),
	                 Act(Kind::HandLetGo, 1, 1.0f),
	                 {.kind = Kind::CallNative,
	                  .delaySeconds = 1.0f,
	                  .script = "SET_PLAYER_ALLY",
	                  .scriptArguments = {{.kind = ScriptArgument::Kind::Number, .number = 1.0f},
	                                      {.kind = ScriptArgument::Kind::Number, .number = 2.0f},
	                                      {.kind = ScriptArgument::Kind::Number, .number = 1.0f}}},
	                 Stroke(1, creature_feedback::BodyPart::Head, 1.0f),
	                 Act(Kind::HandLetGo, 1, 3.0f)},
	});
}

void AddHand(std::vector<Scenario>& all)
{
	using creature_feedback::BodyPart;
	AddAlliedCreatureHold(all);
	// Every part of the body, the head first
	std::vector<Command> strokes;
	for (size_t part = 0; part < creature_feedback::k_BodyPartCount; ++part)
	{
		strokes.push_back(Stroke(0, static_cast<BodyPart>(part), 1.0f));
	}
	strokes.push_back(Act(Kind::HandLetGo, 0, 1.0f, true));
	all.push_back({
	    .id = "hand.stroke",
	    .name = "Stroking each part of the body",
	    .facet = Facet::Hand,
	    .description = "The hand rests on a tiger facing the camera and strokes its head, armpits, belly, groin, feet and "
	                   "hands in turn, then lets go.",
	    .expected = "Each stroke plays the pleased animation for its part, mirrored for the left side, and its face; the "
	                "panel's reward climbs a tenth a stroke to Good Boy! 90%, and letting go warms it to the player.",
	    .framing = {.shot = Shot::Follow, .distance = 1.2f},
	    .creatures = {Posed(CreatureType::Tiger, {0.0f, 0.0f})},
	    .commands = strokes,
	    .repeatFrom = 0,
	});

	all.push_back({
	    .id = "hand.slap",
	    .name = "Slapping gently and hard, high and low",
	    .facet = Facet::Hand,
	    .description = "The hand slaps a tiger facing the camera on the head, the waist and the feet, gently and then "
	                   "hard, from either side, then lets go.",
	    .expected = "Each slap plays its reeling animation for its height, the gentle ones softer, the sweeps to the "
	                "right mirrored; the panel's reward falls to Bad Boy! 100%, and letting go cools it to the player.",
	    .framing = {.shot = Shot::Follow, .distance = 1.2f},
	    .creatures = {Posed(CreatureType::Tiger, {0.0f, 0.0f})},
	    .commands = {Slap(0, 0.85f, true, false, 1.0f), Slap(0, 0.55f, true, true, 1.0f), Slap(0, 0.2f, true, false, 1.0f),
	                 Slap(0, 0.85f, false, true, 1.0f), Slap(0, 0.55f, false, false, 1.0f), Slap(0, 0.2f, false, true, 1.0f),
	                 Act(Kind::HandLetGo, 0, 1.0f, true)},
	    .repeatFrom = 0,
	});

	// The real hand on the body: the pointer swept fast across the middle of the screen, at one height, four times
	std::vector<Command> sweeps {{.kind = Kind::PointerPress, .delaySeconds = 1.0f, .value = 3},
	                             {.kind = Kind::PointerSweep, .delaySeconds = 1.0f, .point = {-0.1f, 0.0f}, .amount = 0.3f}};
	for (int sweep = 0; sweep < 4; ++sweep)
	{
		const float across = sweep % 2 == 0 ? 0.2f : -0.2f;
		sweeps.push_back({.kind = Kind::PointerSweep, .delaySeconds = 2.5f, .point = {across, 0.0f}, .amount = 0.01f});
	}
	sweeps.push_back({.kind = Kind::PointerRelease, .delaySeconds = 1.0f, .value = 3});
	// Its life is not held, so that the hundredth a bruise takes stays off
	auto slapped = Posed(CreatureType::Tiger, {0.0f, 0.0f});
	slapped.needs.life.reset();
	all.push_back({
	    .id = "hand.bruise",
	    .name = "Hard slaps bruise",
	    .facet = Facet::Hand,
	    .description = "The hand holds the player's tiger, which faces the camera, by its middle, and sweeps fast across "
	                   "the middle of the screen four times at the same height, then lets go.",
	    .expected = "Each sweep is a hard slap at the waist. The third hard slap in a row leaves a dark bruise on the "
	                "skin where the line of sight through the hand meets the body, and the tiger's life falls by a "
	                "hundredth. The fourth slap leaves no bruise.",
	    .environment = {.dispenserGrid = false, .cursor = glm::vec2(0.5f, 0.5f)},
	    .framing = {.shot = Shot::Follow, .distance = 1.2f},
	    .creatures = {slapped},
	    .commands = sweeps,
	});

	// The creature that the recorded input of the hand strokes and slaps for the dedicated server's check
	// (tools/sim/hand_inputs/strokes.json). The creature help is on: in a network game every computer counts the time
	// that the reported hand stays over the creature, and the turn sets the look of the held creature.
	all.push_back({
	    .id = "hand.recorded_strokes",
	    .name = "A tiger for the recorded strokes and slaps",
	    .facet = Facet::Hand,
	    .description = "The player's tiger faces the camera in the middle of the screen. The "
	                   "dedicated server and the game both play this scenario, and the game makes the recorded input of "
	                   "tools/sim/hand_inputs/strokes.json: the hand holds the tiger, strokes it, slaps it and lets go.",
	    .expected = "The strokes play the pleased animations and the slaps the reeling ones. In a network game each "
	                "reaction plays at the next turn, on every computer at the same turn.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Follow, .distance = 1.2f},
	    .creatures = {Posed(CreatureType::Tiger, {0.0f, 0.0f})},
	});

	// The carried leash round the wrist while the hand strokes and slaps: slow sweeps stroke, fast sweeps slap
	std::vector<Command> collarHand {Leash(0, LeashType::Rope, 0.5f),
	                                 Leash(0, LeashType::Evil, 0.5f),
	                                 Leash(0, LeashType::Good, 0.5f),
	                                 Act(Kind::TakeOffLeash, 0, 0.5f),
	                                 {.kind = Kind::TapLeashPost, .creature = 0, .delaySeconds = 1.0f, .value = 1},
	                                 {.kind = Kind::PointerPress, .delaySeconds = 1.5f, .value = 3}};
	for (int stroke = 0; stroke < 4; ++stroke)
	{
		const float down = stroke % 2 == 0 ? 0.08f : -0.08f;
		collarHand.push_back({.kind = Kind::PointerSweep, .delaySeconds = 1.5f, .point = {0.0f, down}, .amount = 1.2f});
	}
	for (int sweep = 0; sweep < 4; ++sweep)
	{
		const float across = sweep % 2 == 0 ? 0.2f : -0.2f;
		collarHand.push_back({.kind = Kind::PointerSweep, .delaySeconds = 2.0f, .point = {across, 0.0f}, .amount = 0.01f});
	}
	collarHand.push_back({.kind = Kind::PointerRelease, .delaySeconds = 2.0f, .value = 3});
	all.push_back({
	    .id = "hand.carried_leash_touch",
	    .name = "The carried leash on the stroking and slapping hand",
	    .facet = Facet::Hand,
	    .description = "Your tiger faces the camera and knows all three leashes. The learning leash is picked at the "
	                   "temple, so the hand carries it. The hand holds the tiger by its middle, strokes it up and down "
	                   "slowly four times, then sweeps fast across the middle of the screen four times, then lets go.",
	    .expected = "The twisted rope collar stays round the wrist of the hand all the time. It moves and turns with the "
	                "wrist as the hand rests on the body, strokes and slaps, and it stays in scale with the hand.",
	    .environment = {.dispenserGrid = false, .cursor = glm::vec2(0.5f, 0.5f)},
	    .framing = {.shot = Shot::Follow, .distance = 1.2f},
	    .creatures = {Posed(CreatureType::Tiger, {0.0f, 0.0f})},
	    .temples = {{.offset = {-60.0f, 60.0f}, .angle = 36.0f}},
	    .commands = collarHand,
	});

	// The real hand on the body: a stroke on one part plays a long pleased animation, then the hand strokes another part
	// before that animation is far enough through
	std::vector<Command> refusedStroke {
	    {.kind = Kind::PointerPress, .delaySeconds = 1.0f, .value = 3},
	    {.kind = Kind::PointerSweep, .delaySeconds = 1.5f, .point = {-0.1f, 0.0f}, .amount = 0.3f},
	    {.kind = Kind::PointerSweep, .delaySeconds = 0.6f, .point = {0.2f, 0.0f}, .amount = 0.6f}};
	for (int wiggle = 0; wiggle < 16; ++wiggle)
	{
		const float across = wiggle % 2 == 0 ? 0.02f : -0.02f;
		refusedStroke.push_back({.kind = Kind::PointerSweep, .delaySeconds = 0.35f, .point = {across, 0.0f}, .amount = 0.3f});
	}
	refusedStroke.push_back({.kind = Kind::PointerRelease, .delaySeconds = 2.0f, .value = 3});
	all.push_back({
	    .id = "hand.refused_stroke_face",
	    .name = "A stroke that the creature refuses pulls only the face",
	    .facet = Facet::Hand,
	    .description = "The hand holds the player's tiger, which faces the camera, by its middle. It strokes one part of "
	                   "the body, then moves to another part and strokes it while the first pleased animation plays.",
	    .expected =
	        "The first stroke plays a pleased animation and its face. A stroke on the other part that comes before "
	        "that animation is far enough through pulls only its face, at once: the tiger plays no animation for it and "
	        "the hand sends nothing. The log shows \"only this computer shows its face\". In a network game "
	        "the other computers do not show the face of a stroke.",
	    .environment = {.dispenserGrid = false, .cursor = glm::vec2(0.5f, 0.5f)},
	    .framing = {.shot = Shot::Follow, .distance = 1.2f},
	    .creatures = {Posed(CreatureType::Tiger, {0.0f, 0.0f})},
	    .commands = refusedStroke,
	});

	all.push_back({
	    .id = "hand.slap_after_stroke",
	    .name = "A slap soon after a stroke",
	    .facet = Facet::Hand,
	    .description = "A tiger faces the camera and the hand is free. Hold the action button on the tiger, stroke it once, "
	                   "then sweep the hand fast across the middle of the screen.",
	    .expected = "The stroke plays the pleased animation. The fast sweep plays a reeling animation and the reward falls, "
	                "with no wait after the stroke, once the stroke animation is far enough through.",
	    .framing = {.shot = Shot::Follow, .distance = 1.2f},
	    .creatures = {Posed(CreatureType::Tiger, {0.0f, 0.0f})},
	});

	// The real button on the creature: a click, and then a hold with slow strokes. The draw of the hand sends the leash
	// tap and the feedback in the frame of each release, after the send of that frame.
	std::vector<Command> clickThenStroke {{.kind = Kind::PointerPress, .delaySeconds = 1.0f, .value = 3},
	                                      {.kind = Kind::PointerRelease, .delaySeconds = 0.2f, .value = 3},
	                                      {.kind = Kind::PointerPress, .delaySeconds = 2.0f, .value = 3}};
	for (int stroke = 0; stroke < 4; ++stroke)
	{
		const float down = stroke % 2 == 0 ? 0.08f : -0.08f;
		clickThenStroke.push_back({.kind = Kind::PointerSweep, .delaySeconds = 1.5f, .point = {0.0f, down}, .amount = 1.2f});
	}
	clickThenStroke.push_back({.kind = Kind::PointerRelease, .delaySeconds = 2.0f, .value = 3});
	all.push_back({
	    .id = "hand.click_then_stroke_draw",
	    .name = "A click and a stroke send from the draw of the hand",
	    .facet = Facet::Hand,
	    .description = "Your tiger faces the camera in the middle of the screen. The action button clicks the tiger. Then "
	                   "the button holds the tiger, the hand strokes it up and down slowly four times, and lets go.",
	    .expected = "The click puts the leash on the tiger. The hold strokes the tiger, and the tiger feels the strokes "
	                "when the hand lets go. The log shows \"The draw of the hand sends the leash tap\" for the click and "
	                "\"The draw of the hand sends the feedback\" for each release with a pose on the tiger. In a game "
	                "alone each command does its work one turn after the turn of the release.",
	    .environment = {.dispenserGrid = false, .cursor = glm::vec2(0.5f, 0.5f)},
	    .framing = {.shot = Shot::Follow, .distance = 1.2f},
	    .creatures = {Posed(CreatureType::Tiger, {0.0f, 0.0f})},
	    .commands = clickThenStroke,
	});

	// The real button on the creature: a fast sweep at once after the press, before the hand has its pose, then slow
	// strokes and fast sweeps once it has its pose
	std::vector<Command> poseThenTouch {
	    {.kind = Kind::PointerPress, .delaySeconds = 1.0f, .value = 3},
	    {.kind = Kind::PointerSweep, .delaySeconds = 0.0f, .point = {0.2f, 0.0f}, .amount = 0.01f},
	    {.kind = Kind::PointerSweep, .delaySeconds = 0.05f, .point = {-0.2f, 0.0f}, .amount = 0.3f}};
	for (int stroke = 0; stroke < 4; ++stroke)
	{
		const float down = stroke % 2 == 0 ? 0.08f : -0.08f;
		poseThenTouch.push_back({.kind = Kind::PointerSweep, .delaySeconds = 1.5f, .point = {0.0f, down}, .amount = 1.2f});
	}
	for (int sweep = 0; sweep < 2; ++sweep)
	{
		const float across = sweep % 2 == 0 ? 0.2f : -0.2f;
		poseThenTouch.push_back({.kind = Kind::PointerSweep, .delaySeconds = 2.5f, .point = {across, 0.0f}, .amount = 0.01f});
	}
	poseThenTouch.push_back({.kind = Kind::PointerRelease, .delaySeconds = 2.5f, .value = 3});
	all.push_back({
	    .id = "hand.pose_then_strokes_and_slaps",
	    .name = "The hand strokes and slaps only in its pose",
	    .facet = Facet::Hand,
	    .description = "Your tiger faces the camera in the middle of the screen. The action button holds the tiger, and at "
	                   "once the hand sweeps fast across the middle of the screen. Then the hand strokes the tiger up and "
	                   "down slowly four times, sweeps fast across it two times, and lets go.",
	    .expected = "Until the world holds the tiger, the hand stays the usual hand and the camera stays where it is. The "
	                "first fast sweep is not a slap. The camera then turns to the tiger. When the tiger faces you, the hand "
	                "takes its pose on the body. The strokes and the two late sweeps count. The log shows \"The camera of "
	                "the hold turns\" and then \"The hand takes its pose\".",
	    .environment = {.dispenserGrid = false, .cursor = glm::vec2(0.5f, 0.5f)},
	    .framing = {.shot = Shot::Follow, .distance = 1.2f},
	    .creatures = {Posed(CreatureType::Tiger, {0.0f, 0.0f})},
	    .commands = poseThenTouch,
	});

	// The real button on the own creature while the leash is held loose: a short press is a tap, a long one holds
	all.push_back({
	    .id = "hand.loose_leash_tap_then_hold",
	    .name = "With the leash held loose, a short press taps your creature and a long one holds it",
	    .facet = Facet::Hand,
	    .description = "Your tiger faces the camera in the middle of the screen and wears the learning leash, held loose "
	                   "in the hand. The action button presses the tiger for 0.1 seconds. Then it presses the tiger again, "
	                   "holds it for two seconds and lets go.",
	    .expected = "The short press is a tap: the hand does not hold the tiger, the camera stays where it is and the "
	                "leash stays on. The long press holds the tiger after a short wait, and the camera turns to it. The log "
	                "shows \"let go soon: it is a tap\" and then \"waited long enough: the hand holds it\".",
	    .environment = {.dispenserGrid = false, .cursor = glm::vec2(0.5f, 0.5f)},
	    .framing = {.shot = Shot::Follow, .distance = 1.2f},
	    .creatures = {Posed(CreatureType::Tiger, {0.0f, 0.0f})},
	    .commands = {Leash(0, LeashType::Rope, 0.5f),
	                 {.kind = Kind::PointerPress, .delaySeconds = 2.0f, .value = 3},
	                 {.kind = Kind::PointerRelease, .delaySeconds = 0.1f, .value = 3},
	                 {.kind = Kind::PointerPress, .delaySeconds = 2.0f, .value = 3},
	                 {.kind = Kind::PointerRelease, .delaySeconds = 2.0f, .value = 3}},
	});

	// A tap of the hand on the own creature with the aggression leash held loose: the creature acts on itself, which on
	// the aggression leash is a fight with itself
	all.push_back({
	    .id = "leash.tap_itself_aggression",
	    .name = "A tap on your creature with the aggression leash tells it to fight itself",
	    .facet = Facet::Leash,
	    .description = "Your tiger faces the camera in the middle of the screen and wears the aggression leash, held loose "
	                   "in the hand. The action button taps the tiger for 0.1 seconds.",
	    .expected = "The tap is an order on the tiger itself. The tiger nods, the order ring shows above it and the help "
	                "tells of its anger. It looks down at itself and stands still for some turns, then it stops the order "
	                "and decides again by itself. The leash stays on and the hand does not hold the tiger. The log shows "
	                "\"is told to act on\" with the tiger as the thing, then \"is the partner of creature\" with the "
	                "tiger as its own partner.",
	    .environment = {.dispenserGrid = false, .cursor = glm::vec2(0.5f, 0.5f)},
	    .framing = {.shot = Shot::Follow, .distance = 1.2f},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f})},
	    .commands = {Leash(0, LeashType::Evil, 0.5f),
	                 {.kind = Kind::PointerPress, .delaySeconds = 2.0f, .value = 3},
	                 {.kind = Kind::PointerRelease, .delaySeconds = 0.1f, .value = 3}},
	});

	// The real button on the own creature while it sleeps: a sleeping creature takes hold. Its tiredness stays high, so
	// it does not wake by itself.
	auto sleepy = Content(CreatureType::Tiger, {0.0f, 0.0f});
	sleepy.needs.exhaustion = 0.75f;
	all.push_back({
	    .id = "hand.press_holds_sleeping_creature",
	    .name = "The hand holds your sleeping creature",
	    .facet = Facet::Hand,
	    .description = "Your tiger lies down to sleep in the middle of the screen. The pointer is on the tiger where "
	                   "it lies. The action button presses the tiger, holds it for three seconds and lets go.",
	    .expected = "The hand holds the sleeping tiger as it holds an awake one: the camera turns to the tiger and the "
	                "tiger turns to face you. The leash does not come on. The log shows \"The hand of player 0 holds "
	                "creature\".",
	    .environment = {.dispenserGrid = false, .cursor = glm::vec2(0.4375f, 0.625f)},
	    .framing = {.shot = Shot::Follow, .distance = 1.2f},
	    .creatures = {sleepy},
	    .commands = {Act(Kind::Sleep, 0, 0.5f),
	                 {.kind = Kind::PointerPress, .delaySeconds = 6.0f, .value = 3},
	                 {.kind = Kind::PointerRelease, .delaySeconds = 3.0f, .value = 3}},
	});

	// The real button on the own creature while it is frozen: the hand does not hold it and the press sends no leash click
	all.push_back({
	    .id = "hand.press_frozen_creature_keeps_leash",
	    .name = "A press on your frozen creature leaves its leash on",
	    .facet = Facet::Hand,
	    .description = "Your tiger faces the camera in the middle of the screen and wears the learning leash, held loose "
	                   "in the hand. The freeze spell freezes it. The action button clicks the tiger, then presses it for "
	                   "two seconds.",
	    .expected = "The hand does not hold the frozen tiger and the camera stays where it is. The leash stays on after "
	                "the click and after the long press: a rope still runs from the hand to the collar. The log shows no "
	                "\"Leash command: player 0 taps a creature\".",
	    .environment = {.dispenserGrid = false, .cursor = glm::vec2(0.5f, 0.5f)},
	    .framing = {.shot = Shot::Follow, .distance = 1.2f},
	    .creatures = {Posed(CreatureType::Tiger, {0.0f, 0.0f})},
	    .miracles = {{.type = MagicType::CreatureSpellFreeze,
	                  .target = MiracleCast::Target::Creature,
	                  .creature = 0,
	                  .delaySeconds = 1.0f}},
	    .commands = {Leash(0, LeashType::Rope, 0.5f),
	                 {.kind = Kind::PointerPress, .delaySeconds = 3.0f, .value = 3},
	                 {.kind = Kind::PointerRelease, .delaySeconds = 0.1f, .value = 3},
	                 {.kind = Kind::PointerPress, .delaySeconds = 1.5f, .value = 3},
	                 {.kind = Kind::PointerRelease, .delaySeconds = 2.0f, .value = 3}},
	});

	// A script takes control of the own creature while the press waits for a tap. The press then sends the hold, and the
	// turn holds the creature: only the press asks if the hand may hold it.
	all.push_back({
	    .id = "hand.turn_holds_creature_a_script_took",
	    .name = "The hand holds your creature that a script took during the press",
	    .facet = Facet::Hand,
	    .description = "Your tiger faces the camera in the middle of the screen and wears the learning leash, held loose "
	                   "in the hand. The action button presses the tiger. 0.1 seconds later a script tells the tiger to "
	                   "be frightened on the spot. The press continues for two seconds and then the button lets go.",
	    .expected = "The hand holds the tiger after the short wait of the press, although the script now controls the "
	                "tiger. The camera turns to the tiger. The log shows \"waited long enough: the hand holds it\" and "
	                "then \"The hand of player 0 holds creature\". The log does not show \"The turn does not hold\".",
	    .environment = {.dispenserGrid = false, .cursor = glm::vec2(0.5f, 0.5f)},
	    .framing = {.shot = Shot::Follow, .distance = 1.2f},
	    .creatures = {Posed(CreatureType::Tiger, {0.0f, 0.0f})},
	    .commands =
	        {Leash(0, LeashType::Rope, 0.5f),
	         {.kind = Kind::PointerPress, .delaySeconds = 2.0f, .value = 3},
	         {.kind = Kind::DoAction, .creature = 0, .delaySeconds = 0.1f, .action = "BeFrightenedOnTheSpot", .atCreature = 0},
	         {.kind = Kind::PointerRelease, .delaySeconds = 1.9f, .value = 3}},
	});

	// The hand lets go of a thing over the slope of the lake's far shallows. The empty hand starts its up and its hover
	// distance again.
	all.push_back({
	    .id = "hand.lets_go_stands_straight",
	    .name = "The empty hand stands straight up after it lets go",
	    .facet = Facet::Hand,
	    .description = "The hand takes a barrel over the slope of the lake's far shallows. The pointer moves slowly "
	                   "across the slope, and the hand puts the barrel down. The pointer stays still for two seconds. "
	                   "Then the pointer moves across the slope again.",
	    .expected = "While the hand holds the barrel, it leans to the slope under it. When it lets go, it stands "
	                "straight up and stays straight up while the pointer is still. When the pointer moves across the "
	                "screen, the hand leans to the slope again in a short time.",
	    .environment = {.dispenserGrid = false, .cursor = glm::vec2(0.4f, 0.2f)},
	    .framing = {.shot = Shot::Overview, .include = {{k_Lake.x, k_FarShallows - 25.0f}}, .distance = 0.2f},
	    .objects = {{.type = MobileObjectInfo::EgyptBarrel, .offset = {k_Lake.x + 10.0f, k_FarShallows + 10.0f}}},
	    .commands = {{.kind = Kind::HandTake, .delaySeconds = 1.0f, .object = 0},
	                 {.kind = Kind::PointerSweep, .delaySeconds = 0.5f, .point = {0.2f, 0.0f}, .amount = 2.0f},
	                 {.kind = Kind::HandRelease, .delaySeconds = 2.5f},
	                 {.kind = Kind::PointerSweep, .delaySeconds = 2.0f, .point = {-0.2f, 0.0f}, .amount = 2.0f}},
	});

	all.push_back({
	    .id = "hand.mesh_contact",
	    .name = "The hand on the body",
	    .facet = Facet::Hand,
	    .description = "A tiger faces the camera and the hand is free. Hold the action button on the tiger. Move the hand "
	                   "slowly over its head, its arms and its belly, then off the body and back.",
	    .expected = "The hand stays under the cursor and rests just in front of the skin, also on thin parts. It eases "
	                "back to the skin in a short time and never goes into the body. Off the body, it stays in front of "
	                "the tiger and stands upright.",
	    .framing = {.shot = Shot::Follow, .distance = 1.2f},
	    .creatures = {Posed(CreatureType::Tiger, {0.0f, 0.0f})},
	});

	auto worn = Posed(CreatureType::Tiger, {0.0f, 0.0f});
	worn.needs = {.energy = 0.595f, .exhaustion = 0.325f, .life = 0.745f};
	all.push_back({
	    .id = "hand.status_panel",
	    .name = "The creature's status panel",
	    .facet = Facet::Hand,
	    .description = "The hand on a tiger whose needs are held at 25% damage, 40% hunger and 32% tiredness strokes it "
	                   "three times, slaps it once gently and once hard, and lets go.",
	    .expected = "While the hand is on it the panel at the left shows Damage 25%, Hunger 40% and Tiredness 32% in "
	                "yellow bars, and the reward going from No Reward 0% to Good Boy! 30% in green, then down past the "
	                "middle to Bad Boy! in red. Hovering the hand over it shows the panel with the last reward.",
	    .framing = {.shot = Shot::Follow, .distance = 1.0f},
	    .creatures = {worn},
	    .commands = {Stroke(0, BodyPart::Head, 1.0f), Stroke(0, BodyPart::Belly, 1.0f), Stroke(0, BodyPart::RightHand, 1.0f),
	                 Slap(0, 0.85f, true, false, 2.0f), Slap(0, 0.55f, false, true, 2.0f), Act(Kind::HandLetGo, 0, 3.0f, true)},
	    .repeatFrom = 0,
	});
	// The camera looks down on the north edge of the player's influence, 400 m from the middle: the bottom of the
	// screen is inside it, the rest beyond
	all.push_back({
	    .id = "hand.virtual_influence",
	    .name = "The hand acting past the influence border",
	    .facet = Facet::Hand,
	    .description = "The hand picks up a ball inside the player's influence, carries it past the border and lets it "
	                   "go, picks up a second ball just outside and drops it, then waits further out and tries a third "
	                   "ball once the share it keeps has run out, and comes back in.",
	    .expected = "Just past the border the hand still takes and lets go of the balls, keeping its inside look, with "
	                "the hum playing; the game log gives the share it keeps falling a tenth at a time. Once the log says "
	                "it has none, the third ball can't be taken and the hand changes to its outside look.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 30.0f, 375.0f}, .look = {0.0f, 0.0f, 415.0f}},
	    .objects = {{.type = MobileObjectInfo::Ball, .offset = {0.0f, 393.0f}},
	                {.type = MobileObjectInfo::Ball, .offset = {5.0f, 408.0f}},
	                {.type = MobileObjectInfo::Ball, .offset = {-5.0f, 430.0f}}},
	    .commands =
	        {// Inside, the hand takes the first ball
	         {.kind = Kind::PointerTo, .delaySeconds = 0.5f, .point = {0.5f, 0.96f}},
	         {.kind = Kind::PointerPress, .delaySeconds = 2.0f, .value = 3},
	         {.kind = Kind::PointerRelease, .delaySeconds = 1.5f, .value = 3},
	         // Carries it past the border and lets it go there
	         {.kind = Kind::PointerSweep, .delaySeconds = 0.5f, .point = {0.0f, -0.25f}, .amount = 1.0f},
	         {.kind = Kind::PointerPress, .delaySeconds = 1.5f, .value = 3},
	         {.kind = Kind::PointerRelease, .delaySeconds = 0.3f, .value = 3},
	         // Takes the second ball, just outside, and lets it go
	         {.kind = Kind::PointerTo, .delaySeconds = 1.0f, .point = {0.582f, 0.6f}},
	         {.kind = Kind::PointerPress, .delaySeconds = 0.5f, .value = 3},
	         {.kind = Kind::PointerRelease, .delaySeconds = 1.5f, .value = 3},
	         {.kind = Kind::PointerPress, .delaySeconds = 1.0f, .value = 3},
	         {.kind = Kind::PointerRelease, .delaySeconds = 0.3f, .value = 3},
	         // Waits further out until nothing is left, then tries the third
	         {.kind = Kind::PointerTo, .delaySeconds = 0.5f, .point = {0.44f, 0.02f}},
	         {.kind = Kind::PointerTo, .delaySeconds = 10.0f, .point = {0.441f, 0.296f}},
	         {.kind = Kind::PointerPress, .delaySeconds = 0.5f, .value = 3},
	         {.kind = Kind::PointerRelease, .delaySeconds = 1.5f, .value = 3},
	         // And comes back in
	         {.kind = Kind::PointerTo, .delaySeconds = 2.0f, .point = {0.3f, 0.97f}}},
	});
	// The camera looks down on a temple of player two, 700 m north of the middle and far past the player's influence.
	// The two players are allies.
	all.push_back({
	    .id = "hand.ally_influence",
	    .name = "The hand acting in the influence of an ally",
	    .facet = Facet::Hand,
	    .description = "Player two is an ally of the player, and a temple of player two stands far past the player's own "
	                   "influence. The hand goes over a ball near that temple, takes it, carries it a short distance and "
	                   "lets it go.",
	    .expected = "In the influence of the ally the hand has its inside look. It takes the ball and lets it go as it "
	                "does in the player's own influence. The game log gives no message that the hand is outside.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 40.0f, 620.0f}, .look = {0.0f, 0.0f, 680.0f}},
	    .objects = {{.type = MobileObjectInfo::Ball, .offset = {0.0f, 670.0f}}},
	    .temples = {{.offset = {0.0f, 700.0f}, .owner = PlayerNames::PLAYER_TWO}},
	    .allies = {{.player = PlayerNames::PLAYER_ONE, .other = PlayerNames::PLAYER_TWO, .strength = 1.0f}},
	    .commands = {{.kind = Kind::PointerOnObject, .delaySeconds = 1.0f, .object = 0, .amount = 1.0f},
	                 {.kind = Kind::PointerPress, .delaySeconds = 1.0f, .value = 3},
	                 {.kind = Kind::PointerRelease, .delaySeconds = 1.0f, .value = 3},
	                 {.kind = Kind::PointerSweep, .delaySeconds = 0.5f, .point = {0.1f, 0.0f}, .amount = 1.0f},
	                 {.kind = Kind::PointerPress, .delaySeconds = 1.5f, .value = 3},
	                 {.kind = Kind::PointerRelease, .delaySeconds = 0.3f, .value = 3}},
	});
	// The camera looks down on the north edge of the player's influence, 400 m from the middle: a wood pile and another
	// player's tiger just inside, a wood pile, another player's tiger and the player's own tiger just outside, all clear
	// of the testbed's window. The hand rests on the land between visits, so that each tooltip is submitted afresh. A
	// birch stands just inside, so tall that the land under the cursor on its crown lies outside.
	all.push_back({
	    .id = "hand.tooltips_influence",
	    .name = "The hand's tooltips and grabs either side of the influence border",
	    .facet = Facet::Hand,
	    .description = "The hand starts over the wood pile outside the player's influence, then goes over the other "
	                   "player's tiger outside and the player's own tiger outside. It comes in over the wood pile and the "
	                   "other player's tiger inside, then goes back out over the outside wood pile, rests out on the land "
	                   "for twenty seconds and tries the pile again. Then it presses on the crown of the birch standing "
	                   "just inside, against the land outside, drops what it took on the land inside, and holds the "
	                   "button down on the other player's tiger inside and then on the player's own tiger outside.",
	    .expected = "Outside, the wood pile shows no amount and the other player's tiger no \"Interact\"; the player's "
	                "own tiger shows \"Interact\" anywhere. Inside, the wood pile shows its amount and the other player's "
	                "tiger \"Interact\". Back out at once, the outside pile shows its amount, as the hand keeps some of "
	                "the player's influence past the border; once that has run out it shows nothing again. The birch is "
	                "pulled up and held, as it stands inside though the land under the cursor is outside. The other "
	                "player's tiger is never taken hold of, even inside (the log says so); the player's own tiger is, "
	                "outside, and the camera turns to it.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 45.0f, 355.0f}, .look = {0.0f, 0.0f, 410.0f}},
	    .creatures = {{.label = "own outside",
	                   .species = CreatureType::Tiger,
	                   .offset = {48.0f, 430.0f},
	                   .owner = PlayerNames::PLAYER_ONE,
	                   .size = 0.5f,
	                   .pauseMind = true},
	                  {.label = "other inside",
	                   .species = CreatureType::Tiger,
	                   .offset = {18.0f, 386.0f},
	                   .owner = PlayerNames::PLAYER_TWO,
	                   .size = 0.5f,
	                   .pauseMind = true},
	                  {.label = "other outside",
	                   .species = CreatureType::Tiger,
	                   .offset = {35.0f, 430.0f},
	                   .owner = PlayerNames::PLAYER_TWO,
	                   .size = 0.5f,
	                   .pauseMind = true}},
	    .objects = {{.type = PotInfo::WoodPile_1, .offset = {0.0f, 390.0f}, .amount = 300},
	                {.type = PotInfo::WoodPile_1, .offset = {15.0f, 428.0f}, .amount = 300},
	                {.type = TreeInfo::Birch, .offset = {30.0f, 396.0f}}},
	    .commands =
	        {// Outside: the wood pile, the other player's tiger, the land, the player's own tiger, the land
	         {.kind = Kind::PointerTo, .delaySeconds = 2.0f, .point = {0.626f, 0.379f}},
	         {.kind = Kind::PointerTo, .delaySeconds = 4.0f, .point = {0.798f, 0.327f}},
	         {.kind = Kind::PointerTo, .delaySeconds = 4.0f, .point = {0.7f, 0.45f}},
	         {.kind = Kind::PointerTo, .delaySeconds = 2.0f, .point = {0.909f, 0.327f}},
	         {.kind = Kind::PointerTo, .delaySeconds = 4.0f, .point = {0.7f, 0.45f}},
	         // Inside: the wood pile, the land, the other player's tiger, the land
	         {.kind = Kind::PointerTo, .delaySeconds = 2.0f, .point = {0.5f, 0.702f}},
	         {.kind = Kind::PointerTo, .delaySeconds = 4.0f, .point = {0.62f, 0.9f}},
	         {.kind = Kind::PointerTo, .delaySeconds = 2.0f, .point = {0.759f, 0.71f}},
	         {.kind = Kind::PointerTo, .delaySeconds = 4.0f, .point = {0.62f, 0.9f}},
	         // Straight back out over the outside pile, out on the land until nothing is kept, and the pile again
	         {.kind = Kind::PointerTo, .delaySeconds = 2.0f, .point = {0.626f, 0.379f}},
	         {.kind = Kind::PointerTo, .delaySeconds = 4.0f, .point = {0.7f, 0.45f}},
	         {.kind = Kind::PointerTo, .delaySeconds = 20.0f, .point = {0.626f, 0.379f}},
	         // With nothing kept, the birch standing just inside, its top against the land outside: pulled up and held
	         {.kind = Kind::PointerTo, .delaySeconds = 4.0f, .point = {0.887f, 0.547f}},
	         {.kind = Kind::PointerPress, .delaySeconds = 1.0f, .value = 3},
	         {.kind = Kind::PointerRelease, .delaySeconds = 3.0f, .value = 3},
	         // Dropped on the land inside
	         {.kind = Kind::PointerTo, .delaySeconds = 1.0f, .point = {0.62f, 0.9f}},
	         {.kind = Kind::PointerPress, .delaySeconds = 1.0f, .value = 3},
	         {.kind = Kind::PointerRelease, .delaySeconds = 0.3f, .value = 3},
	         // The other player's tiger inside, held down on: never taken hold of
	         {.kind = Kind::PointerTo, .delaySeconds = 2.0f, .point = {0.759f, 0.71f}},
	         {.kind = Kind::PointerPress, .delaySeconds = 1.0f, .value = 3},
	         {.kind = Kind::PointerRelease, .delaySeconds = 3.0f, .value = 3},
	         // The player's own tiger outside, held down on: taken hold of
	         {.kind = Kind::PointerTo, .delaySeconds = 2.0f, .point = {0.909f, 0.327f}},
	         {.kind = Kind::PointerPress, .delaySeconds = 1.0f, .value = 3},
	         {.kind = Kind::PointerRelease, .delaySeconds = 3.0f, .value = 3}},
	});
	// The camera stands as in the tutorial's throwing demonstration, 9 m over the land and 35 m behind the rock, which
	// sits in the middle of the screen; the flat land beyond leaves the throw room to land
	all.push_back({
	    .id = "hand.throw",
	    .name = "Throwing a rock",
	    .facet = Facet::Hand,
	    .description = "The hand picks up a rock, makes ready to throw low on the screen and flings it with a quick "
	                   "upward sweep of the pointer, in real time.",
	    .expected = "The rock leaves the hand at the hand's speed (the inspector's HandGrab lastThrow), the same at any "
	                "frame rate, and flies on the order of a hundred metres or more before it lands.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {-34.5f, 9.0f, 4.5f}, .look = {0.0f, 0.0f, 0.0f}},
	    .objects = {{.type = MobileStaticInfo::Rock, .offset = {0.0f, 0.0f}}},
	    .commands =
	        {// Takes the rock under the middle of the screen
	         {.kind = Kind::PointerTo, .delaySeconds = 1.0f, .point = {0.5f, 0.5f}},
	         {.kind = Kind::PointerPress, .delaySeconds = 0.5f, .value = 3},
	         {.kind = Kind::PointerRelease, .delaySeconds = 0.5f, .value = 3},
	         // Makes ready low on the screen and sweeps up, letting go as the sweep ends
	         {.kind = Kind::PointerTo, .delaySeconds = 0.5f, .point = {0.45f, 0.85f}},
	         {.kind = Kind::PointerPress, .delaySeconds = 0.5f, .value = 3},
	         {.kind = Kind::PointerSweep, .delaySeconds = 0.3f, .point = {0.03f, -0.4f}, .amount = 0.5f},
	         {.kind = Kind::PointerRelease, .delaySeconds = 0.5f, .value = 3}},
	});

	// The camera looks at a rock from 35 m. The hand takes the rock and throws it. The take and the throw are commands,
	// which the world does at the start of the next turn.
	all.push_back({
	    .id = "hand.commands_next_turn",
	    .name = "The hand's take and throw at the next turn",
	    .facet = Facet::Hand,
	    .description = "The hand holds the Action button on a rock until it takes it, then makes ready to throw and flings "
	                   "it with an upward sweep of the pointer. Step the frames with the inspector (game.step frames 1) "
	                   "and watch players.commands and the hand's HandGrab state.",
	    .expected = "After the wait the hand sends the take: its state is Taking, one command waits, and the rock is still on "
	                "the land, gripped by the hand. At the start of the next turn the rock is in the hand. After the "
	                "release the state is LettingGo and the rock stays in the hand until the next turn, then flies from "
	                "where the hand is, at the speed that the hand had at the release.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {-34.5f, 9.0f, 4.5f}, .look = {0.0f, 0.0f, 0.0f}},
	    .objects = {{.type = MobileStaticInfo::Rock, .offset = {0.0f, 0.0f}}},
	    .commands = {{.kind = Kind::PointerTo, .delaySeconds = 1.0f, .point = {0.5f, 0.5f}},
	                 {.kind = Kind::PointerPress, .delaySeconds = 0.5f, .value = 3},
	                 {.kind = Kind::PointerRelease, .delaySeconds = 0.5f, .value = 3},
	                 {.kind = Kind::PointerTo, .delaySeconds = 0.5f, .point = {0.45f, 0.85f}},
	                 {.kind = Kind::PointerPress, .delaySeconds = 0.5f, .value = 3},
	                 {.kind = Kind::PointerSweep, .delaySeconds = 0.3f, .point = {0.03f, -0.4f}, .amount = 0.5f},
	                 {.kind = Kind::PointerRelease, .delaySeconds = 0.5f, .value = 3}},
	});

	// The reports of the hand come 300 ms late, as in a network game. The hand takes a rock and throws it.
	all.push_back({
	    .id = "hand.throw_lead",
	    .name = "The lead of a throw for a late report",
	    .facet = Facet::Hand,
	    .description = "The reports of the player's hand get a fixed latency of 300 ms. The hand holds the Action button on "
	                   "a rock until it takes it, then makes ready to throw and throws it with an upward sweep of the "
	                   "pointer.",
	    .expected = "At the release the rock stops in the hand's move and starts its flight at once. The hand does not "
	                "carry it. At the next turn the rock jumps forward along its flight, to where it is three turns "
	                "after the release, and flies on from there.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {-34.5f, 9.0f, 4.5f}, .look = {0.0f, 0.0f, 0.0f}},
	    .objects = {{.type = MobileStaticInfo::Rock, .offset = {0.0f, 0.0f}}},
	    .commands = {{.kind = Kind::SetReportLatency, .delaySeconds = 0.0f, .value = 300},
	                 {.kind = Kind::PointerTo, .delaySeconds = 1.0f, .point = {0.5f, 0.5f}},
	                 {.kind = Kind::PointerPress, .delaySeconds = 0.5f, .value = 3},
	                 {.kind = Kind::PointerRelease, .delaySeconds = 0.5f, .value = 3},
	                 {.kind = Kind::PointerTo, .delaySeconds = 0.5f, .point = {0.45f, 0.85f}},
	                 {.kind = Kind::PointerPress, .delaySeconds = 0.5f, .value = 3},
	                 {.kind = Kind::PointerSweep, .delaySeconds = 0.3f, .point = {0.03f, -0.4f}, .amount = 0.5f},
	                 {.kind = Kind::PointerRelease, .delaySeconds = 0.5f, .value = 3}},
	});

	// The same throw with no latency of the reports, to compare with hand.throw_lead
	all.push_back({
	    .id = "hand.throw_no_lead",
	    .name = "A throw with no lead",
	    .facet = Facet::Hand,
	    .description = "The reports of the player's hand have no latency. The hand holds the Action button on "
	                   "a rock until it takes it, then makes ready to throw and throws it with an upward sweep of the "
	                   "pointer.",
	    .expected = "At the release the rock starts its flight from the hand. It flies on with no jump, as in a game alone "
	                "with no latency.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {-34.5f, 9.0f, 4.5f}, .look = {0.0f, 0.0f, 0.0f}},
	    .objects = {{.type = MobileStaticInfo::Rock, .offset = {0.0f, 0.0f}}},
	    .commands = {{.kind = Kind::SetReportLatency, .delaySeconds = 0.0f, .value = 0},
	                 {.kind = Kind::PointerTo, .delaySeconds = 1.0f, .point = {0.5f, 0.5f}},
	                 {.kind = Kind::PointerPress, .delaySeconds = 0.5f, .value = 3},
	                 {.kind = Kind::PointerRelease, .delaySeconds = 0.5f, .value = 3},
	                 {.kind = Kind::PointerTo, .delaySeconds = 0.5f, .point = {0.45f, 0.85f}},
	                 {.kind = Kind::PointerPress, .delaySeconds = 0.5f, .value = 3},
	                 {.kind = Kind::PointerSweep, .delaySeconds = 0.3f, .point = {0.03f, -0.4f}, .amount = 0.5f},
	                 {.kind = Kind::PointerRelease, .delaySeconds = 0.5f, .value = 3}},
	});

	// The camera looks at a cuddly toy and a toy ball on flat land from 35 m. The hand takes each toy in turn.
	all.push_back({
	    .id = "hand.hold_toys",
	    .name = "Holding toys",
	    .facet = Facet::Hand,
	    .description = "The hand takes the cuddly toy, holds it for three seconds and puts it down. Then it takes the "
	                   "toy ball and holds it.",
	    .expected = "The hand holds the cuddly toy from the side, as it holds a skittle. It holds the toy ball from "
	                "above on its palm, as it holds a bowling ball or a die.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {-34.5f, 9.0f, 4.5f}, .look = {0.0f, 0.0f, 0.0f}},
	    .objects = {{.type = MobileStaticInfo::ToyCuddly, .offset = {0.0f, 0.0f}},
	                {.type = MobileStaticInfo::ToyBall, .offset = {0.0f, 6.0f}}},
	    .commands = {{.kind = Kind::PointerTo, .delaySeconds = 1.0f, .point = {0.5f, 0.5f}},
	                 {.kind = Kind::PointerPress, .delaySeconds = 1.0f, .value = 3},
	                 {.kind = Kind::PointerRelease, .delaySeconds = 0.3f, .value = 3},
	                 {.kind = Kind::PointerTo, .delaySeconds = 3.0f, .point = {0.5f, 0.75f}},
	                 {.kind = Kind::PointerPress, .delaySeconds = 1.0f, .value = 3},
	                 {.kind = Kind::PointerRelease, .delaySeconds = 0.3f, .value = 3},
	                 {.kind = Kind::PointerTo, .delaySeconds = 1.0f, .point = {0.41f, 0.48f}},
	                 {.kind = Kind::PointerPress, .delaySeconds = 1.0f, .value = 3},
	                 {.kind = Kind::PointerRelease, .delaySeconds = 0.3f, .value = 3}},
	});

	// The camera looks at a wood pile on flat land from 35 m. The hand scoops from it for four seconds.
	all.push_back({
	    .id = "hand.scoop_pile",
	    .name = "Scooping from a pile",
	    .facet = Facet::Hand,
	    .description = "The hand takes wood from a wood pile and keeps the button down for four seconds. Then it lets "
	                   "go of the button and moves away with the handful.",
	    .expected = "While it scoops, the hand stays above the pile with the side hold of the handful, as it holds the "
	                "handful after the scoop. It does not tip down. It stays as high above the pile as the handful "
	                "hangs below it, and at least 1.9 m.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {-34.5f, 9.0f, 4.5f}, .look = {0.0f, 0.0f, 0.0f}},
	    .objects = {{.type = PotInfo::WoodPile_1, .offset = {0.0f, 0.0f}, .amount = 2000}},
	    .commands = {{.kind = Kind::PointerTo, .delaySeconds = 1.0f, .point = {0.5f, 0.5f}},
	                 {.kind = Kind::PointerPress, .delaySeconds = 1.0f, .value = 3},
	                 {.kind = Kind::PointerRelease, .delaySeconds = 4.0f, .value = 3},
	                 {.kind = Kind::PointerTo, .delaySeconds = 1.0f, .point = {0.5f, 0.7f}}},
	});

	// The camera looks at a ripe wheat field of the player's town from 22 m. The hand scoops from it for three seconds.
	all.push_back({
	    .id = "hand.scoop_field_let_go",
	    .name = "Letting go of a scoop from a field of the player",
	    .facet = Facet::Hand,
	    .description = "The hand takes wheat from a ripe field of the player's town and keeps the button down for three "
	                   "seconds. Then it lets go of the button and moves away with the handful.",
	    .expected = "While it scoops, the amount that the hand holds shows after each turn. When the button is let go, "
	                "the amount shows once more, because the field is of the player of this computer.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {-19.0f, 11.0f, 3.0f}, .look = {0.0f, 0.0f, 0.0f}},
	    .objects = {{.type = FieldTypeInfo::Wheat, .offset = {0.0f, 0.0f}, .ripeCrop = true}},
	    .commands = {{.kind = Kind::PointerTo, .delaySeconds = 1.0f, .point = {0.5f, 0.5f}},
	                 {.kind = Kind::PointerPress, .delaySeconds = 1.0f, .value = 3},
	                 {.kind = Kind::PointerRelease, .delaySeconds = 3.0f, .value = 3},
	                 {.kind = Kind::PointerTo, .delaySeconds = 1.0f, .point = {0.5f, 0.7f}}},
	});

	// The camera looks at a rock on flat land from 35 m. The hand does one thing after another, with a pause between them.
	all.push_back({
	    .id = "hand.state_blend",
	    .name = "The hand blends when it changes what it does",
	    .facet = Facet::Hand,
	    .description = "The hand moves over the bare land, then over the rock. It takes the rock and puts it down. It "
	                   "grips the land and pulls the camera, then turns the camera with the middle button. Last, it "
	                   "gets a food seed. Two seconds between the steps.",
	    .expected = "At each change, every bone of the drawn hand moves in a straight line for 0.13 s from how it looked "
	                "before. A grip of the bare land blends from the pose that the hand kept at the change before. A "
	                "change of the camera animation shows almost no blend.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {-34.5f, 9.0f, 4.5f}, .look = {0.0f, 0.0f, 0.0f}},
	    .objects = {{.type = MobileStaticInfo::Rock, .offset = {0.0f, 0.0f}}},
	    .commands = {{.kind = Kind::PointerTo, .delaySeconds = 1.0f, .point = {0.35f, 0.8f}},
	                 {.kind = Kind::PointerTo, .delaySeconds = 2.0f, .point = {0.5f, 0.5f}},
	                 {.kind = Kind::PointerPress, .delaySeconds = 2.0f, .value = 3},
	                 {.kind = Kind::PointerRelease, .delaySeconds = 0.3f, .value = 3},
	                 {.kind = Kind::PointerTo, .delaySeconds = 2.0f, .point = {0.65f, 0.75f}},
	                 {.kind = Kind::PointerPress, .delaySeconds = 2.0f, .value = 3},
	                 {.kind = Kind::PointerRelease, .delaySeconds = 0.3f, .value = 3},
	                 {.kind = Kind::PointerTo, .delaySeconds = 2.0f, .point = {0.35f, 0.8f}},
	                 {.kind = Kind::PointerPress, .delaySeconds = 2.0f, .value = 1},
	                 {.kind = Kind::PointerSweep, .delaySeconds = 0.5f, .point = {0.1f, -0.1f}, .amount = 1.0f},
	                 {.kind = Kind::PointerRelease, .delaySeconds = 1.5f, .value = 1},
	                 {.kind = Kind::PointerPress, .delaySeconds = 2.0f, .value = 2},
	                 {.kind = Kind::PointerSweep, .delaySeconds = 0.5f, .point = {-0.15f, 0.0f}, .amount = 1.0f},
	                 {.kind = Kind::PointerRelease, .delaySeconds = 1.5f, .value = 2},
	                 {.kind = Kind::HoldSeed, .delaySeconds = 2.0f, .value = static_cast<size_t>(SpellSeedType::Food)}},
	});

	// The camera looks north over the land, a rock and a tiger to its east, all in the player's influence; the pointer stays
	// east of the middle, clear of the testbed's window
	all.push_back({
	    .id = "hand.cast_tooltips",
	    .name = "What the hand offers with a miracle in it",
	    .facet = Facet::Hand,
	    .description = "A food seed is put in the hand and the pointer goes to the bare land, then to a rock; then a storm "
	                   "seed over the land; then the creature spell that makes a creature big over the land and over a "
	                   "tiger. Six seconds each.",
	    .expected = "Food over the land: \"Cast\" with the right mouse button. Over the rock it offers nothing. The storm "
	                "over the land: \"Cast Circle\". The big spell over the land offers nothing; over the tiger, "
	                "\"Cast\".",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 60.0f, -30.0f}, .look = {0.0f, 0.0f, 40.0f}},
	    .creatures = {Content(CreatureType::Tiger, {40.0f, 40.0f})},
	    .objects = {{.type = FeatureInfo::FatPilarChalk, .offset = {20.0f, 40.0f}}},
	    .commands = {{.kind = Kind::HoldSeed, .delaySeconds = 3.0f, .value = static_cast<size_t>(SpellSeedType::Food)},
	                 {.kind = Kind::PointerTo, .delaySeconds = 0.1f, .point = {0.75f, 0.8f}},
	                 {.kind = Kind::PointerTo, .delaySeconds = 6.0f, .point = {0.66f, 0.45f}},
	                 {.kind = Kind::HoldSeed, .delaySeconds = 6.0f, .value = static_cast<size_t>(SpellSeedType::Storm)},
	                 {.kind = Kind::PointerTo, .delaySeconds = 0.1f, .point = {0.75f, 0.8f}},
	                 {.kind = Kind::HoldSeed,
	                  .delaySeconds = 6.0f,
	                  .value = static_cast<size_t>(SpellSeedType::CreatureSpellBig)},
	                 {.kind = Kind::PointerTo, .delaySeconds = 6.0f, .point = {0.83f, 0.45f}}},
	});

	// The camera looks down on a row of things, and the pointer goes from one to the next
	all.push_back({
	    .id = "hand.tooltips_rest",
	    .name = "What the hand offers over the land, over things and with a thing in it",
	    .facet = Facet::Hand,
	    .description =
	        "The pointer rests on the bare land, then on the right edge of the screen and on its bottom edge. "
	        "Then it goes over a toy ball, a big rock, a hut that stands four tenths built and a whole hut. The hand then "
	        "takes the ball and the pointer rests on the land. Last, the hand puts the ball down, takes the "
	        "birch and the pointer rests on the land. Six seconds each.",
	    .expected =
	        "Over the land: \"Move\" with the left button and four arrows. At the right edge: \"Rotate\". At the "
	        "bottom edge: \"Pitch\" with the up and down arrows. Over the ball: \"Pick Up\" with the right "
	        "button. Over the big rock: \"Pick Up\" and \"Tap To Break\" in turns, or \"Tap To Break\" alone "
	        "when the hand cannot take it. Over the hut that is not built: \"Wood Required\" and \"Building Completed: 40%\" "
	        "in turns. Over the whole hut: \"Tap\". With the ball: \"Drop\" and \"Throw\" in turns, \"Throw\" "
	        "with four arrows. With the birch: \"Plant\" and \"Throw\" in turns.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 60.0f, -30.0f}, .look = {0.0f, 0.0f, 40.0f}},
	    .objects = {{.type = MobileStaticInfo::ToyBall, .offset = {-20.0f, 40.0f}},
	                {.type = MobileStaticInfo::RockChalk, .offset = {0.0f, 40.0f}, .scale = 2.0f},
	                {.type = AbodeInfo::CelticHut, .offset = {20.0f, 40.0f}, .built = 0.4f},
	                {.type = AbodeInfo::CelticHut, .offset = {40.0f, 40.0f}},
	                {.type = TreeInfo::Birch, .offset = {-40.0f, 40.0f}}},
	    .commands = {{.kind = Kind::PointerTo, .delaySeconds = 3.0f, .point = {0.5f, 0.85f}},
	                 {.kind = Kind::PointerTo, .delaySeconds = 6.0f, .point = {0.995f, 0.5f}},
	                 {.kind = Kind::PointerTo, .delaySeconds = 6.0f, .point = {0.5f, 0.995f}},
	                 {.kind = Kind::PointerTo, .delaySeconds = 6.0f, .point = {0.342f, 0.488f}},
	                 {.kind = Kind::PointerTo, .delaySeconds = 6.0f, .point = {0.5f, 0.483f}},
	                 {.kind = Kind::PointerTo, .delaySeconds = 6.0f, .point = {0.656f, 0.485f}},
	                 {.kind = Kind::PointerTo, .delaySeconds = 6.0f, .point = {0.813f, 0.488f}},
	                 {.kind = Kind::HandTake, .delaySeconds = 6.0f, .object = 0},
	                 {.kind = Kind::PointerTo, .delaySeconds = 0.1f, .point = {0.5f, 0.85f}},
	                 {.kind = Kind::HandRelease, .delaySeconds = 12.0f},
	                 {.kind = Kind::HandTake, .delaySeconds = 1.0f, .object = 4},
	                 {.kind = Kind::PointerTo, .delaySeconds = 0.1f, .point = {0.5f, 0.85f}}},
	    // The huts' town stays the player's: the hand shows the wood that a building wants only for the player's own
	    .townBeliefInPlayer = 1000.0f,
	});

	// The camera looks down on a row of huts that are not whole, and the pointer goes from one to the next
	all.push_back({
	    .id = "hand.pick_unbuilt",
	    .name = "The hand over buildings that are not built or not mended",
	    .facet = Facet::Hand,
	    .description =
	        "Four huts of the player's town stand in a row: one four tenths built, one a tenth built, one hurt to half its "
	        "life and one with nothing built. The pointer rests on each, six seconds each, a little above the ground where "
	        "the whole hut would stand. Then the hand taps the hut that is four tenths built. Last, the hand takes a pot "
	        "of wood, the pointer goes back on that hut and the hand gives the wood to it.",
	    .expected = "Over the first three huts the hand finds the hut: \"Wood Required\" and \"Building Completed\" take "
	                "turns. The hut a tenth built is found also where only its scaffold shows. Over the hut with nothing "
	                "built the hand finds nothing and shows \"Move\". The tap knocks on the hut. While the hand holds the "
	                "wood over the hut it shows \"Supply\". The wood goes onto the hut's building site and \"Wood "
	                "Required\" goes down by the wood given.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 60.0f, -30.0f}, .look = {0.0f, 0.0f, 40.0f}},
	    .objects = {{.type = AbodeInfo::CelticHut, .offset = {-20.0f, 40.0f}, .built = 0.4f},
	                {.type = AbodeInfo::CelticHut, .offset = {0.0f, 40.0f}, .built = 0.1f},
	                {.type = AbodeInfo::CelticHut, .offset = {20.0f, 40.0f}, .life = 0.5f},
	                {.type = AbodeInfo::CelticHut, .offset = {40.0f, 40.0f}, .built = 0.0f},
	                {.type = PotInfo::WoodPot, .offset = {0.0f, 25.0f}, .amount = 100}},
	    .commands = {{.kind = Kind::PointerTo, .delaySeconds = 3.0f, .point = {0.342f, 0.488f}},
	                 {.kind = Kind::PointerTo, .delaySeconds = 6.0f, .point = {0.5f, 0.488f}},
	                 {.kind = Kind::PointerTo, .delaySeconds = 6.0f, .point = {0.656f, 0.488f}},
	                 {.kind = Kind::PointerTo, .delaySeconds = 6.0f, .point = {0.813f, 0.488f}},
	                 {.kind = Kind::PointerTo, .delaySeconds = 6.0f, .point = {0.342f, 0.488f}},
	                 {.kind = Kind::PointerPress, .delaySeconds = 1.0f, .value = 3},
	                 {.kind = Kind::PointerRelease, .delaySeconds = 0.1f, .value = 3},
	                 {.kind = Kind::HandTake, .delaySeconds = 3.0f, .object = 4},
	                 {.kind = Kind::PointerTo, .delaySeconds = 0.1f, .point = {0.342f, 0.488f}},
	                 {.kind = Kind::PointerPress, .delaySeconds = 6.0f, .value = 3},
	                 {.kind = Kind::PointerRelease, .delaySeconds = 0.1f, .value = 3}},
	    .townBeliefInPlayer = 1000.0f,
	});

	// The camera looks down at the player's creature, and the pointer grips the land and then rests on the creature
	all.push_back({
	    .id = "hand.grip_and_creature_tips",
	    .name = "What the hand offers as it grips the land, and over the player's creature",
	    .facet = Facet::Hand,
	    .description = "The pointer goes on the land in front of the player's tiger and holds the Move button. A second "
	                   "later it also holds the Action button, for six seconds. Then it lets go of both buttons and rests "
	                   "on the tiger for six seconds.",
	    .expected = "While both buttons are held the hand shows \"Zoom\" with the mouse of both buttons and the up and "
	                "down arrows. Over the tiger, in the player's influence, the hand shows \"Interact\" with the mouse "
	                "of the Action button.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 30.0f, -40.0f}, .look = {0.0f, 0.0f, 15.0f}},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 15.0f})},
	    .commands = {{.kind = Kind::PointerTo, .delaySeconds = 3.0f, .point = {0.5f, 0.85f}},
	                 {.kind = Kind::PointerPress, .delaySeconds = 1.0f, .value = 1},
	                 {.kind = Kind::PointerPress, .delaySeconds = 1.0f, .value = 3},
	                 {.kind = Kind::PointerRelease, .delaySeconds = 6.0f, .value = 3},
	                 {.kind = Kind::PointerRelease, .delaySeconds = 0.1f, .value = 1},
	                 {.kind = Kind::PointerTo, .delaySeconds = 1.0f, .point = {0.5f, 0.45f}}},
	});

	// The camera looks along the land just ahead, so the hand is seen up close in the sun
	all.push_back({
	    .id = "hand.lighting",
	    .name = "The hand in the light",
	    .facet = Facet::Hand,
	    .description = "The bare hand sits still near the camera while the player turns evil, then good, ten seconds each.",
	    .expected = "The hand is lit as the game lights the models it poses: the side facing the light at full strength, the "
	                "side away from it never darker than two fifths, in the land's light half as bright again, whatever its "
	                "evil or good shape and skin.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 6.0f, -14.0f}, .look = {0.0f, 1.0f, 4.0f}},
	    .commands = {{.kind = Kind::PointerTo, .delaySeconds = 0.5f, .point = {0.5f, 0.7f}},
	                 {.kind = Kind::SetAlignment, .delaySeconds = 8.0f, .alignment = -1.0f},
	                 {.kind = Kind::SetAlignment, .delaySeconds = 10.0f, .alignment = 1.0f}},
	});

	// The camera looks along the land ahead and nothing moves the pointer, so the hand shows where it starts
	all.push_back({
	    .id = "hand.spawn_point",
	    .name = "The hand before the mouse moves",
	    .facet = Facet::Hand,
	    .description = "A food store in the middle of the view is looked at from a little way up and the pointer is left "
	                   "alone. Run it as the game starts (ob-build run --scenario hand.spawn_point) without touching the "
	                   "mouse.",
	    .expected = "From the first frame the hand is under the cursor when the cursor is over the window. When the "
	                "cursor has never been over the window the pointer is the window's top left corner, as in the "
	                "original, so the hand is off along the line through that corner until the cursor comes over the "
	                "window. An agent holding the input puts it under the middle of the window, over the food store.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 20.0f, -40.0f}, .look = {0.0f, 0.0f, 10.0f}},
	    .objects = {{.type = PotInfo::FoodPot, .offset = {0.0f, 10.0f}}},
	});

	// The camera looks down at the land just ahead, so the hand is seen up close
	all.push_back({
	    .id = "hand.miracle_glow",
	    .name = "The glow over a hand holding a miracle",
	    .facet = Facet::Hand,
	    .description = "A food seed is put in the hand, which sits still near the camera. Then the player turns evil, "
	                   "then good, ten seconds each.",
	    .expected = "The flowing sheet runs over the hand in the player's colour, shaded as the hand is (brighter on the "
	                "side facing the light), added over it at four fifths and lying exactly on its skin whatever its "
	                "evil or good shape.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 6.0f, -14.0f}, .look = {0.0f, 1.0f, 4.0f}},
	    .commands = {{.kind = Kind::PointerTo, .delaySeconds = 0.5f, .point = {0.5f, 0.7f}},
	                 {.kind = Kind::HoldSeed, .delaySeconds = 1.0f, .value = static_cast<size_t>(SpellSeedType::Food)},
	                 {.kind = Kind::SetAlignment, .delaySeconds = 8.0f, .alignment = -1.0f},
	                 {.kind = Kind::SetAlignment, .delaySeconds = 10.0f, .alignment = 1.0f}},
	});

	// The camera looks north over the land from high up; the pointer starts east of the middle, clear of the testbed's
	// window
	all.push_back({
	    .id = "hand.held_miracle_draw",
	    .name = "Miracles that follow the hand as it moves",
	    .facet = Facet::Hand,
	    .description = "The hand gets a water seed and casts it with the right mouse button held. The pointer moves left "
	                   "and right, then to the far edge of the testbed and back, then lets go. Then the hand casts "
	                   "food, wood, lightning and fire, and the pointer moves left and right.",
	    .expected = "The rain cloud and its rain cone stay exactly on the moving hand, with no lag. The food grains "
	                "and the logs fall from where they come out and do not move with the hand. The start of the "
	                "lightning and the fireball in the hand stay on the hand.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 40.0f, -30.0f}, .look = {0.0f, 10.0f, 30.0f}},
	    .commands = {{.kind = Kind::PointerTo, .delaySeconds = 0.5f, .point = {0.6f, 0.8f}},
	                 {.kind = Kind::HoldSeed, .delaySeconds = 1.0f, .value = static_cast<size_t>(SpellSeedType::Water)},
	                 {.kind = Kind::PointerPress, .delaySeconds = 1.0f, .value = 3},
	                 {.kind = Kind::PointerSweep, .delaySeconds = 0.5f, .point = {-0.3f, 0.0f}, .amount = 1.5f},
	                 {.kind = Kind::PointerSweep, .delaySeconds = 1.5f, .point = {0.3f, 0.0f}, .amount = 1.5f},
	                 {.kind = Kind::PointerSweep, .delaySeconds = 1.5f, .point = {0.0f, -0.78f}, .amount = 1.0f},
	                 {.kind = Kind::PointerSweep, .delaySeconds = 2.0f, .point = {0.0f, 0.78f}, .amount = 1.0f},
	                 {.kind = Kind::PointerRelease, .delaySeconds = 1.0f, .value = 3},
	                 {.kind = Kind::HoldSeed, .delaySeconds = 2.0f, .value = static_cast<size_t>(SpellSeedType::Food)},
	                 {.kind = Kind::PointerPress, .delaySeconds = 1.0f, .value = 3},
	                 {.kind = Kind::PointerSweep, .delaySeconds = 0.5f, .point = {-0.3f, 0.0f}, .amount = 1.5f},
	                 {.kind = Kind::PointerSweep, .delaySeconds = 1.5f, .point = {0.3f, 0.0f}, .amount = 1.5f},
	                 {.kind = Kind::PointerRelease, .delaySeconds = 1.5f, .value = 3},
	                 {.kind = Kind::HoldSeed, .delaySeconds = 2.0f, .value = static_cast<size_t>(SpellSeedType::Wood)},
	                 {.kind = Kind::PointerPress, .delaySeconds = 1.0f, .value = 3},
	                 {.kind = Kind::PointerSweep, .delaySeconds = 0.5f, .point = {-0.3f, 0.0f}, .amount = 1.5f},
	                 {.kind = Kind::PointerSweep, .delaySeconds = 1.5f, .point = {0.3f, 0.0f}, .amount = 1.5f},
	                 {.kind = Kind::PointerRelease, .delaySeconds = 1.5f, .value = 3},
	                 {.kind = Kind::HoldSeed, .delaySeconds = 2.0f, .value = static_cast<size_t>(SpellSeedType::LightningBolt)},
	                 {.kind = Kind::PointerPress, .delaySeconds = 1.0f, .value = 3},
	                 {.kind = Kind::PointerSweep, .delaySeconds = 0.5f, .point = {-0.3f, 0.0f}, .amount = 1.5f},
	                 {.kind = Kind::PointerSweep, .delaySeconds = 1.5f, .point = {0.3f, 0.0f}, .amount = 1.5f},
	                 {.kind = Kind::PointerRelease, .delaySeconds = 1.5f, .value = 3},
	                 {.kind = Kind::HoldSeed, .delaySeconds = 2.0f, .value = static_cast<size_t>(SpellSeedType::Fire)},
	                 {.kind = Kind::PointerPress, .delaySeconds = 1.0f, .value = 3},
	                 {.kind = Kind::PointerSweep, .delaySeconds = 0.5f, .point = {-0.3f, 0.0f}, .amount = 1.5f},
	                 {.kind = Kind::PointerSweep, .delaySeconds = 1.5f, .point = {0.3f, 0.0f}, .amount = 1.5f},
	                 {.kind = Kind::PointerRelease, .delaySeconds = 1.5f, .value = 3}},
	});

	// The camera looks down on an oak from about 150 m, as in play, the pointer on its crown
	all.push_back({
	    .id = "hand.tree_tug",
	    .name = "Pulling a tree out of the ground",
	    .facet = Facet::Hand,
	    .description = "Seen from about 150 m up, the hand grips a large oak by its crown, pulls it up the screen until "
	                   "it comes out of the ground, carries it a little, then puts it down.",
	    .expected = "The hand hovers on the crown, drops to the trunk as it takes hold, and the oak leans and stretches "
	                "towards the pull. When it comes out, leaving its roots hole, the hand and the oak move together to "
	                "under the pointer; the hand never leaves the tree.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 110.0f, -80.0f}, .look = {0.0f, 4.0f, 20.0f}},
	    .objects = {{.type = TreeInfo::Oak, .offset = {0.0f, 20.0f}, .scale = 1.6f}},
	    .commands = {{.kind = Kind::PointerTo, .delaySeconds = 1.0f, .point = {0.5f, 0.47f}},
	                 {.kind = Kind::PointerPress, .delaySeconds = 1.0f, .value = 3},
	                 {.kind = Kind::PointerSweep, .delaySeconds = 0.3f, .point = {0.0f, -0.25f}, .amount = 1.5f},
	                 {.kind = Kind::PointerRelease, .delaySeconds = 1.5f, .value = 3},
	                 {.kind = Kind::PointerPress, .delaySeconds = 1.5f, .value = 3},
	                 {.kind = Kind::PointerRelease, .delaySeconds = 0.2f, .value = 3}},
	});

	// The camera looks down at 45 degrees on a large oak from about 150 m, the pointer on its crown. The oak is heavy, so
	// the hand must pull it for a time before it comes out.
	all.push_back({
	    .id = "hand.tree_tug_heavy",
	    .name = "Pulling a heavy tree out of the ground",
	    .facet = Facet::Hand,
	    .description = "Seen from about 150 m away at 45 degrees, the hand grips a very large oak, pulls it slowly up the "
	                   "screen until it comes out of the ground, then carries it.",
	    .expected = "The oak leans and stretches towards the pull. When it comes out, the hand and the oak move together "
	                "to under the pointer. On no frame is the hand drawn away from the oak.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 106.0f, -66.0f}, .look = {0.0f, 0.0f, 40.0f}},
	    .objects = {{.type = TreeInfo::Oak, .offset = {0.0f, 40.0f}, .scale = 2.5f}},
	    .commands = {{.kind = Kind::PointerTo, .delaySeconds = 1.0f, .point = {0.5f, 0.42f}},
	                 {.kind = Kind::PointerPress, .delaySeconds = 1.0f, .value = 3},
	                 {.kind = Kind::PointerSweep, .delaySeconds = 0.3f, .point = {0.0f, -0.3f}, .amount = 3.0f},
	                 {.kind = Kind::PointerRelease, .delaySeconds = 3.5f, .value = 3}},
	});

	// The camera looks down at 45 degrees on a very large oak from about 150 m. The hand pulls it for a tenth of a second
	// and lets go before it comes out, so the oak stays drawn in the lean of the pull.
	all.push_back({
	    .id = "hand.tree_tug_let_go",
	    .name = "Letting go of a tree before it comes out",
	    .facet = Facet::Hand,
	    .description = "Seen from about 150 m away at 45 degrees, the hand grips a very large oak, pulls it up the screen "
	                   "for a tenth of a second, then lets go before the oak comes out of the ground.",
	    .expected = "The oak leans and stretches a little towards the pull. After the hand lets go, the oak stays in "
	                "that lean in a game alone. In a network game, it stands straight again.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 106.0f, -66.0f}, .look = {0.0f, 0.0f, 40.0f}},
	    .objects = {{.type = TreeInfo::Oak, .offset = {0.0f, 40.0f}, .scale = 2.5f}},
	    .commands = {{.kind = Kind::PointerTo, .delaySeconds = 1.0f, .point = {0.5f, 0.42f}},
	                 {.kind = Kind::PointerPress, .delaySeconds = 1.0f, .value = 3},
	                 {.kind = Kind::PointerSweep, .delaySeconds = 0.3f, .point = {0.0f, -0.1f}, .amount = 1.0f},
	                 {.kind = Kind::PointerRelease, .delaySeconds = 0.1f, .value = 3},
	                 {.kind = Kind::PointerTo, .delaySeconds = 0.5f, .point = {0.8f, 0.8f}}},
	});

	// The camera looks at a conifer from 25 m, 20 degrees down, the pointer on its crown. A press of a fifth of a second
	// is shorter than a tap, but the pull starts after the 0.13 s fade and the hand is far from its grip on the trunk.
	// Thus the conifer comes free on the first frame of the pull and the hand takes it. The same press does the same in
	// a network game.
	all.push_back({
	    .id = "hand.tree_short_press",
	    .name = "A short press on a conifer",
	    .facet = Facet::Hand,
	    .description = "Seen from 25 m, 20 degrees down, the hand presses on the crown of a conifer for a fifth of a "
	                   "second and lets go.",
	    .expected = "The conifer comes out of the ground on the first frame of the pull and leaves its roots hole. The "
	                "hand holds it after the button comes up. The conifer does not stay leaning in the ground.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 12.55f, -3.49f}, .look = {0.0f, 4.0f, 20.0f}},
	    .objects = {{.type = TreeInfo::ConiferA, .offset = {0.0f, 20.0f}, .scale = 1.03f}},
	    .commands = {{.kind = Kind::PointerTo, .delaySeconds = 1.0f, .point = {0.5f, 0.35f}},
	                 {.kind = Kind::PointerPress, .delaySeconds = 1.0f, .value = 3},
	                 {.kind = Kind::PointerRelease, .delaySeconds = 0.2f, .value = 3}},
	});

	// The camera looks down on a few people and animals from about 25 m, as in play: three people stand still at different
	// distances, two more walk to and fro, two stand by an oak, and a cow, a sheep, a horse and a pig graze to the left
	all.push_back({
	    .id = "hand.pick_living",
	    .name = "Picking up people and animals",
	    .facet = Facet::Hand,
	    .description = "Three people stand still near, middling and far from the camera, two walk to and fro across the "
	                   "view, two stand by an oak, and a cow, a sheep, a horse and a pig graze. Move the hand near "
	                   "each and press to pick them up.",
	    .expected = "A person is taken whenever the pointer is inside the circle their bounding ball makes on the screen, "
	                "not only on their body: a little beside, above or below a person still picks them. Of two people "
	                "under the pointer the nearer to the camera is taken; by the oak the person is taken rather than "
	                "the tree unless the pointer is on the tree nearer the camera. Animals are taken only with the "
	                "pointer on their bodies.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 14.0f, -25.0f}, .look = {0.0f, 0.0f, 6.0f}},
	    .objects = {{.type = VillagerInfo::CelticFarmerMale, .offset = {-4.0f, 0.0f}, .inScript = true},
	                {.type = VillagerInfo::CelticHousewifeFemale, .offset = {3.0f, 8.0f}, .inScript = true},
	                {.type = VillagerInfo::CelticForesterMale, .offset = {0.0f, 22.0f}, .inScript = true},
	                {.type = VillagerInfo::CelticFarmerMale,
	                 .offset = {-12.0f, 12.0f},
	                 .walkTo = glm::vec2(12.0f, 12.0f),
	                 .walkAfterSeconds = 1.0f,
	                 .walkRepeatSeconds = 12.0f},
	                {.type = VillagerInfo::CelticHousewifeFemale,
	                 .offset = {12.0f, 16.0f},
	                 .walkTo = glm::vec2(-12.0f, 16.0f),
	                 .walkAfterSeconds = 1.0f,
	                 .walkRepeatSeconds = 12.0f},
	                {.type = TreeInfo::Oak, .offset = {10.0f, 3.0f}},
	                {.type = VillagerInfo::CelticForesterMale, .offset = {8.5f, 2.0f}, .inScript = true},
	                {.type = VillagerInfo::CelticHousewifeFemale, .offset = {11.5f, 2.0f}, .inScript = true},
	                {.type = AnimalInfo::Cow, .offset = {-14.0f, 2.0f}, .living = true},
	                {.type = AnimalInfo::Sheep, .offset = {-9.0f, -3.0f}, .living = true},
	                {.type = AnimalInfo::Horse, .offset = {-12.0f, 8.0f}, .living = true},
	                {.type = AnimalInfo::Pig, .offset = {-6.0f, 5.0f}, .living = true}},
	});

	// One of each kind of thing the cursor picks by its own shape, in a row on bare land
	all.push_back({
	    .id = "hand.pick_kinds",
	    .name = "What the cursor picks, kind by kind",
	    .facet = Facet::Hand,
	    .description = "In a row on bare land, left to right: a person, a pig, an oak, a \"did you know\" sign, a silver "
	                   "scroll, a fire dispenser floating its globe, and a lightning globe on its own; behind them, a "
	                   "vortex between the lands. Move the pointer round each and press the Action button on it.",
	    .expected = "A person is picked anywhere in the circle their bounding ball makes, at that ball's near side; the "
	                "pig, the oak and the sign only on their bodies. The scroll is picked on its model or a little "
	                "round it, whichever is nearer. The dispenser is picked on its building and in a wide ball round "
	                "its globe, and a press there knocks on it; on the globe itself the globe is picked, being pushed "
	                "towards the camera, and a quick press puts its miracle in the hand. The loose globe is picked on "
	                "its model or within a ball about its middle 1.2 times its size (moved by the middle of the miracle's "
	                "model it shows). The vortex is picked anywhere within a ball at its middle seven tenths as wide as "
	                "its cylinder, though the land in front of it wins.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 14.0f, -28.0f}, .look = {0.0f, 3.0f, 8.0f}},
	    .objects = {{.type = VillagerInfo::CelticFarmerMale, .offset = {-18.0f, 6.0f}, .inScript = true},
	                {.type = AnimalInfo::Pig, .offset = {-13.0f, 6.0f}},
	                {.type = TreeInfo::Oak, .offset = {-7.0f, 10.0f}}},
	    .vortices = {{.type = VortexType::In, .offset = {0.0f, 45.0f}, .delaySeconds = 0.5f}},
	    .dispensers = {{.type = MagicType::Fireball, .offset = {11.0f, 10.0f}},
	                   {.type = MagicType::LightningBolt, .offset = {19.0f, 6.0f}, .bubbleHeight = 3.0f}},
	    .highlights = {{.kind = 1, .offset = {-1.0f, 6.0f}, .challenge = 0, .tip = std::pair {4128u, 3u}},
	                   {.kind = 2, .offset = {4.0f, 6.0f}}},
	});
	// A healing dispenser floats its globe in the middle of the screen, and a second healing globe floats on its own to
	// its left, both in the player's influence on flat land
	all.push_back({
	    .id = "hand.globe_carry",
	    .name = "Carrying and throwing one-shot globes",
	    .facet = Facet::Hand,
	    .description = "The hand holds the Action button on the dispenser's globe until it takes it, carries it to the "
	                   "right and lets it go gently. Then it takes the globe floating on its own and throws it with a "
	                   "quick upward sweep of the pointer.",
	    .expected = "Held past a tap, each globe is lifted from where it floats and carried hanging from the side of the "
	                "hand, still turned to the camera with its miracle spinning inside; the dispenser floats a new globe "
	                "once its time has passed. Let go gently, the globe falls to the land and settles there, whole. Thrown, it "
	                "flies off "
	                "like any loose thing and lands, whole: a globe never bursts or casts by landing. A quick press on a "
	                "globe still puts its miracle in the hand instead.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {-16.0f, 14.0f, 0.0f}, .look = {0.0f, 2.0f, 0.0f}},
	    .dispensers = {{.type = MagicType::Heal, .offset = {0.0f, 0.0f}},
	                   {.type = MagicType::Heal, .offset = {0.0f, 6.0f}, .bubbleHeight = 3.0f}},
	    .commands =
	        {// Takes the dispenser's globe, holding the button past a tap
	         {.kind = Kind::PointerTo, .delaySeconds = 1.0f, .point = {0.5f, 0.31f}},
	         {.kind = Kind::PointerPress, .delaySeconds = 0.5f, .value = 3},
	         {.kind = Kind::PointerRelease, .delaySeconds = 0.6f, .value = 3},
	         // Carries it to the right and lets it go there gently
	         {.kind = Kind::PointerSweep, .delaySeconds = 0.5f, .point = {0.2f, 0.1f}, .amount = 1.5f},
	         {.kind = Kind::PointerPress, .delaySeconds = 2.0f, .value = 3},
	         {.kind = Kind::PointerRelease, .delaySeconds = 0.3f, .value = 3},
	         // Takes the lone globe and throws it with a quick upward sweep
	         {.kind = Kind::PointerTo, .delaySeconds = 3.0f, .point = {0.26f, 0.37f}},
	         {.kind = Kind::PointerPress, .delaySeconds = 0.5f, .value = 3},
	         {.kind = Kind::PointerRelease, .delaySeconds = 0.6f, .value = 3},
	         {.kind = Kind::PointerTo, .delaySeconds = 1.0f, .point = {0.45f, 0.85f}},
	         {.kind = Kind::PointerPress, .delaySeconds = 0.5f, .value = 3},
	         {.kind = Kind::PointerSweep, .delaySeconds = 0.3f, .point = {0.03f, -0.4f}, .amount = 0.5f},
	         {.kind = Kind::PointerRelease, .delaySeconds = 0.5f, .value = 3}},
	});

	// A healing dispenser floats its globe in the middle of the screen, in the player's influence on flat land. The hand
	// takes the globe and throws it at once.
	all.push_back({
	    .id = "hand.globe_throw",
	    .name = "Throwing a one-shot globe",
	    .facet = Facet::Hand,
	    .description = "The hand holds the Action button on the dispenser's globe until it takes it. Then it makes ready "
	                   "low on the screen and throws the globe with a quick upward sweep of the pointer.",
	    .expected = "The globe leaves the hand turned as it was last drawn, with its dome to the camera, and from the same "
	                "place. It does not jump or turn as it leaves the hand. It flies off and lands, whole.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {-16.0f, 14.0f, 0.0f}, .look = {0.0f, 2.0f, 0.0f}},
	    .dispensers = {{.type = MagicType::Heal, .offset = {0.0f, 0.0f}}},
	    .commands =
	        {// Takes the dispenser's globe, holding the button past a tap
	         {.kind = Kind::PointerTo, .delaySeconds = 1.0f, .point = {0.5f, 0.31f}},
	         {.kind = Kind::PointerPress, .delaySeconds = 0.5f, .value = 3},
	         {.kind = Kind::PointerRelease, .delaySeconds = 0.6f, .value = 3},
	         // Makes ready low on the screen and sweeps up, letting go as the sweep ends
	         {.kind = Kind::PointerTo, .delaySeconds = 1.0f, .point = {0.45f, 0.85f}},
	         {.kind = Kind::PointerPress, .delaySeconds = 0.5f, .value = 3},
	         {.kind = Kind::PointerSweep, .delaySeconds = 0.3f, .point = {0.03f, -0.4f}, .amount = 0.5f},
	         {.kind = Kind::PointerRelease, .delaySeconds = 0.5f, .value = 3}},
	});
	// A dispenser floats a heal globe in front of the camera, in the player's influence, and the pointer rests on it
	all.push_back({
	    .id = "hand.globe_tooltip",
	    .name = "The name of the miracle over a one-shot globe",
	    .facet = Facet::Hand,
	    .description = "The pointer rests on the land for three seconds, then on the dispenser's globe for six seconds.",
	    .expected = "Over the land: \"Move\". Over the globe: the name of its miracle (the heal miracle) at once, with no "
	                "mouse button, and no \"Pick Up\".",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {-16.0f, 14.0f, 0.0f}, .look = {0.0f, 2.0f, 0.0f}},
	    .dispensers = {{.type = MagicType::Heal, .offset = {0.0f, 0.0f}}},
	    .commands = {{.kind = Kind::PointerTo, .delaySeconds = 1.0f, .point = {0.5f, 0.85f}},
	                 {.kind = Kind::PointerTo, .delaySeconds = 3.0f, .point = {0.5f, 0.31f}}},
	});
	// A "did you know" sign stands in front of the camera. The hand taps it, and its tip bubble comes up.
	all.push_back({
	    .id = "hand.bubble_scroll_tooltip",
	    .name = "The tooltip over the tip bubble of a sign",
	    .facet = Facet::Hand,
	    .description = "The pointer goes to the sign and taps it with a quick press of the Action button. Then move the "
	                   "pointer onto the tip bubble, and hold the Action button on it.",
	    .expected = "Over the bubble, and while the hand grips it: \"Scroll\" with the Action button and the up and down "
	                "arrows. Over the land: \"Move\".",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 14.0f, -20.0f}, .look = {0.0f, 3.0f, 6.0f}},
	    .highlights = {{.kind = 1, .offset = {0.0f, 6.0f}, .challenge = 0, .tip = std::pair {4128u, 3u}}},
	    .commands = {{.kind = Kind::PointerTo, .delaySeconds = 1.0f, .point = {0.5f, 0.5f}},
	                 {.kind = Kind::PointerPress, .delaySeconds = 0.5f, .value = 3},
	                 {.kind = Kind::PointerRelease, .delaySeconds = 0.1f, .value = 3}},
	});
	// A tree stands between the camera and the tip bubble of a sign
	all.push_back({
	    .id = "hand.bubble_behind_tree",
	    .name = "A tree in front of a tip bubble hides it",
	    .facet = Facet::Hand,
	    .description = "The pointer goes to a \"did you know\" sign and taps it. An oak stands to its right, between the "
	                   "camera and the bubble that comes up over the sign.",
	    .expected = "The oak hides the part of the bubble's box and words that is behind its trunk and leaves. Between "
	                "the leaves the bubble shows. The blinking arrows and the pictures of the controls show over the "
	                "tree. Turn the camera to look down: more of the bubble then shows, because the bubble comes "
	                "nearer to the camera as it looks down.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 3.0f, -14.0f}, .look = {0.0f, 2.0f, 6.0f}},
	    .objects = {{.type = TreeInfo::Oak, .offset = {5.0f, -2.0f}}},
	    .highlights = {{.kind = 1, .offset = {0.0f, 6.0f}, .challenge = 0, .tip = std::pair {4128u, 3u}}},
	    .commands = {{.kind = Kind::PointerTo, .delaySeconds = 1.0f, .point = {0.5f, 0.5f}},
	                 {.kind = Kind::PointerPress, .delaySeconds = 0.5f, .value = 3},
	                 {.kind = Kind::PointerRelease, .delaySeconds = 0.1f, .value = 3}},
	});
	// The hand pulls a conifer out of the ground: the tree leaves a pile of roots in its hole, which fades away
	all.push_back({
	    .id = "hand.pull_tree_roots_hole",
	    .name = "Tree pulled free leaves a hole of roots",
	    .facet = Facet::Hand,
	    .description =
	        "A conifer stands at the middle of the screen. After a second the pointer goes onto its trunk, "
	        "presses the Action button and moves up, so the hand pulls the tree. Two seconds later the button comes up.",
	    .expected = "The tree leans to the hand, comes free and goes into the hand with a puff of dust. A pile of roots "
	                "and earth stays in the hole where the tree stood, on the shape of the land. After 14 seconds of "
	                "game time the pile fades, and one second later it is gone. Only this computer draws the pile. The "
	                "inspector query magic.state shows it under roots_piles.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 12.0f, -20.0f}, .look = {0.0f, 5.0f, 10.0f}},
	    .objects = {{.type = TreeInfo::Conifer, .offset = {0.0f, 10.0f}}},
	    .commands = {{.kind = Kind::PointerOnObject, .delaySeconds = 1.0f, .object = 0, .amount = 6.0f},
	                 {.kind = Kind::PointerPress, .delaySeconds = 0.2f, .value = 3},
	                 {.kind = Kind::PointerSweep, .delaySeconds = 0.1f, .point = {0.0f, -0.25f}, .amount = 0.6f},
	                 {.kind = Kind::PointerRelease, .delaySeconds = 2.0f, .value = 3}},
	});
}

void AddLeash(std::vector<Scenario>& all)
{
	all.push_back({
	    .id = "leash.types",
	    .name = "Led on each leash",
	    .facet = Facet::Leash,
	    .description = "A tiger is put on the learning leash, then the aggression leash, then the compassion leash, a "
	                   "while each, and the leash is taken off. Move the hand about to lead it.",
	    .expected = "A rope runs from the hand to its collar, coloured for each leash; on the aggression leash anger is "
	                "what it wants most, on the compassion leash kindness and making friends, and on the learning leash "
	                "it watches the player. Taken off the compassion leash, that mood ends at once and its other wants "
	                "come back; taken off the learning leash, nothing it wants changes.",
	    .framing = {.shot = Shot::Testbed},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 40.0f})},
	    .commands = {Leash(0, LeashType::Rope, 1.0f), Leash(0, LeashType::Evil, 10.0f), Leash(0, LeashType::Good, 10.0f),
	                 Act(Kind::TakeOffLeash, 0, 10.0f), Act(Kind::Stop, 0, 3.0f)},
	    .repeatFrom = 0,
	});

	all.push_back({
	    .id = "leash.orders",
	    .name = "Orders given with the leash",
	    .facet = Facet::Leash,
	    .description = "A tiger on the learning leash is told with the Action button to go to a point to the east, told "
	                   "again before it gets there, then to act on a rock, a cow, and the lake.",
	    .expected = "Each order is answered with the acknowledge sound, a nod and the tiger's voice, and marked: six pale "
	                "yellow sparkles over the place, and a throbbing yellow ring with a paw print inside, which follows "
	                "the cow. Told again it runs and then waits a few seconds; the rock it looks at, the cow it picks "
	                "up and holds, at the lake it drinks or looks at its reflection. One marker at a time.",
	    .framing = {.shot = Shot::Overview, .include = {{60.0f, 40.0f}, {0.0f, k_NearShallows}}},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 20.0f})},
	    .objects = {{.type = FeatureInfo::FatPilarChalk, .offset = {-40.0f, 30.0f}},
	                {.type = AnimalInfo::Cow, .offset = {30.0f, 10.0f}}},
	    .commands = {Leash(0, LeashType::Rope, 1.0f), Go(Kind::LeashOrderAt, 0, {60.0f, 40.0f}, 1.0f, false),
	                 Go(Kind::LeashOrderAt, 0, {60.0f, 40.0f}, 2.0f, false), OnObject(Kind::LeashOrderOn, 0, 0, 10.0f, false),
	                 OnObject(Kind::LeashOrderOn, 0, 1, 12.0f, false),
	                 Go(Kind::LeashOrderAt, 0, {0.0f, k_NearShallows + 10.0f}, 15.0f, false),
	                 Act(Kind::TakeOffLeash, 0, 15.0f)},
	    .repeatFrom = 0,
	});

	// A man held by a script out in the lake, as the drowning men of Land 1's silver scroll, and a tiger that shrinks
	// in its temple's pen first, so that it grows again on its way out to him
	constexpr glm::vec2 k_RescueTemple {-150.0f, 150.0f};
	constexpr glm::vec2 k_DrowningMan {-30.0f, k_NearShallows + 8.0f};
	constexpr glm::vec2 k_Bank {-30.0f, k_NearShallows - 20.0f};
	all.push_back({
	    .id = "leash.rescue_from_water",
	    .name = "A man carried out of the water on the leash",
	    .facet = Facet::Leash,
	    .description = "A tiger of size 0.5 walks home into its temple's pen, where it shrinks, and then goes on the "
	                   "learning leash. The Action button of the leash orders it onto a man that a script holds still out "
	                   "in the lake, as the drowning men of Land 1, and some time later onto the bank.",
	    .expected = "It grows back to its own size as it leaves the pen, wades out, picks the man up and holds him, "
	                "facing the player. Told onto the bank, it carries him there and puts him down gently on his feet, "
	                "with no toss: he stays where its hand sets him.",
	    .framing = {.shot = Shot::Overview, .include = {k_RescueTemple, k_DrowningMan, k_Bank}},
	    .creatures = {[&] {
		    auto tiger = Content(CreatureType::Tiger, k_RescueTemple + glm::vec2(60.0f, -40.0f));
		    tiger.size = 0.5f;
		    return tiger;
	    }()},
	    .objects = {{.type = VillagerInfo::NorseFarmerMale, .offset = k_DrowningMan, .inScript = true}},
	    .temples = {{.offset = k_RescueTemple, .angle = 36.0f}},
	    .commands = {Act(Kind::WalkHome, 0, 1.0f), Leash(0, LeashType::Rope, 25.0f),
	                 OnObject(Kind::LeashOrderOn, 0, 0, 1.0f, false), Go(Kind::LeashOrderAt, 0, k_Bank, 45.0f, false)},
	});

	// A full size tiger stops its walk to a man just inside the reach of its hand, which is less straight ahead, so it
	// turns on the spot before it reaches
	constexpr glm::vec2 k_StillMan {40.0f, -40.0f};
	all.push_back({
	    .id = "leash.pick_up_turns_into_reach",
	    .name = "A full size creature turns to get a man in reach",
	    .facet = Facet::Leash,
	    .description = "A tiger of size 1 goes on the learning leash. The Action button of the leash orders it onto a "
	                   "man that a script holds still on the land in front of it.",
	    .expected = "It walks up to the man and stops a hand's reach from him. It turns a little on the spot, so that "
	                "the man is in front of its hand, then picks him up and holds him. Before, it reached straight "
	                "ahead with nothing in its hand.",
	    .framing = {.shot = Shot::Overview, .include = {{0.0f, 20.0f}, k_StillMan}},
	    .creatures = {[&] {
		    auto tiger = Content(CreatureType::Tiger, {0.0f, 20.0f});
		    tiger.size = 1.0f;
		    return tiger;
	    }()},
	    .objects = {{.type = VillagerInfo::NorseFarmerMale, .offset = k_StillMan, .inScript = true}},
	    .commands = {Leash(0, LeashType::Rope, 1.0f), OnObject(Kind::LeashOrderOn, 0, 0, 2.0f, false)},
	});

	all.push_back({
	    .id = "objects.pick_up_from_far",
	    .name = "A creature walks a long way to a small thing and picks it up",
	    .facet = Facet::Objects,
	    .description = "A tiger of size 1 has a lump of poo 40 metres in front of it. After one second its alignment jumps "
	                   "to fully evil, so its body takes the evil shape after it first measured its reach. After one and "
	                   "a half seconds more it is told to pick up the poo.",
	    .expected = "It walks up to the poo and stops a hand's reach from it. It turns a little on the spot if it must, "
	                "then picks the poo up and holds it. Before, it stopped a little too far away for the arms of its new "
	                "shape, reached for nothing and gave up the pick-up.",
	    .framing = {.shot = Shot::Overview, .include = {{0.0f, 60.0f}, {0.0f, 20.0f}}},
	    .creatures = {CreatureSetup {.species = CreatureType::Tiger, .offset = {0.0f, 60.0f}, .size = 1.0f}},
	    .objects = {{.type = MobileObjectInfo::LumpOfPoo, .offset = {0.0f, 20.0f}}},
	    .commands = {{.kind = Kind::SetBodyLook, .creature = 0, .delaySeconds = 1.0f, .value = 0, .amount = -1.0f},
	                 OnObject(Kind::PickUp, 0, 0, 1.5f)},
	});

	// Worn out and wanting to sleep more than anything, far from its player's temple
	CreatureSetup tired {.species = CreatureType::Tiger,
	                     .offset = {0.0f, 20.0f},
	                     .needs = {.energy = 1.0f, .exhaustion = 0.95f, .dehydration = 0.0f, .poo = 0.0f, .life = 1.0f},
	                     .desires = {{.desire = Desire::Tiredness, .fraction = 1.0f}}};
	all.push_back({
	    .id = "leash.tired_orders",
	    .name = "Orders given with the leash to a tired creature",
	    .facet = Facet::Leash,
	    .description = "A worn out tiger (exhaustion 0.95) that wants to sleep is on the learning leash, far from its "
	                   "player's temple. After eight seconds it is told to go to a point to the east, and after twenty "
	                   "seconds to a point to the west.",
	    .expected = "It sets off home to sleep. Each order stops that: it nods and walks to the place it is told. Once "
	                "there and the order is done, it sets off home to sleep again.",
	    .framing = {.shot = Shot::Overview, .include = {{60.0f, 40.0f}, {-60.0f, 40.0f}}},
	    .creatures = {tired},
	    .temples = {{.offset = {-300.0f, 100.0f}, .angle = 36.0f}},
	    .commands = {Leash(0, LeashType::Rope, 1.0f), Go(Kind::LeashOrderAt, 0, {60.0f, 40.0f}, 7.0f, false),
	                 Go(Kind::LeashOrderAt, 0, {-60.0f, 40.0f}, 12.0f, false)},
	});

	// Wholly exhausted, and as young as a creature on the first land, too young to faint
	auto exhausted = tired;
	exhausted.needs.exhaustion = 1.0f;
	exhausted.phase = 4;
	// It stays exhausted though it sleeps
	exhausted.hold = true;
	all.push_back({
	    .id = "leash.exhausted_orders",
	    .name = "Orders given with the leash to a wholly exhausted creature",
	    .facet = Facet::Leash,
	    .description = "A young, wholly exhausted tiger (exhaustion 1) is on the learning leash, far from its player's "
	                   "temple. It is too young to faint, and stays exhausted. After eight seconds it is told to go to a "
	                   "point to the east.",
	    .expected = "It takes no order: no nod, no marker, and it does not go east. The leash still keeps it near: if "
	                "it strays too far on its way to sleep, it walks back. Then it sleeps. An older creature this "
	                "exhausted faints and lies out cold.",
	    .framing = {.shot = Shot::Overview, .include = {{60.0f, 40.0f}, {-60.0f, 40.0f}}},
	    .creatures = {exhausted},
	    .temples = {{.offset = {-300.0f, 100.0f}, .angle = 36.0f}},
	    .commands = {Leash(0, LeashType::Rope, 1.0f), Go(Kind::LeashOrderAt, 0, {60.0f, 40.0f}, 7.0f, false)},
	});

	all.push_back({
	    .id = "leash.order_hurries",
	    .name = "An order with the leash hurries what the body does",
	    .facet = Facet::Leash,
	    .description = "Two tigers sit side by side. The left tiger is on the learning leash. After eight seconds, the "
	                   "left tiger is told to go to a point to the east. At the same time, the right tiger is told to "
	                   "stand up, with no order.",
	    .expected = "The left tiger gets up at two times the usual speed, nods, and walks east. The right tiger gets up "
	                "at the usual speed. A kiss, a cast and a throw keep their usual speed when an order comes.",
	    .framing = {.shot = Shot::Overview, .include = {{-15.0f, 20.0f}, {60.0f, 40.0f}}},
	    .creatures = {Content(CreatureType::Tiger, {-15.0f, 20.0f}), Content(CreatureType::Tiger, {15.0f, 20.0f})},
	    .commands = {Leash(0, LeashType::Rope, 1.0f), Act(Kind::SitDown, 0, 1.0f), Act(Kind::SitDown, 1, 0.0f),
	                 Go(Kind::LeashOrderAt, 0, {60.0f, 40.0f}, 8.0f, false), Act(Kind::StandUp, 1, 0.0f)},
	});

	all.push_back({
	    .id = "leash.marker_flat_and_slope",
	    .name = "The marker of a leash order on flat land and on a slope",
	    .facet = Facet::Leash,
	    .description = "A tiger far away on the learning leash is told with the Action button to go to a point on the "
	                   "flat land. Twenty seconds later it is told to go to a point halfway down the steep bank of the "
	                   "lake.",
	    .expected = "Each time a yellow ring hangs two metres above the point, with a paw print inside. The ring and the "
	                "print always face the camera and do not lie along the land, also on the bank. The ring throbs, its "
	                "width and its height a fifth of a second apart. The print stands on its side, with its top to the "
	                "left, and it is narrower than the ring.",
	    .framing = {.shot = Shot::Overview, .include = {{40.0f, -40.0f}, {0.0f, k_NearShallows - 28.0f}}},
	    .creatures = {Content(CreatureType::Tiger, {-150.0f, -100.0f})},
	    .commands = {Leash(0, LeashType::Rope, 1.0f), Go(Kind::LeashOrderAt, 0, {40.0f, -40.0f}, 1.0f, false),
	                 Go(Kind::LeashOrderAt, 0, {0.0f, k_NearShallows - 28.0f}, 20.0f, false),
	                 Act(Kind::TakeOffLeash, 0, 30.0f)},
	    .repeatFrom = 0,
	});

	all.push_back({
	    .id = "mind.step_looks_reflection",
	    .name = "Looks at its reflection with the faces of each step",
	    .facet = Facet::Mind,
	    .description = "A tiger that is not thirsty, on the learning leash, away from the lake, is told with the Action "
	                   "button to go to the lake.",
	    .expected = "It walks to the water of the nearest region of sea on its map of the land, its head on the place. "
	                "Then it looks down at the water four metres in front of its feet, puzzled, plays its dazed clip "
	                "with an amazed face, and looks down again, puzzled. It walks to the nearest coast with an amazed "
	                "face, looks down with a curious face, plays its dazed clip, walks back to the water amazed, and "
	                "looks down, first curious and then amazed. The coast and the water are near each other, so these "
	                "two walks are short or stop at once.",
	    .framing = {.shot = Shot::Overview, .include = {{0.0f, -60.0f}, {0.0f, k_NearShallows}}},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, -60.0f})},
	    .commands = {Leash(0, LeashType::Rope, 1.0f), Go(Kind::LeashOrderAt, 0, {0.0f, k_NearShallows + 10.0f}, 2.0f, false),
	                 Act(Kind::TakeOffLeash, 0, 45.0f)},
	});

	all.push_back({
	    .id = "mind.step_looks_set_fire",
	    .name = "Sets fire to a tree with the looks and faces of each step",
	    .facet = Facet::Mind,
	    .description = "A script tells a player's lion to set fire to an oak with a burning villager beside it.",
	    .expected = "It walks to the villager, its head on the villager, and picks him up. It walks to the oak, its head "
	                "on the oak. It turns to the oak with an angry face and flutters its eyelids at it, throws the villager "
	                "at it with a smile, watches him until he lands, then holds still a second with an angry face.",
	    .framing = {.shot = Shot::Testbed},
	    .creatures = {Content(CreatureType::Lion, {0.0f, 0.0f}, 90.0f)},
	    .objects = {{.type = TreeInfo::Oak, .offset = {-60.0f, 40.0f}},
	                {.type = VillagerInfo::CelticFarmerMale, .offset = {-20.0f, 10.0f}, .alight = true}},
	    .commands = {{.kind = Kind::DoAction, .delaySeconds = 1.0f, .object = 0, .action = "SetFireToObject", .with = 1}},
	});

	all.push_back({
	    .id = "senses.look_at_mover",
	    .name = "Looks at a thing that moves as it walks",
	    .facet = Facet::Senses,
	    .description = "A tiger on the learning leash is told to go to a point far to the north. Behind it, two "
	                   "villagers walk away to the west and to the east.",
	    .expected = "On its way the head looks at the point for two seconds. For the next two seconds it turns back to "
	                "a villager that walks, at the place where the tiger last saw the villager move. Then it looks from "
	                "side to side for two seconds. Near the point it looks only at the point.",
	    .framing = {.shot = Shot::Overview, .include = {{0.0f, 0.0f}, {0.0f, 110.0f}}},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 0.0f})},
	    .objects = {{.type = VillagerInfo::CelticForesterMale,
	                 .offset = {-30.0f, -5.0f},
	                 .walkTo = glm::vec2(-90.0f, -5.0f),
	                 .walkAfterSeconds = 1.0f,
	                 .walkRepeatSeconds = 30.0f},
	                {.type = VillagerInfo::CelticHousewifeFemale,
	                 .offset = {30.0f, -5.0f},
	                 .walkTo = glm::vec2(90.0f, -5.0f),
	                 .walkAfterSeconds = 1.0f,
	                 .walkRepeatSeconds = 30.0f}},
	    .commands = {Leash(0, LeashType::Rope, 1.0f), Go(Kind::LeashOrderAt, 0, {0.0f, 110.0f}, 2.0f, false),
	                 Go(Kind::LeashOrderAt, 0, {0.0f, 0.0f}, 30.0f, false)},
	    .repeatFrom = 1,
	});

	all.push_back({
	    .id = "senses.hang_around_at_home",
	    .name = "Hangs around at home",
	    .facet = Facet::Senses,
	    .description = "A tiger kept within 40 of its home at the temple is told to hang around at home, again and again. "
	                   "A birch stands to the west as the thing that the order names.",
	    .expected = "It walks home and waits for two to five seconds while it looks about. Then, one time in ten, it yawns. "
	                "Seven times in ten, it walks 32 out from home away from the temple, faces away from the temple and "
	                "sits or points. Then it turns to the temple and sits for ten to fifteen seconds. Two times in ten, it "
	                "only turns to the temple and sits.",
	    .environment = {.dispenserGrid = false, .temple = glm::vec2(0.0f, 80.0f)},
	    .framing = {.shot = Shot::Overview, .include = {{0.0f, 0.0f}, {0.0f, 80.0f}}},
	    .creatures = {Content(CreatureType::Tiger, {-30.0f, 0.0f})},
	    .objects = {{.type = TreeInfo::Birch, .offset = {-60.0f, 0.0f}}},
	    .commands = {{.kind = Kind::ConfineToHome, .delaySeconds = 0.5f, .radius = 40.0f},
	                 {.kind = Kind::DoAction, .delaySeconds = 1.0f, .object = 0, .action = "HangAroundAtHome"},
	                 {.kind = Kind::DoAction, .delaySeconds = 40.0f, .object = 0, .action = "HangAroundAtHome"}},
	    .repeatFrom = 2,
	});

	all.push_back({
	    .id = "leash.temple_leashes",
	    .name = "The leashes hanging on the temple",
	    .facet = Facet::Leash,
	    .description = "Your tiger knows the learning leash. The temple's three leashes are hung in a row in front of "
	                   "the camera; then it is taught the aggression and compassion leashes, the aggression leash is "
	                   "tapped, and tapped again.",
	    .expected = "At first only the learning leash hangs, a twisted rope collar in a white smoky glow, tumbling slowly "
	                "with its texture running round it. Taught the others, the spiked blade (aggression) and the rainbow "
	                "fur (compassion) hang beside it. Tapped with the click sound, the aggression leash is carried in the "
	                "hand and its glow at the temple turns orange; tapped again it hangs back in place, silently. The "
	                "hand over each names it: Leash Of Aggression, Leash Of Learning, Leash Of Compassion.",
	    .framing = {.shot = Shot::Placed, .eye = {6.0f, 6.0f, 26.0f}, .look = {9.0f, 5.0f, 40.0f}},
	    .creatures = {Posed(CreatureType::Tiger, {-20.0f, 70.0f})},
	    .commands = {Leash(0, LeashType::Rope, 0.5f),
	                 Act(Kind::TakeOffLeash, 0, 0.5f),
	                 Go(Kind::HangLeashPosts, 0, {14.0f, 40.0f}, 0.5f, false),
	                 Leash(0, LeashType::Evil, 4.0f),
	                 Leash(0, LeashType::Good, 0.5f),
	                 Act(Kind::TakeOffLeash, 0, 0.5f),
	                 {.kind = Kind::TapLeashPost, .creature = 0, .delaySeconds = 4.0f, .value = 0},
	                 {.kind = Kind::TapLeashPost, .creature = 0, .delaySeconds = 4.0f, .value = 0}},
	});

	const auto templeGlows = [](std::string_view id, std::string_view name, float hour) {
		return Scenario {
		    .id = id,
		    .name = name,
		    .facet = Facet::Leash,
		    .description = "Your tiger knows all three leashes and your temple stands in front of the camera, its leash posts "
		                   "in view. The learning leash is picked at its post, so the hand carries it.",
		    .expected = "The aggression leash (the spiked blade) and the compassion leash (the rainbow fur) tumble slowly on "
		                "their posts, each in a pale smoky glow about four metres across, half seen by day and fading at "
		                "night. By the empty learning post the glow is orange and added to what is behind it. The learning "
		                "leash, a twisted rope collar, hangs round the hand's wrist, turned with the hand, its texture "
		                "running round it. Tapping the orange glow puts the leash back on its post, silently; with the hand "
		                "put away the carried leash and its orange glow are gone.",
		    .environment = {.hour = hour, .clockRuns = false, .dispenserGrid = false},
		    .framing = {.shot = Shot::Placed, .eye = {-262.0f, 30.0f, 120.0f}, .look = {-300.0f, 5.0f, 100.0f}},
		    .creatures = {Posed(CreatureType::Tiger, {-240.0f, 40.0f}, 90.0f)},
		    .temples = {{.offset = {-300.0f, 100.0f}, .angle = 36.0f}},
		    .commands = {Leash(0, LeashType::Rope, 0.5f),
		                 Leash(0, LeashType::Evil, 0.5f),
		                 Leash(0, LeashType::Good, 0.5f),
		                 Act(Kind::TakeOffLeash, 0, 0.5f),
		                 {.kind = Kind::TapLeashPost, .creature = 0, .delaySeconds = 1.0f, .value = 1}},
		};
	};
	all.push_back(templeGlows("leash.temple_glows", "The glows round the temple's leashes by day", 12.0f));
	all.push_back(templeGlows("leash.temple_glows_night", "The glows round the temple's leashes at night", 23.0f));

	all.push_back({
	    .id = "leash.l_key_picks_post",
	    .name = "The leash key picks the leash at the temple",
	    .facet = Facet::Leash,
	    .description = "Your tiger knows all three leashes and your temple stands in front of the camera. The compassion "
	                   "leash is tapped at its post, then tapped again, so no leash is picked. Then the leash key puts "
	                   "the compassion leash on. Last, the leash key takes it off.",
	    .expected = "After the second tap, no glow is orange and the hand carries no leash. When the leash key puts the "
	                "leash on, the compassion leash (the rainbow fur) goes round the hand's wrist again and its glow "
	                "turns orange. When the leash comes off, the leash stays in the hand and its glow stays orange.",
	    .environment = {.hour = 12.0f, .clockRuns = false, .dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {-262.0f, 30.0f, 120.0f}, .look = {-300.0f, 5.0f, 100.0f}},
	    .creatures = {Posed(CreatureType::Tiger, {-240.0f, 40.0f}, 90.0f)},
	    .temples = {{.offset = {-300.0f, 100.0f}, .angle = 36.0f}},
	    .commands = {{.kind = Kind::TapLeashPost, .creature = 0, .delaySeconds = 1.0f, .value = 2},
	                 {.kind = Kind::TapLeashPost, .creature = 0, .delaySeconds = 2.0f, .value = 2},
	                 {.kind = Kind::LeashKey, .delaySeconds = 3.0f, .value = 0},
	                 {.kind = Kind::LeashKey, .delaySeconds = 3.0f, .value = 0}},
	});

	all.push_back({
	    .id = "leash.pull_to_hand",
	    .name = "Pulled to the hand",
	    .facet = Facet::Leash,
	    .description = "A tiger far off to the north is put on the learning leash. Put the hand on the land near the "
	                   "camera, well away from it.",
	    .expected = "When the tiger is further from the hand than the full length of the leash, it stops what it does "
	                "and walks back. It is faster when it is further away. It walks to the point below the hand. When it "
	                "goes out of the area again, it walks back again.",
	    .framing = {.shot = Shot::Testbed},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 150.0f})},
	    .commands = {Leash(0, LeashType::Rope, 1.0f)},
	});

	all.push_back({
	    .id = "leash.walk_back_on_the_way",
	    .name = "Walked back while on the way",
	    .facet = Facet::Leash,
	    .description = "A tiger starts in the middle of the testbed. It is kept within 400 units of where it stands, as "
	                   "a young creature is kept near its home. After one second its player orders it to look at its "
	                   "reflection, and it walks north to the shore of the lake. After six seconds, on its way, the "
	                   "area that holds it becomes 10 units wide.",
	    .expected = "When the area becomes small, the tiger is out of it. It turns at once and walks back to where it "
	                "started. It does not walk on to the lake first.",
	    .framing = {.shot = Shot::Overview, .include = {{-60.0f, -20.0f}, {60.0f, 160.0f}}},
	    .creatures = {CreatureSetup {
	        .species = CreatureType::Tiger,
	        .offset = {0.0f, 0.0f},
	    }},
	    .commands = {{.kind = Kind::ConfineToHome, .delaySeconds = 0.1f, .radius = 400.0f},
	                 {.kind = Kind::ForceAction, .delaySeconds = 1.0f, .action = "LookAtReflection"},
	                 {.kind = Kind::ConfineToHome, .delaySeconds = 6.0f, .radius = 10.0f}},
	});

	all.push_back({
	    .id = "leash.tied",
	    .name = "Tied to a tree and a rock",
	    .facet = Facet::Leash,
	    .description = "A tiger on the learning leash is tied to a tree and told to walk far off, untied back to the "
	                   "hand, then tied to a pillar of rock and told to walk far off again.",
	    .expected = "The rope runs from what it is tied to, and it is kept within the rope's length of it however far "
	                "it is told to walk; untied, the rope goes back to the hand.",
	    .framing = {.shot = Shot::Overview, .include = {{-120.0f, 60.0f}, {120.0f, 60.0f}}},
	    .creatures = {Posed(CreatureType::Tiger, {0.0f, 0.0f})},
	    .objects = {{.type = TreeInfo::Oak, .offset = {-40.0f, 10.0f}},
	                {.type = FeatureInfo::FatPilarChalk, .offset = {40.0f, 10.0f}}},
	    .commands = {Leash(0, LeashType::Rope, 1.0f), OnObject(Kind::TieLeash, 0, 0, 1.0f, false),
	                 Go(Kind::WalkTo, 0, {-120.0f, 60.0f}, 1.0f, false), Act(Kind::UntieLeash, 0, 8.0f),
	                 OnObject(Kind::TieLeash, 0, 1, 2.0f, false), Go(Kind::WalkTo, 0, {120.0f, 60.0f}, 1.0f, false),
	                 Act(Kind::TakeOffLeash, 0, 8.0f)},
	    .repeatFrom = 0,
	});

	all.push_back({
	    .id = "leash.hand_attach_sounds",
	    .name = "Attach sounds of the leash in the hand",
	    .facet = Facet::Leash,
	    .description = "A tiger on the learning leash is tied to a tree and untied back to the hand two times, every "
	                   "four seconds.",
	    .expected = "Each tie and each return to the hand plays a leash-attach sound. The two sounds play in turn. The "
	                "first return to the hand plays the first sound, and the second return plays the second sound.",
	    .framing = {.shot = Shot::Overview, .include = {{-40.0f, 10.0f}, {0.0f, 0.0f}}},
	    .creatures = {Posed(CreatureType::Tiger, {0.0f, 0.0f})},
	    .objects = {{.type = TreeInfo::Oak, .offset = {-40.0f, 10.0f}}},
	    .commands = {Leash(0, LeashType::Rope, 1.0f), OnObject(Kind::TieLeash, 0, 0, 1.0f, false),
	                 Act(Kind::UntieLeash, 0, 4.0f), OnObject(Kind::TieLeash, 0, 0, 4.0f, false),
	                 Act(Kind::UntieLeash, 0, 4.0f)},
	    .repeatFrom = 0,
	});

	all.push_back({
	    .id = "leash.tied_thrown",
	    .name = "Tied to a thrown rock",
	    .facet = Facet::Leash,
	    .description = "A tiger on the learning leash is tied to a rock of nobody's, which is then thrown off to the "
	                   "east, every eight seconds.",
	    .expected = "The leash stays tied to the rock as it flies and after it lands: the rope runs from the rock "
	                "throughout and never goes back to the hand.",
	    .framing = {.shot = Shot::Overview, .include = {{-30.0f, 10.0f}, {60.0f, 10.0f}, {0.0f, 0.0f}}},
	    .creatures = {Posed(CreatureType::Tiger, {0.0f, 0.0f})},
	    .objects = {{.type = MobileStaticInfo::Rock, .offset = {-30.0f, 10.0f}}},
	    .commands = {Leash(0, LeashType::Rope, 1.0f), OnObject(Kind::TieLeash, 0, 0, 1.0f, false)},
	    .throws = {{.object = 0, .height = 4.0f, .velocity = {12.0f, 8.0f, 0.0f}, .delaySeconds = 3.0f, .repeatSeconds = 8.0f}},
	});

	all.push_back({
	    .id = "leash.tie_by_hand",
	    .name = "Ordering and tying with the hand",
	    .facet = Facet::Leash,
	    .description = "Your tiger is clicked to put the leash on, a tree and a pillar of rock stand ahead. With the hand: "
	                   "right click the tree once, then double click it, then double click it again; right click the "
	                   "rock, and double click the rock.",
	    .expected = "Over the tree with the leash in the hand the tip says Attach Leash. A single click sends the tiger to "
	                "act on the tree (marked); the double click ties the leash to it, the rope running from the tree, and "
	                "over the tree the tip now says Detach Leash; double clicked again it comes back to the hand. The "
	                "same for the rock. The hand never pulls the tree up on a quick click.",
	    .framing = {.shot = Shot::Overview, .include = {{-40.0f, 60.0f}, {40.0f, 60.0f}, {0.0f, 20.0f}}},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 20.0f})},
	    .objects = {{.type = TreeInfo::Oak, .offset = {-40.0f, 60.0f}},
	                {.type = FeatureInfo::FatPilarChalk, .offset = {40.0f, 60.0f}}},
	    .commands = {{.kind = Kind::HandTapLeash, .creature = 0, .delaySeconds = 1.0f}},
	});

	all.push_back({
	    .id = "leash.snap_rope",
	    .name = "Snap the tied leash",
	    .facet = Facet::Leash,
	    .description = "Your tiger's leash is tied to a tree. Put the hand on the rope (the hand opens its fingers), hold "
	                   "the right button to take hold of it, drag it out over the land and let go.",
	    .expected = "Over the rope the hand opens its fingers to pinch it and no tip shows. Holding it, the fingers pinch "
	                "it and the rope runs as two, one from the collar and one from the tree, both to the hand. Let go, the "
	                "tiger is sent to the place under the hand (marked) and is kept within the distance from the tree to "
	                "where the hand let go.",
	    .framing = {.shot = Shot::Overview, .include = {{-40.0f, 60.0f}, {60.0f, 20.0f}, {0.0f, 0.0f}}},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 20.0f})},
	    .objects = {{.type = TreeInfo::Oak, .offset = {-40.0f, 60.0f}}},
	    .commands = {Leash(0, LeashType::Rope, 1.0f), OnObject(Kind::TieLeash, 0, 0, 1.0f, false)},
	});

	// The hand that holds the rope of the tied leash stands straight up, also when the pointer leaves the rope
	all.push_back({
	    .id = "hand.rope_hold_stand",
	    .name = "The hand that holds the rope of the tied leash",
	    .facet = Facet::Hand,
	    .description = "Your tiger's leash is tied to a tree, and the tiger lies down to sleep. The pointer goes onto the "
	                   "rope and the Action button takes hold of it. The pointer then moves away from the rope, across "
	                   "the lake onto its far bank, and the button comes up. Then the pointer moves a little to the "
	                   "right.",
	    .expected = "Holding the rope, the hand pinches it and stands straight up, not turned to the slope of the bank, "
	                "also away from the rope. It does not offer to turn the camera. Let go, it turns to the slope when the "
	                "pointer moves across the screen.",
	    .framing = {.shot = Shot::Placed, .eye = {-20.0f, 40.0f, 20.0f}, .look = {-20.0f, 0.0f, 120.0f}},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 75.0f})},
	    .objects = {{.type = TreeInfo::Oak, .offset = {-40.0f, 100.0f}}},
	    .commands = {Leash(0, LeashType::Rope, 1.0f),
	                 OnObject(Kind::TieLeash, 0, 0, 1.0f, false),
	                 Act(Kind::Sleep, 0, 0.5f),
	                 {.kind = Kind::PointerOnRope, .creature = 0, .delaySeconds = 5.0f},
	                 {.kind = Kind::PointerOnRope, .creature = 0, .delaySeconds = 0.5f},
	                 {.kind = Kind::PointerPress, .delaySeconds = 0.05f, .value = 3},
	                 {.kind = Kind::PointerSweep, .delaySeconds = 0.5f, .point = {0.12f, -0.3f}, .amount = 4.0f},
	                 {.kind = Kind::PointerRelease, .delaySeconds = 5.5f, .value = 3},
	                 {.kind = Kind::PointerSweep, .delaySeconds = 1.5f, .point = {0.03f, 0.0f}, .amount = 0.5f}},
	});

	all.push_back({
	    .id = "leash.hand",
	    .name = "Leash your creature with the hand",
	    .facet = Facet::Leash,
	    .description = "Your tiger stands to the north. The hand clicks it with the right button, putting the learning "
	                   "leash on. Move the hand about to lead it; press L, right click it or shake the hand to "
	                   "take the leash off again, and right click it again to put it back on.",
	    .expected = "A rope runs from the hand to its collar; pulled taut, the tiger walks to the hand. The readout says "
	                "\"on\".",
	    .framing = {.shot = Shot::Testbed},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 60.0f})},
	    .commands = {{.kind = Kind::HandTapLeash, .creature = 0, .delaySeconds = 1.0f}},
	});

	// The real button clicks the creature that wears the leash: the click is the leash key, so the leash comes off
	all.push_back({
	    .id = "leash.click_takes_off",
	    .name = "A click on your leashed creature takes the leash off",
	    .facet = Facet::Leash,
	    .description = "Your tiger faces the camera in the middle of the screen and wears the learning leash. The action "
	                   "button clicks the tiger for 0.35 seconds and the hand moves to the right. Then the hand comes "
	                   "back, clicks the tiger again and moves to the right again. With the leash held loose, a click "
	                   "must be longer than the wait for a tap.",
	    .expected = "After the first click no rope follows the hand: the click took the leash off. After the second "
	                "click a rope runs from the hand to the collar again. The log shows \"The draw of the hand sends the "
	                "leash tap\" for each click, and a turn later \"Leash command: player 0 taps a creature\".",
	    .environment = {.dispenserGrid = false, .cursor = glm::vec2(0.5f, 0.5f)},
	    .framing = {.shot = Shot::Follow, .distance = 1.2f},
	    .creatures = {Posed(CreatureType::Tiger, {0.0f, 0.0f})},
	    .commands = {Leash(0, LeashType::Rope, 0.5f),
	                 {.kind = Kind::PointerPress, .delaySeconds = 2.0f, .value = 3},
	                 {.kind = Kind::PointerRelease, .delaySeconds = 0.35f, .value = 3},
	                 {.kind = Kind::PointerSweep, .delaySeconds = 0.5f, .point = {0.25f, 0.0f}, .amount = 1.0f},
	                 {.kind = Kind::PointerSweep, .delaySeconds = 2.5f, .point = {-0.25f, 0.0f}, .amount = 1.0f},
	                 {.kind = Kind::PointerPress, .delaySeconds = 1.5f, .value = 3},
	                 {.kind = Kind::PointerRelease, .delaySeconds = 0.35f, .value = 3},
	                 {.kind = Kind::PointerSweep, .delaySeconds = 0.5f, .point = {0.25f, 0.0f}, .amount = 1.0f}},
	});

	all.push_back({
	    .id = "audio.leash_tie_placed",
	    .name = "The tie sound of another player's leash",
	    .facet = Facet::Audio,
	    .description = "Your tiger on the left is tied to a tree. Then player two's lion on the right is tied to a "
	                   "pillar of rock.",
	    .expected = "The tie of your tiger's leash is heard in the middle, with no place. The tie of the lion's leash "
	                "is heard from the rock on the right.",
	    .framing = {.shot = Shot::Testbed},
	    .creatures = {Content(CreatureType::Tiger, {-30.0f, 60.0f}),
	                  [] {
		                  auto lion = Content(CreatureType::Lion, {30.0f, 60.0f});
		                  lion.owner = PlayerNames::PLAYER_TWO;
		                  return lion;
	                  }()},
	    .objects = {{.type = TreeInfo::Oak, .offset = {-60.0f, 70.0f}},
	                {.type = FeatureInfo::FatPilarChalk, .offset = {60.0f, 70.0f}}},
	    .commands = {OnObject(Kind::TieLeash, 0, 0, 1.0f, false), OnObject(Kind::TieLeash, 1, 1, 3.0f, false)},
	});

	all.push_back({
	    .id = "leash.someone_elses",
	    .name = "Another player's creature refuses the leash",
	    .facet = Facet::Leash,
	    .description = "Your tiger on the left and player two's lion on the right. Your hand taps the lion, then your "
	                   "tiger.",
	    .expected = "The lion is refused, as it belongs to another player, and the log says so; then your tiger is "
	                "leashed. Right clicking the lion yourself is refused the same way.",
	    .framing = {.shot = Shot::Testbed},
	    .creatures = {Content(CreatureType::Tiger, {-30.0f, 60.0f}),
	                  [] {
		                  auto lion = Content(CreatureType::Lion, {30.0f, 60.0f});
		                  lion.owner = PlayerNames::PLAYER_TWO;
		                  return lion;
	                  }()},
	    .commands = {{.kind = Kind::HandTapLeash, .creature = 1, .delaySeconds = 1.0f},
	                 {.kind = Kind::HandTapLeash, .creature = 0, .delaySeconds = 2.0f}},
	});

	all.push_back({
	    .id = "leash.only_leashable",
	    .name = "Only the leashable one of two",
	    .facet = Facet::Leash,
	    .description = "You have two tigers: the left one is the one you can lead, the right one isn't. The hand taps "
	                   "the right one, then the left one.",
	    .expected = "The right tiger is refused as it isn't the one you can lead; the left one is leashed.",
	    .framing = {.shot = Shot::Testbed},
	    .creatures = {Content(CreatureType::Tiger, {-30.0f, 60.0f}), Content(CreatureType::Tiger, {30.0f, 60.0f})},
	    .commands = {{.kind = Kind::HandTapLeash, .creature = 1, .delaySeconds = 1.0f},
	                 {.kind = Kind::HandTapLeash, .creature = 0, .delaySeconds = 2.0f}},
	});

	all.push_back({
	    .id = "leash.switch",
	    .name = "Switch the leashable creature",
	    .facet = Facet::Leash,
	    .description = "Your left tiger is on the leash. The right tiger is made the one you can lead, then the hand "
	                   "taps the left one and the right one.",
	    .expected = "Made leashable, the right tiger takes over: the left one's leash comes off and tapping it is "
	                "refused, and the right one is leashed.",
	    .framing = {.shot = Shot::Testbed},
	    .creatures = {Content(CreatureType::Tiger, {-30.0f, 60.0f}), Content(CreatureType::Tiger, {30.0f, 60.0f})},
	    .commands = {{.kind = Kind::HandTapLeash, .creature = 0, .delaySeconds = 1.0f},
	                 {.kind = Kind::MakeLeashable, .creature = 1, .delaySeconds = 4.0f},
	                 {.kind = Kind::HandTapLeash, .creature = 0, .delaySeconds = 2.0f},
	                 {.kind = Kind::HandTapLeash, .creature = 1, .delaySeconds = 2.0f}},
	});

	all.push_back({
	    .id = "leash.keys",
	    .name = "The leash shortcuts",
	    .facet = Facet::Leash,
	    .description = "Your tiger knows all three leashes. The keys are pressed in turn: L puts the learning leash "
	                   "on, B steps down to the aggression leash, V steps back up to the learning leash and on up to "
	                   "the compassion leash, and L takes it off, however many leashes it knows.",
	    .expected = "The rope goes on, changes colour three times, and comes off; the readout names each leash. "
	                "Pressing L, V and B yourself does the same.",
	    .framing = {.shot = Shot::Testbed},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 60.0f})},
	    .commands = {{.kind = Kind::LeashKey, .delaySeconds = 1.0f, .value = 0},
	                 {.kind = Kind::LeashKey, .delaySeconds = 3.0f, .value = 2},
	                 {.kind = Kind::LeashKey, .delaySeconds = 3.0f, .value = 1},
	                 {.kind = Kind::LeashKey, .delaySeconds = 3.0f, .value = 1},
	                 {.kind = Kind::LeashKey, .delaySeconds = 3.0f, .value = 0}},
	    .repeatFrom = 0,
	});

	all.push_back({
	    .id = "leash.key_next_turn",
	    .name = "The leash key acts at the start of the next turn",
	    .facet = Facet::Leash,
	    .description = "Your tiger knows the learning leash. The scenario presses the leash key through the game's "
	                   "controls, as a player does, and three seconds later presses it again. The key makes a command "
	                   "of the player, and the world does the command at the start of the next game turn.",
	    .expected = "The rope goes on one or two game turns after the press (a tenth to a fifth of a second), not on the "
	                "frame of the press, and comes off again after the second press. The players.commands query shows "
	                "the command that waits.",
	    .framing = {.shot = Shot::Testbed},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 60.0f})},
	    .commands = {{.kind = Kind::PressKey,
	                  .delaySeconds = 1.0f,
	                  .value = static_cast<size_t>(input::BindableActionMap::LEASH_UNLEASH_CREATURE)},
	                 {.kind = Kind::PressKey,
	                  .delaySeconds = 3.0f,
	                  .value = static_cast<size_t>(input::BindableActionMap::LEASH_UNLEASH_CREATURE)}},
	});

	all.push_back({
	    .id = "leash.click_or_hold",
	    .name = "Hold to stroke, right click to leash",
	    .facet = Facet::Leash,
	    .description = "The hand holds on to your tiger with the right button, stroking its head and belly, and lets go; "
	                   "then it clicks the tiger, which puts the leash on. Try it yourself: holding the right button on "
	                   "it for a second strokes it, sweeping the held hand across it slaps it, and a quick right click "
	                   "leashes it.",
	    .expected = "The hold strokes it without leashing it; the click then puts the rope on without stroking it.",
	    .framing = {.shot = Shot::Testbed},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 50.0f})},
	    .commands = {Stroke(0, creature_feedback::BodyPart::Head, 1.0f),
	                 Stroke(0, creature_feedback::BodyPart::Belly, 2.5f),
	                 Act(Kind::HandLetGo, 0, 2.0f, true),
	                 {.kind = Kind::HandTapLeash, .creature = 0, .delaySeconds = 2.0f}},
	});

	all.push_back({
	    .id = "leash.shake",
	    .name = "Shake the leash off",
	    .facet = Facet::Leash,
	    .description = "Your tiger is clicked to put the leash on, and a scribble is then drawn with the empty hand, as "
	                   "the game shakes a leash off. Draw it yourself (move the mouse quickly side to side a few times, "
	                   "no button held) to try it.",
	    .expected = "The rope goes on, and comes off once the scribble is recognised; the log says so.",
	    .framing = {.shot = Shot::Testbed},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 50.0f})},
	    .commands = {{.kind = Kind::HandTapLeash, .creature = 0, .delaySeconds = 1.0f},
	                 {.kind = Kind::LeashShake, .creature = 0, .delaySeconds = 4.0f}},
	    .repeatFrom = 0,
	});

	all.push_back({
	    .id = "leash.key_off",
	    .name = "L takes the leash off",
	    .facet = Facet::Leash,
	    .description = "Your tiger is clicked to put the leash on, then L is pressed.",
	    .expected = "The rope goes on, then comes off with L.",
	    .framing = {.shot = Shot::Testbed},
	    .creatures = {Content(CreatureType::Tiger, {0.0f, 50.0f})},
	    .commands = {{.kind = Kind::HandTapLeash, .creature = 0, .delaySeconds = 1.0f},
	                 {.kind = Kind::LeashKey, .delaySeconds = 4.0f, .value = 0}},
	    .repeatFrom = 0,
	});

	all.push_back({
	    .id = "leash.other_gods_creatures",
	    .name = "Other gods' creatures can't be slapped",
	    .facet = Facet::Leash,
	    .description = "Your tiger on the left, a tortoise of player three (as Khazar's is) in the middle and a wolf of "
	                   "player four (as Lethys's is) on the right. The hand tries to slap the tortoise and the wolf, "
	                   "then to leash the tortoise.",
	    .expected = "Neither is slapped: the hand never takes hold of another god's creature, inside the player's "
	                "influence or out, and the log says so. Leashing the tortoise is refused too, as it isn't yours.",
	    .framing = {.shot = Shot::Testbed},
	    .creatures = {Content(CreatureType::Tiger, {-40.0f, 60.0f}),
	                  [] {
		                  auto tortoise = Content(CreatureType::Tortoise, {0.0f, 60.0f});
		                  tortoise.owner = PlayerNames::PLAYER_THREE;
		                  return tortoise;
	                  }(),
	                  [] {
		                  auto wolf = Content(CreatureType::Wolf, {40.0f, 60.0f});
		                  wolf.owner = PlayerNames::PLAYER_FOUR;
		                  return wolf;
	                  }()},
	    .commands = {Slap(1, 0.85f, false, true, 1.0f),
	                 Act(Kind::HandLetGo, 1, 2.0f, true),
	                 Slap(2, 0.85f, false, true, 1.0f),
	                 Act(Kind::HandLetGo, 2, 2.0f, true),
	                 {.kind = Kind::HandTapLeash, .creature = 1, .delaySeconds = 1.0f}},
	});

	all.push_back({
	    .id = "leash.home",
	    .name = "Kept at home",
	    .facet = Facet::Leash,
	    .description = "A tiger is kept within 40 units of where it stands, as a young creature is kept near its home, "
	                   "and told to walk 120 units away.",
	    .expected = "Once it strays past the radius it turns back and walks home again.",
	    .framing = {.shot = Shot::Overview, .include = {{120.0f, 0.0f}, {-40.0f, 0.0f}}},
	    .creatures = {Posed(CreatureType::Tiger, {0.0f, 0.0f}, 90.0f)},
	    .commands = {{.kind = Kind::ConfineToHome, .delaySeconds = 0.5f, .radius = 40.0f},
	                 Go(Kind::WalkTo, 0, {120.0f, 0.0f}, 1.0f),
	                 Go(Kind::WalkTo, 0, {0.0f, 0.0f}, 6.0f)},
	    .repeatFrom = 1,
	});
}

/// A content tiger for a fight, its owner, strength and alignment given
CreatureSetup Fighter(glm::vec2 offset, float facing, std::string_view label, PlayerNames owner, float strength,
                      float alignment)
{
	auto creature = Content(CreatureType::Tiger, offset, facing, label);
	creature.owner = owner;
	creature.strength = strength;
	creature.alignment = alignment;
	return creature;
}

void AddCombat(std::vector<Scenario>& all)
{
	constexpr size_t k_High = 0;
	constexpr size_t k_Mid = 1;
	constexpr size_t k_Low = 2;
	constexpr size_t k_Forward = 0;
	constexpr size_t k_Back = 1;
	const glm::vec2 left {-25.0f, 40.0f};
	const glm::vec2 right {25.0f, 40.0f};
	const auto red = [&](PlayerNames owner, float strength, float alignment) {
		return Fighter(left, 90.0f, "red", owner, strength, alignment);
	};
	const auto blue = [&](PlayerNames owner, float strength, float alignment) {
		return Fighter(right, 270.0f, "blue", owner, strength, alignment);
	};
	const Framing ringside {.shot = Shot::Overview, .include = {{-45.0f, 40.0f}, {45.0f, 40.0f}}, .distance = 0.8f};

	all.push_back({
	    .id = "combat.ai_duel",
	    .name = "Two tigers fight by themselves",
	    .facet = Facet::Combat,
	    .description = "Two somewhat evil tigers, so aggressive fighters, the red one stronger, are told to fight, and both "
	                   "fight by themselves.",
	    .expected = "They walk to their places either side of the arena, face each other and maybe taunt, then duel: "
	                "blows high, in the middle and low with their reels, blocks and steps. The panel at the top left "
	                " shows both fight healths falling. The loser faints, and the winner finishes and shows "
	                "off.",
	    .framing = ringside,
	    .creatures = {red(PlayerNames::PLAYER_ONE, 0.9f, -0.45f), blue(PlayerNames::PLAYER_TWO, 0.3f, -0.45f)},
	    .commands = {Fight(Kind::StartFight, 0, 1, 1.0f), Fight(Kind::FightAuto, 0, 1, 0.2f)},
	});

	all.push_back({
	    .id = "combat.blow_no_wobble",
	    .name = "A struck tiger reels with no wobble on top",
	    .facet = Facet::Combat,
	    .description = "Two tigers duel by themselves, the red one stronger, so that blows land on both bodies.",
	    .expected = "Each blow that lands makes the struck body reel high, in the middle or low. No second clip shakes "
	                "the body on top of the reel. The animation query shows only the body and blend layers, and maybe a "
	                "face or a gesture.",
	    .framing = ringside,
	    .creatures = {red(PlayerNames::PLAYER_ONE, 0.9f, -0.45f), blue(PlayerNames::PLAYER_TWO, 0.3f, -0.45f)},
	    .commands = {Fight(Kind::StartFight, 0, 1, 1.0f), Fight(Kind::FightAuto, 0, 1, 0.2f)},
	});

	// The first land's Ogre, and creatures on later lands, heal by a script after they win a fight: the script gives the
	// victor a hundredth of life every six seconds, until it is full or it fights again
	const auto healScript = [](float delay) {
		return Command {.kind = Kind::RunScript,
		                .delaySeconds = delay,
		                .script = "HealFightVictorOverTime",
		                .scriptArguments = {{.kind = ScriptArgument::Kind::Creature, .index = 0}}};
	};
	const auto victor = [&](PlayerNames owner) {
		auto creature = red(owner, 0.6f, -0.45f);
		creature.label = "victor";
		creature.needs.life = 0.9f;
		creature.hold = false;
		return creature;
	};
	all.push_back({
	    .id = "combat.victor_heals_over_time",
	    .name = "A fight victor heals slowly after the fight",
	    .facet = Facet::Combat,
	    .description = "A tiger with nine tenths of its life. Half a second in, the game's script that heals a creature "
	                   "after it wins a fight starts on it, as the first land's Ogre quest starts it when the Ogre wins.",
	    .expected = "The life of the tiger increases by 0.01 at once and then by 0.01 every six seconds: 0.91, 0.92 after "
	                "six seconds, and so on. When the life is 1 or more, the script stops.",
	    .framing = ringside,
	    .creatures = {victor(PlayerNames::PLAYER_ONE)},
	    .commands = {healScript(0.5f)},
	    .checks =
	        {{.label = "the first hundredth at once",
	          .quantity = checks::Quantity::CreatureLife,
	          .afterTurns = 20,
	          .atLeast = 0.905,
	          .atMost = 0.915},
	         {.label = "a hundredth more after six seconds",
	          .quantity = checks::Quantity::CreatureLife,
	          .afterTurns = 80,
	          .atLeast = 0.915,
	          .atMost = 0.925},
	         {.label = "full after a minute", .quantity = checks::Quantity::CreatureLife, .afterTurns = 620, .atLeast = 0.999}},
	});
	all.push_back({
	    .id = "combat.victor_heal_stops_in_fight",
	    .name = "A fight victor stops healing when it fights again",
	    .facet = Facet::Combat,
	    .description = "As combat.victor_heals_over_time, with a second tiger. Nine seconds in, the healed tiger asks the "
	                   "other tiger to fight, and both fight by themselves.",
	    .expected = "The life of the red tiger increases by 0.01 every six seconds while the tigers walk to the arena "
	                "and take their places, because for the scripts a creature fights only while it duels. At the first "
	                "check of the script in the duel, the script stops: the life gets no more hundredths from it. The "
	                "inspector's script.tasks query no longer lists the script.",
	    .framing = ringside,
	    .creatures = {victor(PlayerNames::PLAYER_ONE), blue(PlayerNames::PLAYER_TWO, 0.6f, -0.45f)},
	    .commands = {healScript(0.5f), Fight(Kind::StartFight, 0, 1, 8.5f), Fight(Kind::FightAuto, 0, 1, 0.2f)},
	});

	all.push_back({
	    .id = "combat.strength_gain_at_stop",
	    .name = "A knock-out makes both tigers stronger",
	    .facet = Facet::Combat,
	    .description = "Two tigers, the red one of strength 0.6 and the blue one of strength 0.4, fight by themselves.",
	    .expected = "At the knock-out the strength of each tiger increases two times by the strength gain of the fight "
	                "action, 0.012 each time. The fight transcript has a record strength_gain with two gains for each "
	                "tiger.",
	    .framing = ringside,
	    .creatures = {red(PlayerNames::PLAYER_ONE, 0.6f, -0.45f), blue(PlayerNames::PLAYER_TWO, 0.4f, -0.45f)},
	    .commands = {Fight(Kind::StartFight, 0, 1, 1.0f), Fight(Kind::FightAuto, 0, 1, 0.2f)},
	});

	all.push_back({
	    .id = "combat.life_pays_at_each_stop",
	    .name = "A leash order in the duel costs life at each stop",
	    .facet = Facet::Combat,
	    .description = "The red tiger asks the blue tiger to fight, and both fight by themselves. In the duel the red "
	                   "tiger gets the rope leash, and a second later the player tells it with the leash to go to a point "
	                   "to the south.",
	    .expected = "The order calls the fight off with no winner. The fight transcript has a record fight_end for each "
	                "tiger: when its body is in its stance, a move, a block or a cast, the life of the red tiger pays two "
	                "times (life_payments 2) and the life of the blue tiger three times. A tiger that finishes pays "
	                "nothing.",
	    .framing = ringside,
	    .creatures = {red(PlayerNames::PLAYER_ONE, 0.6f, -0.45f), blue(PlayerNames::PLAYER_TWO, 0.6f, -0.45f)},
	    .commands = {Fight(Kind::StartFight, 0, 1, 1.0f), Fight(Kind::FightAuto, 0, 1, 0.2f), Leash(0, LeashType::Rope, 18.0f),
	                 Go(Kind::LeashOrderAt, 0, {-25.0f, 0.0f}, 1.0f, false)},
	});

	all.push_back({
	    .id = "combat.life_pays_after_the_fight",
	    .name = "A stroke just after a call-off costs life again",
	    .facet = Facet::Combat,
	    .description = "The red tiger asks the blue tiger to fight, and both fight by themselves. In the duel the red "
	                   "tiger gets the rope leash, and a second later the player tells it with the leash to go to a point "
	                   "to the south. At once after the order, the player strokes the blue tiger.",
	    .expected = "The order calls the fight off with no winner. The stroke stops the blue tiger again. While its body "
	                "still plays its stance, a move, a block or a cast, its life pays one time more: the fight transcript "
	                "has a level record for its life with why stopped_again and life_payments 1. A body that finishes "
	                "pays nothing.",
	    .framing = ringside,
	    .creatures = {red(PlayerNames::PLAYER_ONE, 0.6f, -0.45f), blue(PlayerNames::PLAYER_TWO, 0.6f, -0.45f)},
	    .commands = {Fight(Kind::StartFight, 0, 1, 1.0f), Fight(Kind::FightAuto, 0, 1, 0.2f), Leash(0, LeashType::Rope, 18.0f),
	                 Go(Kind::LeashOrderAt, 0, {-25.0f, 0.0f}, 1.0f, false), Act(Kind::Stroke, 1, 0.0f)},
	});

	{
		// Each tiger holds a rock as the fight starts. It lets go of the rock before its taunt.
		auto start = Fight(Kind::StartFight, 0, 1, 1.0f);
		start.waitUntilFree = true;
		// The minds wait, so that each tiger keeps its rock until the fight
		auto holder = red(PlayerNames::PLAYER_ONE, 0.9f, -0.45f);
		auto other = blue(PlayerNames::PLAYER_TWO, 0.3f, -0.45f);
		holder.pauseMind = true;
		other.pauseMind = true;
		all.push_back({
		    .id = "combat.let_go_before_taunt",
		    .name = "Two tigers let go of their rocks before they taunt",
		    .facet = Facet::Combat,
		    .description = "The blue tiger picks up a rock, and a fifth of a second later the red tiger picks up a rock. "
		                   "A second after the red tiger holds its rock, it asks the blue tiger to fight. Both fight by "
		                   "themselves.",
		    .expected = "Each tiger keeps its rock while it turns to face the other tiger. Then each tiger throws its rock "
		                "away or puts it down with its let-go clip, and after that it taunts. No rock falls from a hand at "
		                "the start of the fight.",
		    .framing = ringside,
		    .creatures = {holder, other},
		    .objects = {{.type = MobileStaticInfo::RockChalk, .offset = {-21.0f, 34.0f}},
		                {.type = MobileStaticInfo::RockChalk, .offset = {21.0f, 34.0f}}},
		    .commands = {OnObject(Kind::PickUp, 1, 1, 1.0f), OnObject(Kind::PickUp, 0, 0, 0.2f), start,
		                 Fight(Kind::FightAuto, 0, 1, 0.2f)},
		});
	}

	{
		// The other tiger plays an action as the fight starts. The action plays on, and its turn to face waits for it.
		auto asker = red(PlayerNames::PLAYER_ONE, 0.9f, -0.45f);
		auto asked = blue(PlayerNames::PLAYER_TWO, 0.3f, -0.45f);
		asker.pauseMind = true;
		asked.pauseMind = true;
		all.push_back({
		    .id = "combat.face_off_waits_for_body",
		    .name = "A tiger finishes its action before it turns to fight",
		    .facet = Facet::Combat,
		    .description = "The blue tiger starts its impress action. Six seconds later, near the end of the action, the "
		                   "red tiger asks the blue tiger to fight. Both fight by themselves.",
		    .expected = "The red tiger turns to face the blue tiger. The blue tiger plays its action to the end, and only "
		                "then turns to face the red tiger. Then both taunt.",
		    .framing = ringside,
		    .creatures = {asker, asked},
		    .commands = {Play(Kind::PlayAction, 1, animations::k_Impress, 1.0f, false), Fight(Kind::StartFight, 0, 1, 6.0f),
		                 Fight(Kind::FightAuto, 0, 1, 0.2f)},
		});
	}

	{
		// The other tiger walks as the fight starts. It continues until the first tiger tells it to obey, two turns later.
		auto asker = red(PlayerNames::PLAYER_ONE, 0.9f, -0.45f);
		auto asked = blue(PlayerNames::PLAYER_TWO, 0.3f, -0.45f);
		asker.pauseMind = true;
		asked.pauseMind = true;
		all.push_back({
		    .id = "combat.asked_walks_on",
		    .name = "A tiger walks on for a moment when it is asked to fight",
		    .facet = Facet::Combat,
		    .description = "The blue tiger starts to walk away from the red tiger. One second later, the red tiger asks the "
		                   "blue tiger to fight. Both fight by themselves.",
		    .expected = "The red tiger stops at once and turns to face the blue tiger. The blue tiger walks on for two "
		                "turns, then slows to a stand and turns to face the red tiger. Then both taunt. In the arena, "
		                "each tiger turns on the spot to face the other. The red tiger does not start its duel before the "
		                "blue tiger.",
		    .framing = ringside,
		    .creatures = {asker, asked},
		    .commands = {Go(Kind::WalkTo, 1, {25.0f, 0.0f}, 1.0f, false), Fight(Kind::StartFight, 0, 1, 1.0f),
		                 Fight(Kind::FightAuto, 0, 1, 0.2f)},
		});
	}

	{
		// The first tiger stops the fight on the turn after the request, before it tells the other tiger to obey
		auto asker = red(PlayerNames::PLAYER_ONE, 0.9f, -0.45f);
		auto asked = blue(PlayerNames::PLAYER_TWO, 0.3f, -0.45f);
		asker.pauseMind = true;
		asked.pauseMind = true;
		all.push_back({
		    .id = "combat.asked_stops_on_early_end",
		    .name = "An early end of a fight stops the tiger that was asked",
		    .facet = Facet::Combat,
		    .description = "The blue tiger starts to walk away from the red tiger. One second later, the red tiger asks the "
		                   "blue tiger to fight. One turn after that, the fight of the red tiger stops.",
		    .expected = "The red tiger stops at once. On the turn that the fight stops, the blue tiger also stops its walk "
		                "and slows to a stand. No arena appears.",
		    .framing = ringside,
		    .creatures = {asker, asked},
		    .commands = {Go(Kind::WalkTo, 1, {25.0f, 0.0f}, 1.0f, false), Fight(Kind::StartFight, 0, 1, 1.0f),
		                 Fight(Kind::FightAbort, 0, 1, 0.1f)},
		});
	}

	{
		// The needs of the blue tiger are not set again each frame, so the script can take its life
		auto asked = blue(PlayerNames::PLAYER_TWO, 0.5f, -0.2f);
		asked.hold = false;
		all.push_back({
		    .id = "combat.asked_stops_after_duel",
		    .name = "The end of the first tiger stops the asked tiger after the duel",
		    .facet = Facet::Combat,
		    .description = "The red tiger asks the blue tiger to fight, and both fight by themselves. The creatures do not "
		                   "faint. When the duel is on, a script sets the life of the blue tiger to one half. Two seconds "
		                   "later the fight of the red tiger stops.",
		    .expected = "The duel stops, and both tigers finish their moves and play their responses. The red tiger has "
		                "more life, and its response ends first. Two turns later its fight ends. The blue tiger then "
		                "stops at once with no fear, and it does not obey the red tiger again.",
		    .environment = {.fainting = false},
		    .framing = ringside,
		    .creatures = {red(PlayerNames::PLAYER_ONE, 0.6f, -0.2f), asked},
		    .commands = {Fight(Kind::StartFight, 0, 1, 0.5f),
		                 Fight(Kind::FightAuto, 0, 1, 0.2f),
		                 {.kind = Kind::CallNative,
		                  .delaySeconds = 12.0f,
		                  .script = "SET_PROPERTY",
		                  .scriptArguments = {{.kind = ScriptArgument::Kind::Integer,
		                                       .number = static_cast<float>(script::ObjectPropertyType::Health)},
		                                      {.kind = ScriptArgument::Kind::Creature, .index = 1},
		                                      {.kind = ScriptArgument::Kind::Number, .number = 0.5f}}},
		                 Fight(Kind::FightAbort, 0, 0, 2.0f)},
		});
	}

	all.push_back({
	    .id = "saves.arena_ring_back",
	    .name = "Save and load: the ring of the arena shows again",
	    .facet = Facet::Combat,
	    .description = "Two tigers are told to fight, and both fight by themselves. 5 seconds later the game saves into "
	                   "slot 0. 3 seconds after the save the game loads slot 0.",
	    .expected = "Before the save the ring of light stands round the arena. After the load the ring stands round the "
	                "arena again while the fight continues, as before the save.",
	    .framing = ringside,
	    .creatures = {red(PlayerNames::PLAYER_ONE, 0.9f, -0.45f), blue(PlayerNames::PLAYER_TWO, 0.3f, -0.45f)},
	    .commands = {Fight(Kind::StartFight, 0, 1, 1.0f),
	                 Fight(Kind::FightAuto, 0, 1, 0.2f),
	                 {.kind = Kind::SaveGame, .delaySeconds = 5.0f, .value = 0},
	                 {.kind = Kind::LoadGame, .delaySeconds = 3.0f, .value = 0}},
	});

	all.push_back({
	    .id = "saves.loaded_arena_ring",
	    .name = "The arena of a loaded saved game of the original game makes a white ring",
	    .facet = Facet::Combat,
	    .description = "An arena of radius 30 m, as an arena after the load of a saved game of the original game, is put "
	                   "midway between two tigers. It has no ring. 1 second later the two tigers are told to fight, and "
	                   "both fight by themselves.",
	    .expected = "Before the fight no ring stands round the arena. When the fight takes the arena, a ring of light "
	                "appears round it with its sound. The ring is white, not the grey of the ring of an arena that the land "
	                "or a fight makes. It has the same shape, height and stars.",
	    .framing = ringside,
	    .creatures = {red(PlayerNames::PLAYER_ONE, 0.9f, -0.45f), blue(PlayerNames::PLAYER_TWO, 0.3f, -0.45f)},
	    .commands = {{.kind = Kind::PutLoadedArena, .creature = 0, .delaySeconds = 0.5f, .value = 1, .radius = 30.0f},
	                 Fight(Kind::StartFight, 0, 1, 1.0f),
	                 Fight(Kind::FightAuto, 0, 1, 0.2f)},
	});

	{
		// A lion against a tiger, so that two species show their heads
		auto lion = red(PlayerNames::PLAYER_ONE, 0.6f, -0.2f);
		lion.species = CreatureType::Lion;
		all.push_back({
		    .id = "combat.head_look",
		    .name = "Heads in a fight",
		    .facet = Facet::Combat,
		    .description = "The player's red lion and a blue tiger start to look at the camera. A second later they fight "
		                   "by themselves, so each head has a point to look at from before the fight. The camera looks at "
		                   "the arena from the side.",
		    .expected = "Before the duel each creature looks at the other as it walks to its place. In the fight stance "
		                "each head follows the head of the other creature. During a blow, a block, a cast or a fall the "
		                "head turns back to straight ahead. No head looks off to the left or the right of the opponent.",
		    .framing = ringside,
		    .creatures = {lion, blue(PlayerNames::PLAYER_TWO, 0.5f, -0.2f)},
		    .commands = {{.kind = Kind::DoAction, .delaySeconds = 0.5f, .object = 0, .action = "LookAtCamera"},
		                 {.kind = Kind::DoAction, .delaySeconds = 0.0f, .object = 1, .action = "LookAtCamera"},
		                 Fight(Kind::StartFight, 0, 1, 1.0f),
		                 Fight(Kind::FightAuto, 0, 1, 0.2f)},
		});
	}

	{
		// The two tigers start back to back, so that each one must turn to face the other
		auto away = red(PlayerNames::PLAYER_ONE, 0.6f, -0.2f);
		away.facingDegrees = 90.0f;
		auto otherAway = blue(PlayerNames::PLAYER_TWO, 0.5f, -0.2f);
		otherAway.facingDegrees = 90.0f;
		all.push_back({
		    .id = "combat.lead_in",
		    .name = "The steps before a duel",
		    .facet = Facet::Combat,
		    .description = "The red tiger and the blue tiger stand back to back. The red tiger starts a fight with the blue "
		                   "tiger, and both fight by themselves.",
		    .expected = "Both tigers stop and turn to face each other. Each one taunts or makes the rude gesture. Then "
		                "the ring of the arena appears with its sound, and the red tiger points at the arena. Both walk to "
		                "their places and look at each other with angry faces. They turn to face each other again, and "
		                "the duel starts.",
		    .framing = ringside,
		    .creatures = {away, otherAway},
		    .commands = {Fight(Kind::StartFight, 0, 1, 1.0f), Fight(Kind::FightAuto, 0, 1, 0.2f)},
		});
	}

	all.push_back({
	    .id = "combat.fight_view",
	    .name = "Watching a fight",
	    .facet = Facet::Combat,
	    .description = "The player's red tiger and a blue tiger are told to fight, both by themselves, while the camera "
	                   "looks at the arena from within it. Play it with the hand: the Move button (left) on the blue "
	                   "tiger strikes at once, the Action button (right) queues the move, on the red tiger it blocks and "
	                   "on the ground it steps; hold either to charge a blow, and draw a star for the special move.",
	    .expected = "After a second of looking, the camera flies to the side of the arena and then follows the duel "
	                "side on, easing round as the tigers move, and the fight panel shows at the top left. Zooming far "
	                "out leaves the fight; once it is over the view stays three seconds and gives the camera back.",
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 80.0f, 10.0f}, .look = {0.0f, 0.0f, 40.0f}},
	    .creatures = {red(PlayerNames::PLAYER_ONE, 0.6f, -0.2f), blue(PlayerNames::PLAYER_TWO, 0.5f, -0.2f)},
	    .commands = {Fight(Kind::StartFight, 0, 1, 1.0f), Fight(Kind::FightAuto, 0, 1, 0.2f)},
	});

	// The fight that the recorded input of the hand directs for the dedicated server's check (tools/sim/hand_inputs).
	// The creature help is on: in a network game every computer counts the time that the reported hand stays over the
	// creature.
	all.push_back({
	    .id = "combat.recorded_presses",
	    .name = "A fight for the recorded presses of the hand",
	    .facet = Facet::Combat,
	    .description = "As Watching a fight: the player's red tiger and a blue tiger fight by "
	                   "themselves, and the hand's presses direct the red one. The dedicated server and the game both "
	                   "play this scenario, and the game makes the recorded input of tools/sim/hand_inputs/fight.json.",
	    .expected = "The camera flies to the side of the arena. Each press on the blue tiger, on the red tiger or on the "
	                "ground puts its sparkle up at once, and the red tiger makes the move at the next turn.",
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 80.0f, 10.0f}, .look = {0.0f, 0.0f, 40.0f}},
	    .creatures = {red(PlayerNames::PLAYER_ONE, 0.6f, -0.2f), blue(PlayerNames::PLAYER_TWO, 0.5f, -0.2f)},
	    .commands = {Fight(Kind::StartFight, 0, 1, 1.0f), Fight(Kind::FightAuto, 0, 1, 0.2f)},
	});

	all.push_back({
	    .id = "combat.fight_view_keys_held",
	    .name = "A fight starts while the left arrow key moves the camera",
	    .facet = Facet::Combat,
	    .description = "As Watching a fight, but the left arrow key (Move Left) goes down half a second before the tigers "
	                   "are told to fight, and stays down for three seconds. In a fight the arrow keys turn the camera.",
	    .expected = "Before the fight the camera moves to the left. In the first step of the fight the keys of the last "
	                "step still move the camera to the left, and the view of the fight does not follow the fighters in "
	                "that step. Then the arrow key turns the view round the fighters, and the view follows them.",
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 80.0f, 10.0f}, .look = {0.0f, 0.0f, 40.0f}},
	    .creatures = {red(PlayerNames::PLAYER_ONE, 0.6f, -0.2f), blue(PlayerNames::PLAYER_TWO, 0.5f, -0.2f)},
	    .commands = {Command {
	                     .kind = Kind::PlainKeyDown, .delaySeconds = 0.5f, .value = static_cast<size_t>(SDL_SCANCODE_LEFT)},
	                 Fight(Kind::StartFight, 0, 1, 0.5f), Fight(Kind::FightAuto, 0, 1, 0.2f),
	                 Command {.kind = Kind::PlainKeyUp, .delaySeconds = 2.3f, .value = static_cast<size_t>(SDL_SCANCODE_LEFT)}},
	});

	all.push_back({
	    .id = "combat.gesture_view_moves",
	    .name = "Drawing a gesture while the fight view moves",
	    .facet = Facet::Combat,
	    .description = "The player's red tiger and a blue tiger fight by themselves, while the camera looks at the arena "
	                   "from within it. When the fight view follows the duel and moves each frame, the hand draws the "
	                   "star of the special move across the middle of the screen.",
	    .expected = "The camera flies to the side of the arena and follows the duel. The star drawn while the view "
	                "moves is recognised: the red tiger does its special move. Another camera that moves would wipe "
	                "the path, but the fight view lets the hand draw on.",
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 80.0f, 10.0f}, .look = {0.0f, 0.0f, 40.0f}},
	    .creatures = {red(PlayerNames::PLAYER_ONE, 0.6f, -0.2f), blue(PlayerNames::PLAYER_TWO, 0.5f, -0.2f)},
	    .commands = {Fight(Kind::StartFight, 0, 1, 1.0f),
	                 Fight(Kind::FightAuto, 0, 1, 0.2f),
	                 {.kind = Kind::DrawGesture, .delaySeconds = 8.0f, .value = static_cast<size_t>(GestureType::Star)}},
	});

	{
		// The player's tiger knows the five miracles that a creature casts in a fight
		const auto knows = [](MagicType magic) {
			return Command {
			    .kind = Kind::KnowMiracle, .creature = 0, .delaySeconds = 0.1f, .value = static_cast<size_t>(magic)};
		};
		all.push_back({
		    .id = "interface.hint_fight_seeds",
		    .name = "The seeds over the hints of the fight miracles",
		    .facet = Facet::Interface,
		    .description = "The player's red tiger knows the fireball, the lightning bolt, the heal, and the weak and the "
		                   "strong creature spells. It fights a blue tiger, and the two fight by themselves.",
		    .expected = "When the duel starts, the hints along the bottom right of the screen show the star of the "
		                "special move and the gesture of each of the five miracles. Over each gesture of a miracle, the "
		                "seed of the miracle turns slowly, with the effect of its holder over it. The seeds are small, in "
		                "white light, and they do not go away while the duel continues.",
		    .framing = {.shot = Shot::Placed, .eye = {0.0f, 80.0f, 10.0f}, .look = {0.0f, 0.0f, 40.0f}},
		    .creatures = {red(PlayerNames::PLAYER_ONE, 0.6f, -0.2f), blue(PlayerNames::PLAYER_TWO, 0.5f, -0.2f)},
		    .commands = {knows(MagicType::Fireball), knows(MagicType::LightningBolt), knows(MagicType::Heal),
		                 knows(MagicType::CreatureSpellWeak), knows(MagicType::CreatureSpellStrong),
		                 Fight(Kind::StartFight, 0, 1, 1.0f), Fight(Kind::FightAuto, 0, 1, 0.2f)},
		});
	}

	{
		// Two creatures that are not the player's fight by themselves, and the player's own lion stands near the arena
		// The Action button is the right button of the mouse
		constexpr size_t k_RightButton = 3;
		constexpr size_t k_Own = 2;
		auto own = Content(CreatureType::Lion, {12.0f, 15.0f}, 0.0f, "own");
		own.owner = PlayerNames::PLAYER_ONE;
		all.push_back({
		    .id = "combat.view_eases_back",
		    .name = "The fight view eases back after the hand lets go",
		    .facet = Facet::Combat,
		    .description = "A red tiger and a blue tiger that are not the player's fight by themselves. The player's own "
		                   "lion stands near the arena. A double click into the arena makes the camera show the fight. "
		                   "A second later the hand holds the lion with the Action button for four seconds, and then "
		                   "lets go.",
		    .expected = "The camera flies to the side of the arena and then follows the duel. While the hand holds the "
		                "lion, the camera looks at the lion. When the hand lets go, the camera flies back to where the "
		                "fight view was, and then eases on to the fight slowly: first over 5 seconds, then faster, over "
		                "2 seconds after the first 2 seconds. It does not jump.",
		    .framing = {.shot = Shot::Placed, .eye = {0.0f, 80.0f, 10.0f}, .look = {0.0f, 0.0f, 40.0f}},
		    .creatures = {red(PlayerNames::PLAYER_TWO, 0.6f, -0.2f), blue(PlayerNames::NEUTRAL, 0.5f, -0.2f), own},
		    .commands = {Fight(Kind::StartFight, 0, 1, 1.0f),
		                 Fight(Kind::FightAuto, 0, 1, 0.2f),
		                 Fight(Kind::FightAuto, 1, 1, 0.2f),
		                 Fight(Kind::FightView, 0, 0, 7.0f),
		                 {.kind = Kind::PointerOnCreature, .creature = k_Own, .delaySeconds = 1.2f},
		                 {.kind = Kind::PointerPress, .delaySeconds = 0.0f, .value = k_RightButton},
		                 {.kind = Kind::PointerRelease, .delaySeconds = 4.0f, .value = k_RightButton}},
		});
	}

	{
		// The Move button is the left button of the mouse
		constexpr size_t k_LeftButton = 1;
		all.push_back({
		    .id = "combat.view_flight_woosh",
		    .name = "The fight view flies in from far away with a woosh",
		    .facet = Facet::Combat,
		    .description = "The player's red tiger and a blue tiger fight by themselves. The camera looks down on the "
		                   "arena from high above it. The camera shows the fight when the fighters have made their arena.",
		    .expected = "The camera flies down to the side of the arena through a point half way, and a woosh is heard "
		                "once as the flight starts. Then the camera follows the duel.",
		    .framing = {.shot = Shot::Placed, .eye = {0.0f, 320.0f, 30.0f}, .look = {0.0f, 0.0f, 40.0f}},
		    .creatures = {red(PlayerNames::PLAYER_ONE, 0.6f, -0.2f), blue(PlayerNames::PLAYER_TWO, 0.5f, -0.2f)},
		    .commands = {Fight(Kind::StartFight, 0, 1, 1.0f), Fight(Kind::FightAuto, 0, 1, 0.2f),
		                 Fight(Kind::FightAuto, 1, 1, 0.2f), Fight(Kind::FightView, 0, 0, 1.0f)},
		});
		all.push_back({
		    .id = "combat.view_flight_stopped",
		    .name = "A turn of the wheel stops the flight of the fight view",
		    .facet = Facet::Combat,
		    .description = "The same fight, seen from high above. The camera shows the fight when the fighters have made "
		                   "their arena, and a third of a second later the wheel turns one notch.",
		    .expected = "The flight stops at once: the camera does not go on through the point half way. It goes "
		                "straight to the fight on its slow curves and then follows the duel.",
		    .framing = {.shot = Shot::Placed, .eye = {0.0f, 320.0f, 30.0f}, .look = {0.0f, 0.0f, 40.0f}},
		    .creatures = {red(PlayerNames::PLAYER_ONE, 0.6f, -0.2f), blue(PlayerNames::PLAYER_TWO, 0.5f, -0.2f)},
		    .commands = {Fight(Kind::StartFight, 0, 1, 1.0f),
		                 Fight(Kind::FightAuto, 0, 1, 0.2f),
		                 Fight(Kind::FightAuto, 1, 1, 0.2f),
		                 Fight(Kind::FightView, 0, 0, 1.0f),
		                 {.kind = Kind::WheelTurn, .delaySeconds = 5.75f, .value = 1}},
		});
		all.push_back({
		    .id = "combat.view_drag_away",
		    .name = "Dragging the land in the fight view, and away from the fight",
		    .facet = Facet::Combat,
		    .description = "A red tiger and a blue tiger that are not the player's fight by themselves. A double click "
		                   "into the arena makes the camera show the fight. Seven seconds later the hand grips the land "
		                   "with the Move button and drags it a little, and lets go. Four seconds later it grips the land "
		                   "again and drags it far.",
		    .expected = "While the hand grips the land, the camera stops following the fight and moves with the land. "
		                "When the hand lets go, the camera eases back to the fight with no jump. In the long drag, the "
		                "fight view ends when the camera is far from the arena, and the land moves on with the hand.",
		    .framing = {.shot = Shot::Placed, .eye = {0.0f, 80.0f, 10.0f}, .look = {0.0f, 0.0f, 40.0f}},
		    .creatures = {red(PlayerNames::PLAYER_TWO, 0.6f, -0.2f), blue(PlayerNames::NEUTRAL, 0.5f, -0.2f)},
		    .commands = {Fight(Kind::StartFight, 0, 1, 1.0f),
		                 Fight(Kind::FightAuto, 0, 1, 0.2f),
		                 Fight(Kind::FightAuto, 1, 1, 0.2f),
		                 Fight(Kind::FightView, 0, 0, 1.0f),
		                 {.kind = Kind::PointerTo, .delaySeconds = 7.0f, .point = {0.5f, 0.75f}},
		                 {.kind = Kind::PointerPress, .delaySeconds = 0.1f, .value = k_LeftButton},
		                 {.kind = Kind::PointerSweep, .delaySeconds = 0.1f, .point = {0.0f, -0.2f}, .amount = 0.5f},
		                 {.kind = Kind::PointerRelease, .delaySeconds = 0.6f, .value = k_LeftButton},
		                 {.kind = Kind::PointerTo, .delaySeconds = 4.0f, .point = {0.5f, 0.8f}},
		                 {.kind = Kind::PointerPress, .delaySeconds = 0.1f, .value = k_LeftButton},
		                 {.kind = Kind::PointerSweep, .delaySeconds = 0.1f, .point = {0.0f, -0.7f}, .amount = 2.0f},
		                 {.kind = Kind::PointerRelease, .delaySeconds = 3.0f, .value = k_LeftButton}},
		});
		// The temple key is space
		constexpr auto k_TempleKey = static_cast<size_t>(input::BindableActionMap::ZOOM_TO_TEMPLE);
		all.push_back({
		    .id = "combat.view_temple_key",
		    .name = "A double tap of the temple key while the camera shows a fight",
		    .facet = Facet::Combat,
		    .description = "The player's red tiger and a blue tiger fight by themselves. The player's temple stands 150 "
		                   "metres south of the arena. The camera shows the fight when the fighters have made their arena. "
		                   "Eight seconds later the scenario presses the temple key twice, 0.2 seconds apart.",
		    .expected = "The camera starts to fly towards the temple and rises high on the way. After about three "
		                "quarters of a second it turns back, and the view of the fight eases the camera back to the "
		                "duel. A single tap alone does nothing while the camera shows the fight.",
		    .environment = {.dispenserGrid = false, .temple = glm::vec2 {0.0f, -150.0f}},
		    .framing = {.shot = Shot::Placed, .eye = {0.0f, 80.0f, 10.0f}, .look = {0.0f, 0.0f, 40.0f}},
		    .creatures = {red(PlayerNames::PLAYER_ONE, 0.6f, -0.2f), blue(PlayerNames::PLAYER_TWO, 0.5f, -0.2f)},
		    .commands = {Fight(Kind::StartFight, 0, 1, 1.0f),
		                 Fight(Kind::FightAuto, 0, 1, 0.2f),
		                 Fight(Kind::FightAuto, 1, 1, 0.2f),
		                 Fight(Kind::FightView, 0, 0, 1.0f),
		                 {.kind = Kind::PressKey, .delaySeconds = 8.0f, .value = k_TempleKey},
		                 {.kind = Kind::PressKey, .delaySeconds = 0.2f, .value = k_TempleKey}},
		});
	}

	{
		// The fight lesson of the guide: no one chooses the moves of either creature, and the guide belongs to no player
		// The Action button is the right button of the mouse
		constexpr size_t k_RightButton = 3;
		// The double click comes after the arena is made, nearly six seconds into the fight, so it finds the arena. The
		// presses come after the duel starts, nearly fourteen seconds into the fight.
		constexpr float k_ViewAfterArena = 6.5f;
		constexpr float k_PressAfterDuel = 8.0f;
		auto guide = Fighter(right, 270.0f, "guide", PlayerNames::NEUTRAL, 0.5f, 0.0f);
		guide.species = CreatureType::Lion;
		all.push_back({
		    .id = "combat.lesson_press",
		    .name = "The guide's fight lesson: a press on the guide",
		    .facet = Facet::Combat,
		    .description = "The player's red tiger fights a lion that belongs to no player, as the guide's fight lesson "
		                   "starts it. No one chooses the moves of either creature. A double click into the arena makes "
		                   "the camera show the fight. Then every four seconds the hand presses the Action button on the "
		                   "middle of the lion's body.",
		    .expected = "The camera shows the fight side on at once. Each press puts a sparkle on the lion's body, and the "
		                "red tiger strikes at the middle. The lion reels from each blow that lands. Neither creature makes "
		                "a move by itself.",
		    .framing = ringside,
		    .creatures = {red(PlayerNames::PLAYER_ONE, 0.5f, 0.0f), guide},
		    .commands = {Fight(Kind::StartFight, 0, 1, 1.0f),
		                 Fight(Kind::FightAuto, 0, 0, 0.2f),
		                 Fight(Kind::FightAuto, 1, 0, 0.2f),
		                 Fight(Kind::FightView, 0, 0, k_ViewAfterArena),
		                 Fight(Kind::PointerOnOpponent, 0, 0, k_PressAfterDuel),
		                 {.kind = Kind::PointerPress, .delaySeconds = 0.1f, .value = k_RightButton},
		                 {.kind = Kind::PointerRelease, .delaySeconds = 0.1f, .value = k_RightButton},
		                 Fight(Kind::PointerOnOpponent, 0, 0, 3.8f)},
		    .repeatFrom = 5,
		});
		// While a press on the opponent's body is held, the hand goes to the sparkle that the press put there
		all.push_back({
		    .id = "hand.fight_sparkle",
		    .name = "The hand on the sparkle of a held press in a fight",
		    .facet = Facet::Hand,
		    .description = "The player's red tiger fights a lion that belongs to no player, as the guide's fight lesson "
		                   "starts it. A double click into the arena makes the camera show the fight. Then every five "
		                   "seconds the hand presses the Action button on the middle of the lion's body and holds it for "
		                   "two seconds.",
		    .expected = "While the button is down, the hand sits on the sparkle on the lion's body and moves with the "
		                "body. When the button comes up, or a blow takes the sparkle away, the hand goes back under the "
		                "pointer.",
		    .framing = ringside,
		    .creatures = {red(PlayerNames::PLAYER_ONE, 0.5f, 0.0f), guide},
		    .commands = {Fight(Kind::StartFight, 0, 1, 1.0f),
		                 Fight(Kind::FightAuto, 0, 0, 0.2f),
		                 Fight(Kind::FightAuto, 1, 0, 0.2f),
		                 Fight(Kind::FightView, 0, 0, k_ViewAfterArena),
		                 Fight(Kind::PointerOnOpponent, 0, 0, k_PressAfterDuel),
		                 {.kind = Kind::PointerPress, .delaySeconds = 0.1f, .value = k_RightButton},
		                 {.kind = Kind::PointerRelease, .delaySeconds = 2.0f, .value = k_RightButton},
		                 Fight(Kind::PointerOnOpponent, 0, 0, 2.9f)},
		    .repeatFrom = 5,
		});
		{
			// Eighteen quick presses fill the queue of twelve moves, also when the fight plays some of the moves; the fight
			// refuses the presses after that
			constexpr size_t k_QuickPresses = 18;
			std::vector<Command> commands {Fight(Kind::StartFight, 0, 1, 1.0f), Fight(Kind::FightAuto, 0, 0, 0.2f),
			                               Fight(Kind::FightAuto, 1, 0, 0.2f), Fight(Kind::FightView, 0, 0, k_ViewAfterArena),
			                               Fight(Kind::PointerOnOpponent, 0, 0, k_PressAfterDuel)};
			for (size_t press = 0; press < k_QuickPresses; ++press)
			{
				commands.push_back({.kind = Kind::PointerPress, .delaySeconds = 0.05f, .value = k_RightButton});
				commands.push_back({.kind = Kind::PointerRelease, .delaySeconds = 0.05f, .value = k_RightButton});
			}
			all.push_back({
			    .id = "combat.refused_press_does_nothing",
			    .name = "A press that the fight refuses does nothing",
			    .facet = Facet::Combat,
			    .description = "The player's red tiger fights a lion that belongs to no player, as the guide's fight "
			                   "lesson starts it. No one chooses the moves of either creature. A double click into the "
			                   "arena makes the camera show the fight. Then the hand presses the Action button on the lion "
			                   "eighteen times quickly.",
			    .expected = "The presses put sparkles on the lion until twelve show. The fight refuses the presses after "
			                "that (the fight transcript shows refused_full), and nothing else takes them: the log shows no "
			                "line that the hand can't hold the lion, and the leash does not answer.",
			    .framing = ringside,
			    .creatures = {red(PlayerNames::PLAYER_ONE, 0.5f, 0.0f), guide},
			    .commands = std::move(commands),
			});
		}
		// The fight autopilot plays the stages of the lesson with the real mouse, then checks them in the transcript
		constexpr size_t k_LessonPolicy = 0;
		constexpr float k_LessonCheckAfterSeconds = 45.0f;
		all.push_back({
		    .id = "combat.autopilot_lesson",
		    .name = "The fight autopilot plays the guide's fight lesson",
		    .facet = Facet::Combat,
		    .description = "The player's red tiger fights a lion that belongs to no player, as the guide's fight lesson "
		                   "starts it. No one chooses the moves of either creature. The fight autopilot fights for the "
		                   "player with the real mouse. It double clicks into the arena, then does each stage of the "
		                   "lesson: one blow, three quick blows, a block and a step. Then it hits until the lion falls. "
		                   "After 45 seconds the fight transcript is checked for the four stages in their order. Start the "
		                   "game with the inspector (--inspect-port 0).",
		    .expected = "The camera shows the fight after the double click. The red tiger strikes, the lion reels, three "
		                "sparkles queue three blows, the tiger blocks, then steps. The log line of the fight lesson check "
		                "starts with \"passed\". The tiger wins.",
		    .framing = ringside,
		    .creatures = {red(PlayerNames::PLAYER_ONE, 0.5f, 0.0f), guide},
		    .commands = {Fight(Kind::StartFight, 0, 1, 1.0f), Fight(Kind::FightAuto, 0, 0, 0.2f),
		                 Fight(Kind::FightAuto, 1, 0, 0.2f), Fight(Kind::FightAutopilot, 0, k_LessonPolicy, 0.5f),
		                 Fight(Kind::FightLessonCheck, 0, 0, k_LessonCheckAfterSeconds)},
		});
	}

	all.push_back({
	    .id = "combat.player",
	    .name = "Directing the player's creature",
	    .facet = Facet::Combat,
	    .description = "The player's red tiger fights a blue tiger that fights by itself. The red one is given blows "
	                   "high, in the middle and low, a step forward, a block and a fully charged high blow in turn, as "
	                   "clicking the blue tiger's head, middle and legs, the ground ahead, and the red tiger itself would. "
	                   "Click them yourself to direct it too.",
	    .expected = "Each order plays at once: the blow chosen lands at the height asked for, stepping in first when "
	                "out of reach; the block holds until the next order. Left alone for 15 seconds, it fights by "
	                "itself.",
	    .framing = ringside,
	    .creatures = {red(PlayerNames::PLAYER_ONE, 0.5f, 0.0f), blue(PlayerNames::PLAYER_TWO, 0.5f, 0.3f)},
	    .commands = {Fight(Kind::StartFight, 0, 1, 1.0f), Fight(Kind::FightBlow, 0, k_High, 8.0f, 400.0f),
	                 Fight(Kind::FightBlow, 0, k_Mid, 2.5f, 400.0f), Fight(Kind::FightBlow, 0, k_Low, 2.5f, 400.0f),
	                 Fight(Kind::FightStep, 0, k_Forward, 2.5f), Fight(Kind::FightBlock, 0, 0, 2.0f),
	                 Fight(Kind::FightBlow, 0, k_High, 3.0f, creature_fight::k_MaxChargeMs)},
	    .repeatFrom = 1,
	});

	all.push_back({
	    .id = "combat.blocking",
	    .name = "Blocking an aggressive opponent",
	    .facet = Facet::Combat,
	    .description = "The player's red tiger blocks while an evil blue tiger, as aggressive as fighters get, attacks "
	                   "it; every twelve seconds the red one steps back out of its block and blocks again.",
	    .expected = "The red tiger raises its guard and holds it. Blows on it make it reel back in its block and take "
	                "a tenth of the damage, leaving no wounds; the blue tiger attacks again and again.",
	    .framing = ringside,
	    .creatures = {red(PlayerNames::PLAYER_ONE, 0.5f, 0.0f), blue(PlayerNames::PLAYER_TWO, 0.8f, -0.9f)},
	    .commands = {Fight(Kind::StartFight, 0, 1, 1.0f), Fight(Kind::FightBlock, 0, 0, 7.0f),
	                 Fight(Kind::FightStep, 0, k_Back, 12.0f), Fight(Kind::FightBlock, 0, 0, 1.5f)},
	    .repeatFrom = 2,
	});

	all.push_back({
	    .id = "combat.charged",
	    .name = "Charged and quick blows",
	    .facet = Facet::Combat,
	    .description = "The player's red tiger strikes a good, defensive blue tiger with blows in the middle, in turn "
	                   "let go at once and held the full 1.2 seconds.",
	    .expected = "A quick click's blow plays at half speed and does little; a fully charged blow plays half as fast "
	                "again as normal and takes three times as much off the blue tiger's fight health.",
	    .framing = ringside,
	    .creatures = {red(PlayerNames::PLAYER_ONE, 0.5f, 0.0f), blue(PlayerNames::PLAYER_TWO, 0.5f, 0.9f)},
	    .commands = {Fight(Kind::StartFight, 0, 1, 1.0f), Fight(Kind::FightBlow, 0, k_Mid, 8.0f, 0.0f),
	                 Fight(Kind::FightBlow, 0, k_Mid, 4.0f, creature_fight::k_MaxChargeMs)},
	    .repeatFrom = 1,
	});

	auto worn = Fighter({0.0f, 0.0f}, 0.0f, "worn", PlayerNames::PLAYER_ONE, 0.5f, 0.0f);
	worn.hold = false;
	worn.pauseMind = true;
	worn.needs.life = 0.6f;
	worn.needs.exhaustion = 0.7f;
	// The winner of a knock-out is made to do something to the loser, by its mind
	{
		constexpr float k_DuelStartsSeconds = 12.0f;
		constexpr float k_EvilAlignment = -0.8f;
		constexpr float k_GoodAlignment = 0.8f;
		all.push_back({
		    .id = "combat.winner_poos",
		    .name = "An evil winner has a poo on the loser",
		    .facet = Facet::Combat,
		    .description = "An evil red tiger (alignment -0.8) fights a blue tiger. When the duel has started, the blue tiger "
		                   "is knocked out as a fight's loser is.",
		    .expected = "The red tiger finishes its move and leaves the fight. Then it walks to the blue tiger and has a poo "
		                "on it. The blue tiger lies out cold.",
		    .framing = ringside,
		    .creatures = {Fighter({-15.0f, 0.0f}, 90.0f, "red", PlayerNames::PLAYER_ONE, 0.5f, k_EvilAlignment),
		                  Fighter({15.0f, 0.0f}, 270.0f, "blue", PlayerNames::PLAYER_TWO, 0.5f, 0.0f)},
		    .commands = {Fight(Kind::StartFight, 0, 1, 1.0f), Fight(Kind::KnockOut, 1, 0, k_DuelStartsSeconds)},
		});
		auto passer = Fighter({60.0f, -40.0f}, 270.0f, "passer", PlayerNames::PLAYER_TWO, 0.5f, 0.0f);
		all.push_back({
		    .id = "combat.after_fight_block",
		    .name = "After a knock-out, the winner takes no notice until its poo is done",
		    .facet = Facet::Combat,
		    .description = "An evil red tiger (alignment -0.8) fights a blue tiger, and the blue tiger is knocked out as "
		                   "in combat.winner_poos. A third tiger then walks past the red tiger.",
		    .expected = "The red tiger walks to the blue tiger and has a poo on it. It does not stop to look over the "
		                "tiger that walks past. Once the poo is done, it takes up reactions again.",
		    .framing = ringside,
		    .creatures = {Fighter({-15.0f, 0.0f}, 90.0f, "red", PlayerNames::PLAYER_ONE, 0.5f, k_EvilAlignment),
		                  Fighter({15.0f, 0.0f}, 270.0f, "blue", PlayerNames::PLAYER_TWO, 0.5f, 0.0f), passer},
		    .commands = {Fight(Kind::StartFight, 0, 1, 1.0f), Fight(Kind::KnockOut, 1, 0, k_DuelStartsSeconds),
		                 Go(Kind::WalkTo, 2, {-60.0f, -40.0f}, 1.0f, false)},
		});
		all.push_back({
		    .id = "combat.winner_shows_off",
		    .name = "A winner that is not evil shows off at the loser",
		    .facet = Facet::Combat,
		    .description = "A good red tiger (alignment 0.8) fights a blue tiger. When the duel has started, the blue tiger is "
		                   "knocked out as a fight's loser is.",
		    .expected = "The red tiger finishes its move and leaves the fight. Then it shows off at the blue tiger, which "
		                "lies out cold.",
		    .framing = ringside,
		    .creatures = {Fighter({-15.0f, 0.0f}, 90.0f, "red", PlayerNames::PLAYER_ONE, 0.5f, k_GoodAlignment),
		                  Fighter({15.0f, 0.0f}, 270.0f, "blue", PlayerNames::PLAYER_TWO, 0.5f, 0.0f)},
		    .commands = {Fight(Kind::StartFight, 0, 1, 1.0f), Fight(Kind::KnockOut, 1, 0, k_DuelStartsSeconds)},
		});
	}

	all.push_back({
	    .id = "combat.faint_recovery",
	    .name = "Knocked out, taken home and back up",
	    .facet = Facet::Combat,
	    .description = "A worn tiger, life 0.6 and exhaustion 0.7, is given a home where it stands, walks 90 units away "
	                   "and is knocked out there as a fight's loser is.",
	    .expected = "It faints and lies out cold with its eyes closed for 12 seconds, is taken home, lies there a few "
	                "seconds more, then rests until it is no more exhausted than 0.3, and gets up.",
	    .framing = {.shot = Shot::Overview, .include = {{90.0f, 30.0f}, {-20.0f, 0.0f}}, .distance = 0.6f},
	    .creatures = {worn},
	    .commands = {{.kind = Kind::ConfineToHome, .delaySeconds = 0.5f, .radius = 400.0f},
	                 Go(Kind::WalkTo, 0, {90.0f, 30.0f}, 0.5f, false),
	                 Act(Kind::KnockOut, 0, 1.0f, true)},
	});
	all.push_back({
	    .id = "combat.script_moves_knocked_out",
	    .name = "A script moves a creature that lies knocked out",
	    .facet = Facet::Combat,
	    .description = "A worn tiger, life 0.6 and exhaustion 0.7, is given a home where it stands and is knocked out "
	                   "there as a fight's loser is. Six seconds later, while it lies still, a script puts it 40 units "
	                   "to the east with SET_POSITION.",
	    .expected = "It shows at the new place at once, and its body stands there, as the game's move stands it. It "
	                "stays knocked out for the same time as in combat.faint_recovery, counted from when it first lay "
	                "still. Then it is taken home, waits there, rests and gets up. It does not stay knocked out for ever.",
	    .framing = {.shot = Shot::Overview, .include = {{40.0f, 0.0f}, {-20.0f, 0.0f}}, .distance = 0.6f},
	    .creatures = {worn},
	    .commands = {{.kind = Kind::ConfineToHome, .delaySeconds = 0.5f, .radius = 400.0f},
	                 Act(Kind::KnockOut, 0, 1.0f, false),
	                 {.kind = Kind::CallNative,
	                  .delaySeconds = 6.0f,
	                  .script = "SET_POSITION",
	                  .scriptArguments = {{.kind = ScriptArgument::Kind::Creature, .index = 0},
	                                      {.kind = ScriptArgument::Kind::Position, .point = {40.0f, 0.0f}}}}},
	});

	{
		constexpr float k_HurtLife = 0.3f;
		auto hurt = Fighter({0.0f, 0.0f}, 0.0f, "hurt", PlayerNames::PLAYER_ONE, 0.5f, 0.0f);
		hurt.hold = false;
		hurt.needs.life = k_HurtLife;
		all.push_back({
		    .id = "combat.knocked_out_no_emergency",
		    .name = "Knocked out, a hurt creature has no emergency until it gets up",
		    .facet = Facet::Combat,
		    .description = "A hurt tiger, life 0.3, is knocked out at once, as a fight's loser is. A life below 0.4 is an "
		                   "emergency for a creature that is awake.",
		    .expected = "It lies out cold, is taken home and rests. The emergency that its low life started stops when it "
		                "faints. While it lies there, the mind starts no new emergency (inspector: emergencyRunning of "
		                "CreatureMindState stays false). It can have an emergency again only after it gets up.",
		    .framing = {.shot = Shot::Overview, .include = {{0.0f, 0.0f}}, .distance = 0.4f},
		    .creatures = {hurt},
		    .commands = {Act(Kind::KnockOut, 0, 0.0f, true)},
		});
	}

	all.push_back({
	    .id = "combat.leash",
	    .name = "Leashed onto another creature",
	    .facet = Facet::Combat,
	    .description = "The player's red tiger is put on the aggression leash, which is tied to the blue tiger.",
	    .expected = "Tied to another creature, it fights it: both walk to the arena and duel, the red one fighting by "
	                "itself once left alone for three seconds.",
	    .framing = ringside,
	    .creatures = {red(PlayerNames::PLAYER_ONE, 0.6f, 0.0f), blue(PlayerNames::PLAYER_TWO, 0.5f, 0.0f)},
	    .commands = {Leash(0, LeashType::Evil, 1.0f), Fight(Kind::TieLeashToCreature, 0, 1, 2.0f)},
	});

	{
		// A ring of villagers of the player's town round the place of the arena, which see the knock-out. The fight
		// autopilot fights for the player with the aggressive policy.
		constexpr size_t k_AggressivePolicy = 1;
		std::vector<ObjectSetup> crowd;
		constexpr int k_Villagers = 8;
		constexpr float k_RingRadius = 44.0f;
		for (int i = 0; i < k_Villagers; ++i)
		{
			const auto angle = glm::two_pi<float>() * static_cast<float>(i) / static_cast<float>(k_Villagers);
			crowd.push_back({.type = i % 2 == 0 ? VillagerInfo::CelticFarmerMale : VillagerInfo::CelticHousewifeFemale,
			                 .offset = glm::vec2 {20.0f, 35.0f} +
			                           (k_RingRadius * glm::vec2 {portable_math::Cos(angle), portable_math::Sin(angle)}),
			                 .joinTown = true});
		}
		auto strong = red(PlayerNames::PLAYER_ONE, 0.9f, -0.6f);
		strong.facingDegrees = 90.0f;
		// The weak tiger has little life, thus little fight health, and soon faints
		auto weak = blue(PlayerNames::PLAYER_TWO, 0.2f, 0.0f);
		weak.needs.life = 0.15f;
		all.push_back({
		    .id = "combat.leash_lead_in",
		    .name = "A fight on the leash, from the face-off to the faint",
		    .facet = Facet::Combat,
		    .description = "The player's strong red tiger stands with its back to a weak blue tiger. The aggression leash "
		                   "on the red tiger is tied to the blue tiger. Eight villagers stand round the place of the arena.",
		    .expected = "Both tigers stop and turn to face each other, and each one taunts or makes the rude gesture. The "
		                "arena appears with its sound, the red tiger points at it, and both walk to their places. They "
		                "turn to face each other again and duel. A fighter near the edge steps back to the middle. The "
		                "blue tiger faints, and the villagers near it react as to a death.",
		    .framing = {.shot = Shot::Placed, .eye = {20.0f, 45.0f, -25.0f}, .look = {20.0f, 0.0f, 35.0f}},
		    .creatures = {strong, weak},
		    .objects = crowd,
		    .commands = {Leash(0, LeashType::Evil, 1.0f), Fight(Kind::TieLeashToCreature, 0, 1, 2.0f),
		                 Fight(Kind::FightAutopilot, 0, k_AggressivePolicy, 0.5f)},
		});
	}

	{
		// Two tigers with the same life start a fight. The debug tools stop the duel, so that no one is knocked out.
		auto even = red(PlayerNames::PLAYER_ONE, 0.6f, -0.2f);
		auto other = blue(PlayerNames::PLAYER_TWO, 0.5f, -0.2f);
		other.needs.life = 0.5f;
		even.needs.life = 0.5f;
		auto stronger = red(PlayerNames::PLAYER_ONE, 0.6f, -0.2f);
		stronger.needs.life = 0.9f;
		constexpr float k_StopAfterSeconds = 25.0f;
		const std::vector<Command> stopped {Fight(Kind::StartFight, 0, 1, 1.0f), Fight(Kind::FightAuto, 0, 1, 0.2f),
		                                    Fight(Kind::FightAbort, 0, 0, k_StopAfterSeconds)};
		all.push_back({
		    .id = "combat.stopped_tie",
		    .name = "A fight stopped with no knock-out, the two tigers with the same life",
		    .facet = Facet::Combat,
		    .description = "Two tigers fight by themselves. The scenario keeps the life of both at 0.5, thus they have the "
		                   "same life after the duel. After 25 seconds the debug tools stop the duel.",
		    .expected = "Each tiger finishes its move. Both tigers are sad. The first tiger to finish fears the other "
		                "and runs from it, and the other tiger stops its response and is free again.",
		    .framing = ringside,
		    .creatures = {even, other},
		    .commands = stopped,
		});
		auto weaker = red(PlayerNames::PLAYER_ONE, 0.6f, -0.2f);
		weaker.needs.life = 0.3f;
		auto healthier = blue(PlayerNames::PLAYER_TWO, 0.5f, -0.2f);
		healthier.needs.life = 0.9f;
		all.push_back({
		    .id = "combat.stopped_starter_weaker",
		    .name = "A fight stopped with no knock-out, the starter with less life",
		    .facet = Facet::Combat,
		    .description = "The red tiger starts the fight but has less life than the blue tiger. Both fight by themselves. "
		                   "After 25 seconds the debug tools stop the duel.",
		    .expected = "Each tiger finishes its move. The red tiger is sad and the blue tiger is happy. The red tiger then "
		                "fears the blue tiger and runs from it. The blue tiger is free again: it does not stay stopped.",
		    .framing = ringside,
		    .creatures = {weaker, healthier},
		    .commands = stopped,
		});
		all.push_back({
		    .id = "combat.stopped_happy_sad",
		    .name = "A fight stopped with no knock-out, one tiger with more life",
		    .facet = Facet::Combat,
		    .description = "The red tiger has much more life than the blue tiger. Both fight by themselves. After 25 seconds "
		                   "the debug tools stop the duel.",
		    .expected = "Each tiger finishes its move. The red tiger is happy. The blue tiger is sad and then runs from the "
		                "red tiger in fear.",
		    .framing = ringside,
		    .creatures = {stronger, other},
		    .commands = stopped,
		});
	}

	auto angry = red(PlayerNames::PLAYER_ONE, 0.6f, -0.3f);
	angry.desires = OnlyDesire(Desire::Anger);
	all.push_back({
	    .id = "combat.anger",
	    .name = "An angry creature picks a fight",
	    .facet = Facet::Combat,
	    .description = "A red tiger as angry as it gets, wanting nothing else, stands near a blue tiger.",
	    .expected = "Its anger makes a plan to fight the blue tiger. It asks the blue tiger to fight, and they duel. The "
	                "blue tiger obeys the red tiger until the fight is over.",
	    .framing = ringside,
	    .creatures = {angry, blue(PlayerNames::PLAYER_TWO, 0.5f, 0.0f)},
	});

	auto unpractised = red(PlayerNames::PLAYER_THREE, 0.6f, -0.3f);
	unpractised.desires = OnlyDesire(Desire::Anger);
	all.push_back({
	    .id = "combat.lean_until_completed",
	    .name = "A creature that did not complete a fight leans against its alignment again",
	    .facet = Facet::Combat,
	    .description = "A slightly evil red tiger (alignment -0.30) as angry as it gets stands near a blue tiger. Both "
	                   "fight by themselves. The scenario gives the red tiger an aggressive lean of +0.70, but the red "
	                   "tiger did not complete a fight.",
	    .expected = "Its anger makes a plan to fight the blue tiger. When the fight starts, the lean of the red tiger "
	                "changes to +0.30, against its alignment (the inspector shows CreatureFightRecord tendency). That "
	                "lean is below the aggressive tier, thus the red tiger fights as a defensive fighter and blocks and "
	                "steps more than it strikes. The blue tiger has not started a fight, thus it chooses its moves with a "
	                "lean of 0.",
	    .framing = ringside,
	    .creatures = {unpractised, blue(PlayerNames::PLAYER_TWO, 0.5f, 0.0f)},
	    .commands = {Command {.kind = Kind::SetFightLean, .creature = 0, .delaySeconds = 0.0f, .amount = 0.7f}},
	});

	auto lion = Fighter(left, 90.0f, "evil lion", PlayerNames::PLAYER_TWO, 1.0f, -0.9f);
	lion.species = CreatureType::Lion;
	lion.size = 1.3f;
	auto small = Fighter(right, 270.0f, "small tiger", PlayerNames::PLAYER_ONE, 0.1f, 0.8f);
	small.size = 0.8f;
	small.needs.life = 0.3f;
	// Left to heal as it rests after being knocked out
	small.hold = false;
	all.push_back({
	    .id = "combat.evil_winner",
	    .name = "An evil winner has a poo on the loser",
	    .facet = Facet::Combat,
	    .description = "A big, strong, evil lion fights a small, weak, good tiger with little life left, so starting "
	                   "with little fight health, both by themselves.",
	    .expected = "The tiger soon faints. The lion finishes, walks up to it and has a poo on it, rather than showing "
	                "off as a good winner does.",
	    .framing = ringside,
	    .creatures = {lion, small},
	    .commands = {Fight(Kind::StartFight, 0, 1, 1.0f), Fight(Kind::FightAuto, 1, 1, 0.2f)},
	});

	all.push_back({
	    .id = "leash.pulled_out_of_fight",
	    .name = "The leash pulls a creature out of its fight",
	    .facet = Facet::Leash,
	    .description = "The player's red tiger wears the learning leash held in the hand and fights a blue tiger, both "
	                   "by themselves. Hold the hand near the red tiger, then move it further away than the full length of the "
	                   "leash.",
	    .expected = "With the hand near, the rope hangs slack and the duel goes on. With the hand further away than the "
	                "full length of the leash, the red tiger is taken out of the fight: the fight is called off with "
	                "no winner and the red tiger walks back to the hand. The help says nothing.",
	    .framing = ringside,
	    .creatures = {red(PlayerNames::PLAYER_ONE, 0.5f, 0.0f), blue(PlayerNames::PLAYER_TWO, 0.5f, 0.0f)},
	    .commands = {Leash(0, LeashType::Rope, 0.5f), Fight(Kind::StartFight, 0, 1, 1.0f), Fight(Kind::FightAuto, 0, 1, 0.2f)},
	});

	// Big against small: the game never resizes a creature for a fight; it only refuses one outside half to twice its
	// height
	auto big = Fighter(left, 90.0f, "big tiger", PlayerNames::PLAYER_ONE, 0.5f, 0.0f);
	big.size = 1.5f;
	auto smaller = Fighter(right, 270.0f, "small tiger", PlayerNames::PLAYER_TWO, 0.5f, 0.0f);
	smaller.size = 0.8f;
	all.push_back({
	    .id = "combat.big_vs_small",
	    .name = "A big creature fights a small one",
	    .facet = Facet::Combat,
	    .description = "A big tiger (size 1.5) fights a small one (size 0.8), just under twice its height, both by "
	                   "themselves.",
	    .expected = "The fight starts and both keep their sizes throughout: the arena is sized for the big one, its "
	                "blows reach further and hit harder, and the small one has to step in to land its own.",
	    .framing = ringside,
	    .creatures = {big, smaller},
	    .commands = {Fight(Kind::StartFight, 0, 1, 1.0f), Fight(Kind::FightAuto, 0, 1, 0.2f),
	                 Fight(Kind::FightAuto, 1, 1, 0.0f)},
	});
	auto tiny = Fighter(right, 270.0f, "tiny tiger", PlayerNames::PLAYER_TWO, 0.5f, 0.0f);
	tiny.size = 0.6f;
	all.push_back({
	    .id = "combat.too_small",
	    .name = "A creature too small to fight",
	    .facet = Facet::Combat,
	    .description = "A big tiger (size 1.5) is told to fight a tiny one (size 0.6), less than half its height.",
	    .expected = "The tiny tiger refuses: no arena appears and neither moves to fight (the command reports "
	                "\"refused\").",
	    .framing = ringside,
	    .creatures = {big, tiny},
	    .commands = {Fight(Kind::StartFight, 0, 1, 1.0f)},
	});

	// Miracles in a fight: the player's hurt tiger heals itself, then strikes its opponent with lightning
	auto hurt = red(PlayerNames::PLAYER_ONE, 0.5f, 0.0f);
	hurt.needs.life = 0.3f;
	all.push_back({
	    .id = "combat.casting",
	    .name = "Casting miracles in a fight",
	    .facet = Facet::Combat,
	    .description = "The player's red tiger, hurt (life 0.3, so fight health 0.65), knows Heal and Lightning. In "
	                   "the fight it is given Heal, as drawing Heal's gesture does, then Lightning, then Heal again; the "
	                   "blue tiger fights by itself.",
	    .expected = "Each miracle is cast from the fight stance: the creature raises its arms (start of the cast), "
	                "casts as the start ends, holds the casting pose only a moment and lowers its arms. Heal is cast on "
	                "itself and its fight health bar rises; Lightning strikes the blue tiger, which reels. Each cast "
	                "takes from the red tiger's stamina (the blue bar); without enough stamina a cast does nothing.",
	    .framing = ringside,
	    .creatures = {hurt, blue(PlayerNames::PLAYER_TWO, 0.5f, 0.0f)},
	    .commands =
	        {{.kind = Kind::KnowMiracle, .creature = 0, .delaySeconds = 0.5f, .value = static_cast<size_t>(MagicType::Heal)},
	         {.kind = Kind::KnowMiracle,
	          .creature = 0,
	          .delaySeconds = 0.0f,
	          .value = static_cast<size_t>(MagicType::LightningBolt)},
	         Fight(Kind::StartFight, 0, 1, 0.5f),
	         Fight(Kind::FightAuto, 1, 1, 0.2f),
	         Fight(Kind::FightSpell, 0, static_cast<size_t>(MagicType::Heal), 8.0f),
	         Fight(Kind::FightSpell, 0, static_cast<size_t>(MagicType::LightningBolt), 6.0f),
	         Fight(Kind::FightSpell, 0, static_cast<size_t>(MagicType::Heal), 6.0f)},
	});

	// Forfeiting: there is no forfeit, but taking the creature away ends the fight with no winner
	all.push_back({
	    .id = "combat.forfeit",
	    .name = "Taking a creature out of a fight",
	    .facet = Facet::Combat,
	    .description = "The player's red tiger wears the leash held in the hand and fights a blue tiger, both by "
	                   "themselves. Ten seconds into the duel the player gives a leash order to go to a point, as the "
	                   "Action button outside the fight view does. Try it yourself too: pull the leash taut by moving "
	                   "the hand well away from the red tiger.",
	    .expected = "The fight is called off with no winner: both tigers finish the move they are making, play their "
	                "finish and stand, without celebrating or sulking; the ring of light goes. The red tiger then walks "
	                "to the point. Pulled further than the full length of the leash instead, the red tiger leaves the "
	                "fight and walks back to the hand, and the help says nothing.",
	    .framing = ringside,
	    .creatures = {red(PlayerNames::PLAYER_ONE, 0.5f, 0.0f), blue(PlayerNames::PLAYER_TWO, 0.5f, 0.0f)},
	    .commands = {Leash(0, LeashType::Rope, 0.5f), Fight(Kind::StartFight, 0, 1, 0.5f), Fight(Kind::FightAuto, 0, 1, 0.2f),
	                 Go(Kind::LeashOrderAt, 0, {0.0f, -20.0f}, 16.0f, false)},
	});

	// A fight next to a village: its people come to watch
	all.push_back({
	    .id = "combat.villagers_watch",
	    .name = "Villagers watching a fight",
	    .facet = Facet::Combat,
	    .description = "A hut of the player's town south of an arena with six of its people about it. The player's red "
	                   "tiger and a blue tiger fight there, both by themselves.",
	    .expected = "Once the fight is on, the villagers within the arena's reach take up its reaction and walk to places "
	                "spread round a ring 1.4 times the arena's radius, the newest first at the east, the next opposite, "
	                "then the quarters. There they face the arena: for fifteen seconds after a blow lands they cheer "
	                "when the tiger they don't support took it last and despair when theirs did, otherwise they chat. "
	                "They go back to what they were doing when the reaction's time is up or the arena goes.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-60.0f, -25.0f}, {60.0f, 95.0f}}, .distance = 0.8f},
	    .creatures = {red(PlayerNames::PLAYER_ONE, 0.5f, 0.0f), blue(PlayerNames::PLAYER_TWO, 0.5f, 0.0f)},
	    .objects = {{.type = AbodeInfo::CelticHut, .offset = {-10.0f, -20.0f}},
	                {.type = VillagerInfo::CelticFarmerMale, .offset = {-2.0f, -12.0f}, .joinTown = true},
	                {.type = VillagerInfo::CelticForesterMale, .offset = {-4.0f, -6.0f}, .joinTown = true},
	                {.type = VillagerInfo::CelticHousewifeFemale, .offset = {2.0f, -8.0f}, .joinTown = true},
	                {.type = VillagerInfo::CelticHousewifeFemale, .offset = {-6.0f, -14.0f}, .joinTown = true},
	                {.type = VillagerInfo::CelticFishermanMale, .offset = {6.0f, -10.0f}, .joinTown = true},
	                {.type = VillagerInfo::CelticFarmerMale, .offset = {-12.0f, -8.0f}, .joinTown = true}},
	    .commands = {Fight(Kind::StartFight, 0, 1, 1.0f), Fight(Kind::FightAuto, 0, 1, 0.2f)},
	});

	// The fight music of the player's creature, heard near its arena
	all.push_back({
	    .id = "combat.fight_music",
	    .name = "The fight music",
	    .facet = Facet::Combat,
	    .description = "The player's red tiger and a blue tiger fight by themselves. The camera looks at the arena from "
	                   "nearer than 100 metres to its middle.",
	    .expected = "When the arena appears, the land's music fades out and the creature fight music fades in, centred "
	                "on the listener. It plays until the fight is over, then the land's music comes back. Moving the "
	                "camera more than 100 metres from the middle of the arena stops it, unless the hand directs the "
	                "fight in the fight view.",
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 50.0f, -10.0f}, .look = {0.0f, 0.0f, 40.0f}},
	    .creatures = {red(PlayerNames::PLAYER_ONE, 0.6f, -0.2f), blue(PlayerNames::PLAYER_TWO, 0.5f, -0.2f)},
	    .commands = {Fight(Kind::StartFight, 0, 1, 1.0f), Fight(Kind::FightAuto, 0, 1, 0.2f)},
	});

	// A double click into the arena from far away: the fight view does not start, the advisors speak instead
	all.push_back({
	    .id = "combat.far_fight_advisors",
	    .name = "The advisors speak of a fight too far to show",
	    .facet = Facet::Combat,
	    .description = "The player's red tiger and a blue tiger fight by themselves. The camera is far away from the "
	                   "arena. When the duel is on, a double click goes into the arena.",
	    .expected = "The camera does not show the fight, because it was too far away. An advisor says one of the lines "
	                "about a creature fight, such as \"There's a humdinger of a fight going on.\"",
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 120.0f, -900.0f}, .look = {0.0f, 0.0f, -800.0f}},
	    .creatures = {red(PlayerNames::PLAYER_ONE, 0.6f, -0.2f), blue(PlayerNames::PLAYER_TWO, 0.5f, -0.2f)},
	    .commands = {{.kind = Kind::EndGuidanceWaits, .delaySeconds = 0.5f},
	                 Fight(Kind::StartFight, 0, 1, 0.5f),
	                 Fight(Kind::FightAuto, 0, 1, 0.2f),
	                 Fight(Kind::FightView, 0, 0, 14.0f)},
	});

	{
		// A horse's blows are of the wound type that bleeds, so its opponent bleeds where they land
		auto horse = red(PlayerNames::PLAYER_ONE, 0.9f, -0.45f);
		horse.species = CreatureType::Horse;
		all.push_back({
		    .id = "combat.blow_blood",
		    .name = "Blood and dust where blows land",
		    .facet = Facet::Combat,
		    .description = "A strong red horse and a weak blue tiger fight by themselves. The camera is near the middle "
		                   "of the arena.",
		    .expected = "Faint white puffs follow the striking hand or foot as each blow lands. Where a blow meets the "
		                "body, a wound shows on the skin there, and puffs of sand-coloured dust fly along the blow. When "
		                "the blow is not blocked, red and brown drops of blood fly along the blow, fall to the land and "
		                "slide a little on it, then go. On the tiger, blood from many of the horse's wounds runs down the "
		                "skin for some seconds. The body reels to the side, the top or the bottom by the swing of the "
		                "blow, or straight back when the blow swings ahead.",
		    .framing = {.shot = Shot::Placed, .eye = {0.0f, 16.0f, 18.0f}, .look = {0.0f, 8.0f, 40.0f}},
		    .creatures = {horse, blue(PlayerNames::PLAYER_TWO, 0.3f, -0.45f)},
		    .commands = {Fight(Kind::StartFight, 0, 1, 1.0f), Fight(Kind::FightAuto, 0, 1, 0.2f)},
		});
	}

	{
		// The needs of the blue tiger are not set again each frame, so the script can take its life
		auto loser = blue(PlayerNames::PLAYER_TWO, 0.5f, -0.2f);
		loser.hold = false;
		all.push_back({
		    .id = "combat.opponent_out_of_life",
		    .name = "An opponent with no life left loses the duel",
		    .facet = Facet::Combat,
		    .description = "The red tiger of the player and a blue tiger fight by themselves, and the creatures do not faint. "
		                   "When the duel is on, a script sets the life of the blue tiger to zero. Its fight health stays.",
		    .expected = "On its next turn, the red tiger wins. It finishes its move and stands. An advisor says one of the "
		                "lines for good news, because the place of the red tiger is on screen. The blue tiger does not "
		                "faint. It stays in its stance and faces the red tiger. Two turns after the red tiger stands, the "
		                "red tiger orders the blue tiger to respond and plays no response. The blue tiger finishes its "
		                "move, plays the sad response and fears the red tiger. The fight view ends a few seconds after the "
		                "win. The fight transcript shows a win with by_blow false and order_respond.",
		    .environment = {.fainting = false},
		    .framing = ringside,
		    .creatures = {red(PlayerNames::PLAYER_ONE, 0.6f, -0.2f), loser},
		    .commands = {{.kind = Kind::EndGuidanceWaits, .delaySeconds = 0.5f},
		                 Fight(Kind::StartFight, 0, 1, 0.5f),
		                 Fight(Kind::FightAuto, 0, 1, 0.2f),
		                 {.kind = Kind::CallNative,
		                  .delaySeconds = 12.0f,
		                  .script = "SET_PROPERTY",
		                  .scriptArguments = {{.kind = ScriptArgument::Kind::Integer,
		                                       .number = static_cast<float>(script::ObjectPropertyType::Health)},
		                                      {.kind = ScriptArgument::Kind::Creature, .index = 1},
		                                      {.kind = ScriptArgument::Kind::Number, .number = 0.0f}}}},
		});
	}

	{
		// The same win by the life of the opponent, but after the win the script gives the blue tiger its life back and
		// halves the life of the red tiger, so the blue tiger has more life when it responds
		auto winner = red(PlayerNames::PLAYER_ONE, 0.6f, -0.2f);
		winner.hold = false;
		auto loser = blue(PlayerNames::PLAYER_TWO, 0.5f, -0.2f);
		loser.hold = false;
		const auto setLife = [](size_t creature, float life, float delay) {
			return Command {.kind = Kind::CallNative,
			                .delaySeconds = delay,
			                .script = "SET_PROPERTY",
			                .scriptArguments = {{.kind = ScriptArgument::Kind::Integer,
			                                     .number = static_cast<float>(script::ObjectPropertyType::Health)},
			                                    {.kind = ScriptArgument::Kind::Creature, .index = creature},
			                                    {.kind = ScriptArgument::Kind::Number, .number = life}}};
		};
		all.push_back({
		    .id = "combat.loser_waits_free_mind",
		    .name = "The loser waits after its response until its own mind takes a plan",
		    .facet = Facet::Combat,
		    .description = "The red tiger of the player and a blue tiger fight by themselves, and the creatures do not faint. "
		                   "When the duel is on, a script sets the life of the blue tiger to zero. A moment after the red "
		                   "tiger wins, the script gives the blue tiger full life and sets the life of the red tiger to 0.5.",
		    .expected =
		        "The red tiger wins, finishes its move, orders the blue tiger to respond and plays no response. "
		        "The blue tiger finishes its move, plays the happy response and does not fear the red tiger. Then "
		        "it waits with its mind free. Its own plans do not take the place of the order, because each is "
		        "worth less than two times the order. The red tiger goes home to get better. An order of another "
		        "creature, such as to share a smile, can still take its place. The fight transcript shows order_respond.",
		    .environment = {.fainting = false},
		    .framing = ringside,
		    .creatures = {winner, loser},
		    .commands = {{.kind = Kind::EndGuidanceWaits, .delaySeconds = 0.5f},
		                 Fight(Kind::StartFight, 0, 1, 0.5f),
		                 Fight(Kind::FightAuto, 0, 1, 0.2f),
		                 setLife(1, 0.0f, 18.0f),
		                 setLife(1, 1.0f, 0.3f),
		                 setLife(0, 0.5f, 0.0f)},
		});
	}

	all.push_back({
	    .id = "combat.held_in_duel",
	    .name = "The hand holds a creature in its duel",
	    .facet = Facet::Combat,
	    .description = "The red tiger of the player and a blue tiger fight by themselves. When the duel is on, the hand "
	                   "holds the red tiger and strokes its head. Four seconds later the hand lets go.",
	    .expected = "When the hand takes hold, the red tiger leaves the duel. It finishes its move while the hand holds "
	                "it. Then it turns to the camera and shows what it wants. The blue tiger stays in its stance and "
	                "faces the red tiger. When the hand lets go, both tigers stop with no response. The fight transcript "
	                "shows held_by_hand, let_go_by_hand and partner_stopped.",
	    .framing = ringside,
	    .creatures = {red(PlayerNames::PLAYER_ONE, 0.6f, -0.2f), blue(PlayerNames::PLAYER_TWO, 0.5f, -0.2f)},
	    .commands = {{.kind = Kind::EndGuidanceWaits, .delaySeconds = 0.5f},
	                 Fight(Kind::StartFight, 0, 1, 0.5f),
	                 Fight(Kind::FightAuto, 0, 1, 0.2f),
	                 Act(Kind::HandLock, 0, 18.0f),
	                 {.kind = Kind::HandStroke, .bodyPart = static_cast<size_t>(creature_feedback::BodyPart::Head)},
	                 Act(Kind::HandLetGo, 0, 4.0f)},
	});

	{
		// Blows strike the shapes round the bones of the body: a tiger against a horse, which has a body of a different
		// shape
		auto horse = blue(PlayerNames::PLAYER_TWO, 0.6f, -0.45f);
		horse.species = CreatureType::Horse;
		all.push_back({
		    .id = "combat.hit_shapes",
		    .name = "Blows strike the shapes of the body",
		    .facet = Facet::Combat,
		    .description = "A red tiger and a blue horse fight by themselves. The camera is near the middle of the arena.",
		    .expected = "Each creature steps until a blow reaches into the other's body, then strikes. A blow lands where its "
		                "swing meets the body: on the long head and neck of the horse, on its back or on its legs. A blow "
		                "that swings past the body does not land.",
		    .framing = {.shot = Shot::Placed, .eye = {0.0f, 16.0f, 18.0f}, .look = {0.0f, 8.0f, 40.0f}},
		    .creatures = {red(PlayerNames::PLAYER_ONE, 0.6f, -0.45f), horse},
		    .commands = {Fight(Kind::StartFight, 0, 1, 1.0f), Fight(Kind::FightAuto, 0, 1, 0.2f)},
		});
	}

	{
		all.push_back({
		    .id = "combat.bodies_apart",
		    .name = "Fighters keep their bodies apart",
		    .facet = Facet::Combat,
		    .description = "The red tiger starts a fight with the blue tiger to its east, and both fight by themselves. The "
		                   "camera looks at the arena from the side.",
		    .expected = "On the walk to their places, the two tigers go through each other, because the red tiger takes the "
		                "east place. The original game does the same: a creature that walks does not keep clear of another "
		                "creature that walks. In the duel, the bodies do not stay in each other. When a blow reaches into "
		                "the other body, both tigers move apart at once. In the stance and in each step, each tiger turns "
		                "straight to the other, and the two bodies move apart when they touch, also when the other tiger "
		                "blocks or casts after a blow.",
		    .framing = ringside,
		    .creatures = {red(PlayerNames::PLAYER_ONE, 0.6f, -0.2f), blue(PlayerNames::PLAYER_TWO, 0.5f, -0.2f)},
		    .commands = {Fight(Kind::StartFight, 0, 1, 1.0f), Fight(Kind::FightAuto, 0, 1, 0.2f)},
		});
	}

	{
		// The first fight on the first land is the guide's lesson. The guide starts the fight and takes the east place, so
		// the tiger to its east walks past it to the west place.
		auto guide = Fighter(left, 90.0f, "guide", PlayerNames::NEUTRAL, 0.5f, 0.0f);
		guide.species = CreatureType::Lion;
		auto tiger = Fighter(right, 270.0f, "red", PlayerNames::PLAYER_ONE, 0.5f, 0.0f);
		all.push_back({
		    .id = "combat.lesson_bodies",
		    .name = "The guide's fight: bodies on the walk and in the duel",
		    .facet = Facet::Combat,
		    .description = "The guide lion starts a fight with the player's red tiger to its east, and both fight by "
		                   "themselves. The camera looks at the arena from the side.",
		    .expected = "On the walk to their places, the lion and the tiger go through each other, because the lion takes "
		                "the east place. The original game does the same. In the duel, a body can reach into the other "
		                "body for a moment during a blow or a step, because the fighters measure their bodies as they were "
		                "at the last game turn. Then both move apart.",
		    .framing = ringside,
		    .creatures = {guide, tiger},
		    .commands = {Fight(Kind::StartFight, 0, 1, 1.0f), Fight(Kind::FightAuto, 0, 1, 0.2f),
		                 Fight(Kind::FightAuto, 1, 1, 0.2f)},
		});
	}

	{
		all.push_back({
		    .id = "combat.start_turn",
		    .name = "Fighters turn on the spot as the duel starts",
		    .facet = Facet::Combat,
		    .description = "The red tiger starts a fight with the blue tiger to its east, and both fight by themselves. The "
		                   "camera looks at the arena from the side.",
		    .expected = "As the duel starts, each tiger first turns on the spot to the other with its turn animation. A very "
		                "small turn also plays this animation. Then each tiger plays its start animation, and it faces the "
		                "other at once on each game turn. As a tiger starts or ends a block, it does not turn.",
		    .framing = ringside,
		    .creatures = {red(PlayerNames::PLAYER_ONE, 0.6f, -0.2f), blue(PlayerNames::PLAYER_TWO, 0.5f, -0.2f)},
		    .commands = {Fight(Kind::StartFight, 0, 1, 1.0f), Fight(Kind::FightAuto, 0, 1, 0.2f)},
		});
	}

	{
		all.push_back({
		    .id = "combat.turn_blend",
		    .name = "Fighters move smoothly between game turns",
		    .facet = Facet::Combat,
		    .description = "The red tiger starts a fight with the blue tiger to its east, and both fight by themselves. The "
		                   "camera looks at the arena from the side.",
		    .expected = "Each body takes a pose once a game turn, and each frame shows the body between its last two poses. "
		                "A lunge or a step moves the body smoothly forward. The body does not move ahead between two turns "
		                "and then jump back. A push apart also shows over one turn.",
		    .framing = ringside,
		    .creatures = {red(PlayerNames::PLAYER_ONE, 0.6f, -0.2f), blue(PlayerNames::PLAYER_TWO, 0.5f, -0.2f)},
		    .commands = {Fight(Kind::StartFight, 0, 1, 1.0f), Fight(Kind::FightAuto, 0, 1, 0.2f)},
		});
		all.push_back({
		    .id = "combat.queued_blows",
		    .name = "Queued blows follow each other at once",
		    .facet = Facet::Combat,
		    .description = "The player's red tiger fights a blue tiger that fights by itself. The red tiger gets three "
		                   "blows in the middle quickly, so the second blow and the third blow wait in its queue.",
		    .expected = "Each queued blow starts in the game turn in which the blow before it ends, and it moves in that "
		                "turn. The last part of each blow, less than one turn, does not play. The original game does the "
		                "same.",
		    .framing = ringside,
		    .creatures = {red(PlayerNames::PLAYER_ONE, 0.5f, 0.0f), blue(PlayerNames::PLAYER_TWO, 0.5f, 0.3f)},
		    .commands = {Fight(Kind::StartFight, 0, 1, 1.0f), Fight(Kind::FightBlow, 0, k_Mid, 14.0f, 0.0f),
		                 Fight(Kind::FightBlow, 0, k_Mid, 0.1f, 0.0f), Fight(Kind::FightBlow, 0, k_Mid, 0.1f, 0.0f)},
		    .repeatFrom = 1,
		});
		all.push_back({
		    .id = "combat.own_end_tests",
		    .name = "The next fight state plays in the turn that the last one ends",
		    .facet = Facet::Combat,
		    .description = "The player's red tiger knows Heal and fights a blue tiger that fights by itself. The red tiger "
		                   "blocks, then gets a blow in the middle, then Heal. Then the debug tools stop the fight.",
		    .expected =
		        "The start of the fight plays by the span of the turn and ends when the span gets to its end. "
		        "When the start, the start or end of the block, or the start, hold or end of the cast ends, the "
		        "next state moves on in the same turn. The next move starts in the turn that the block ends. The finish "
		        "ends when the span gets to its end, a little before its last frame. The original game does the "
		        "same.",
		    .framing = ringside,
		    .creatures = {red(PlayerNames::PLAYER_ONE, 0.5f, 0.0f), blue(PlayerNames::PLAYER_TWO, 0.5f, 0.3f)},
		    .commands = {{.kind = Kind::KnowMiracle,
		                  .creature = 0,
		                  .delaySeconds = 0.5f,
		                  .value = static_cast<size_t>(MagicType::Heal)},
		                 Fight(Kind::StartFight, 0, 1, 0.5f),
		                 Fight(Kind::FightBlock, 0, 0, 14.0f),
		                 Fight(Kind::FightBlow, 0, k_Mid, 3.0f, 0.0f),
		                 Fight(Kind::FightSpell, 0, static_cast<size_t>(MagicType::Heal), 5.0f),
		                 Fight(Kind::FightAbort, 0, 0, 6.0f)},
		});
		all.push_back({
		    .id = "appearance.burning_body_flames",
		    .name = "Flames stay on the limbs of a burning creature",
		    .facet = Facet::Appearance,
		    .description = "A tiger of no player stands north of the middle, short of the testbed's lake, and catches "
		                   "fire. The camera follows it closely.",
		    .expected = "It runs to the lake. Each flame stays on one part of the body: a flame on a leg moves with "
		                "the leg in each frame. The flames do not stay behind at the place where the limb was.",
		    .framing = {.shot = Shot::Follow, .distance = 1.4f},
		    .creatures = {CreatureSetup {
		        .species = CreatureType::Tiger, .offset = {0.0f, 60.0f}, .owner = PlayerNames::NEUTRAL}},
		    .commands = {{.kind = Kind::SetAlight, .creature = 0, .delaySeconds = 1.0f}},
		});
		all.push_back({
		    .id = "leash.rope_starts_at_collar",
		    .name = "A new leash rope starts at the collar",
		    .facet = Facet::Leash,
		    .description = "A tiger walks to the west. While it walks, it goes on the learning leash, then the leash "
		                   "comes off. The leash goes on again three times.",
		    .expected = "Each time the leash goes on, the rope starts straight from the collar to the hand. The rope "
		                "then follows the collar in each frame. It does not start at a point away from the body.",
		    .framing = {.shot = Shot::Follow, .distance = 1.4f},
		    .creatures = {Content(CreatureType::Tiger, {0.0f, 40.0f})},
		    .commands = {Go(Kind::WalkTo, 0, {-80.0f, 40.0f}, 1.0f, false), Leash(0, LeashType::Rope, 1.0f),
		                 Act(Kind::TakeOffLeash, 0, 2.0f), Leash(0, LeashType::Rope, 1.0f), Act(Kind::TakeOffLeash, 0, 2.0f),
		                 Leash(0, LeashType::Rope, 1.0f), Act(Kind::TakeOffLeash, 0, 2.0f), Leash(0, LeashType::Rope, 1.0f)},
		    .repeatFrom = 0,
		});
	}
}

void AddMind(std::vector<Scenario>& all)
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
	                "did not go to yet, at the ground there. When that place is no longer in front of it, it looks ahead "
	                "again until the hill or the sea comes back into view.",
	    .framing = {.shot = Shot::Testbed},
	    .creatures = {Content(CreatureType::Lion, {0.0f, 0.0f}, 180.0f)},
	    .objects = {{.type = TreeInfo::Birch, .offset = {100.0f, 0.0f}}},
	    .commands = {{.kind = Kind::DoAction, .delaySeconds = 1.0f, .object = 0, .action = "BeIdle"},
	                 {.kind = Kind::DoAction, .delaySeconds = 15.0f, .object = 0, .action = "BeIdle"}},
	    .repeatFrom = 1,
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
	                "stand. The inspector shows the partners while they wave.",
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
}

void AddParticles(std::vector<Scenario>& all)
{
	all.push_back({
	    .id = "particles.smoke_and_fire",
	    .name = "Smoke, steam and a bonfire",
	    .facet = Facet::Particles,
	    .description = "The spot visuals' smoke, steam, evil smoke and a bonfire, side by side on the plain.",
	    .expected = "Grey smoke and white steam puffs rise, spin and fade; the evil smoke is dark; the bonfire's flames "
	                "flicker in its additive sprites with smoke rising above them.",
	    .framing = {.shot = Shot::Overview, .include = {{-40.0f, 0.0f}, {40.0f, 0.0f}}, .distance = 0.3f},
	    .particles = {{.type = ParticleType::Smoke, .offset = {-30.0f, 0.0f}, .magnitude = 8.0f},
	                  {.type = ParticleType::Steam, .offset = {-10.0f, 0.0f}, .magnitude = 8.0f},
	                  {.type = ParticleType::EvilSmoke, .offset = {10.0f, 0.0f}, .magnitude = 8.0f},
	                  {.type = ParticleType::Bonfire, .offset = {30.0f, 0.0f}, .magnitude = 5.0f}},
	});

	all.push_back({
	    .id = "particles.sparkles",
	    .name = "Sparkles and magic",
	    .facet = Facet::Particles,
	    .description = "Short effects started again every few seconds: the failed cast, the hand gripping the land, a "
	                   "magic object made, an object appearing in the second player's colour, and a forest made.",
	    .expected = "Each plays out in sprites from the sprite sheets and starts again: the cross of the failed cast, "
	                "a ring of sparks, a spinning cloud of sparks, sparks fountaining up in green, and falling leaves.",
	    .framing = {.shot = Shot::Overview, .include = {{-30.0f, 0.0f}, {30.0f, 0.0f}}, .distance = 0.45f},
	    .particles =
	        {{.type = ParticleType::SpellFail, .offset = {-30.0f, 0.0f}, .height = 5.0f, .restartSeconds = 4.0f},
	         {.type = ParticleType::GripLandscape, .offset = {-15.0f, 0.0f}, .restartSeconds = 3.0f},
	         {.type = ParticleType::MagicObjectCreated,
	          .offset = {0.0f, 0.0f},
	          .height = 3.0f,
	          .magnitude = 3.0f,
	          .restartSeconds = 5.0f},
	         {.type = ParticleType::ObjectAppear,
	          .offset = {15.0f, 0.0f},
	          .magnitude = 3.0f,
	          .player = 1,
	          .restartSeconds = 4.0f},
	         {.type = ParticleType::ForestCreated, .offset = {30.0f, 0.0f}, .magnitude = 1.0f, .restartSeconds = 6.0f}},
	});

	all.push_back({
	    .id = "particles.models",
	    .name = "Models among the sparks",
	    .facet = Facet::Particles,
	    .description = "Effects whose particles are models: the vortex rising where a magic object is made, the glowing "
	                   "cone of the beam marking a place, the paper streamers of the ticker tape and the fireball's flash.",
	    .expected = "A spinning additive vortex model rises and fades; a glowing cone turns to face the camera wherever it "
	                "looks from; coloured streamers fall from a disk; the flash's model plays through its texture's frames.",
	    .framing = {.shot = Shot::Overview, .include = {{-30.0f, 0.0f}, {30.0f, 0.0f}}, .distance = 0.4f},
	    .particles = {{.file = "SF_MagicObjectCreated2", .offset = {-30.0f, 0.0f}, .magnitude = 2.0f, .restartSeconds = 5.0f},
	                  {.file = "SF_SeeThisBeam", .offset = {-10.0f, 0.0f}, .magnitude = 1.0f, .restartSeconds = 6.0f},
	                  {.file = "SF_TickerTape", .offset = {10.0f, 0.0f}, .height = 15.0f, .restartSeconds = 6.0f},
	                  {.file = "SF_Flash", .offset = {30.0f, 0.0f}, .height = 3.0f, .magnitude = 2.0f, .restartSeconds = 3.0f}},
	});

	// The beam that marks a place for the player, as the first land marks where its vortex opens
	all.push_back({
	    .id = "particles.see_this_beam",
	    .name = "The beam that marks a place",
	    .facet = Facet::Particles,
	    .description = "The yellow beam that a script puts on a place to show it to the player, as the first land shows "
	                   "where its vortex will open. It starts again every ten seconds.",
	    .expected = "A yellow cone of light stands on the land and turns to face the camera. Its brightness beats with "
	                "the scrolls' highlights, about once every 1.3 seconds, from a dim glow to full and back.",
	    .environment = {.hour = 12.0f, .clockRuns = false, .dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-6.0f, -6.0f}, {6.0f, 6.0f}}, .distance = 0.5f},
	    .particles = {{.file = "SF_SeeThisBeam", .offset = {0.0f, 0.0f}, .magnitude = 1.0f, .restartSeconds = 10.0f}},
	});

	all.push_back({
	    .id = "particles.tumble",
	    .name = "Tumbling streamers and wood",
	    .facet = Facet::Particles,
	    .description = "Effects whose particles tumble as they move: the paper streamers of the ticker tape and the planks of "
	                   "wood that a store gives out. Each turns about its forward axis when it moves more east to west, else "
	                   "about its east to west axis.",
	    .expected = "Streamers fall from a disk and planks fly out, each turning as it goes, faster the faster it moves.",
	    .framing = {.shot = Shot::Overview, .include = {{-12.0f, 0.0f}, {12.0f, 0.0f}}, .distance = 0.25f},
	    .particles = {{.file = "SF_TickerTape", .offset = {-8.0f, 0.0f}, .height = 15.0f, .restartSeconds = 6.0f},
	                  {.file = "SF_Wood", .offset = {8.0f, 0.0f}, .height = 2.0f, .magnitude = 1.0f, .restartSeconds = 4.0f}},
	});

	all.push_back({
	    .id = "particles.light_maps",
	    .name = "Light on the land",
	    .facet = Facet::Particles,
	    .description = "Effects that light the ground under them, as fire and lightning do, at night so the light shows: "
	                   "the ground effect's ring and the light a landscape vortex leaves, each many cells across.",
	    .expected = "A ring of light spreads over the ground and fades, again and again; a steady pool of coloured light "
	                "lies on the ground beside it, lighting what stands there too.",
	    .environment = {.hour = 23.0f},
	    .framing = {.shot = Shot::Overview, .include = {{-120.0f, 0.0f}, {120.0f, 0.0f}}, .distance = 0.4f},
	    .particles = {{.file = "SF_GroundEffect", .offset = {-70.0f, 0.0f}, .magnitude = 2.0f, .restartSeconds = 4.0f},
	                  {.file = "SF_LandscapeVortexLightMap", .offset = {70.0f, 0.0f}}},
	});

	// The singing stones' heal as the funeral march's raising starts it: the script makes it 13.5 times its size, in the
	// colour of this computer's player
	all.push_back({
	    .id = "particles.singing_stones_heal_rings",
	    .name = "The rings of light of the singing stones' heal",
	    .facet = Facet::Particles,
	    .description = "The singing stones' heal effect at 13.5 times its size, in the first player's colour, at night. "
	                   "Its rule stands three rings of light on the land round the place where the effect starts, each "
	                   "about 15 m across from the centre, under the glow that the effect's light map puts on the land.",
	    .expected = "Three rings of light stand one on the other round the light on the land, 10 high, rising and "
	                "falling in waves that roll round them, their stars sliding up. They are a dim red: the player's red "
	                "times the alpha of 30 of the effect's points. They follow the land and stay where they were made.",
	    .environment = {.hour = 23.0f, .dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-20.0f, -20.0f}, {20.0f, 20.0f}}, .distance = 0.5f},
	    .particles = {{.file = "SF_SingingStonesHeal", .offset = {0.0f, 0.0f}, .magnitude = 13.5f, .player = 0}},
	});

	all.push_back({
	    .id = "particles.mist_shadows",
	    .name = "Shadows of a storm's mist",
	    .facet = Facet::Particles,
	    .description = "The lightning storm effect over the land at noon. Each puff of its cloud stamps a grey light map on "
	                   "the land that darkens the land's luminosity, as the sky's clouds do, for the frame after it is drawn.",
	    .expected = "Dark patches move on the ground under the storm's clouds and fade with them. The ground goes darker, "
	                "not a different colour, and the things that stand on it go darker with it.",
	    .environment = {.hour = 12.0f},
	    .framing = {.shot = Shot::Overview, .include = {{-120.0f, -120.0f}, {120.0f, 120.0f}}, .distance = 0.5f},
	    .particles = {{.file = "SF_LightningStorm", .offset = {0.0f, 0.0f}, .magnitude = 1.0f, .restartSeconds = 20.0f}},
	});

	all.push_back({
	    .id = "particles.vortex_and_flies_sounds",
	    .name = "The landscape vortex's and the flies' sounds",
	    .facet = Facet::Particles,
	    .description = "The landscape vortex as it opens and as it closes, and a swarm of flies, side by side and close up, "
	                   "every eight seconds. Their sounds are named in the effect files by sound actions the game's header "
	                   "numbers by counting on from the one before.",
	    .expected = "The vortices play the vortex's hum and the flies their buzz as they start; the debug log names an "
	                "emitter for each.",
	    .framing = {.shot = Shot::Overview, .include = {{-15.0f, 30.0f}, {15.0f, 50.0f}}, .distance = 0.4f},
	    .particles = {{.file = "SF_LandscapeVortexInBefore", .offset = {-8.0f, 40.0f}, .restartSeconds = 8.0f},
	                  {.file = "SF_LandscapeVortexOutAfter", .offset = {8.0f, 40.0f}, .restartSeconds = 8.0f},
	                  {.file = "SF_Flies", .offset = {0.0f, 36.0f}, .height = 2.0f, .restartSeconds = 8.0f}},
	});

	all.push_back({
	    .id = "particles.flies_face_their_way",
	    .name = "Flies turned to the way they fly",
	    .facet = Facet::Particles,
	    .description = "A swarm of flies close up, every eight seconds. Each fly is a sprite that the flock rule turns on "
	                   "the screen.",
	    .expected = "Each fly's picture turns to point the way that the fly moves across the screen, as the game turns "
	                "it. A fly that flies to the right on the screen points to the right.",
	    .framing = {.shot = Shot::Overview, .include = {{-3.0f, 33.0f}, {3.0f, 39.0f}}, .distance = 0.15f},
	    .particles = {{.file = "SF_Flies", .offset = {0.0f, 36.0f}, .height = 2.0f, .restartSeconds = 8.0f}},
	});

	all.push_back({
	    .id = "particles.ribbon_slide",
	    .name = "A ribbon's corners and its texture sliding along it",
	    .facet = Facet::Particles,
	    .description = "The simple beam, three ribbons from a point ten metres up to a creature twenty metres away, "
	                   "started again every six seconds and seen close from the side.",
	    .expected = "Each beam is a ribbon that faces the camera from every side, its corners meeting half way where two "
	                "segments join. Its texture slides along it at twice the speed of its data, as the game adds the "
	                "slide to the texture twice. The slide moves on only by the frames drawn, and stops while the game "
	                "pauses.",
	    .framing = {.shot = Shot::Overview, .include = {{-15.0f, -5.0f}, {25.0f, 5.0f}}, .distance = 0.35f},
	    .creatures = {{.offset = {20.0f, 0.0f}, .pauseMind = true}},
	    .particles = {{.file = "SF_SimpleBeam",
	                   .targetsCreatures = true,
	                   .offset = {-10.0f, 0.0f},
	                   .height = 10.0f,
	                   .magnitude = 1.0f,
	                   .restartSeconds = 6.0f}},
	});

	all.push_back({
	    .id = "particles.mist",
	    .name = "Mist over the holders",
	    .facet = Facet::Particles,
	    .description = "The water and storm miracles' mists, as they sit over a holder, and the water miracle in the hand.",
	    .expected = "Puffs of mist, shaped wider than tall, turn to face the camera, shrink seen edge on and grow seen from "
	                "above, and play through their frames of the smoke texture.",
	    .framing = {.shot = Shot::Overview, .include = {{-20.0f, 0.0f}, {20.0f, 0.0f}}, .distance = 0.3f},
	    .particles = {{.file = "SF_WaterOnHolder", .offset = {-15.0f, 0.0f}, .height = 4.0f, .magnitude = 3.0f},
	                  {.file = "SF_LightningStormOnHolder", .offset = {0.0f, 0.0f}, .height = 4.0f, .magnitude = 3.0f},
	                  {.file = "SF_WaterInHand", .offset = {15.0f, 0.0f}, .height = 4.0f, .magnitude = 3.0f}},
	});

	all.push_back({
	    .id = "particles.symbols_and_heal",
	    .name = "Player symbols and the heal chakra",
	    .facet = Facet::Particles,
	    .description = "The player icon fountain throwing up the casting player's symbol in each player's colour, and the "
	                   "heal miracle's chakra over two creatures.",
	    .expected = "Symbols between two glows of the player's colour, one turning, arc up out of each fountain and fade; "
	                "a chakra appears over each creature and follows it, a burst of sparks under it rising and fading.",
	    .framing = {.shot = Shot::Overview, .include = {{-30.0f, 0.0f}, {30.0f, 0.0f}}, .distance = 0.45f},
	    .creatures = {{.offset = {15.0f, 0.0f}}, {.offset = {30.0f, 5.0f}}},
	    .particles = {{.file = "SF_PlayerIconFountain", .offset = {-30.0f, 0.0f}, .magnitude = 2.0f, .player = 0},
	                  {.file = "SF_PlayerIconFountain", .offset = {-15.0f, 0.0f}, .magnitude = 2.0f, .player = 1},
	                  {.file = "SF_HealChakra",
	                   .targetsCreatures = true,
	                   .offset = {22.0f, 0.0f},
	                   .magnitude = 2.0f,
	                   .restartSeconds = 6.0f}},
	});

	all.push_back({
	    .id = "particles.profile_symbol",
	    .name = "The symbol of the player's profile in the world",
	    .facet = Facet::Particles,
	    .description = "The player icon fountain of the first player and of the second player, side by side. The world "
	                   "shows the tenth symbol for the first player, as when the player's profile has that symbol.",
	    .expected = "The fountain on the left throws up the tenth symbol of the players' symbols page in the first "
	                "player's colour. The fountain on the right throws up the second player's symbol as the file of the "
	                "players' symbols has it.",
	    .environment = {.profileSymbol = 9u},
	    .framing = {.shot = Shot::Overview, .include = {{-15.0f, 0.0f}, {15.0f, 0.0f}}, .distance = 0.35f},
	    .particles = {{.file = "SF_PlayerIconFountain", .offset = {-10.0f, 0.0f}, .magnitude = 2.0f, .player = 0},
	                  {.file = "SF_PlayerIconFountain", .offset = {10.0f, 0.0f}, .magnitude = 2.0f, .player = 1}},
	});

	all.push_back({
	    .id = "particles.gods_symbols",
	    .name = "The computer gods' pictures in the world",
	    .facet = Facet::Particles,
	    .description = "As on the second land, a land script switches on the computer minds of the second and third "
	                   "players. The sheet of the players' symbols is then made again, as at the start of a land. The "
	                   "player icon fountains of the first three players stand side by side.",
	    .expected = "The fountain on the left throws up the symbol of the player's profile. The fountain in the middle "
	                "throws up the picture of Lethys for the second player. The fountain on the right throws up the "
	                "picture of Khazar for the third player. Each picture is in the colour of its player.",
	    .environment = {.symbolsAfterLandScript = true, .landNumber = 2},
	    .framing = {.shot = Shot::Overview, .include = {{-20.0f, 0.0f}, {20.0f, 0.0f}}, .distance = 0.35f},
	    .particles = {{.file = "SF_PlayerIconFountain", .offset = {-15.0f, 0.0f}, .magnitude = 2.0f, .player = 0},
	                  {.file = "SF_PlayerIconFountain", .offset = {0.0f, 0.0f}, .magnitude = 2.0f, .player = 1},
	                  {.file = "SF_PlayerIconFountain", .offset = {15.0f, 0.0f}, .magnitude = 2.0f, .player = 2}},
	    .landScript = R"(TOGGLE_COMPUTER_PLAYER("PLAYER_TWO", 1)
TOGGLE_COMPUTER_PLAYER("PLAYER_THREE", 1)
)",
	});

	all.push_back({
	    .id = "particles.draw_paths",
	    .name = "Sorted and queued sprites",
	    .facet = Facet::Particles,
	    .description = "The same smoke drawn three ways side by side: each puff in its own place among what blends, the "
	                   "whole effect at its origin as some spot visuals are, and as the miracle in the hand is.",
	    .expected = "The sorted smoke's puffs blend far to near from any side; the queued smoke draws newest first as one, "
	                "so seen from above older puffs cover newer ones; the third matches the queued without a hand.",
	    .framing = {.shot = Shot::Overview, .include = {{-20.0f, 0.0f}, {20.0f, 0.0f}}, .distance = 0.3f},
	    .particles = {{.type = ParticleType::Smoke, .offset = {-20.0f, 0.0f}, .magnitude = 8.0f},
	                  {.type = ParticleType::Smoke,
	                   .path = particles::draw::DrawPath::Queued,
	                   .offset = {0.0f, 0.0f},
	                   .magnitude = 8.0f},
	                  {.type = ParticleType::Smoke,
	                   .path = particles::draw::DrawPath::Immediate,
	                   .offset = {20.0f, 0.0f},
	                   .magnitude = 8.0f}},
	});

	all.push_back({
	    .id = "particles.fireball_rows",
	    .name = "The fireball's heat by its power-up",
	    .facet = Facet::Particles,
	    .description = "Three fireballs are thrown at three oaks every twelve seconds: the plain fireball on the left, "
	                   "the first power-up in the middle and the second power-up on the right.",
	    .expected = "The first power-up uses the second row of the fireball table, so it starts hotter and sets its oak "
	                "on fire sooner. The plain fireball and the second power-up use the first row.",
	    .framing = {.shot = Shot::Overview, .include = {{-40.0f, 0.0f}, {40.0f, 30.0f}}, .distance = 0.45f},
	    .objects = {{.type = TreeInfo::Oak, .offset = {-30.0f, 25.0f}},
	                {.type = TreeInfo::Oak, .offset = {0.0f, 25.0f}},
	                {.type = TreeInfo::Oak, .offset = {30.0f, 25.0f}}},
	    .miracles = {{.type = MagicType::Fireball,
	                  .point = {-30.0f, 25.0f},
	                  .handOffset = {-30.0f, 10.0f},
	                  .handHeight = 6.0f,
	                  .throwVelocity = {0.0f, 0.0f, 8.0f},
	                  .delaySeconds = 2.0f,
	                  .repeatSeconds = 12.0f},
	                 {.type = MagicType::FireballPowerUpOne,
	                  .point = {0.0f, 25.0f},
	                  .handOffset = {0.0f, 10.0f},
	                  .handHeight = 6.0f,
	                  .throwVelocity = {0.0f, 0.0f, 8.0f},
	                  .delaySeconds = 2.0f,
	                  .repeatSeconds = 12.0f},
	                 {.type = MagicType::FireballPowerUpTwo,
	                  .point = {30.0f, 25.0f},
	                  .handOffset = {30.0f, 10.0f},
	                  .handHeight = 6.0f,
	                  .throwVelocity = {0.0f, 0.0f, 8.0f},
	                  .delaySeconds = 2.0f,
	                  .repeatSeconds = 12.0f}},
	});

	all.push_back({
	    .id = "particles.lightning_forks_arcs",
	    .name = "The lightning bolt's forks and electric arcs",
	    .facet = Facet::Particles,
	    .description = "A lightning bolt held over five villagers and an oak for six seconds, again every ten seconds.",
	    .expected = "The bolt keeps two forks for each target that it finds, and no more. Electric arcs crawl over the "
	                "things that it strikes, from one point of each model to another.",
	    .framing = {.shot = Shot::Overview, .include = {{-20.0f, 0.0f}, {20.0f, 20.0f}}, .distance = 0.35f},
	    .objects = {{.type = VillagerInfo::CelticFarmerMale, .offset = {-8.0f, 10.0f}, .inScript = true},
	                {.type = VillagerInfo::CelticHousewifeFemale, .offset = {-3.0f, 14.0f}, .inScript = true},
	                {.type = VillagerInfo::CelticForesterMale, .offset = {3.0f, 9.0f}, .inScript = true},
	                {.type = VillagerInfo::CelticShepherdMale, .offset = {7.0f, 13.0f}, .inScript = true},
	                {.type = VillagerInfo::CelticTraderMale, .offset = {0.0f, 17.0f}, .inScript = true},
	                {.type = TreeInfo::Oak, .offset = {10.0f, 18.0f}}},
	    .miracles = {{.type = MagicType::LightningBolt,
	                  .point = {0.0f, 12.0f},
	                  .handOffset = {0.0f, 0.0f},
	                  .handHeight = 15.0f,
	                  .delaySeconds = 2.0f,
	                  .holdSeconds = 6.0f,
	                  .repeatSeconds = 10.0f}},
	});

	all.push_back({
	    .id = "particles.globe_alpha",
	    .name = "The one-shot globe's see-through bubble and rings",
	    .facet = Facet::Particles,
	    .description = "Two one-shot globes close up: a plain fireball and the fireball's first power-up, whose miracle "
	                   "shows one ring.",
	    .expected = "Each bubble is added over what is behind it at an alpha of 149, the globe's 150 times the land's "
	                "fully opaque light. The ring round the powered-up miracle shows at an alpha of 34.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 12.0f, -18.0f}, .look = {0.0f, 4.0f, 8.0f}},
	    .dispensers = {{.type = MagicType::Fireball, .offset = {-5.0f, 8.0f}},
	                   {.type = MagicType::FireballPowerUpOne, .offset = {5.0f, 8.0f}}},
	});
}

void AddEditor(std::vector<Scenario>& all)
{
	all.push_back({
	    .id = "editor.one_of_each",
	    .name = "One of each kind to pick",
	    .facet = Facet::Editor,
	    .description = "A tiger, a villager, an oak, a pillar of rock and a ball, spread out in view, for trying the "
	                   "editor (F2) on: the outliner lists each under its kind, a click on the land or in the list "
	                   "picks one, and the inspector shows it.",
	    .expected = "Each picks with a box drawn round it. The Move (E) and Rotate (R) tools drag the picked thing over "
	                "the land and turn it; Ctrl+D copies it beside itself and Delete removes it. The tiger's inspector "
	                "has every creature section.",
	    .environment = {.clockRuns = false},
	    .framing = {.shot = Shot::Overview, .include = {{-40.0f, 30.0f}, {40.0f, -30.0f}}},
	    .creatures = {Posed(CreatureType::Tiger, {0.0f, 0.0f})},
	    .objects = {{.type = VillagerInfo::CelticFarmerMale, .offset = {-25.0f, -10.0f}},
	                {.type = TreeInfo::Oak, .offset = {-40.0f, 20.0f}},
	                {.type = FeatureInfo::FatPilarChalk, .offset = {40.0f, 20.0f}},
	                {.type = MobileObjectInfo::Ball, .offset = {20.0f, -15.0f}}},
	});

	all.push_back({
	    .id = "editor.orbit_walker",
	    .name = "A walking creature to orbit and follow",
	    .facet = Facet::Editor,
	    .description = "A tiger walks round a square 80 units across, again and again, for trying the editor's cameras "
	                   "on it: pick it, then Orbit (O) or Follow (Shift+O).",
	    .expected = "Orbiting, the camera keeps round the tiger as it walks, turned by dragging with the right button "
	                "and drawn in and out by the wheel. Following, it eases along behind the tiger as it turns each "
	                "corner. Free, or Escape, hands the camera back where it is.",
	    .framing = {.shot = Shot::Overview, .include = {{-40.0f, -40.0f}, {40.0f, 40.0f}}},
	    .creatures = {Posed(CreatureType::Tiger, {-40.0f, -40.0f})},
	    .commands = {Go(Kind::WalkTo, 0, {40.0f, -40.0f}, 0.5f), Go(Kind::WalkTo, 0, {40.0f, 40.0f}, 0.5f),
	                 Go(Kind::WalkTo, 0, {-40.0f, 40.0f}, 0.5f), Go(Kind::WalkTo, 0, {-40.0f, -40.0f}, 0.5f)},
	    .repeatFrom = 0,
	});

	all.push_back({
	    .id = "editor.pick_several_undo",
	    .name = "Things to pick together, then undo and redo",
	    .facet = Facet::Editor,
	    .description = "A row of five oaks, a ring of four pillars of rock and two villagers, still, for trying the "
	                   "editor's picks of several things and its history (F2): a drag over the land with the Select (W) "
	                   "tool picks all in the rectangle, Ctrl+click picks or lets go of one more, Shift+click adds one, "
	                   "and Shift+click in the outliner picks a run of rows.",
	    .expected = "Each picked thing has a box; the main pick has the brighter box. Move (E) drags all the picks "
	                "together and Rotate (R) turns them about their middle. Ctrl+D copies each, Delete removes all. "
	                "Ctrl+Z puts each step back and Ctrl+Y (or Ctrl+Shift+Z) does it again; the tool bar names the step.",
	    .environment = {.clockRuns = false},
	    .framing = {.shot = Shot::Overview, .include = {{-40.0f, 30.0f}, {40.0f, -30.0f}}},
	    .objects = {{.type = TreeInfo::Oak, .offset = {-30.0f, 20.0f}},
	                {.type = TreeInfo::Oak, .offset = {-15.0f, 20.0f}},
	                {.type = TreeInfo::Oak, .offset = {0.0f, 20.0f}},
	                {.type = TreeInfo::Oak, .offset = {15.0f, 20.0f}},
	                {.type = TreeInfo::Oak, .offset = {30.0f, 20.0f}},
	                {.type = FeatureInfo::FatPilarChalk, .offset = {-10.0f, -10.0f}},
	                {.type = FeatureInfo::FatPilarChalk, .offset = {10.0f, -10.0f}},
	                {.type = FeatureInfo::FatPilarChalk, .offset = {-10.0f, -25.0f}},
	                {.type = FeatureInfo::FatPilarChalk, .offset = {10.0f, -25.0f}},
	                {.type = VillagerInfo::CelticFarmerMale, .offset = {-30.0f, -15.0f}},
	                {.type = VillagerInfo::CelticHousewifeFemale, .offset = {30.0f, -15.0f}}},
	});

	all.push_back({
	    .id = "editor.land_tools",
	    .name = "The land brush, footpaths, streams and script things",
	    .facet = Facet::Editor,
	    .description = "Two huts, a row of oaks and a pillar of rock on flat land, and the editor opens after a second. "
	                   "In the Land tools tab, the Land tool raises or lowers the land in a circle or paints its "
	                   "country, and the Paths tool draws a footpath or a stream, a click for each point. The Script tab "
	                   "of the palette places markers, scrolls and the scrolls of challenges. Save land files writes a "
	                   "landscape file, a land script, a footpath file and a world file into the profile folder's Editor "
	                   "folder.",
	    .expected = "A stroke of Raise makes a round hill: the full step in the inner circle, less to none at the outer "
	                "circle. The oaks and the pillar stay on the land. Paint country changes the ground's texture in "
	                "the outer circle. A footpath from hut to hut shows as a line of nodes in the View menu's "
	                "footpaths. A stream shows its bed and water. A marker shows as a pink diamond while the editor is "
	                "open.",
	    .environment = {.clockRuns = false},
	    .framing = {.shot = Shot::Overview, .include = {{-60.0f, 40.0f}, {60.0f, -40.0f}}},
	    .objects = {{.type = AbodeInfo::CelticHut, .offset = {-45.0f, -25.0f}},
	                {.type = AbodeInfo::CelticHut, .offset = {45.0f, -25.0f}},
	                {.type = TreeInfo::Oak, .offset = {-20.0f, 25.0f}},
	                {.type = TreeInfo::Oak, .offset = {0.0f, 25.0f}},
	                {.type = TreeInfo::Oak, .offset = {20.0f, 25.0f}},
	                {.type = FeatureInfo::FatPilarChalk, .offset = {0.0f, 0.0f}}},
	    .commands = {{.kind = Kind::PressPlainKey,
	                  .delaySeconds = 1.0f,
	                  .value = static_cast<size_t>(input::platform_keys::EditorToggleKey(k_Platform))}},
	});
}

/// A benchmark of a crowd of the size, to compare runs of several sizes and see which costs grow faster than the crowd
/// does
void AddCrowd(std::vector<Scenario>& all, Crowd::Kind kind, size_t count, std::string_view id, std::string_view name)
{
	const bool creatures = kind == Crowd::Kind::Creatures;
	all.push_back({
	    .id = id,
	    .name = name,
	    .facet = Facet::Benchmark,
	    .description = creatures ? "Creatures of every species and of four players, spread evenly over the land round the "
	                               "middle and clear of the lake, the same way every run. Nothing holds them: their minds, "
	                               "needs, walking and route finding all run as in the game."
	                             : "Villagers of every tribe and role in towns of fifty, each a ring of huts about a storage "
	                               "pit, spread evenly over the land round the middle and clear of the lake, the same way "
	                               "every run, and left to go about their lives.",
	    .expected = "They spawn a batch a frame, then the frames are measured once they have settled: the mean, 95th "
	                "percentile and slowest frame and the costliest profiler stages show as it runs, and Save results "
	                "writes them out to compare with the other sizes.",
	    // The crowd is spread over the middle, where the miracle dispensers would stand
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Testbed},
	    .crowd = Crowd {.kind = kind, .count = count, .seed = k_BenchmarkSeed, .perFrame = creatures ? 100u : 250u},
	});
}

/// A benchmark of 100 creatures that live their usual lives among towns, the most creatures a frame must hold at 100
/// frames a second
void AddCreaturesAtWork(std::vector<Scenario>& all)
{
	all.push_back({
	    .id = "benchmark.creatures_at_work",
	    .name = "100 creatures among towns",
	    .facet = Facet::Benchmark,
	    .description = "100 creatures of every species and of four players, spread evenly round four towns of 50 "
	                   "villagers each, clear of the homes and the lake, the same way every run. Nothing holds them: they "
	                   "eat, play, rest, look at the villagers and the buildings, walk and fight as in the game.",
	    .expected = "The towns spawn first, then the creatures, a batch a frame. Then the frames are measured once they "
	                "have settled. Every frame must be under 10 ms on the processor.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Testbed},
	    .crowd = Crowd {.kind = Crowd::Kind::Creatures,
	                    .count = 100,
	                    .seed = k_BenchmarkSeed,
	                    .perFrame = 100,
	                    .villagers = 4 * k_VillagersPerTown},
	});
}

void AddBenchmark(std::vector<Scenario>& all)
{
	// Steps close enough together that a cost growing faster than the crowd shows between them
	AddCrowd(all, Crowd::Kind::Creatures, 10, "benchmark.creatures_10", "10 active creatures");
	AddCrowd(all, Crowd::Kind::Creatures, 25, "benchmark.creatures_25", "25 active creatures");
	AddCrowd(all, Crowd::Kind::Creatures, 50, "benchmark.creatures_50", "50 active creatures");
	AddCrowd(all, Crowd::Kind::Creatures, 100, "benchmark.creatures_100", "100 active creatures");
	AddCrowd(all, Crowd::Kind::Villagers, 100, "benchmark.villagers_100", "100 active villagers");
	AddCrowd(all, Crowd::Kind::Villagers, 250, "benchmark.villagers_250", "250 active villagers");
	AddCrowd(all, Crowd::Kind::Villagers, 500, "benchmark.villagers_500", "500 active villagers");
	AddCrowd(all, Crowd::Kind::Villagers, 1'000, "benchmark.villagers_1000", "1,000 active villagers");
	AddCreaturesAtWork(all);
}

/// A task of a land's script that stops gives back the objects that its variables hold. The objects stay in a script
/// until the scripts' turn ends, and then they are in no script
void AddScriptTaskRelease(std::vector<Scenario>& all)
{
	using Argument = ScriptArgument;
	const auto stop = [](float delaySeconds) {
		return Command {.kind = Kind::StopScript, .delaySeconds = delaySeconds, .script = "CheckForCommentOnFoodItems"};
	};
	all.push_back({
	    .id = "story.stopped_task_lets_go",
	    .name = "A stopped script task lets go of the object that it holds",
	    .facet = Facet::Story,
	    .description = "The game's CheckForCommentOnFoodItems script runs with a food pot as its parameter, which it "
	                   "keeps in a variable. One second later the scenario stops the script, and one second after that "
	                   "it stops the script again.",
	    .expected = "The log tells that the first stop stopped one task and that the pot is still in a script for "
	                "the rest of that turn. The second stop stopped no task, and the pot is in no script.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview},
	    .objects = {{.type = PotInfo::FoodPot, .offset = {0.0f, -20.0f}}},
	    .commands = {{.kind = Kind::RunScript,
	                  .delaySeconds = 0.5f,
	                  .script = "CheckForCommentOnFoodItems",
	                  .scriptArguments = {{.kind = Argument::Kind::Object, .index = 0}}},
	                 stop(1.0f),
	                 stop(1.0f)},
	});

	all.push_back({
	    .id = "temple.room_help_without_sound",
	    .name = "The temple's room help when no sound can play",
	    .facet = Facet::Story,
	    .description = "Start the game with no sound device (for example with the environment ALSOFT_DRIVERS=none). "
	                   "The player has a temple, and is put in its main room for the first time.",
	    .expected = "The advisors come out and the help shows its lines one at a time, with no voice. Each line stays for "
	                "its reading time: 5 game turns for each word and 8 more. The help shows all nine lines, then the "
	                "advisors go home. With a sound device, each line stays until the advisor stops talking.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview},
	    .temples = {{.offset = {-300.0f, 100.0f}, .angle = 36.0f}},
	    .commands = {{.kind = Kind::EnterTemple, .delaySeconds = 2.0f, .value = 3}},
	});

	all.push_back({
	    .id = "temple.main_room_lean",
	    .name = "The temple's main room leans at the screen's edge and as the floor is dragged",
	    .facet = Facet::Story,
	    .description = "The player has a temple and is put in its main room. Move the pointer near the top or the bottom "
	                   "of the screen, then drag the floor up and down.",
	    .expected = "The camera starts to lean when the pointer is more than 0.8 of the way from the middle of the "
	                "screen to the top or bottom edge, and leans faster nearer the edge. Dragging the floor leans the "
	                "camera by 0.06 for each pixel and second, less the nearer the pointer is to the sides.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview},
	    .temples = {{.offset = {-300.0f, 100.0f}, .angle = 36.0f}},
	    .commands = {{.kind = Kind::EnterTemple, .delaySeconds = 5.0f, .value = 3}},
	});

	all.push_back({
	    .id = "temple.low_detail_rooms",
	    .name = "At the lowest detail the temple's rooms have no light maps",
	    .facet = Facet::Story,
	    .description = "Start the game with --detail-level 0. The player has a temple and is put in its main room. Walk "
	                   "through the doors into the other rooms.",
	    .expected = "Each room, its floor and its pools are the rooms' low models: plainer walls with no light from "
	                "light maps, each wall hazed as one piece. The domes are as at other levels. The doors, the "
	                "scrolls and the clicks on them work as at other levels. With no --detail-level, or above 0, the "
	                "rooms have their light maps.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview},
	    .temples = {{.offset = {-300.0f, 100.0f}, .angle = 36.0f}},
	    .commands = {{.kind = Kind::EnterTemple, .delaySeconds = 2.0f, .value = 3}},
	});

	all.push_back({
	    .id = "temple.low_detail_scroll_text",
	    .name = "At the lowest detail levels the scrolls have textures of half the size",
	    .facet = Facet::Appearance,
	    .description = "Start the game with --detail-level 1 (Low) or 0 (Minimum). The player has a temple and is put in "
	                   "its main room. Look at the scrolls on the walls from a distance, then start again with "
	                   "--detail-level 4 to compare.",
	    .expected = "At Low and Minimum the text and the parchment of each scroll are softer, with half the texels "
	                "along each side. Each texel is the mean of four texels of the full text. When the camera goes "
	                "close to a scroll, the text in front of it is as sharp as at other levels.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview},
	    .temples = {{.offset = {-300.0f, 100.0f}, .angle = 36.0f}},
	    .commands = {{.kind = Kind::EnterTemple, .delaySeconds = 2.0f, .value = 3}},
	});

	all.push_back({
	    .id = "temple.low_detail_map",
	    .name = "At the lowest detail levels the temple's map has a texture of half the size",
	    .facet = Facet::Appearance,
	    .description = "Start the game with --detail-level 1 (Low) or 0 (Minimum). The player has a temple and is put in "
	                   "its main room. Look at the map of the island over the pool, then start again with "
	                   "--detail-level 4 to compare.",
	    .expected = "At Low and Minimum the land on the map has half the texels along each side: 4 for each block of the "
	                "land, not 8. Each texel has the colour of one texel of the full map, not a mean of four. The "
	                "shape of the map, its markers and its fade at the coast do not change.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview},
	    .temples = {{.offset = {-300.0f, 100.0f}, .angle = 36.0f}},
	    .commands = {{.kind = Kind::EnterTemple, .delaySeconds = 2.0f, .value = 3}},
	});

	all.push_back({
	    .id = "temple.way_out_glow",
	    .name = "A white light grows at the temple's way out as the player leaves",
	    .facet = Facet::Story,
	    .description = "The player has a temple and is put in its main room. After ten seconds the camera walks to the "
	                   "door out of the temple and through it, as a click on that door does.",
	    .expected = "The camera turns to the door out. 2.2 seconds after the walk starts, a white sheet of light grows "
	                "on the floor from the door towards the middle of the room, added over all of the room, its texture "
	                "slowly drifting. It is brightest and widest at the door and fades to nothing towards the middle. "
	                "Then the player leaves the temple.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview},
	    .temples = {{.offset = {-300.0f, 100.0f}, .angle = 36.0f}},
	    .commands = {{.kind = Kind::EnterTemple, .delaySeconds = 2.0f, .value = 3},
	                 {.kind = Kind::EnterTemple, .delaySeconds = 10.0f, .value = static_cast<size_t>(TempleRoom::Unknown)}},
	});
	// The second visit uses the draws and the scroll textures of the first visit again
	all.push_back({
	    .id = "temple.visit_again",
	    .name = "The player goes into the temple, leaves it and goes in again",
	    .facet = Facet::Story,
	    .description = "The player has a temple and goes into its main room after two seconds. After six seconds the "
	                   "camera walks out through the door. After sixteen seconds the player goes into the main room "
	                   "again.",
	    .expected = "Each time, the camera comes into the main room along its path and the room, its scrolls and the "
	                "map of the land show as on the first visit. Outside, the land and its things show as before the "
	                "visit. No frame of a visit, of the walk out or of the land after it takes 10 ms or more.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview},
	    .temples = {{.offset = {-300.0f, 100.0f}, .angle = 36.0f}},
	    .commands = {{.kind = Kind::EnterTemple, .delaySeconds = 2.0f, .value = static_cast<size_t>(TempleRoom::Main)},
	                 {.kind = Kind::EnterTemple, .delaySeconds = 6.0f, .value = static_cast<size_t>(TempleRoom::Unknown)},
	                 {.kind = Kind::EnterTemple, .delaySeconds = 16.0f, .value = static_cast<size_t>(TempleRoom::Main)}},
	});
}

/// Land 1's script natives and land script commands that the lane land1-script-stubs completed
void AddLandOneScriptStubScenarios(std::vector<Scenario>& all)
{
	using Argument = ScriptArgument;
	// A script's dance about a person, as the Pied Piper's dance of Land 1, which wants its camera at once
	all.push_back({
	    .id = "story.dance_camera",
	    .name = "The camera of a dance holds the view until a grip of the land",
	    .facet = Facet::Story,
	    .description = "A leader stands in the middle with six villagers round him. After one second, a script makes the "
	                   "dance about a person (the dance of the Pied Piper on Land 1) about the leader, and the six "
	                   "villagers join it. The scenario says that the hand was near the dance for more than ten seconds, "
	                   "so the dance wants its camera.",
	    .expected = "The villagers walk to their places and dance. At the first key frame of the dance that asks for "
	                "the camera, the camera holds its view. The keys, the wheel and the mouse do not move it. A grip of "
	                "the land with the left button gives the camera back in the next game turn, and the drag of the land "
	                "starts. The dance does not take the camera again. The log tells the number of dancers.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-15.0f, -15.0f}, {15.0f, 15.0f}}},
	    .objects = {{.type = VillagerInfo::CelticLeaderMale, .offset = {0.0f, 0.0f}},
	                {.type = VillagerInfo::CelticHousewifeFemale, .offset = {6.0f, 0.0f}},
	                {.type = VillagerInfo::CelticFarmerMale, .offset = {-6.0f, 0.0f}},
	                {.type = VillagerInfo::CelticHousewifeFemale, .offset = {0.0f, 6.0f}},
	                {.type = VillagerInfo::CelticForesterMale, .offset = {0.0f, -6.0f}},
	                {.type = VillagerInfo::CelticHousewifeFemale, .offset = {4.0f, 4.0f}},
	                {.type = VillagerInfo::CelticFishermanMale, .offset = {-4.0f, -4.0f}}},
	    .commands = {{.kind = Kind::MakeDance,
	                  .delaySeconds = 1.0f,
	                  .value = static_cast<size_t>(DanceInfo::DanceAroundPerson),
	                  .object = 0,
	                  .ctrl = true}},
	});

	all.push_back({
	    .id = "script.set_target_throw",
	    .name = "A script throws a villager to get to a place in a time",
	    .facet = Facet::Story,
	    .description = "A villager stands in the middle. After one second, SET_TARGET throws the villager to a point "
	                   "20 metres east and 10 metres north, 5 metres above the land, in 3 seconds. The magic mushroom "
	                   "challenge of Land 1 throws its actor at the camera in this way.",
	    .expected = "The villager tumbles up in an arc with no air drag and gets to the point after 3 seconds. Then it "
	                "falls to the land. The log tells that SET_TARGET ran and gave back no values.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-10.0f, -10.0f}, {30.0f, 20.0f}}},
	    .objects = {{.type = VillagerInfo::CelticHousewifeFemale, .offset = {0.0f, 0.0f}}},
	    .commands = {{.kind = Kind::CallNative,
	                  .delaySeconds = 1.0f,
	                  .script = "SET_TARGET",
	                  .scriptArguments = {{.kind = Argument::Kind::Object, .index = 0},
	                                      {.kind = Argument::Kind::Position, .point = {20.0f, 10.0f}, .height = 5.0f},
	                                      {.kind = Argument::Kind::Number, .number = 3.0f}}}},
	});

	// A script opens a window of the screen and closes it again, as the end of the fifth land does
	all.push_back({
	    .id = "script.clipping_window",
	    .name = "A script opens a window of the screen and closes it again",
	    .facet = Facet::Story,
	    .description = "After one second, SET_CLIPPING_WINDOW opens a window 40% wide and 60% high, 10% from the left "
	                   "and 20% from the top, in half a second. Four seconds later, CLEAR_CLIPPING_WINDOW closes it in "
	                   "half a second. The waving villagers at the end of the fifth land are shown in this window.",
	    .expected = "Four black boxes fade in around the window in half a second and cover all of the screen out of it. "
	                "The world shows in the window. After four seconds the boxes fade out in half a second.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-20.0f, -20.0f}, {20.0f, 20.0f}}},
	    .commands = {{.kind = Kind::CallNative,
	                  .delaySeconds = 1.0f,
	                  .script = "SET_CLIPPING_WINDOW",
	                  .scriptArguments = {{.kind = Argument::Kind::Number, .number = 0.1f},
	                                      {.kind = Argument::Kind::Number, .number = 0.2f},
	                                      {.kind = Argument::Kind::Number, .number = 0.4f},
	                                      {.kind = Argument::Kind::Number, .number = 0.6f},
	                                      {.kind = Argument::Kind::Number, .number = 0.5f}}},
	                 {.kind = Kind::CallNative,
	                  .delaySeconds = 4.0f,
	                  .script = "CLEAR_CLIPPING_WINDOW",
	                  .scriptArguments = {{.kind = Argument::Kind::Number, .number = 0.5f}}}},
	});

	// A script gives a clip to a flock of villagers, as Idol Pyre does to its extra worshippers
	all.push_back({
	    .id = "script.flock_plays_clip",
	    .name = "A script gives a flock of villagers a clip to play",
	    .facet = Facet::Story,
	    .description = "Three villagers stand in a row. After one second, a script makes a flock of them. SET_SCRIPT_ULONG "
	                   "gives the flock the prayer clip once, and SET_SCRIPT_STATE tells the flock to play it. Idol Pyre "
	                   "on Land 2 gives its extra worshippers the clip in this way.",
	    .expected = "Each villager of the flock plays the prayer clip one time. The script then controls each villager. "
	                "The log tells that the two natives ran and gave back no values.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 4.0f, -12.0f}, .look = {0.0f, 1.0f, 0.0f}},
	    .objects = {{.type = VillagerInfo::CelticHousewifeFemale, .offset = {-4.0f, 0.0f}},
	                {.type = VillagerInfo::CelticFarmerMale, .offset = {0.0f, 0.0f}},
	                {.type = VillagerInfo::CelticHousewifeFemale, .offset = {4.0f, 0.0f}}},
	    .commands = {{.kind = Kind::CallNative,
	                  .delaySeconds = 1.0f,
	                  .script = "SET_SCRIPT_ULONG",
	                  .scriptArguments = {{.kind = Argument::Kind::Flock},
	                                      {.kind = Argument::Kind::Integer, .number = 343.0f},
	                                      {.kind = Argument::Kind::Integer, .number = 1.0f}}},
	                 {.kind = Kind::CallNative,
	                  .delaySeconds = 1.0f,
	                  .script = "SET_SCRIPT_STATE",
	                  .scriptArguments = {{.kind = Argument::Kind::Flock},
	                                      {.kind = Argument::Kind::Integer, .number = 200.0f}}}},
	});

	// A script's camera follows a flock of villagers and looks at it, as at the place of its leader
	all.push_back({
	    .id = "script.camera_follow_flock",
	    .name = "A script's camera follows a flock of villagers",
	    .facet = Facet::Story,
	    .description = "Three villagers stand in a row. After one second, a script makes a flock of them and takes the "
	                   "camera. SET_POSITION_FOLLOW and SET_FOCUS_FOLLOW give the camera the flock. After three seconds, "
	                   "MOVE_GAME_THING sends each villager 30 to the north.",
	    .expected = "The camera goes to the leader of the flock at once and looks down at the middle of its body from "
	                "about 2 away. It then follows the leader as the villagers walk. The log tells that the natives ran.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 12.0f, -24.0f}, .look = {0.0f, 1.0f, 0.0f}},
	    .objects = {{.type = VillagerInfo::CelticHousewifeFemale, .offset = {-4.0f, 0.0f}},
	                {.type = VillagerInfo::CelticFarmerMale, .offset = {0.0f, 0.0f}},
	                {.type = VillagerInfo::CelticHousewifeFemale, .offset = {4.0f, 0.0f}}},
	    .commands = {{.kind = Kind::CallNative, .delaySeconds = 1.0f, .script = "START_CAMERA_CONTROL"},
	                 {.kind = Kind::CallNative,
	                  .delaySeconds = 0.0f,
	                  .script = "SET_POSITION_FOLLOW",
	                  .scriptArguments = {{.kind = Argument::Kind::Flock}}},
	                 {.kind = Kind::CallNative,
	                  .delaySeconds = 0.0f,
	                  .script = "SET_FOCUS_FOLLOW",
	                  .scriptArguments = {{.kind = Argument::Kind::Flock}}},
	                 {.kind = Kind::CallNative,
	                  .delaySeconds = 3.0f,
	                  .script = "MOVE_GAME_THING",
	                  .scriptArguments = {{.kind = Argument::Kind::Object, .index = 0, .kept = true},
	                                      {.kind = Argument::Kind::Position, .point = {-4.0f, 30.0f}},
	                                      {.kind = Argument::Kind::Number, .number = 0.0f}}},
	                 {.kind = Kind::CallNative,
	                  .delaySeconds = 0.0f,
	                  .script = "MOVE_GAME_THING",
	                  .scriptArguments = {{.kind = Argument::Kind::Object, .index = 1, .kept = true},
	                                      {.kind = Argument::Kind::Position, .point = {0.0f, 30.0f}},
	                                      {.kind = Argument::Kind::Number, .number = 0.0f}}},
	                 {.kind = Kind::CallNative,
	                  .delaySeconds = 0.0f,
	                  .script = "MOVE_GAME_THING",
	                  .scriptArguments = {{.kind = Argument::Kind::Object, .index = 2, .kept = true},
	                                      {.kind = Argument::Kind::Position, .point = {4.0f, 30.0f}},
	                                      {.kind = Argument::Kind::Number, .number = 0.0f}}}},
	});

	// The two-thing camera of a script keeps a flock of villagers and a tree in view. It reads the flock at the own place
	// of the flock, not at its leader.
	all.push_back({
	    .id = "script.camera_dual_flock",
	    .name = "The two-thing camera of a script on a flock and a tree",
	    .facet = Facet::Story,
	    .description = "Three villagers stand in a row, and an oak stands 30 to the north. After one second, a script "
	                   "makes a flock of the villagers at the first villager and takes the camera. START_DUAL_CAMERA "
	                   "gives the camera the flock and the oak. After four seconds, MOVE_GAME_THING sends each villager "
	                   "20 to the east.",
	    .expected = "The camera glides in two seconds to the middle between the place of the flock and the oak. It looks "
	                "from the side of the line between them, from about 83 away. The flock counts as 30 wide and has no "
	                "height. The camera does not move when the villagers walk to the east, because the place of the flock "
	                "does not change.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 12.0f, -24.0f}, .look = {0.0f, 1.0f, 0.0f}},
	    .objects = {{.type = VillagerInfo::CelticHousewifeFemale, .offset = {-4.0f, 0.0f}},
	                {.type = VillagerInfo::CelticFarmerMale, .offset = {0.0f, 0.0f}},
	                {.type = VillagerInfo::CelticHousewifeFemale, .offset = {4.0f, 0.0f}},
	                {.type = TreeInfo::Oak, .offset = {0.0f, 30.0f}}},
	    .commands = {{.kind = Kind::CallNative, .delaySeconds = 1.0f, .script = "START_CAMERA_CONTROL"},
	                 {.kind = Kind::CallNative,
	                  .delaySeconds = 0.0f,
	                  .script = "START_DUAL_CAMERA",
	                  .scriptArguments = {{.kind = Argument::Kind::Flock}, {.kind = Argument::Kind::Object, .index = 3}}},
	                 {.kind = Kind::CallNative,
	                  .delaySeconds = 4.0f,
	                  .script = "MOVE_GAME_THING",
	                  .scriptArguments = {{.kind = Argument::Kind::Object, .index = 0, .kept = true},
	                                      {.kind = Argument::Kind::Position, .point = {16.0f, 0.0f}},
	                                      {.kind = Argument::Kind::Number, .number = 0.0f}}},
	                 {.kind = Kind::CallNative,
	                  .delaySeconds = 0.0f,
	                  .script = "MOVE_GAME_THING",
	                  .scriptArguments = {{.kind = Argument::Kind::Object, .index = 1, .kept = true},
	                                      {.kind = Argument::Kind::Position, .point = {20.0f, 0.0f}},
	                                      {.kind = Argument::Kind::Number, .number = 0.0f}}},
	                 {.kind = Kind::CallNative,
	                  .delaySeconds = 0.0f,
	                  .script = "MOVE_GAME_THING",
	                  .scriptArguments = {{.kind = Argument::Kind::Object, .index = 2, .kept = true},
	                                      {.kind = Argument::Kind::Position, .point = {24.0f, 0.0f}},
	                                      {.kind = Argument::Kind::Number, .number = 0.0f}}}},
	});

	// The leans of the three dead trees of Land 1, in degrees about x and z, and one tree with a large lean
	all.push_back({
	    .id = "nature.dead_tree_lean",
	    .name = "Dead trees from a land script lean about x and z",
	    .facet = Facet::Nature,
	    .description = "Four dead trees in a row from the west. The first three have the kinds and the leans of the three "
	                   "dead trees on Land 1. The last tree leans 20 degrees about x and 10 degrees about z, so that the "
	                   "direction of the lean is clear.",
	    .expected = "The first three trees lean by a few degrees, each in its own direction. The last tree leans "
	                "clearly. The turn about the vertical comes first, then the lean about x, then the lean about z.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-25.0f, -8.0f}, {25.0f, 8.0f}}},
	    .objects = {{.type = TreeInfo::Pine, .offset = {-18.0f, 0.0f}, .dead = true, .leanDegrees = {-0.4544f, -3.1648f}},
	                {.type = TreeInfo::ConiferA, .offset = {-6.0f, 0.0f}, .dead = true, .leanDegrees = {2.2700f, 0.0694f}},
	                {.type = TreeInfo::ConiferA, .offset = {6.0f, 0.0f}, .dead = true, .leanDegrees = {-0.4841f, 1.8931f}},
	                {.type = TreeInfo::ConiferA, .offset = {18.0f, 0.0f}, .dead = true, .leanDegrees = {20.0f, 10.0f}}},
	});

	// Bonfires from a land script: the turn is the third number, and the second number is not used
	all.push_back({
	    .id = "nature.bonfire_land_script_turn",
	    .name = "Bonfires from a land script take their turn from the third number",
	    .facet = Facet::Nature,
	    .description = "Two bonfires in a row from the west, made by land script lines. The first has the numbers of the "
	                   "two bonfires on Land 1: 24.7, then 0. The second has 0, then a quarter turn.",
	    .expected = "The first bonfire stands with no turn, as on Land 1, because the second number is not used. The "
	                "second bonfire is turned by a quarter turn about the vertical.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-12.0f, -6.0f}, {12.0f, 6.0f}}},
	    .landScript = "CREATE_BONFIRE(\"-6.0,0.0\", 24.700001, 0.000000, 1.000000)\n"
	                  "CREATE_BONFIRE(\"6.0,0.0\", 0.000000, 1.570796, 1.000000)\n",
	});

	// Mobile statics that a land script turns and leans: the lean follows the way each one faces
	all.push_back({
	    .id = "nature.mobile_static_lean",
	    .name = "Mobile statics from a land script lean and turn in the game's order",
	    .facet = Facet::Nature,
	    .description = "Four mobile statics in a row from the west, each turned 45 degrees about the vertical. The first "
	                   "fence leans 30 degrees about x. The second fence leans 30 degrees about z. The long rock and the "
	                   "totem lean about x and z.",
	    .expected = "Each thing leans in the direction that it faces after its turn, not along the axes of the map. The "
	                "first fence tips forward along its facing. The second fence tips to its side.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-25.0f, -8.0f}, {25.0f, 8.0f}}},
	    .objects = {{.type = MobileStaticInfo::CeltFenceTall,
	                 .offset = {-18.0f, 0.0f},
	                 .yawDegrees = 45.0f,
	                 .leanDegrees = {30.0f, 0.0f}},
	                {.type = MobileStaticInfo::CeltFenceTall,
	                 .offset = {-6.0f, 0.0f},
	                 .yawDegrees = 45.0f,
	                 .leanDegrees = {0.0f, 30.0f}},
	                {.type = MobileStaticInfo::LongrockSandstone,
	                 .offset = {6.0f, 0.0f},
	                 .yawDegrees = 45.0f,
	                 .leanDegrees = {20.0f, 20.0f}},
	                {.type = MobileStaticInfo::GateTotemApe,
	                 .offset = {18.0f, 0.0f},
	                 .yawDegrees = 45.0f,
	                 .leanDegrees = {25.0f, -15.0f}}},
	});

	// Two rows of small pines along the path of a low throw. The camera stands as for the hand's throw.
	std::vector<ObjectSetup> bendTrees {{.type = MobileStaticInfo::Rock, .offset = {0.0f, 0.0f}}};
	for (int row = 0; row < 10; ++row)
	{
		const float along = 6.0f + static_cast<float>(row) * 3.0f;
		bendTrees.push_back({.type = TreeInfo::Pine, .offset = {along, -2.0f}, .scale = 0.5f});
		bendTrees.push_back({.type = TreeInfo::Pine, .offset = {along, 2.0f}, .scale = 0.5f});
	}
	all.push_back({
	    .id = "nature.trees_bend_round_a_thrown_rock",
	    .name = "Trees bend away from a rock that flies between them",
	    .facet = Facet::Nature,
	    .description = "The hand takes a rock and throws it low between two rows of small pines. Then the rock comes to "
	                   "rest.",
	    .expected = "While the rock flies, the pines near it lean away from it, more when the rock is near. A pine that "
	                "the rock passes stands straight again when the rock is out of its reach. When the rock comes to "
	                "rest among the pines, the pines in its reach stay bent away from it.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {-34.5f, 9.0f, 4.5f}, .look = {0.0f, 0.0f, 0.0f}},
	    .objects = bendTrees,
	    .commands =
	        {// Takes the rock under the middle of the screen
	         {.kind = Kind::PointerTo, .delaySeconds = 1.0f, .point = {0.5f, 0.5f}},
	         {.kind = Kind::PointerPress, .delaySeconds = 0.5f, .value = 3},
	         {.kind = Kind::PointerRelease, .delaySeconds = 0.5f, .value = 3},
	         // Makes ready low on the screen and sweeps a little up, so that the throw stays low
	         {.kind = Kind::PointerTo, .delaySeconds = 0.5f, .point = {0.45f, 0.7f}},
	         {.kind = Kind::PointerPress, .delaySeconds = 0.5f, .value = 3},
	         {.kind = Kind::PointerSweep, .delaySeconds = 0.3f, .point = {0.03f, -0.15f}, .amount = 0.3f},
	         {.kind = Kind::PointerRelease, .delaySeconds = 0.5f, .value = 3}},
	});

	// Two rows of pines with a lane between them. The player's creature walks along the lane and back.
	std::vector<ObjectSetup> lanePines;
	for (int row = 0; row < 10; ++row)
	{
		const float along = static_cast<float>(row) * 3.0f;
		lanePines.push_back({.type = TreeInfo::Pine, .offset = {along, -4.5f}});
		lanePines.push_back({.type = TreeInfo::Pine, .offset = {along, 4.5f}});
	}
	all.push_back({
	    .id = "nature.trees_bend_round_the_creature",
	    .name = "Trees bend away from the player's creature",
	    .facet = Facet::Nature,
	    .description = "The player's tiger walks along a lane between two rows of pines, then walks back.",
	    .expected = "The pines near the tiger lean away from it, more when the tiger is near, and crash about. When the "
	                "tiger has gone by, they stand straight again.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {{-12.0f, -12.0f}, {40.0f, 12.0f}}},
	    .creatures = {CreatureSetup {
	        .label = "yours", .species = CreatureType::Tiger, .offset = {-10.0f, 0.0f}, .owner = PlayerNames::PLAYER_ONE}},
	    .objects = lanePines,
	    .commands = {{.kind = Kind::WalkTo, .creature = 0, .delaySeconds = 1.0f, .point = {40.0f, 0.0f}},
	                 {.kind = Kind::WalkTo, .creature = 0, .delaySeconds = 8.0f, .point = {-10.0f, 0.0f}}},
	});
}

void AddVillagerSizeAndReachScenarios(std::vector<Scenario>& all)
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
void AddSwayAndFrightScenarios(std::vector<Scenario>& all)
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

void AddStoreScenarios(std::vector<Scenario>& all)
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

void AddPoisonScenarios(std::vector<Scenario>& all)
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

void AddWorkshopScenarios(std::vector<Scenario>& all)
{
	constexpr glm::vec2 k_Workshop {-30.0f, 30.0f};
	all.push_back({
	    .id = "objects.workshop_makes_scaffolds",
	    .name = "A workshop makes scaffolds from its wood",
	    .facet = Facet::Objects,
	    .description = "A Celtic town of the player's with a village centre and a workshop that holds 10000 wood, the wood of "
	                   "four scaffolds. The yard of the workshop faces the camera.",
	    .expected = "A pile of logs stands at the back of the workshop and holds its wood. The workshop uses the wood of "
	                "one scaffold from the pile, and the pile becomes smaller. The work sound starts. When the time to make a "
	                "scaffold is over, a small scaffold appears in the first free place of the yard, a horn sounds and "
	                "the work sound stops. The workshop makes scaffolds until it has three in its yard. The hand can "
	                "take a scaffold from the yard, and the workshop then makes one more.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_Workshop - glm::vec2 {15.0f}, k_Workshop + glm::vec2 {15.0f}}},
	    .objects = {{.type = AbodeInfo::CelticTownCentre, .offset = {0.0f, 50.0f}},
	                {.type = AbodeInfo::CelticWorkshop, .offset = k_Workshop, .yawDegrees = 180.0f, .storedWood = 10000}},
	});

	all.push_back({
	    .id = "objects.workshop_graveyard_burial_scaffold",
	    .name = "A burial on a workshop that stands in for the graveyard makes a scaffold with no wood",
	    .facet = Facet::Objects,
	    .description = "A Celtic town of the player's with a village centre, a workshop with no wood, its yard towards the "
	                   "camera, and a graveyard. After one second the graveyard goes, and the workshop becomes the "
	                   "graveyard of the town. After two seconds a villager of the town with little life catches fire.",
	    .expected = "The villager burns and dies. The workshop counts the burial: its scaffold shows over the workshop "
	                "from then on. On the next turn of the workshop a small scaffold appears in the first place of the "
	                "yard and a horn sounds, although the workshop has no wood. The work sound does not play. The "
	                "inspector shows the stand-in graves count 1 on the workshop.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_Workshop - glm::vec2 {15.0f}, k_Workshop + glm::vec2 {15.0f}}},
	    .objects = {{.type = AbodeInfo::CelticTownCentre, .offset = {0.0f, 50.0f}},
	                {.type = AbodeInfo::CelticWorkshop, .offset = k_Workshop, .yawDegrees = 180.0f, .storedWood = 0},
	                {.type = AbodeInfo::CelticGraveyard, .offset = {30.0f, 30.0f}},
	                {.type = VillagerInfo::CelticFarmerMale,
	                 .offset = k_Workshop + glm::vec2 {12.0f, -12.0f},
	                 .joinTown = true,
	                 .life = 0.05f}},
	    .commands = {{.kind = Command::Kind::RemoveObject, .delaySeconds = 1.0f, .object = 2},
	                 {.kind = Command::Kind::SetAlight, .delaySeconds = 1.0f, .value = 1, .object = 3}},
	    .townBeliefInPlayer = 1.0f,
	});

	// The scenario turns the workshop so that its yard faces the camera. The first place of the Celtic workshop's yard is
	// 7.8 m in front of the workshop.
	constexpr glm::vec2 k_FirstPlace = k_Workshop + glm::vec2 {0.0f, -7.8f};
	all.push_back({
	    .id = "objects.scaffold_snaps_into_yard",
	    .name = "A scaffold put down near a workshop's yard goes into its place",
	    .facet = Facet::Objects,
	    .description = "A Celtic town of the player's with a village centre and a workshop with no wood, its yard towards the "
	                   "camera, and a loose "
	                   "scaffold that no workshop counts. After one second the scaffold is lifted and let go one metre "
	                   "from the first place of the yard, as the hand puts one down.",
	    .expected = "The scaffold comes to rest and moves into the first place of the yard, turned as the place is "
	                "turned. No puff shows. The workshop now counts the scaffold as its own (the inspector shows it), "
	                "and the physics does not move the scaffold while it stands there.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_Workshop - glm::vec2 {12.0f}, k_Workshop + glm::vec2 {12.0f}}},
	    .objects = {{.type = AbodeInfo::CelticTownCentre, .offset = {0.0f, 50.0f}},
	                {.type = AbodeInfo::CelticWorkshop, .offset = k_Workshop, .yawDegrees = 180.0f, .storedWood = 0},
	                {.type = ScaffoldInfo::Normal, .offset = k_Workshop + glm::vec2 {10.0f, 10.0f}, .scale = 0.7f}},
	    .throws = {{.object = 2,
	                .from = k_FirstPlace + glm::vec2 {0.7f, -0.7f},
	                .height = 1.0f,
	                .velocity = {0.0f, -1.0f, 0.0f},
	                .delaySeconds = 1.0f}},
	});

	all.push_back({
	    .id = "objects.scaffold_tap_breaks",
	    .name = "A tap breaks one scaffold off a scaffold, and a storage pit takes a scaffold as wood",
	    .facet = Facet::Objects,
	    .description = "A Celtic town of the player's with a village centre and a storage pit, and a loose scaffold that "
	                   "is worth three scaffolds and that no workshop counts. Put the hand over the scaffold, tap it, "
	                   "then pick up a part and hold it over the other part and over the storage pit.",
	    .expected = "Over the scaffold the tooltip shows \"Scaffold Value\" with 3, and \"Tap\". A tap breaks one small "
	                "scaffold off. The two parts move apart along a quarter turn of the scaffold and one of four sounds "
	                "plays. The old scaffold is now worth 2. A held part over the other part shows \"Combine "
	                "Scaffolds\". Over the storage pit it shows \"Supply\", and the pit takes the wood of all the "
	                "scaffolds that the part is worth. Elsewhere the tooltip shows the name of the building that the "
	                "phantom shows, or \"Build\", then \"Throw\".",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_Workshop - glm::vec2 {15.0f}, k_Workshop + glm::vec2 {15.0f}}},
	    .objects = {{.type = AbodeInfo::CelticTownCentre, .offset = {0.0f, 50.0f}},
	                {.type = AbodeInfo::CelticStoragePit, .offset = k_Workshop + glm::vec2 {-15.0f, 0.0f}},
	                {.type = ScaffoldInfo::Normal, .offset = k_Workshop, .scale = 1.1f}},
	});

	all.push_back({
	    .id = "objects.workshop_wood_by_hand",
	    .name = "The hand takes wood from a workshop's pile and gives it back",
	    .facet = Facet::Objects,
	    .description = "A Celtic town of the player's with a village centre and a workshop that holds 1000 wood, less "
	                   "than the wood of one scaffold, so the workshop makes no scaffold. The pile of logs of the "
	                   "workshop faces the camera. The input tools/sim/hand_inputs/workshop_wood.json moves the hand.",
	    .expected = "With a held press on the pile, the hand takes logs from it, and the pile becomes smaller. The "
	                "tooltip of the workshop shows less wood. When the hand lets the logs go on the workshop, the "
	                "workshop takes them into its pile again, and the tooltip shows the wood that it had at the start.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_Workshop - glm::vec2 {12.0f}, k_Workshop + glm::vec2 {12.0f}}},
	    .objects = {{.type = AbodeInfo::CelticTownCentre, .offset = {0.0f, 50.0f}},
	                {.type = AbodeInfo::CelticWorkshop, .offset = k_Workshop, .yawDegrees = 90.0f, .storedWood = 1000}},
	});

	all.push_back({
	    .id = "objects.workshop_takes_thrown_wood",
	    .name = "A workshop takes a handful of wood that the hand throws at it",
	    .facet = Facet::Objects,
	    .description = "A Celtic town of the player's with a village centre and a workshop that holds 1000 wood. A "
	                   "handful of 600 wood is lifted 20 m west of the workshop and thrown at it as the hand throws it.",
	    .expected = "The handful flies east and comes down at the workshop. It goes, and the workshop takes the 600 wood "
	                "into its pile: the workshop then holds 1600 wood, still less than the wood of one scaffold.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_Workshop - glm::vec2 {25.0f}, k_Workshop + glm::vec2 {15.0f}}},
	    .objects = {{.type = AbodeInfo::CelticTownCentre, .offset = {0.0f, 50.0f}},
	                {.type = AbodeInfo::CelticWorkshop, .offset = k_Workshop, .yawDegrees = 90.0f, .storedWood = 1000},
	                {.type = PotInfo::HandWood, .offset = k_Workshop + glm::vec2 {-20.0f, 0.0f}, .amount = 600}},
	    .throws = {{.object = 2,
	                .from = k_Workshop + glm::vec2 {-20.0f, 0.0f},
	                .height = 6.0f,
	                .velocity = {11.0f, 3.0f, 0.0f},
	                .delaySeconds = 2.0f}},
	});

	all.push_back({
	    .id = "objects.workshop_wood_flag",
	    .name = "The flag of a workshop shows the wood that the workshop needs for a scaffold",
	    .facet = Facet::Objects,
	    .description = "The town and the workshop of objects.workshop_wood_by_hand: the workshop holds 1000 wood, less "
	                   "than the wood of one scaffold. The input tools/sim/hand_inputs/workshop_wood.json moves the hand: "
	                   "it takes logs from the pile of the workshop, then gives them back.",
	    .expected = "The wood flag flies beside the workshop and turns to face the camera. When the hand takes logs, the "
	                "flag moves up smoothly, because the workshop needs more wood. When the hand gives the logs back, the "
	                "flag moves down to its first height.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_Workshop - glm::vec2 {12.0f}, k_Workshop + glm::vec2 {12.0f}}},
	    .objects = {{.type = AbodeInfo::CelticTownCentre, .offset = {0.0f, 50.0f}},
	                {.type = AbodeInfo::CelticWorkshop, .offset = k_Workshop, .yawDegrees = 90.0f, .storedWood = 1000}},
	});

	all.push_back({
	    .id = "hand.tooltips_workshop_wood_flag",
	    .name = "The hand over the wood flag of a workshop",
	    .facet = Facet::Objects,
	    .description = "The town and the workshop of objects.workshop_wood_by_hand: the workshop holds 1000 wood, less "
	                   "than the wood of one scaffold, and has room for three scaffolds. Put the hand over the wood flag "
	                   "beside the workshop, also out of the influence of the player.",
	    .expected = "Over the flag the tooltip shows \"Wood Required\" with the wood of three scaffolds less 1000, at "
	                "once and with no mouse button drawn. It shows also where the hand is out of the influence.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_Workshop - glm::vec2 {12.0f}, k_Workshop + glm::vec2 {12.0f}}},
	    .objects = {{.type = AbodeInfo::CelticTownCentre, .offset = {0.0f, 50.0f}},
	                {.type = AbodeInfo::CelticWorkshop, .offset = k_Workshop, .yawDegrees = 90.0f, .storedWood = 1000}},
	});

	all.push_back({
	    .id = "hand.tooltips_workshop_production",
	    .name = "The hand over a workshop that makes a scaffold",
	    .facet = Facet::Objects,
	    .description = "A Celtic town of the player's with a village centre and a workshop that holds 10000 wood, the "
	                   "wood of four scaffolds. The hand is over the workshop, in the influence of the player.",
	    .expected = "The workshop starts a scaffold, and its chimney smokes. Over the workshop the tooltip shows "
	                "\"Production Completed\" with the part of the scaffold that the workshop made, as a whole percent "
	                "that goes up from 0 to 99. When the scaffold is finished, the percent starts again at 0 for the next "
	                "scaffold. When the yard holds three scaffolds, the workshop stops, the tooltip shows 0, and the "
	                "smoke stops.",
	    .environment = {.dispenserGrid = false, .cursor = glm::vec2 {0.5f, 0.5f}},
	    .framing = {.shot = Shot::Placed,
	                .eye = {k_Workshop.x, 22.0f, k_Workshop.y - 32.0f},
	                .look = {k_Workshop.x, 3.0f, k_Workshop.y}},
	    .objects = {{.type = AbodeInfo::CelticTownCentre, .offset = {0.0f, 50.0f}},
	                {.type = AbodeInfo::CelticWorkshop, .offset = k_Workshop, .yawDegrees = 180.0f, .storedWood = 10000}},
	});

	// The place where the hand puts the villager down, in front of the workshop and beside its yard
	constexpr glm::vec2 k_DropPlace = k_Workshop + glm::vec2 {6.0f, -6.0f};
	all.push_back({
	    .id = "villagers.craftsman_supplies_workshop",
	    .name = "Craftsmen bring the wood of the storage pit to the workshop",
	    .facet = Facet::Objects,
	    .description = "A Celtic town of the player's (which believes in the player from the start) with a village centre, "
	                   "a storage pit that holds 3000 wood 25 m to the west, and a built workshop with no wood. A "
	                   "craftsman disciple stands by the workshop. A grown villager of the town, not a disciple, stands "
	                   "to the east. After one second the hand takes that villager, and after three more seconds it puts "
	                   "the villager down by the workshop. Best watched at game speed 4.",
	    .expected = "The villager that the hand puts down by the workshop becomes a craftsman disciple: it glows and the "
	                "craftsman's line plays. Each craftsman walks to the storage pit, takes as much wood as it can carry, "
	                "walks to the workshop and puts the wood in. When the workshop has the wood of a scaffold, it makes a "
	                "scaffold. The town's desire for wood (the inspector's living.town_desires, desire 1) adds the share of "
	                "the adults that are craftsmen.",
	    .environment = {.dispenserGrid = false, .cursor = glm::vec2 {0.5f, 0.5f}},
	    .framing = {.shot = Shot::Placed,
	                .eye = {k_DropPlace.x - 10.0f, 30.0f, k_DropPlace.y - 40.0f},
	                .look = {k_DropPlace.x, 0.0f, k_DropPlace.y}},
	    .objects = {{.type = AbodeInfo::CelticTownCentre, .offset = {0.0f, 50.0f}},
	                {.type = AbodeInfo::CelticStoragePit, .offset = k_Workshop + glm::vec2 {-25.0f, 0.0f}, .storedWood = 3000},
	                {.type = AbodeInfo::CelticWorkshop, .offset = k_Workshop, .yawDegrees = 180.0f, .storedWood = 0},
	                {.type = VillagerInfo::CelticFarmerMale,
	                 .offset = k_Workshop + glm::vec2 {-6.0f, -8.0f},
	                 .joinTown = true,
	                 .disciple = VillagerDisciple::Craftsman},
	                {.type = VillagerInfo::CelticFarmerMale,
	                 .offset = k_Workshop + glm::vec2 {20.0f, -8.0f},
	                 .joinTown = true}},
	    .commands = {{.kind = Kind::HandTake, .delaySeconds = 1.0f, .object = 4},
	                 {.kind = Kind::HandRelease, .delaySeconds = 3.0f}},
	    // The town belongs to the player at the start. Thus the hand of the player holds a villager of its own.
	    .townBeliefInPlayer = 1000.0f,
	});

	constexpr glm::vec2 k_LooseScaffold {-25.0f, 30.0f};
	constexpr glm::vec2 k_BuildPlace {-5.0f, 25.0f};
	constexpr glm::vec2 k_FoundingScaffold {-60.0f, 80.0f};
	all.push_back({
	    .id = "objects.scaffold_starts_building",
	    .name = "A scaffold put down near a town starts the building that it offers",
	    .facet = Facet::Objects,
	    .description = "A Celtic town of the player's with a village centre and a loose scaffold of the value of one "
	                   "scaffold. The input tools/sim/hand_inputs/scaffold_build.json moves the hand: it takes the "
	                   "scaffold, holds it still near the village centre, and puts it down.",
	    .expected = "While the hand holds the scaffold still near the village centre, the phantom of the building that "
	                "the town wants grows below the scaffold and turns slowly. When the hand puts the scaffold down, no "
	                "puff shows, a sound plays, and a building site of that building opens where the scaffold is. The "
	                "scaffold stays on the site. While nothing of the building is built, the hand can take the scaffold "
	                "again, and the site then goes.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_LooseScaffold - glm::vec2 {10.0f}, k_BuildPlace + glm::vec2 {10.0f}}},
	    .objects = {{.type = AbodeInfo::CelticTownCentre, .offset = {0.0f, 50.0f}},
	                {.type = ScaffoldInfo::Normal, .offset = k_LooseScaffold, .scale = 0.7f}},
	});

	constexpr glm::vec2 k_JoinTarget {-25.0f, 18.0f};
	all.push_back({
	    .id = "hand.scaffold_phantom_sounds_and_join",
	    .name = "A held scaffold plays the sounds of its phantom, names the building, and joins a scaffold",
	    .facet = Facet::Hand,
	    .description = "A Celtic town of the player's with a village centre, a loose scaffold of the value of two "
	                   "scaffolds and, 12 m south of it, a loose scaffold of the value of one. Take the larger "
	                   "scaffold and hold it still near the village centre. Then hold it over the smaller scaffold and "
	                   "press the Action button. Run with the audio log at the debug level to see the sounds.",
	    .expected = "Near the village centre the phantom of the building that the town wants grows below the scaffold, "
	                "and the appear sound plays one time. The tooltip shows the name of that building, then \"Throw\". "
	                "When the offer changes or goes, the phantom becomes small and the vanish sound plays one time. Over "
	                "the smaller scaffold the tooltip shows \"Combine Scaffolds\". The press removes the held scaffold. "
	                "The scaffold on the land now has the value of three. A puff shows at it, and the first of the four "
	                "join sounds plays at the hand.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_JoinTarget - glm::vec2 {10.0f}, k_BuildPlace + glm::vec2 {10.0f}}},
	    .objects = {{.type = AbodeInfo::CelticTownCentre, .offset = {0.0f, 50.0f}},
	                {.type = ScaffoldInfo::Normal, .offset = k_LooseScaffold, .scale = 0.9f},
	                {.type = ScaffoldInfo::Normal, .offset = k_JoinTarget, .scale = 0.7f}},
	    .townBeliefInPlayer = 1.0f,
	});

	all.push_back({
	    .id = "light.snow_on_scaffold_phantom",
	    .name = "The phantom of a scaffold carries snow in the hand and none on its site",
	    .facet = Facet::Light,
	    .description = "The town, the scaffold and the camera of objects.scaffold_starts_building, in deep snow over the "
	                   "whole island. The input tools/sim/hand_inputs/scaffold_build.json moves the hand: it takes the "
	                   "scaffold, holds it still near the village centre, and puts it down.",
	    .expected = "While the hand holds the scaffold still, the phantom of the building grows below the scaffold, and "
	                "snow comes onto it in about four seconds. When the hand puts the scaffold down, the phantom on the "
	                "building site shows no snow, and it never shows snow again. The scaffold itself shows snow as "
	                "other things do.",
	    .environment = {.dispenserGrid = false, .snowDepth = 200.0f},
	    .framing = {.shot = Shot::Overview, .include = {k_LooseScaffold - glm::vec2 {10.0f}, k_BuildPlace + glm::vec2 {10.0f}}},
	    .objects = {{.type = AbodeInfo::CelticTownCentre, .offset = {0.0f, 50.0f}},
	                {.type = ScaffoldInfo::Normal, .offset = k_LooseScaffold, .scale = 0.7f}},
	});

	constexpr glm::vec2 k_ImpressedHut {-25.0f, 47.0f};
	all.push_back({
	    .id = "objects.scaffold_impresses_town",
	    .name = "A scaffold that the player puts down impresses the people of the near town",
	    .facet = Facet::Objects,
	    .description = "The town, the scaffold and the camera of objects.scaffold_starts_building, with a hut of four "
	                   "villagers of the town beside the village centre. The input tools/sim/hand_inputs/scaffold_build.json "
	                   "moves the hand: it takes the scaffold, holds it still near the village centre, and puts it down.",
	    .expected = "When the hand puts the scaffold down, a building site opens. At the same time, symbols of belief in "
	                "the player rise from the first villagers of the hut: from a part of the town, and from one villager "
	                "at least. The belief of the town in the player becomes larger.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_LooseScaffold - glm::vec2 {10.0f}, k_BuildPlace + glm::vec2 {10.0f}}},
	    .objects = {{.type = AbodeInfo::CelticTownCentre, .offset = {0.0f, 50.0f}},
	                {.type = ScaffoldInfo::Normal, .offset = k_LooseScaffold, .scale = 0.7f},
	                {.type = AbodeInfo::CelticHut, .offset = k_ImpressedHut},
	                {.type = VillagerInfo::CelticFarmerMale,
	                 .offset = k_ImpressedHut + glm::vec2 {-6.0f, -6.0f},
	                 .joinTown = true,
	                 .homeIn = 2},
	                {.type = VillagerInfo::CelticHousewifeFemale,
	                 .offset = k_ImpressedHut + glm::vec2 {-3.0f, -7.0f},
	                 .joinTown = true,
	                 .homeIn = 2},
	                {.type = VillagerInfo::CelticForesterMale,
	                 .offset = k_ImpressedHut + glm::vec2 {3.0f, -7.0f},
	                 .joinTown = true,
	                 .homeIn = 2},
	                {.type = VillagerInfo::CelticHousewifeFemale,
	                 .offset = k_ImpressedHut + glm::vec2 {6.0f, -6.0f},
	                 .joinTown = true,
	                 .homeIn = 2}},
	});

	all.push_back({
	    .id = "objects.scaffold_offers_wonder",
	    .name = "A scaffold of the value of seven offers the town a wonder",
	    .facet = Facet::Objects,
	    .description = "The town and the camera of objects.scaffold_starts_building. The town believes in the player, "
	                   "so it wants a wonder. The loose scaffold has the value of seven scaffolds. An oak stands near the "
	                   "place where the hand puts the scaffold down. The input tools/sim/hand_inputs/scaffold_build.json "
	                   "moves the hand: it takes the scaffold, holds it still near the village centre, and puts it down.",
	    .expected = "While the hand holds the scaffold still, the phantom of the Celtic wonder grows below the scaffold. "
	                "Its size comes from the wonder power at that place: the desire of the town for a wonder, and the "
	                "things near the place. When the hand puts the scaffold down, a building site of the wonder opens. "
	                "The inspector query town.wonders shows the wonder and its power. When the wonder is finished, the "
	                "most Celtic power of the player becomes larger by the power of the wonder.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_LooseScaffold - glm::vec2 {10.0f}, k_BuildPlace + glm::vec2 {10.0f}}},
	    .objects = {{.type = AbodeInfo::CelticTownCentre, .offset = {0.0f, 50.0f}},
	                {.type = ScaffoldInfo::Normal, .offset = k_LooseScaffold, .scale = 1.9f},
	                {.type = TreeInfo::Oak, .offset = k_BuildPlace + glm::vec2 {-4.0f, -6.0f}}},
	    .townBeliefInPlayer = 1.0f,
	});

	all.push_back({
	    .id = "objects.scaffold_site_destroyed_starts_again",
	    .name = "A scaffold starts a new site when an effect destroys its unfinished building",
	    .facet = Facet::Objects,
	    .description = "The town, the scaffold and the camera of objects.scaffold_starts_building. After one second the "
	                   "scaffold starts the building that the town offers where it lies, as when the hand puts it down. "
	                   "After four seconds more, an effect destroys the unfinished building of the site, as when a fire "
	                   "takes the last of its life.",
	    .expected = "A building site opens at the scaffold, with the phantom of its building. When the effect destroys "
	                "the building, a ghost of the building flickers out, and at once the scaffold opens a new site where "
	                "it lies. The new site can show another building or another angle, because the town looks for its "
	                "offer again. The planting sound plays, and the hand can take the scaffold again for a time.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_LooseScaffold - glm::vec2 {10.0f}, k_BuildPlace + glm::vec2 {10.0f}}},
	    .objects = {{.type = AbodeInfo::CelticTownCentre, .offset = {0.0f, 50.0f}},
	                {.type = ScaffoldInfo::Normal, .offset = k_LooseScaffold, .scale = 0.7f}},
	    .commands = {{.kind = Kind::ScaffoldStartsSite, .delaySeconds = 1.0f, .object = 1},
	                 {.kind = Kind::DestroyScaffoldSite, .delaySeconds = 4.0f, .object = 1}},
	});

	all.push_back({
	    .id = "objects.scaffold_site_phantom_see_through",
	    .name = "The phantom of the building on a scaffold's site draws in the see-through modes",
	    .facet = Facet::Objects,
	    .description = "The town and the scaffold of objects.scaffold_starts_building, with the camera near the scaffold. "
	                   "After one second the scaffold starts the building that the town offers where it lies, as when "
	                   "the hand puts it down.",
	    .expected = "A building site opens at the scaffold, with the phantom of its building over the site at about a "
	                "third of its alpha. Each material of the phantom draws in its see-through mode with the alpha "
	                "reference 10, which becomes 0 at the alpha of the phantom. Thus no part of the phantom is cut: also "
	                "the edges that its textures make clear show at the alpha of their texels, and write depth.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview,
	                .include = {k_LooseScaffold - glm::vec2 {12.0f}, k_LooseScaffold + glm::vec2 {12.0f}}},
	    .objects = {{.type = AbodeInfo::CelticTownCentre, .offset = {0.0f, 50.0f}},
	                {.type = ScaffoldInfo::Normal, .offset = k_LooseScaffold, .scale = 0.7f}},
	    .commands = {{.kind = Kind::ScaffoldStartsSite, .delaySeconds = 1.0f, .object = 1}},
	});

	all.push_back({
	    .id = "objects.scaffold_script_active",
	    .name = "A script sets a scaffold active, and the scaffold starts its building at once",
	    .facet = Facet::Objects,
	    .description = "The town, the scaffold and the camera of objects.scaffold_starts_building. After one second a "
	                   "script sets the scaffold inactive. After one second more, the script sets the scaffold active.",
	    .expected = "The first call does not change the scaffold. At the second call, a building site opens at the "
	                "scaffold, with the phantom of the building that the town offers where the scaffold lies. No player "
	                "put the scaffold down, so the planting sound does not play for the local player.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_LooseScaffold - glm::vec2 {10.0f}, k_BuildPlace + glm::vec2 {10.0f}}},
	    .objects = {{.type = AbodeInfo::CelticTownCentre, .offset = {0.0f, 50.0f}},
	                {.type = ScaffoldInfo::Normal, .offset = k_LooseScaffold, .scale = 0.7f}},
	    .commands = {{.kind = Kind::CallNative,
	                  .delaySeconds = 1.0f,
	                  .script = "SET_ACTIVE",
	                  .scriptArguments = {{.kind = ScriptArgument::Kind::Integer, .number = 0.0f},
	                                      {.kind = ScriptArgument::Kind::Object, .index = 1}}},
	                 {.kind = Kind::CallNative,
	                  .delaySeconds = 1.0f,
	                  .script = "SET_ACTIVE",
	                  .scriptArguments = {{.kind = ScriptArgument::Kind::Integer, .number = 1.0f},
	                                      {.kind = ScriptArgument::Kind::Object, .index = 1}}}},
	});

	all.push_back({
	    .id = "objects.wood_poured_on_scaffold_site",
	    .name = "The hand pours wood onto a scaffold on a site, and the site gets the wood",
	    .facet = Facet::Objects,
	    .description = "The town, the scaffold and the camera of objects.scaffold_starts_building. After one second the "
	                   "scaffold starts the building that the town offers where it lies. After three seconds more, the "
	                   "hand of the player pours 300 wood over the scaffold.",
	    .expected = "A building site opens at the scaffold. The poured wood goes into the site, as into a store. No pile "
	                "of wood stays on the ground at the scaffold. The inspector shows the wood of the site go up by the "
	                "wood that the hand poured.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_LooseScaffold - glm::vec2 {10.0f}, k_BuildPlace + glm::vec2 {10.0f}}},
	    .objects = {{.type = AbodeInfo::CelticTownCentre, .offset = {0.0f, 50.0f}},
	                {.type = ScaffoldInfo::Normal, .offset = k_LooseScaffold, .scale = 0.7f}},
	    .commands = {{.kind = Kind::ScaffoldStartsSite, .delaySeconds = 1.0f, .object = 1},
	                 {.kind = Kind::HandPour, .delaySeconds = 3.0f, .point = k_LooseScaffold, .value = 300}},
	});

	all.push_back({
	    .id = "objects.scaffold_founds_town",
	    .name = "A scaffold put down away from every town founds a new town",
	    .facet = Facet::Objects,
	    .description = "A Celtic town of the player's in the middle of the map, with no buildings, and a scaffold of the "
	                   "town with the value of five scaffolds, 100 m away from the town. After one second the scaffold "
	                   "starts the building that it offers where it lies, as when the hand puts it down.",
	    .expected = "A building site of a Celtic village centre opens at the scaffold, with the phantom of the village "
	                "centre. The inspector query town.list shows a new Celtic town of the player's with the id 703058. "
	                "The new town believes in the player with the starting belief of a new town, and has no reach of "
	                "its own until it has buildings.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview,
	                .include = {k_FoundingScaffold - glm::vec2 {20.0f}, k_FoundingScaffold + glm::vec2 {20.0f}}},
	    .objects = {{.type = ScaffoldInfo::Normal, .offset = k_FoundingScaffold, .scale = 1.5f, .joinTown = true}},
	    .commands = {{.kind = Kind::ScaffoldStartsSite, .delaySeconds = 1.0f, .object = 0}},
	});

	all.push_back({
	    .id = "objects.burning_scaffold_tap",
	    .name = "A tap on a burning scaffold breaks off a burning piece, and the fire burns the scaffolds away",
	    .facet = Facet::Objects,
	    .description = "A loose scaffold that is worth three scaffolds and that no workshop counts, alight as at the start "
	                   "of a fire, with no town near. After one second the hand of the player taps the scaffold.",
	    .expected = "The tap breaks one small scaffold off. The two parts move apart and one of four sounds plays. The "
	                "piece burns as the old scaffold burns: the inspector shows a Fire on the two scaffolds. The fire "
	                "takes the life of each scaffold. When a scaffold has no life, it goes and leaves nothing.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview,
	                .include = {k_FoundingScaffold - glm::vec2 {12.0f}, k_FoundingScaffold + glm::vec2 {12.0f}}},
	    .objects = {{.type = ScaffoldInfo::Normal, .offset = k_FoundingScaffold, .scale = 1.1f, .alight = true}},
	    .commands = {{.kind = Kind::HandTapObject, .delaySeconds = 1.0f, .object = 0}},
	});

	all.push_back({
	    .id = "objects.scaffold_clears_ground",
	    .name = "A scaffold that a script marked clears its ground when the hand lets it go",
	    .facet = Facet::Objects,
	    .description = "The town, the scaffold and the camera of objects.scaffold_starts_building. A script marked the "
	                   "scaffold to destroy the things in its way. A bush and a ball stand where the hand puts the "
	                   "scaffold down. The input tools/sim/hand_inputs/scaffold_build.json moves the hand: it takes the "
	                   "scaffold, holds it still near the village centre, and puts it down.",
	    .expected = "When the hand lets the scaffold go, the bush and the ball jump out of the way. Each starts from the "
	                "height of the scaffold above the land and flies a short distance. A building site then opens where "
	                "the scaffold is.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_LooseScaffold - glm::vec2 {10.0f}, k_BuildPlace + glm::vec2 {10.0f}}},
	    .objects = {{.type = AbodeInfo::CelticTownCentre, .offset = {0.0f, 50.0f}},
	                {.type = ScaffoldInfo::Normal, .offset = k_LooseScaffold, .scale = 0.7f, .clearsGround = true},
	                {.type = TreeInfo::Bush, .offset = k_BuildPlace},
	                {.type = MobileObjectInfo::Ball, .offset = k_BuildPlace + glm::vec2 {0.0f, -1.5f}}},
	});

	all.push_back({
	    .id = "objects.scaffold_clears_ground_creature",
	    .name = "A scaffold that clears its ground leaves a creature alone",
	    .facet = Facet::Objects,
	    .description =
	        "A loose scaffold that a script marked to destroy the things in its way, away from every town. The "
	        "player's tiger stands still 3 m east of the scaffold, and a bush stands 1.4 m north-east of it. After one "
	        "second the scaffold clears its ground, as when the hand lets it go.",
	    .expected = "The bush jumps out of the way. The tiger stays where it is and stays in the game: the inspector "
	                "still shows it. The game log tells of the push of the bush and of no removal.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview,
	                .include = {k_FoundingScaffold - glm::vec2 {15.0f}, k_FoundingScaffold + glm::vec2 {15.0f}}},
	    .creatures = {{.species = CreatureType::Tiger,
	                   .offset = k_FoundingScaffold + glm::vec2 {3.0f, 0.0f},
	                   .pauseMind = true}},
	    .objects = {{.type = ScaffoldInfo::Normal, .offset = k_FoundingScaffold, .scale = 0.7f, .clearsGround = true},
	                {.type = TreeInfo::Bush, .offset = k_FoundingScaffold + glm::vec2 {1.0f, 1.0f}}},
	    .commands = {{.kind = Kind::ScaffoldClearsGround, .delaySeconds = 1.0f, .object = 0}},
	});

	all.push_back({
	    .id = "objects.scaffold_clears_ground_fixed_rock",
	    .name = "A scaffold that clears its ground removes a rock and pushes a ball",
	    .facet = Facet::Objects,
	    .description = "A loose scaffold that a script marked to destroy the things in its way, away from every town. A "
	                   "rock stands 1.5 m east of it and a ball 1.5 m north of it. After one second the scaffold clears "
	                   "its ground, as when the hand lets it go.",
	    .expected = "The rock has the fixed mark of the land, so the scaffold does not push it: the rock goes from the "
	                "game at once. The ball has no mark, so it jumps out of the way and stays in the game. The game log "
	                "tells of the removal of the rock and of the push of the ball.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview,
	                .include = {k_FoundingScaffold - glm::vec2 {15.0f}, k_FoundingScaffold + glm::vec2 {15.0f}}},
	    .objects = {{.type = ScaffoldInfo::Normal, .offset = k_FoundingScaffold, .scale = 0.7f, .clearsGround = true},
	                {.type = MobileStaticInfo::Rock, .offset = k_FoundingScaffold + glm::vec2 {1.5f, 0.0f}},
	                {.type = MobileObjectInfo::Ball, .offset = k_FoundingScaffold + glm::vec2 {0.0f, 1.5f}}},
	    .commands = {{.kind = Kind::ScaffoldClearsGround, .delaySeconds = 1.0f, .object = 0}},
	    .checks = {{.label = "the rock goes; the scaffold and the ball stay in the game",
	                .quantity = checks::Quantity::ObjectsAlive,
	                .afterTurns = 50,
	                .atLeast = 2,
	                .atMost = 2}},
	});

	all.push_back({
	    .id = "objects.scaffold_push_on_high_land",
	    .name = "A thing that a clearing scaffold pushes starts under high land",
	    .facet = Facet::Objects,
	    .description = "A loose scaffold that a script marked to destroy the things in its way lies on the land, which is "
	                   "about 21 m high here. A bush stands 1.5 m east of it and a ball 1.5 m north of it. After one second "
	                   "the scaffold clears its ground, as when the hand lets it go.",
	    .expected = "As in the game, the bush and the ball start at the height of the scaffold above the land plus half a "
	                "metre, twice as far from the scaffold as the edge of the scaffold. That is a height of the world, so "
	                "each starts under the land here. The land throws each one far across the map.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview,
	                .include = {k_FoundingScaffold - glm::vec2 {15.0f}, k_FoundingScaffold + glm::vec2 {15.0f}}},
	    .objects = {{.type = ScaffoldInfo::Normal, .offset = k_FoundingScaffold, .scale = 0.7f, .clearsGround = true},
	                {.type = TreeInfo::Bush, .offset = k_FoundingScaffold + glm::vec2 {1.5f, 0.0f}},
	                {.type = MobileObjectInfo::Ball, .offset = k_FoundingScaffold + glm::vec2 {0.0f, 1.5f}}},
	    .commands = {{.kind = Kind::ScaffoldClearsGround, .delaySeconds = 1.0f, .object = 0}},
	});

	all.push_back({
	    .id = "objects.scaffold_push_straight_west_or_south",
	    .name = "A clearing scaffold pushes a thing straight west and a thing straight south",
	    .facet = Facet::Objects,
	    .description = "A loose scaffold that a script marked to destroy the things in its way. A bush stands 1.5 m "
	                   "straight west of it and a ball 1.5 m straight south of it. After one second the scaffold clears "
	                   "its ground, as when the hand lets it go.",
	    .expected = "The bush starts west and the ball starts south of the scaffold, under the land, as in the game. The "
	                "land throws each one far, and the bush becomes a dead tree. The game divides the push by 0 here and "
	                "the things go from the map. openblack divides by the larger size, so both things stay on the map.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview,
	                .include = {k_FoundingScaffold - glm::vec2 {15.0f}, k_FoundingScaffold + glm::vec2 {15.0f}}},
	    .objects = {{.type = ScaffoldInfo::Normal, .offset = k_FoundingScaffold, .scale = 0.7f, .clearsGround = true},
	                {.type = TreeInfo::Bush, .offset = k_FoundingScaffold + glm::vec2 {-1.5f, 0.0f}},
	                {.type = MobileObjectInfo::Ball, .offset = k_FoundingScaffold + glm::vec2 {0.0f, -1.5f}}},
	    .commands = {{.kind = Kind::ScaffoldClearsGround, .delaySeconds = 1.0f, .object = 0}},
	    .checks = {{.label = "the scaffold and the ball stay in the game",
	                .quantity = checks::Quantity::ObjectsAlive,
	                .afterTurns = 100,
	                .atLeast = 2},
	               {.label = "the ball lands near the scaffold, not at the corner of the map",
	                .quantity = checks::Quantity::ObjectDistance,
	                .index = 2,
	                .point = k_FoundingScaffold,
	                .afterTurns = 100,
	                .atMost = 100.0}},
	});

	all.push_back({
	    .id = "objects.workshop_site_wood_on_its_pile",
	    .name = "The wood for a workshop that goes up lies on the pile of the workshop",
	    .facet = Facet::Objects,
	    .description = "A Celtic town of the player's with a village centre, a storage pit that holds 6000 wood 25 m to the "
	                   "west, four builders of the town, and a workshop that is a tenth built. The workshop holds 1000 "
	                   "wood. Best watched at game speed 4.",
	    .expected = "The wood of the workshop lies on the pile of logs at the back of the workshop. No other pile of wood "
	                "comes for the site. The builders take wood from this pile to build, and they bring wood from the "
	                "storage pit to the workshop. When the workshop is finished, the wood that is left stays on the pile "
	                "as the wood of the workshop. The villagers do not take it away.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_Workshop - glm::vec2 {15.0f}, k_Workshop + glm::vec2 {15.0f}}},
	    .objects =
	        {{.type = AbodeInfo::CelticTownCentre, .offset = {0.0f, 50.0f}},
	         {.type = AbodeInfo::CelticStoragePit, .offset = k_Workshop + glm::vec2 {-25.0f, 0.0f}, .storedWood = 6000},
	         {.type = AbodeInfo::CelticWorkshop, .offset = k_Workshop, .yawDegrees = 180.0f, .storedWood = 1000, .built = 0.1f},
	         {.type = VillagerInfo::CelticFarmerMale, .offset = k_Workshop + glm::vec2 {-6.0f, -10.0f}, .joinTown = true},
	         {.type = VillagerInfo::CelticFarmerMale, .offset = k_Workshop + glm::vec2 {-3.0f, -11.0f}, .joinTown = true},
	         {.type = VillagerInfo::CelticForesterMale, .offset = k_Workshop + glm::vec2 {3.0f, -11.0f}, .joinTown = true},
	         {.type = VillagerInfo::CelticFarmerMale, .offset = k_Workshop + glm::vec2 {6.0f, -10.0f}, .joinTown = true}},
	});

	all.push_back({
	    .id = "story.reward_chest_scaffold",
	    .name = "A reward chest gives a town with a workshop a scaffold",
	    .facet = Facet::Objects,
	    .description = "A Celtic town of the player's with a village centre and a built workshop with no wood. After one "
	                   "second a reward chest of a scaffold lies opened in the town, near the workshop.",
	    .expected = "A small scaffold, worth one scaffold and not turned, appears where the chest is. The workshop counts "
	                "it as its own (the inspector shows it in the scaffolds of the workshop), but it does not move into "
	                "the yard.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_Workshop - glm::vec2 {15.0f}, k_Workshop + glm::vec2 {15.0f}}},
	    .objects = {{.type = AbodeInfo::CelticTownCentre, .offset = {0.0f, 50.0f}},
	                {.type = AbodeInfo::CelticWorkshop, .offset = k_Workshop, .yawDegrees = 180.0f, .storedWood = 0}},
	    .rewards = {{.type = RewardObjectInfo::Scaffold,
	                 .offset = k_Workshop + glm::vec2 {12.0f, -12.0f},
	                 .fromSky = false,
	                 .inTown = true,
	                 .delaySeconds = 1.0f,
	                 .opened = true}},
	});

	// The tiger and a second loose scaffold stand inside the shot of objects.scaffold_starts_building, away from the
	// hand's way, so that the shot and the hand's input stay the same. The tiger stays still until the input is over.
	// The second scaffold is far from the village centre: the creature walks round the centre, and a scaffold near its
	// edge is out of the creature's reach from there.
	constexpr glm::vec2 k_SiteWatcher {-32.0f, 47.0f};
	constexpr glm::vec2 k_SecondScaffold {-24.0f, 42.0f};
	constexpr float k_WatcherWaits = 20.0f;
	all.push_back({
	    .id = "objects.creature_scaffold_on_site",
	    .name = "A creature leaves a scaffold that a building site keeps",
	    .facet = Facet::Objects,
	    .description = "The town, the scaffold and the camera of objects.scaffold_starts_building, with the player's tiger "
	                   "and a second loose scaffold north of the scaffold. The tiger stays still for 20 seconds. The input "
	                   "tools/sim/hand_inputs/scaffold_build.json moves the hand: it takes the scaffold, holds it still "
	                   "near the village centre, and puts it down. After 100 seconds a script tells the tiger to examine "
	                   "the scaffold on the site by picking it up, and after 40 seconds more, the loose scaffold.",
	    .expected = "The scaffold opens a building site. After the time that a hand can take it back, the tiger cannot "
	                "pick it up: a creature takes a scaffold only when a hand can take it. The tiger picks up the loose "
	                "scaffold and looks at it.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_LooseScaffold - glm::vec2 {10.0f}, k_BuildPlace + glm::vec2 {10.0f}}},
	    .creatures = {{.species = CreatureType::Tiger,
	                   .offset = k_SiteWatcher,
	                   .pauseMind = true,
	                   .thinksAfterSeconds = k_WatcherWaits}},
	    .objects = {{.type = AbodeInfo::CelticTownCentre, .offset = {0.0f, 50.0f}},
	                {.type = ScaffoldInfo::Normal, .offset = k_LooseScaffold, .scale = 0.7f},
	                {.type = ScaffoldInfo::Normal, .offset = k_SecondScaffold, .scale = 0.7f}},
	    .commands = {{.kind = Kind::DoAction, .delaySeconds = 100.0f, .object = 1, .action = "ExamineByPickingUp"},
	                 {.kind = Kind::DoAction, .delaySeconds = 40.0f, .object = 2, .action = "ExamineByPickingUp"}},
	});

	// The abode number of a house and the size, as a land script gives Khazar's scaffold for a house
	constexpr float k_HouseNumber = 4.0f;
	constexpr float k_ScriptSize = 2.0f;
	all.push_back({
	    .id = "objects.script_gives_scaffold",
	    .name = "A script makes a scaffold, gives it a building and a size, and locks it",
	    .facet = Facet::Objects,
	    .description = "The town of objects.scaffold_starts_building with no loose scaffold. After one second, the "
	                   "script lines of a land give a scaffold: CREATE makes a scaffold (type 39) where the loose "
	                   "scaffold was, and SET_SCAFFOLD_PROPERTIES gives it the house of abode number 4 and the size 2. "
	                   "The input tools/sim/hand_inputs/scaffold_build.json moves the hand: it takes the scaffold, holds "
	                   "it still near the village centre, and puts it down. After ten more seconds, SET_ID_PICKUPABLE "
	                   "and SET_ID_MOVEABLE lock the scaffold.",
	    .expected = "A scaffold appears on the land. It has the size of a scaffold of the value of two scaffolds. While "
	                "the hand holds it still, the phantom shows only a house of abode number 4. When the hand puts it "
	                "down, a building site of that house opens. After the lock, the hand can not take the scaffold, and "
	                "the inspector shows CannotBePickedUp and Immovable on it.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_LooseScaffold - glm::vec2 {10.0f}, k_BuildPlace + glm::vec2 {10.0f}}},
	    .objects = {{.type = AbodeInfo::CelticTownCentre, .offset = {0.0f, 50.0f}}},
	    .commands = {{.kind = Kind::CallNative,
	                  .delaySeconds = 1.0f,
	                  .script = "CREATE",
	                  .scriptArguments = {{.kind = ScriptArgument::Kind::Integer,
	                                       .number = static_cast<float>(script::ObjectType::Scaffold)},
	                                      {.kind = ScriptArgument::Kind::Integer, .number = 5000.0f},
	                                      {.kind = ScriptArgument::Kind::Position, .point = k_LooseScaffold}}},
	                 {.kind = Kind::CallNative,
	                  .delaySeconds = 0.0f,
	                  .script = "SET_SCAFFOLD_PROPERTIES",
	                  .scriptArguments = {{.kind = ScriptArgument::Kind::Made, .index = 0},
	                                      {.kind = ScriptArgument::Kind::Integer, .number = k_HouseNumber},
	                                      {.kind = ScriptArgument::Kind::Number, .number = k_ScriptSize},
	                                      {.kind = ScriptArgument::Kind::Integer, .number = 0.0f}}},
	                 {.kind = Kind::CallNative,
	                  .delaySeconds = 10.0f,
	                  .script = "SET_ID_PICKUPABLE",
	                  .scriptArguments = {{.kind = ScriptArgument::Kind::Integer, .number = 0.0f},
	                                      {.kind = ScriptArgument::Kind::Made, .index = 0}}},
	                 {.kind = Kind::CallNative,
	                  .delaySeconds = 0.0f,
	                  .script = "SET_ID_MOVEABLE",
	                  .scriptArguments = {{.kind = ScriptArgument::Kind::Integer, .number = 0.0f},
	                                      {.kind = ScriptArgument::Kind::Made, .index = 0}}}},
	});

	// The workshop's abode number and the size, as the third land's script lays out the workshop of the home town
	constexpr float k_WorkshopNumber = 9.0f;
	constexpr float k_WorkshopSize = 3.0f;
	constexpr glm::vec2 k_RockyPlace {-45.0f, 15.0f};
	all.push_back({
	    .id = "objects.scaffold_builds_among_rocks",
	    .name = "A script scaffold starts its workshop among rocks",
	    .facet = Facet::Objects,
	    .description = "A Celtic town of the player's (which believes in the player from the start) with a village "
	                   "centre. Four rocks lie 4 m round a place 57 m from the centre. After one second, CREATE makes a "
	                   "scaffold (type 39) at that place, and "
	                   "SET_SCAFFOLD_PROPERTIES gives it the workshop (abode number 9) and the size 3. Then SET_ACTIVE "
	                   "sets the scaffold active, as the third land's script does with the workshop of the home town.",
	    .expected = "A building site of a workshop opens at the scaffold, with the phantom of the workshop over the "
	                "rocks. The rocks do not stop the building. The scaffold stays on the site.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_RockyPlace - glm::vec2 {15.0f}, k_RockyPlace + glm::vec2 {15.0f}}},
	    .objects = {{.type = AbodeInfo::CelticTownCentre, .offset = {0.0f, 50.0f}},
	                {.type = MobileStaticInfo::RockChalk, .offset = k_RockyPlace + glm::vec2 {4.0f, 0.0f}},
	                {.type = MobileStaticInfo::RockChalk, .offset = k_RockyPlace + glm::vec2 {-4.0f, 0.0f}},
	                {.type = MobileStaticInfo::RockChalk, .offset = k_RockyPlace + glm::vec2 {0.0f, 4.0f}},
	                {.type = MobileStaticInfo::RockChalk, .offset = k_RockyPlace + glm::vec2 {0.0f, -4.0f}}},
	    .commands = {{.kind = Kind::CallNative,
	                  .delaySeconds = 1.0f,
	                  .script = "CREATE",
	                  .scriptArguments = {{.kind = ScriptArgument::Kind::Integer,
	                                       .number = static_cast<float>(script::ObjectType::Scaffold)},
	                                      {.kind = ScriptArgument::Kind::Integer, .number = 5000.0f},
	                                      {.kind = ScriptArgument::Kind::Position, .point = k_RockyPlace}}},
	                 {.kind = Kind::CallNative,
	                  .delaySeconds = 0.0f,
	                  .script = "SET_SCAFFOLD_PROPERTIES",
	                  .scriptArguments = {{.kind = ScriptArgument::Kind::Made, .index = 0},
	                                      {.kind = ScriptArgument::Kind::Integer, .number = k_WorkshopNumber},
	                                      {.kind = ScriptArgument::Kind::Number, .number = k_WorkshopSize},
	                                      {.kind = ScriptArgument::Kind::Integer, .number = 0.0f}}},
	                 {.kind = Kind::CallNative,
	                  .delaySeconds = 0.0f,
	                  .script = "SET_ACTIVE",
	                  .scriptArguments = {{.kind = ScriptArgument::Kind::Integer, .number = 1.0f},
	                                      {.kind = ScriptArgument::Kind::Made, .index = 0}}}},
	    // A town of the neutral player wants no workshop, so the town stays the player's
	    .townBeliefInPlayer = 1000.0f,
	});
	// A fish farm 10 m from the scaffold, in the middle of the next cell and inside the outline of the workshop
	constexpr glm::vec2 k_FarmPlace {10.0f, 0.0f};
	all.push_back({
	    .id = "objects.scaffold_stopped_by_fish_farm",
	    .name = "A fish farm stops the workshop of a script scaffold",
	    .facet = Facet::Objects,
	    .description = "A Celtic town of the player's (which believes in the player from the start) with a village "
	                   "centre. A fish farm stands 10 m from a place 57 m from the centre. After one second, CREATE makes "
	                   "a scaffold (type 39) at that place, and SET_SCAFFOLD_PROPERTIES gives it the workshop (abode "
	                   "number 9) and the size 3. Then SET_ACTIVE sets the scaffold active.",
	    .expected = "No building site opens at the scaffold, because the fish farm is in the outline of the workshop. "
	                "The scaffold stays where it is, with no phantom of the workshop.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Overview, .include = {k_RockyPlace - glm::vec2 {15.0f}, k_RockyPlace + glm::vec2 {15.0f}}},
	    .objects = {{.type = AbodeInfo::CelticTownCentre, .offset = {0.0f, 50.0f}},
	                {.type = FishFarmInfo::Normal, .offset = k_RockyPlace + k_FarmPlace}},
	    .commands = {{.kind = Kind::CallNative,
	                  .delaySeconds = 1.0f,
	                  .script = "CREATE",
	                  .scriptArguments = {{.kind = ScriptArgument::Kind::Integer,
	                                       .number = static_cast<float>(script::ObjectType::Scaffold)},
	                                      {.kind = ScriptArgument::Kind::Integer, .number = 5000.0f},
	                                      {.kind = ScriptArgument::Kind::Position, .point = k_RockyPlace}}},
	                 {.kind = Kind::CallNative,
	                  .delaySeconds = 0.0f,
	                  .script = "SET_SCAFFOLD_PROPERTIES",
	                  .scriptArguments = {{.kind = ScriptArgument::Kind::Made, .index = 0},
	                                      {.kind = ScriptArgument::Kind::Integer, .number = k_WorkshopNumber},
	                                      {.kind = ScriptArgument::Kind::Number, .number = k_WorkshopSize},
	                                      {.kind = ScriptArgument::Kind::Integer, .number = 0.0f}}},
	                 {.kind = Kind::CallNative,
	                  .delaySeconds = 0.0f,
	                  .script = "SET_ACTIVE",
	                  .scriptArguments = {{.kind = ScriptArgument::Kind::Integer, .number = 1.0f},
	                                      {.kind = ScriptArgument::Kind::Made, .index = 0}}}},
	    .townBeliefInPlayer = 1000.0f,
	});

	// The Workshop of the second land with a home town of more than 40 grown ups. The quest's script counts at most 40
	// of them when it finds the share of homeless grown ups. The places are the script's, from the middle of the land.
	constexpr glm::vec2 k_LandTwoMiddle {2240.0f, 2720.0f};
	const auto onLandTwo = [k_LandTwoMiddle](float x, float z) { return glm::vec2(x, z) - k_LandTwoMiddle; };
	constexpr uint32_t k_HomeTown = 0;
	const auto homeWorkshop = onLandTwo(3252.0894f, 3341.0461f);
	std::vector<ObjectSetup> homeTown {
	    {.type = AbodeInfo::NorseWorkshop,
	     .offset = homeWorkshop,
	     .yawDegrees = 160.0f,
	     .landTown = k_HomeTown,
	     .storedWood = 0},
	    {.type = AbodeInfo::NorseTempleX, .offset = onLandTwo(3190.0f, 3440.0f), .landTown = k_HomeTown},
	    {.type = AbodeInfo::NorseTempleX, .offset = onLandTwo(3215.0f, 3445.0f), .landTown = k_HomeTown},
	    {.type = AbodeInfo::NorseTempleX, .offset = onLandTwo(3170.0f, 3420.0f), .landTown = k_HomeTown},
	    {.type = AbodeInfo::NorseTempleX, .offset = onLandTwo(3240.0f, 3450.0f), .landTown = k_HomeTown}};
	constexpr size_t k_GrownUps = 45;
	for (size_t i = 0; i < k_GrownUps; ++i)
	{
		homeTown.push_back(
		    {.type = i % 2 == 0 ? VillagerInfo::NorseHousewifeFemale : VillagerInfo::NorseFarmerMale,
		     .offset = onLandTwo(3226.0f + static_cast<float>(i % 6) * 2.0f, 3405.0f + static_cast<float>(i / 6) * 2.0f),
		     .landTown = k_HomeTown});
	}
	all.push_back({
	    .id = "story.workshop_homeless_cap",
	    .name = "Story: The Workshop with a home town of more than 40 grown ups",
	    .facet = Facet::Story,
	    .description = "The second land of the story. The player's first town gets a workshop at the place of the "
	                   "quest, four houses and 45 grown ups. It then has more than 40 grown ups and room for 20 of "
	                   "them. The quest's script for the finished workshop runs. Play the quest by hand: click the "
	                   "scroll, give wood to the workshop, put its scaffold down as a house and build the house.",
	    .expected = "In the housing step the quest counts 40 grown ups, not more. With room for 20, half of the 40 have "
	                "no home and the step waits. When the room is 25 or more, fewer than 40 in 100 have no home and the "
	                "step ends: the second scroll appears at the workshop. With no limit of 40 the step would wait "
	                "until the room is 30.",
	    .environment = {.land = Land::Story, .storyLand = "Land2.txt", .dispenserGrid = false},
	    .framing = {.shot = Shot::Overview,
	                .include = {homeWorkshop + glm::vec2(-60.0f, -20.0f), homeWorkshop + glm::vec2(30.0f, 110.0f)}},
	    .objects = homeTown,
	    .commands = {{.kind = Kind::RunScript,
	                  .delaySeconds = 1.0f,
	                  .script = "TheWorkshopComplete",
	                  .scriptArguments = {{.kind = ScriptArgument::Kind::Object, .index = 0}}}},
	});
}

std::vector<Scenario> Build()
{
	std::vector<Scenario> all;
	AddIdle(all);
	AddExpressions(all);
	AddSenses(all);
	AddNeeds(all);
	AddRestAndHome(all);
	AddGrowth(all);
	AddAppearance(all);
	AddLight(all);
	AddMovement(all);
	AddFootprints(all);
	AddAudio(all);
	AddObjects(all);
	AddHand(all);
	AddLeash(all);
	AddCombat(all);
	AddMind(all);
	AddParticles(all);
	AddEditor(all);
	AddMiracleScenarios(all);
	AddGlobeScenarios(all);
	AddLifeLightScenarios(all);
	AddCreatureCastingScenarios(all);
	AddFriendActionScenarios(all);
	AddBenchmark(all);
	AddCreatureModeScenarios(all);
	AddSavedCameraScenarios(all);
	AddGestureScenarios(all);
	AddStormScenarios(all);
	AddFlockScenarios(all);
	AddBirdScenarios(all);
	AddTeleportScenarios(all);
	AddVortexScenarios(all);
	AddLandFiveQuestScenarios(all);
	AddLandsFourFiveScrollScenarios(all);
	AddLand4StoneScenarios(all);
	AddLand4SlegScenarios(all);
	AddPuzzleQuestScenarios(all);
	AddUndeadTotemScenarios(all);
	AddRewardScenarios(all);
	AddTornadoScenarios(all);
	AddPhysicsScenarios(all);
	AddNatureScenarios(all);
	AddHandNavigationScenarios(all);
	AddHandLookScenarios(all);
	AddGateScenarios(all);
	AddVillageTotemScenarios(all);
	AddTempleVillagerScenarios(all);
	AddFishScenarios(all);
	AddKnockScenarios(all);
	AddFootballScenarios(all);
	AddVillagerSpeedScenarios(all);
	AddDiscipleScenarios(all);
	AddVillagerLabelScenarios(all);
	AddTempleKeyScenarios(all);
	AddHighlightScenarios(all);
	AddSkyScenarios(all);
	AddAdvisorScenarios(all);
	AddStory(all);
	AddExplorersBoat(all);
	AddTownGrowthScenarios(all);
	AddStatisticsScenarios(all);
	AddMultiplayerFrontEndScenarios(all);
	AddOtherGodScenarios(all);
	AddJoinOnlineScenarios(all);
	AddTutorialScenarios(all);
	AddCreatureDanceScenarios(all);
	AddCreatureLearningScenarios(all);
	AddComputerPlayerScenarios(all);
	AddGameEndScenarios(all);
	AddEndingScenarios(all);
	AddLastStandScenarios(all);
	AddWorldSoundScenarios(all);
	AddTownMiracleScenarios(all);
	AddFixedKindScenarios(all);
	AddCreatureExplorationScenarios(all);
	AddCreatureActionScenarios(all);
	AddScriptTaskRelease(all);
	AddLandOneScriptStubScenarios(all);
	AddVillagerSizeAndReachScenarios(all);
	AddSwayAndFrightScenarios(all);
	AddStoreScenarios(all);
	AddPoisonScenarios(all);
	AddWorkshopScenarios(all);
	AddGodsRingMapScenarios(all);
	AddSelfCheckScenarios(all);
	AddWorshipSiteScenarios(all);
	AddLateSilverScrollScenarios(all);
	return all;
}

bool InRange(float value, float low, float high)
{
	return value >= low && value <= high;
}

void CheckNeeds(const NeedOverrides& needs, std::vector<std::string>& problems, std::string_view who)
{
	const std::array<std::pair<std::string_view, std::optional<float>>, 5> k_UnitNeeds {{
	    {"exhaustion", needs.exhaustion},
	    {"dehydration", needs.dehydration},
	    {"poo", needs.poo},
	    {"life", needs.life},
	    {"energy", needs.energy},
	}};
	for (const auto& [name, value] : k_UnitNeeds)
	{
		if (value.has_value() && !InRange(*value, 0.0f, creature_physiology::k_MaxGrownSize))
		{
			problems.push_back(fmt::format("{}: {} {} out of range", who, name, *value));
		}
	}
	if (needs.warmth.has_value() && !InRange(*needs.warmth, -1.0f, 1.0f))
	{
		problems.push_back(fmt::format("{}: warmth {} out of range", who, *needs.warmth));
	}
}

bool ValidAnimation(Kind kind, size_t animation)
{
	switch (kind)
	{
	case Kind::PlayAction:
		return animation >= animations::k_FirstAction && animation < animations::k_FirstAction + animations::k_ActionCount;
	case Kind::PlayGesture:
		return animation >= animations::k_FirstGesture && animation < animations::k_FirstGesture + animations::k_GestureCount;
	case Kind::PullFace:
		return animation >= animations::k_FirstFace && animation < animations::k_FirstFace + animations::k_FaceCount;
	default:
		return true;
	}
}

bool ValidOffset(glm::vec2 offset)
{
	return std::abs(offset.x) <= k_MaxOffset && std::abs(offset.y) <= k_MaxOffset;
}

/// A script run names a script, and every object, creature and marker it hands over is the scenario's and on the map.
/// An object that a native call gave back must come from a native call before this command.
bool ScriptArgumentsFit(const Command& command, const Scenario& scenario)
{
	const auto before =
	    std::ranges::find_if(scenario.commands, [&command](const Command& other) { return &other == &command; });
	const auto earlierCalls = static_cast<size_t>(
	    std::count_if(scenario.commands.begin(), before, [](const Command& other) { return other.kind == Kind::CallNative; }));
	const auto fits = [&scenario, &command, earlierCalls](const ScriptArgument& argument) {
		switch (argument.kind)
		{
		case ScriptArgument::Kind::Number:
		case ScriptArgument::Kind::Integer:
		case ScriptArgument::Kind::Result:
		case ScriptArgument::Kind::LastResult:
		case ScriptArgument::Kind::Town:
			return true;
		case ScriptArgument::Kind::Marker:
		case ScriptArgument::Kind::Position:
			return ValidOffset(argument.point);
		case ScriptArgument::Kind::Object:
			return argument.index < scenario.objects.size();
		case ScriptArgument::Kind::Creature:
			return argument.index < scenario.creatures.size();
		case ScriptArgument::Kind::Temple:
			return argument.index < scenario.temples.size();
		case ScriptArgument::Kind::Flock:
			// The flock needs a villager or an animal of the scenario
			return std::ranges::any_of(scenario.objects, [](const ObjectSetup& object) {
				return std::holds_alternative<VillagerInfo>(object.type) || std::holds_alternative<AnimalInfo>(object.type);
			});
		case ScriptArgument::Kind::Made:
			return argument.index < earlierCalls;
		case ScriptArgument::Kind::Text:
			return command.kind == Command::Kind::CallNative && !argument.text.empty();
		}
		return false;
	};
	return !command.script.empty() && std::ranges::all_of(command.scriptArguments, fits) &&
	       std::ranges::all_of(command.scriptGlobals,
	                           [&fits](const auto& global) { return !global.first.empty() && fits(global.second); });
}

/// What is wrong with what a command acts on, if anything: the object it picks up must be of a kind a creature may pick
/// up (a thing, pot, animal, villager, static, tree or magic mushroom), what it knocks down a thing or a tree, the
/// part it strokes, the way it looks something over and the leash put on real ones
std::string_view CommandProblem(const Command& command, std::span<const ObjectSetup> objects)
{
	const auto* object = command.object < objects.size() ? &objects[command.object] : nullptr;
	switch (command.kind)
	{
	case Kind::PickUp:
		return object != nullptr &&
		               (std::holds_alternative<MobileObjectInfo>(object->type) ||
		                std::holds_alternative<AnimalInfo>(object->type) ||
		                std::holds_alternative<VillagerInfo>(object->type) || std::holds_alternative<PotInfo>(object->type) ||
		                std::holds_alternative<MobileStaticInfo>(object->type) ||
		                std::holds_alternative<TreeInfo>(object->type) ||
		                (std::holds_alternative<FeatureInfo>(object->type) &&
		                 std::get<FeatureInfo>(object->type) == FeatureInfo::MagicMushroom))
		           ? ""
		           : "picks up no thing";
	case Kind::KnockDown:
		return object != nullptr && !std::holds_alternative<FeatureInfo>(object->type) ? "" : "knocks down no thing or tree";
	case Kind::TieLeash:
		return object != nullptr ? "" : "ties the leash to nothing";
	case Kind::JoinFootball:
		return object != nullptr && std::holds_alternative<VillagerInfo>(object->type) ? ""
		                                                                               : "sends no villager to the football";
	case Kind::Examine:
		return command.value < creature_object_actions::k_KeepAnimationCount ? "" : "no such way of looking it over";
	case Kind::HandStroke:
		return command.bodyPart < creature_feedback::k_BodyPartCount ? "" : "no such part of the body";
	case Kind::HandSlap:
		return command.slapHeight > 0.0f && command.slapHeight < creature_feedback::k_SlapAbove ? "" : "slaps above it";
	case Kind::PutOnLeash:
		return command.leash == LeashType::Rope || command.leash == LeashType::Evil || command.leash == LeashType::Good
		           ? ""
		           : "no such leash";
	case Kind::ConfineToHome:
		return command.radius > 0.0f ? "" : "keeps it nowhere";
	case Kind::LeashKey:
		return command.value < 3 ? "" : "no such leash key";
	case Kind::HandTapLeash:
		return command.player < PlayerNames::_COUNT ? "" : "no such player";
	case Kind::FightBlow:
		return command.value < 3 && command.chargeMs >= 0.0f && command.chargeMs <= creature_fight::k_MaxChargeMs
		           ? ""
		           : "no such blow or charge";
	case Kind::FightStep:
		return command.value < 4 ? "" : "no such step";
	default:
		return "";
	}
}
} // namespace

bool testbed_scenarios::NeedsNoCreature(Command::Kind kind)
{
	return kind == Kind::SetHour || kind == Kind::RestoreDeviceInDomeFollow || kind == Kind::RestoreDevice ||
	       kind == Kind::PutLoadedArena || kind == Kind::HoldSeed || kind == Kind::DrawGesture || kind == Kind::SummonSeed ||
	       kind == Kind::PressKey || kind == Kind::HandTakeFireBall || kind == Kind::SetAlignment || kind == Kind::WideScreen ||
	       kind == Kind::SetInterfaceLevel || kind == Kind::FlyToBookmark || kind == Kind::TabToTown ||
	       kind == Kind::SetOpenClose || kind == Kind::LayGateStone || kind == Kind::HandTapObject || kind == Kind::Advisor ||
	       kind == Kind::RunText || kind == Kind::PlayHandDemo || kind == Kind::Reframe || kind == Kind::DestroyTemple ||
	       kind == Kind::RunHelpMessage || kind == Kind::EndGuidanceWaits || kind == Kind::SetTownBeliefInRival ||
	       kind == Kind::GiveLandTown || kind == Kind::RecordChallenge || kind == Kind::EnterTemple || kind == Kind::HandTake ||
	       kind == Kind::HandApply || kind == Kind::HandTakeShownSeed || kind == Kind::RunScript || kind == Kind::ClickScroll ||
	       kind == Kind::ClickCreatureAt || kind == Kind::CameraLooksAt || kind == Kind::HandTakeNear ||
	       kind == Kind::ClickCreatureNear || kind == Kind::TapRewardNear || kind == Kind::PlayBoatFilm ||
	       kind == Kind::HandPour || kind == Kind::HandRelease || kind == Kind::SetReportLatency ||
	       kind == Kind::RemoveObject || kind == Kind::ReadTip || kind == Kind::LookAtTempleScroll ||
	       kind == Kind::HandGripTotem || kind == Kind::ProfileFolderCheck || kind == Kind::PlayCustomMusic ||
	       kind == Kind::ShowCustomMusicControls || kind == Kind::OwnTattooPicture || kind == Kind::OpenTattooMenu ||
	       kind == Kind::OpenControlsBox || kind == Kind::OpenMenuPage || kind == Kind::PressMenuKey ||
	       kind == Kind::StopScript || kind == Kind::StartScriptMusic || kind == Kind::SaveGame || kind == Kind::LoadGame ||
	       kind == Kind::QuickSave || kind == Kind::ContinueGame || kind == Kind::PlayLogoEffect ||
	       kind == Kind::PlayFallingSpellFilm || kind == Kind::PlayIntroFilm || kind == Kind::SayLine ||
	       kind == Kind::PlayGuidanceVoice || kind == Kind::ScriptRelease || kind == Kind::FillSiteIcons ||
	       kind == Kind::ChargeIconWithSeedHeld || kind == Kind::TakeIconSeed || kind == Kind::TapSiteIcon ||
	       kind == Kind::TapTempleEntrance || kind == Kind::FinishBuilding || kind == Kind::FillBuiltAmount ||
	       kind == Kind::ScaffoldStartsSite || kind == Kind::DestroyScaffoldSite || kind == Kind::ScaffoldClearsGround ||
	       kind == Kind::CallNative || kind == Kind::PauseGame || kind == Kind::SetGameSpeed ||
	       kind == Kind::RemoveTeleportStone || kind == Kind::MakeDance || kind == Kind::OpenMultiplayerBox ||
	       kind == Kind::BuildWorshipSites || kind == Kind::ScriptBuildsWorshipSites || kind == Kind::PressPlainKey ||
	       kind == Kind::PlainKeyDown || kind == Kind::PlainKeyRepeat || kind == Kind::PlainKeyUp ||
	       kind == Kind::OtherGodHand || kind == Kind::ObserverCamera || kind == Kind::OwnGod || kind == Kind::JoinOnline ||
	       kind == Kind::EndBox || kind == Kind::BringCreature || kind == Kind::TattooEditorAsNetworkGame ||
	       kind == Kind::ShowDemoPing || kind == Kind::AddIncomingMessage || kind == Kind::Chat ||
	       kind == Kind::ComputerPersonality || kind == Kind::ComputerUntaughtSends || kind == Kind::SkirmishBox ||
	       kind == Kind::LookAtSaveGamePicture || kind == Kind::HandCastOnDispenser || kind == Kind::HandCastOnCreature ||
	       kind == Kind::CloseHeldMiracle || kind == Kind::JoinFootball || IsPointerCommand(kind);
}

std::string_view testbed_scenarios::Name(Facet facet)
{
	constexpr std::array<std::string_view, k_FacetCount> k_Names {
	    "Idle",       "Expressions", "Senses",    "Needs",         "Growth",      "Appearance", "Light",   "Movement",
	    "Footprints", "Audio",       "Objects",   "Hand",          "Leash",       "Combat",     "Mind",    "Particles",
	    "Editor",     "Miracles",    "Benchmark", "Creature Mode", "Physics",     "Nature",     "Animals", "Sky",
	    "Advisors",   "Story",       "Tutorial",  "Interface",     "Multiplayer",
	};
	return k_Names.at(static_cast<size_t>(facet));
}

std::string_view testbed_scenarios::Name(Weather weather)
{
	constexpr std::array<std::string_view, 5> k_Names {"clear", "rain", "thunderstorm", "snow", "blizzard"};
	return k_Names.at(static_cast<size_t>(weather));
}

std::string_view testbed_scenarios::Name(Shot shot)
{
	constexpr std::array<std::string_view, 5> k_Names {"testbed", "overview", "follow", "head", "placed"};
	return k_Names.at(static_cast<size_t>(shot));
}

std::string_view testbed_scenarios::Name(Command::Kind kind)
{
	constexpr std::array<std::string_view, 231> k_Names {
	    "walk to",
	    "run to",
	    "follow",
	    "flee from",
	    "turn to face",
	    "face the camera",
	    "stop",
	    "play action",
	    "gesture",
	    "pull face",
	    "draw a gesture",
	    "show feeling",
	    "sit down",
	    "stand up",
	    "sleep",
	    "wake",
	    "eat",
	    "drink",
	    "poo",
	    "fart",
	    "puke",
	    "faint",
	    "stroke",
	    "slap",
	    "set hour",
	    "pick up",
	    "put down",
	    "toss away",
	    "eat it",
	    "look it over",
	    "throw at",
	    "knock down",
	    "point at",
	    "hand strokes",
	    "hand slaps",
	    "hand lets go",
	    "hand takes hold",
	    "put on leash",
	    "tie leash to",
	    "tie leash to desire flag",
	    "tie leash to worship site flag",
	    "untie leash",
	    "take off leash",
	    "keep at home",
	    "make leashable",
	    "hand taps to leash",
	    "leash key",
	    "shake the hand",
	    "order to a point",
	    "order on the object",
	    "hang the temple's leashes",
	    "tap a temple leash",
	    "tap a building with the leash",
	    "start fight",
	    "fight blow",
	    "fight block",
	    "fight step",
	    "fight special",
	    "fight by itself",
	    "fight miracle",
	    "show the fight",
	    "pointer on opponent",
	    "pointer on the cave creature",
	    "fight autopilot",
	    "fight lesson check",
	    "knock out",
	    "bring round",
	    "abort fight",
	    "tie leash to creature",
	    "set desire",
	    "set stage",
	    "reward if",
	    "see skill",
	    "see miracle",
	    "player did",
	    "player did it to the object",
	    "press C",
	    "double click",
	    "camera keys",
	    "clear the view",
	    "give the camera back",
	    "saved follow camera",
	    "saved turning camera",
	    "saved free camera",
	    "saved camera below the land",
	    "press F5",
	    "tattoo",
	    "take tattoo off",
	    "open tattoo editor",
	    "hold seed",
	    "draw gesture",
	    "summon seed",
	    "know miracle",
	    "cast miracle",
	    "press key",
	    "hand takes fireball",
	    "pointer to",
	    "press button",
	    "let go of button",
	    "move mouse",
	    "turn wheel",
	    "set alignment",
	    "open or close",
	    "lay gate stone",
	    "cinema bars",
	    "set interface level",
	    "set fight lean",
	    "set miracle sightings",
	    "hand tap",
	    "tell an advisor",
	    "walk home",
	    "run a help message",
	    "end the guidance's waits",
	    "set town belief in rival",
	    "destroy temple",
	    "script play",
	    "run text",
	    "play hand demo",
	    "reframe",
	    "hand take",
	    "hand give",
	    "hand take shown seed",
	    "hand apply",
	    "swap minds",
	    "record challenge",
	    "enter temple",
	    "play boat film",
	    "run script",
	    "hand release",
	    "hand pour",
	    "do action",
	    "make friends",
	    "only desire",
	    "remove object",
	    "read tip",
	    "look at temple scroll",
	    "look at a save game picture",
	    "set alight",
	    "hand grips totem",
	    "pointer on altar",
	    "pointer on rope",
	    "pointer on object",
	    "set fizz",
	    "check the profile folder",
	    "give the player a picture of their own",
	    "open the tattoo editor from the menu",
	    "open the box of the controls page",
	    "open a page of the menu",
	    "press a key in the menu",
	    "play custom music",
	    "show the custom music controls",
	    "save the game",
	    "load the game",
	    "script reaction",
	    "script place",
	    "stop script",
	    "pointer on creature",
	    "start script music",
	    "write the web page",
	    "fly to a bookmark",
	    "press the Tab key",
	    "keep the creature in a bookmark and follow it",
	    "play the falling spell film",
	    "play the intro film",
	    "give hand glows",
	    "give a creed power",
	    "attach a script sound",
	    "attach a script sound to the object",
	    "say a line",
	    "play a guidance voice",
	    "stop at once",
	    "turn to face when still",
	    "script release",
	    "fill the worship site icons",
	    "quick save",
	    "continue the game",
	    "play the logo effect",
	    "charge an icon with its seed in the hand",
	    "finish the building",
	    "fill the built amount",
	    "start a site with the scaffold",
	    "destroy the site of the scaffold",
	    "clear the ground of the scaffold",
	    "call a script native",
	    "animal flees",
	    "hold the route search",
	    "pause the game",
	    "set the game's speed",
	    "force an action",
	    "remove the teleport stone",
	    "make a dance",
	    "open a multiplayer box",
	    "build the worship sites",
	    "press a key on the keyboard",
	    "show another god's hand",
	    "show an observer's camera",
	    "be the god of a player at this computer",
	    "press Join Online Game of the page",
	    "end box",
	    "bring a creature into a network game",
	    "give the hand's reports a latency",
	    "tattoo editor as in a network game",
	    "show a made-up ping",
	    "add an incoming message",
	    "chat",
	    "computer personality",
	    "computer sends with no lesson",
	    "skirmish box",
	    "put a key down",
	    "repeat a held key",
	    "let go of a key",
	    "a god orders its creature",
	    "take a seed from its icon",
	    "tap a worship site icon",
	    "change the body's look",
	    "tap the temple's entrance",
	    "give a town of the land to the player",
	    "cast the held miracle on a dispenser",
	    "cast the held miracle on a creature",
	    "close the held miracle and let go",
	    "click a scroll",
	    "put the camera to look at a point",
	    "join the football of the town",
	    "a script builds the worship sites",
	    "take the mobile object nearest a point into the hand",
	    "click the creature nearest a point",
	    "tap the reward chest nearest a point",
	    "click a creature",
	    "make the graphics device again while the dome follows the sky",
	    "make the graphics device again",
	    "put an arena of a loaded file between two creatures",
	};
	return k_Names.at(static_cast<size_t>(kind));
}

bool testbed_scenarios::IsPointerCommand(Command::Kind kind)
{
	return kind == Kind::PointerTo || kind == Kind::PointerPress || kind == Kind::PointerRelease ||
	       kind == Kind::PointerSweep || kind == Kind::WheelTurn || kind == Kind::PointerOnOpponent ||
	       kind == Kind::PointerOnCaveCreature || kind == Kind::PointerOnCreature || kind == Kind::PointerOnAltar ||
	       kind == Kind::PointerOnRope || kind == Kind::PointerOnObject;
}

bool NeedOverrides::Empty() const
{
	return !energy && !exhaustion && !dehydration && !poo && !life && !warmth && !age;
}

std::span<const Scenario> testbed_scenarios::All()
{
	static const std::vector<Scenario> k_All = Build();
	return k_All;
}

const Scenario* testbed_scenarios::Find(std::string_view id)
{
	const auto all = All();
	const auto found = std::ranges::find(all, id, &Scenario::id);
	return found != all.end() ? &*found : nullptr;
}

std::vector<std::string> testbed_scenarios::Problems(const Scenario& scenario)
{
	std::vector<std::string> problems;
	if (scenario.id.empty() || scenario.name.empty() || scenario.description.empty() || scenario.expected.empty())
	{
		problems.emplace_back("missing its id, name, description or what to expect");
	}
	if (static_cast<size_t>(scenario.facet) >= k_FacetCount)
	{
		problems.emplace_back("no such facet");
	}
	const auto& environment = scenario.environment;
	if (!InRange(environment.hour, 0.0f, k_HoursPerDay) || environment.bodyTimeScale < 0.0f)
	{
		problems.emplace_back("hour or body time out of range");
	}
	// A land other than the testbed brings its own things
	if (environment.land == Land::Testbed && scenario.creatures.empty() && scenario.particles.empty() &&
	    scenario.miracles.empty() && scenario.dispensers.empty() && scenario.birdFlocks.empty() && scenario.temples.empty() &&
	    !environment.dispenserGrid && !scenario.crowd.has_value() && !environment.playerAlignment.has_value() &&
	    scenario.throws.empty() && scenario.objects.empty() && scenario.vortices.empty() && scenario.rewards.empty() &&
	    scenario.fireflyRewards.empty() && scenario.landStorms.empty() && scenario.highlights.empty() &&
	    scenario.landScript.empty() &&
	    std::ranges::none_of(scenario.commands, [](const Command& command) { return NeedsNoCreature(command.kind); }))
	{
		problems.emplace_back("no creatures, things, particles, miracles, dispensers, birds, temples, vortices, reward "
		                      "chests, crowd, fireflies' rewards, land storms, scrolls or signs, land script lines, player's "
		                      "commands or alignment for the "
		                      "hand");
	}
	for (const auto& storm : scenario.landStorms)
	{
		if (!ValidOffset(storm.offset) || !ValidOffset(storm.destination) || storm.age < 0.0f || storm.cloudCount < 0)
		{
			problems.emplace_back("a land storm off the map, or with a negative age or cloud count");
		}
	}
	if ((environment.playerAlignment && !InRange(*environment.playerAlignment, -1.0f, 1.0f)) ||
	    (environment.cursor && (!InRange(environment.cursor->x, 0.0f, 1.0f) || !InRange(environment.cursor->y, 0.0f, 1.0f))))
	{
		problems.emplace_back("alignment or cursor out of range");
	}
	if (scenario.crowd.has_value() && (scenario.crowd->count == 0 || scenario.crowd->perFrame == 0))
	{
		problems.emplace_back("a crowd of no one, or spawned none a frame");
	}
	for (const auto& miracle : scenario.miracles)
	{
		const auto type = static_cast<size_t>(miracle.type);
		if (type == 0 || type >= k_Miracles)
		{
			problems.emplace_back("a miracle of no such magic type");
		}
		if (miracle.target == MiracleCast::Target::Creature && miracle.creature >= scenario.creatures.size())
		{
			problems.emplace_back("a miracle cast on a creature that isn't there");
		}
		if (!ValidOffset(miracle.point) || !ValidOffset(miracle.handOffset) || miracle.delaySeconds < 0.0f ||
		    miracle.holdSeconds.value_or(0.0f) < 0.0f || miracle.repeatSeconds.value_or(1.0f) <= 0.0f)
		{
			problems.emplace_back("a miracle's points or times out of range");
		}
	}
	for (const auto& dispenser : scenario.dispensers)
	{
		const auto type = static_cast<size_t>(dispenser.type);
		if (type == 0 || type >= k_Miracles || !ValidOffset(dispenser.offset))
		{
			problems.emplace_back("a dispenser of no such magic type, or off the map");
		}
	}
	for (const auto& particle : scenario.particles)
	{
		if (particle.file.empty() && particles::ParticleTypeFile(particle.type).empty())
		{
			problems.emplace_back("a particle type that has no file");
		}
		if (particle.targetsCreatures && scenario.creatures.empty())
		{
			problems.emplace_back("a particle effect given creatures to act on, without creatures");
		}
		if (particle.magnitude < 0.0f || particle.restartSeconds < 0.0f)
		{
			problems.emplace_back("a particle effect's magnitude or restart out of range");
		}
	}
	const auto creatures = scenario.creatures.size();
	// The commands can also name the creatures of the filled slots: one for each temple at most
	const auto others = creatures + (scenario.fillEmptySlots ? scenario.temples.size() : 0);
	const auto& framing = scenario.framing;
	if ((framing.shot == Shot::Follow || framing.shot == Shot::Head) && framing.creature >= creatures)
	{
		problems.emplace_back("the camera frames a creature that isn't there");
	}
	if (framing.distance <= 0.0f || !std::ranges::all_of(framing.include, ValidOffset))
	{
		problems.emplace_back("the framing's distance or points are out of range");
	}
	for (size_t i = 0; i < creatures; ++i)
	{
		const auto& creature = scenario.creatures.at(i);
		const auto who = fmt::format("creature {}", i);
		const auto species = static_cast<int32_t>(creature.species);
		if (species < 1 || species >= static_cast<int32_t>(CreatureType::_COUNT))
		{
			problems.push_back(fmt::format("{}: no such species", who));
		}
		if (static_cast<size_t>(creature.owner) >= static_cast<size_t>(PlayerNames::_COUNT))
		{
			problems.push_back(fmt::format("{}: no such owner", who));
		}
		if (!ValidOffset(creature.offset))
		{
			problems.push_back(fmt::format("{}: off the map", who));
		}
		if ((creature.alignment && !InRange(*creature.alignment, -1.0f, 1.0f)) ||
		    (creature.fatness && !InRange(*creature.fatness, 0.0f, 1.0f)) ||
		    (creature.strength && !InRange(*creature.strength, 0.0f, 1.0f)) ||
		    (creature.size && !InRange(*creature.size, 0.05f, 4.0f)))
		{
			problems.push_back(fmt::format("{}: body out of range", who));
		}
		if (!creature.mindFile.empty() && !creature.mindFile.starts_with("chosen:") && !creature.mindFile.starts_with("game:"))
		{
			problems.push_back(fmt::format("{}: a mind file is chosen: or game:", who));
		}
		if (creature.phase && *creature.phase > k_LastPhase)
		{
			problems.push_back(fmt::format("{}: no such stage of growing up", who));
		}
		CheckNeeds(creature.needs, problems, who);
		for (const auto& desire : creature.desires)
		{
			if (static_cast<size_t>(desire.desire) >= creature_desires::k_DesireCount || !InRange(desire.fraction, 0.0f, 1.0f))
			{
				problems.push_back(fmt::format("{}: desire out of range", who));
			}
		}
		for (const auto& tattoo : creature.tattoos)
		{
			if (tattoo.design >= creature_tattoo::k_DesignCount || tattoo.Empty())
			{
				problems.push_back(fmt::format("{}: no such tattoo design or site", who));
			}
		}
		if (creature.tattoos.size() > creature_tattoo::k_SlotCount)
		{
			problems.push_back(fmt::format("{}: more tattoos than slots", who));
		}
		for (const auto& mark : creature.wounds)
		{
			if (mark.type >= creature_marks::k_WoundLifetimes.size() || mark.column >= 8 || mark.skin >= 4)
			{
				problems.push_back(fmt::format("{}: no such wound", who));
			}
		}
	}
	for (const auto& object : scenario.objects)
	{
		const bool valid = std::visit(
		    []<typename T>(T type) {
			    const auto index = static_cast<int32_t>(type);
			    return index >= 0 && index < static_cast<int32_t>(T::_COUNT);
		    },
		    object.type);
		if (!valid || object.scale <= 0.0f || !ValidOffset(object.offset))
		{
			problems.emplace_back("an object of no such kind, size or place");
		}
		// The model of this kind of food pile is the grain at the side of a worship site. It lies about 50 m from the
		// point where the pile is, so you do not see the pile there. Use a different kind of food pile.
		if (const auto* pot = std::get_if<PotInfo>(&object.type); pot != nullptr && *pot == PotInfo::FoodPile)
		{
			problems.emplace_back("a food pile of the kind whose model is far from its point");
		}
		// On a land of the story, the scenario's town is a town of the testbed, not a town of the land. Name the land's
		// town with landTown. Only a land of the story has towns of its own.
		const bool storyLand = scenario.environment.land == Land::Story;
		if (object.joinTown && storyLand)
		{
			problems.emplace_back("an object that joins the scenario's town on a land of the story; use landTown");
		}
		if (object.landTown.has_value() && !storyLand)
		{
			problems.emplace_back("an object with a town of the land on a land that is not of the story");
		}
	}
	for (const auto& footpath : scenario.footpaths)
	{
		const auto outOfRange = [&scenario](size_t object) { return object >= scenario.objects.size(); };
		if ((footpath.startsAt.has_value() && outOfRange(*footpath.startsAt)) ||
		    std::ranges::any_of(footpath.leadsFrom, outOfRange) ||
		    !std::ranges::all_of(footpath.nodes, [](glm::vec2 node) { return ValidOffset(node); }) || footpath.nodes.empty() ||
		    footpath.leadsFrom.empty())
		{
			problems.emplace_back("a footpath with no nodes, a node off the map or an object that isn't there");
		}
	}
	for (const auto& throwing : scenario.throws)
	{
		const auto things = throwing.reward ? scenario.rewards.size() : scenario.objects.size();
		if (throwing.object >= things || (throwing.from.has_value() && !ValidOffset(*throwing.from)) || throwing.height < 0.0f)
		{
			problems.emplace_back("a throw of no such object, or from no such place");
		}
	}
	for (size_t i = 0; i < scenario.commands.size(); ++i)
	{
		const auto& command = scenario.commands.at(i);
		const auto what = fmt::format("command {} ({})", i, Name(command.kind));
		// An object set alight (value 1 or 2) needs no creature, but the object must be there
		const bool objectAlight = command.kind == Kind::SetAlight && (command.value == 1 || command.value == 2);
		if (objectAlight && command.object >= scenario.objects.size())
		{
			problems.push_back(fmt::format("{}: no such object", what));
		}
		if (!NeedsNoCreature(command.kind) && !objectAlight && command.creature >= creatures)
		{
			problems.push_back(fmt::format("{}: no such creature", what));
		}
		if (command.kind == Kind::SetAlignment && !InRange(command.alignment, -1.0f, 1.0f))
		{
			problems.push_back(fmt::format("{}: alignment out of range", what));
		}
		if ((command.kind == Kind::SetOpenClose && command.object >= scenario.objects.size()) ||
		    (command.kind == Kind::LayGateStone &&
		     (command.object >= scenario.objects.size() || command.value >= scenario.objects.size())))
		{
			problems.push_back(fmt::format("{}: no such object", what));
		}
		if ((command.kind == Kind::Follow || command.kind == Kind::StartFight || command.kind == Kind::TieLeashToCreature ||
		     command.kind == Kind::MakeFriends) &&
		    (command.value >= others || command.value == command.creature))
		{
			problems.push_back(fmt::format("{}: acts on no other creature", what));
		}
		if (!ValidAnimation(command.kind, command.value))
		{
			problems.push_back(fmt::format("{}: animation {} isn't of its kind", what, command.value));
		}
		if (const auto problem = CommandProblem(command, scenario.objects); !problem.empty())
		{
			problems.push_back(fmt::format("{}: {}", what, problem));
		}
		if ((command.kind == Kind::RunScript || command.kind == Kind::CallNative) && !ScriptArgumentsFit(command, scenario))
		{
			problems.push_back(fmt::format("{}: runs no script, or hands it what the scenario hasn't", what));
		}
		const bool offMapOrder = command.kind == Kind::LeashOrderAt && std::abs(command.point.x) <= k_MaxOrderOffset &&
		                         std::abs(command.point.y) <= k_MaxOrderOffset;
		if ((!ValidOffset(command.point) && !offMapOrder) || command.delaySeconds < 0.0f ||
		    !InRange(command.hour, 0.0f, k_HoursPerDay))
		{
			problems.push_back(fmt::format("{}: point, delay or hour out of range", what));
		}
		if ((command.kind == Kind::SetDesire &&
		     (command.value >= creature_desires::k_DesireCount || !InRange(command.amount, 0.0f, 1.0f))) ||
		    (command.kind == Kind::SetPhase && command.value > k_LastPhase) ||
		    (command.kind == Kind::SetFightLean && !InRange(command.amount, -1.0f, 1.0f)) ||
		    (command.kind == Kind::SetMiracleSightings && (command.value >= k_Miracles || command.amount < 0.0f)) ||
		    (command.kind == Kind::ShowFeeling && command.value >= creature_face::k_CueCount) ||
		    (command.kind == Kind::SeeSkill && command.value >= k_Skills) ||
		    ((command.kind == Kind::SeeMiracle || command.kind == Kind::KnowMiracle || command.kind == Kind::CastMiracle ||
		      command.kind == Kind::FightSpell) &&
		     command.value >= k_Miracles) ||
		    (command.kind == Kind::CastMiracle &&
		     (command.atCreature.has_value() ? *command.atCreature >= scenario.creatures.size()
		                                     : command.object >= scenario.objects.size())) ||
		    ((command.kind == Kind::PlayerDid || command.kind == Kind::PlayerDidOn) && command.value >= k_Deeds) ||
		    (command.kind == Kind::CameraKeys && (command.value >= 4 || command.amount <= 0.0f)) ||
		    (command.kind == Kind::PointerTo &&
		     (!InRange(command.point.x, 0.0f, 1.0f) || !InRange(command.point.y, 0.0f, 1.0f))) ||
		    ((command.kind == Kind::PointerPress || command.kind == Kind::PointerRelease) &&
		     (command.value < 1 || command.value > 3)) ||
		    (command.kind == Kind::PointerSweep && command.amount <= 0.0f) ||
		    (command.kind == Kind::OpenCreatureCave && command.value >= creature_cave::k_PageCount) ||
		    ((command.kind == Kind::ApplyTattoo || command.kind == Kind::RemoveTattoo) &&
		     (command.value >= creature_tattoo::k_DesignCount || command.bodyPart >= creature_tattoo::k_SlotCount)) ||
		    ((command.kind == Kind::HoldSeed || command.kind == Kind::SummonSeed) && command.value >= k_SeedCount) ||
		    (command.kind == Kind::DrawGesture && (command.value == 0 || command.value > k_LastGesture)))
		{
			problems.push_back(fmt::format("{}: value {} out of range", what, command.value));
		}
	}
	if (scenario.repeatFrom.has_value() && *scenario.repeatFrom >= scenario.commands.size())
	{
		problems.emplace_back("repeats from a command that isn't there");
	}
	return problems;
}

glm::vec2 testbed_scenarios::MapPoint(glm::vec2 middle, glm::vec2 offset)
{
	return middle + offset;
}

std::optional<uint32_t> testbed_scenarios::HelpTextNumber(std::span<const std::string> names, std::string_view name)
{
	const auto found = std::ranges::find(names, name);
	if (found == names.end())
	{
		return std::nullopt;
	}
	return static_cast<uint32_t>(std::distance(names.begin(), found));
}

float testbed_scenarios::CreatureHeight(float size)
{
	// A creature of size 1 stands about 15 units tall, whatever its species' mesh
	constexpr float k_HeightPerSize = 15.0f;
	return k_HeightPerSize * std::max(size, 0.05f);
}

CameraPlacement testbed_scenarios::Overview(glm::vec3 centre, glm::vec2 halfSize, glm::vec2 fieldOfView, float distanceFactor)
{
	// Looking down from the south at this angle, far enough back for the box's width to fit across the view and its
	// depth, foreshortened, up and down it, with a margin round it for the creatures' heights
	constexpr float k_Pitch = 0.65f;
	constexpr float k_MinDistance = 40.0f;
	constexpr float k_Margin = 25.0f;
	const auto halfAngles = glm::clamp(fieldOfView * 0.5f, 0.1f, 1.5f);
	// The near side of the box is closer to the camera than its middle, so its width takes a wider angle
	const auto across =
	    ((halfSize.x + k_Margin) / portable_math::Tan(halfAngles.x)) + ((halfSize.y + k_Margin) * portable_math::Cos(k_Pitch));
	const auto upAndDown = (halfSize.y + k_Margin) * portable_math::Sin(k_Pitch) / portable_math::Tan(halfAngles.y);
	const auto distance = distanceFactor * std::max({across, upAndDown, k_MinDistance});
	return {
	    .origin = centre + glm::vec3(0.0f, distance * portable_math::Sin(k_Pitch), -distance * portable_math::Cos(k_Pitch)),
	    .focus = centre,
	};
}

CameraPlacement testbed_scenarios::Follow(glm::vec3 position, float height, float distanceFactor)
{
	constexpr float k_MinDistance = 25.0f;
	const glm::vec3 focus = position + glm::vec3(0.0f, height * 0.5f, 0.0f);
	const auto distance = distanceFactor * std::max(height * 3.5f, k_MinDistance);
	return {.origin = focus + glm::vec3(0.0f, distance * 0.45f, -distance * 0.9f), .focus = focus};
}

CameraPlacement testbed_scenarios::Head(glm::vec3 position, glm::vec2 ahead, float height, float distanceFactor)
{
	const auto length = glm::length(ahead);
	const auto forward = length > 0.0f ? ahead / length : glm::vec2(0.0f, -1.0f);
	const glm::vec3 forward3 {forward.x, 0.0f, forward.y};
	// The head is high on the body and a little forward of its middle
	const glm::vec3 focus = position + glm::vec3(0.0f, height * 0.75f, 0.0f) + (forward3 * (height * 0.3f));
	const auto distance = distanceFactor * height;
	return {.origin = focus + (forward3 * distance) + glm::vec3(0.0f, height * 0.15f, 0.0f), .focus = focus};
}

Bounds testbed_scenarios::BoundsOf(std::span<const glm::vec2> points, glm::vec2 minimumHalfSize)
{
	if (points.empty())
	{
		return {.centre = glm::vec2(0.0f), .halfSize = minimumHalfSize};
	}
	auto low = points.front();
	auto high = points.front();
	for (const auto& point : points)
	{
		low = glm::min(low, point);
		high = glm::max(high, point);
	}
	return {.centre = (low + high) * 0.5f, .halfSize = glm::max((high - low) * 0.5f, minimumHalfSize)};
}

void testbed_scenarios::Apply(const NeedOverrides& overrides, creature_physiology::Needs& needs, float size)
{
	const auto set = [](float& value, const std::optional<float>& wanted, float low, float high) {
		if (wanted.has_value())
		{
			value = std::clamp(*wanted, low, high);
		}
	};
	set(needs.energy, overrides.energy, 0.0f, std::max(1.0f, size));
	set(needs.exhaustion, overrides.exhaustion, 0.0f, 1.0f);
	set(needs.dehydration, overrides.dehydration, 0.0f, 1.0f);
	set(needs.poo, overrides.poo, 0.0f, 1.0f);
	set(needs.life, overrides.life, 0.0f, 1.0f);
	set(needs.warmth, overrides.warmth, -1.0f, 1.0f);
	if (overrides.age.has_value())
	{
		needs.age = *overrides.age;
	}
}

void testbed_scenarios::Apply(std::span<const DesireOverride> overrides, creature_desires::Desires& desires)
{
	for (const auto& override : overrides)
	{
		auto& state = desires[override.desire];
		state.activated = true;
		state.value = std::clamp(override.fraction, 0.0f, 1.0f) * std::max(state.max, 0.0f);
	}
}

std::vector<size_t> testbed_scenarios::GivesAt(const std::map<uint32_t, std::vector<size_t>>& gives, uint32_t turn,
                                               size_t commands)
{
	std::vector<size_t> given;
	if (const auto found = gives.find(turn); found != gives.end())
	{
		std::ranges::copy_if(found->second, std::back_inserter(given), [commands](size_t place) { return place < commands; });
	}
	return given;
}

std::vector<size_t> testbed_scenarios::Advance(Timeline& timeline, std::span<const Command> commands,
                                               std::optional<size_t> repeatFrom, float seconds,
                                               const std::function<bool(const Command& command)>& isFree)
{
	std::vector<size_t> due;
	if (commands.empty() || timeline.next >= commands.size())
	{
		timeline.done = true;
	}
	if (timeline.done)
	{
		return due;
	}
	timeline.seconds += seconds;
	timeline.sinceGiven += seconds;
	// At most a round of the commands at once, so commands that go round again without waiting can't run away
	while (!timeline.done && due.size() < commands.size())
	{
		const auto& command = commands[timeline.next];
		if (command.waitUntilFree && !timeline.freed)
		{
			if (timeline.sinceGiven < k_SettleSeconds || !isFree(command))
			{
				// Its delay counts from when the creature is free
				timeline.seconds = 0.0f;
				break;
			}
			timeline.freed = true;
		}
		if (timeline.seconds < command.delaySeconds)
		{
			break;
		}
		due.push_back(timeline.next);
		timeline.seconds = 0.0f;
		timeline.sinceGiven = 0.0f;
		timeline.freed = false;
		if (++timeline.next >= commands.size())
		{
			if (repeatFrom.has_value() && *repeatFrom < commands.size())
			{
				timeline.next = *repeatFrom;
			}
			else
			{
				timeline.done = true;
			}
		}
	}
	return due;
}

bool testbed_scenarios::KeepsDispenserGrid(const Scenario& scenario)
{
	if (!scenario.environment.dispenserGrid)
	{
		return false;
	}
	const auto onGrid = [](const auto& setup) { return testbed_dispensers::InGridArea(setup.offset); };
	return std::ranges::none_of(scenario.creatures, onGrid) && std::ranges::none_of(scenario.objects, onGrid);
}
