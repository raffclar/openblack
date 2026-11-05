/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Testbed scenarios for the hand of the player.

#include "3D/TempleInteriorInterface.h"
#include "Creature/CreatureCave.h"
#include "Creature/CreatureFeedback.h"
#include "Creature/CreatureFight.h"
#include "Creature/CreatureIdleMind.h"
#include "TestbedScenarioBuilders.h"
#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

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
/// The player's tiger and a rival god's tiger, with different needs: the status panel shows only for a creature that a
/// hand holds
void AddStatusPanelOtherCreature(std::vector<Scenario>& all)
{
	auto own = Content(CreatureType::Tiger, {-15.0f, 0.0f}, 90.0f, "player's");
	own.needs = {.energy = 0.595f, .exhaustion = 0.325f, .life = 0.745f};
	auto rival = Content(CreatureType::Tiger, {15.0f, 0.0f}, 270.0f, "rival's");
	rival.owner = PlayerNames::PLAYER_TWO;
	rival.needs = {.energy = 0.9f, .exhaustion = 0.1f, .life = 0.5f};
	all.push_back({
	    .id = "hand.status_panel_other_creature",
	    .name = "No status panel over a creature that no hand holds",
	    .facet = Facet::Hand,
	    .description = "The player's tiger (25% damage, 40% hunger, 32% tiredness) and the tiger of a rival god (50% "
	                   "damage, 10% hunger, 10% tiredness) stand 30 apart. The hand strokes the player's tiger and lets "
	                   "go. Then move the hand over each tiger.",
	    .expected = "While the hand holds the player's tiger, the panel shows Damage 25%, Hunger 40% and Tiredness 32%. "
	                "When the tiger is still after the release, the panel goes. Over the rival's tiger and over the "
	                "player's tiger, no panel shows, because no hand holds them.",
	    .environment = {.dispenserGrid = false, .players = {PlayerNames::PLAYER_TWO}},
	    .framing = {.shot = Shot::Overview, .distance = 0.5f},
	    .creatures = {own, rival},
	    .commands = {Stroke(0, creature_feedback::BodyPart::Head, 1.0f), Act(Kind::HandLetGo, 0, 1.0f)},
	});
}
} // namespace

void testbed_scenarios::AddHand(std::vector<Scenario>& all)
{
	using creature_feedback::BodyPart;
	AddAlliedCreatureHold(all);
	AddStatusPanelOtherCreature(all);
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
	                "middle to Bad Boy! in red. After the release the panel shows until the tiger is still. Then "
	                "the hand over the tiger shows no panel.",
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
