/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The testbed's scenarios of the gestures drawn with the hand: each gesture is drawn as a path through the same
// recogniser the cursor goes through, so the debug window's gesture overlay shows it being drawn and matched

#include <utility>

#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

namespace
{
using Kind = Command::Kind;

/// A gesture takes a second or two to draw; the next command waits for it and the hand's rest after it
constexpr float k_DrawSeconds = 3.0f;

Command HoldSeed(SpellSeedType seed, float delaySeconds)
{
	return {.kind = Kind::HoldSeed, .delaySeconds = delaySeconds, .value = static_cast<size_t>(seed)};
}

/// A command to the hand that waits until the hand has finished drawing, its delay counting from then
Command AfterDrawing(Command command)
{
	command.waitUntilFree = true;
	return command;
}

Command Draw(GestureType gesture, float delaySeconds = k_DrawSeconds)
{
	return {.kind = Kind::DrawGesture, .delaySeconds = delaySeconds, .value = static_cast<size_t>(gesture)};
}

CreatureSetup Tiger()
{
	return {.label = "your tiger",
	        .species = CreatureType::Tiger,
	        .offset = {0.0f, 50.0f},
	        .facingDegrees = 0.0f,
	        .needs = {.energy = 1.0f, .exhaustion = 0.0f, .dehydration = 0.0f, .poo = 0.0f, .life = 1.0f},
	        .hold = true};
}
} // namespace

void testbed_scenarios::AddGestureScenarios(std::vector<Scenario>& all)
{
	all.push_back({
	    .id = "miracles.script_plays_gesture",
	    .name = "A script shows a gesture on the land",
	    .facet = Facet::Miracles,
	    .description = "After two seconds a script calls PLAY_GESTURE with the star gesture, a corner at -15, -15 "
	                   "metres from the middle of the testbed and a size of 30 metres.",
	    .expected = "The trail of the star gesture shows on the land, as the trail of a gesture that the hand drew and "
	                "the game knew. Its square is 30 metres across and starts at the corner, towards the east and the "
	                "south. The trail follows the land and then fades. No miracle starts and no hand moves.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Testbed},
	    .commands = {{.kind = Kind::CallNative,
	                  .delaySeconds = 2.0f,
	                  .script = "PLAY_GESTURE",
	                  .scriptArguments = {{.kind = ScriptArgument::Kind::Integer,
	                                       .number = static_cast<float>(GestureType::Star)},
	                                      {.kind = ScriptArgument::Kind::Position, .point = {-15.0f, -15.0f}},
	                                      {.kind = ScriptArgument::Kind::Number, .number = 30.0f}}}},
	});

	all.push_back({
	    .id = "miracles.gesture_circle_shield",
	    .name = "Size a shield with a circle",
	    .facet = Facet::Miracles,
	    .description = "A shield seed is put in the hand and a circle is drawn round the middle of the screen with the "
	                   "Action button held, which readies the shield. Open the "
	                   "Gestures debug window to see the path and the template it matches. Hold a shield or storm seed "
	                   "and draw a circle with the right button held to try it.",
	    .expected = "The circle is recognised: the log gives its middle on the land and its radius across the land, "
	                "and the miracles take it for the shield, which is cast there at that size on letting go. It is "
	                "remembered for five seconds. The hand draws the circle: while it holds the shield seed a faint "
	                "ribbon in the player's colour trails behind it, each part fading and narrowing away over two "
	                "seconds. On recognition the gesture's sound plays and its trail appears on the land: 234 small "
	                "glows in the player's colour grow from both ends of the drawn path over 0.6 seconds while flowing "
	                "onto a true circle laid on the land, raised a little towards the camera, dim and wiggling; a sheet "
	                "of stars in the player's colour rises along the circle in a wave, tallest two seconds in and gone "
	                "by four; at 2.4 seconds the glows flash to full brightness and the hand glows the player's colour "
	                "for half a second, the flash dying away by 4.5 seconds; from three seconds the wiggle shrinks away "
	                "over two seconds and the trail goes at seven.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Testbed},
	    .commands = {HoldSeed(SpellSeedType::Shield, 1.0f), Draw(GestureType::Circle, 1.0f)},
	    .repeatFrom = 0,
	});

	all.push_back({
	    .id = "miracles.gesture_circle_shield_to_side",
	    .name = "Size a shield with a circle drawn well to one side of the screen",
	    .facet = Facet::Miracles,
	    .description = "A shield seed is put in the hand and a circle is drawn with the Action button held, with its "
	                   "middle a third of the screen's width to the right of the screen's middle. The hand lets go when "
	                   "the circle is recognised, and the shield is cast. The scenario repeats.",
	    .expected = "The circle is recognised and the shield is cast on the land at the circle's middle. The radius "
	                "is the distance across the land that the drawn edge has from the middle, measured as the game "
	                "does, from the eye. A circle far from the middle of the screen thus gives a smaller shield than a "
	                "measure along the view would give, by about 8 percent at 25 degrees from the middle.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Testbed},
	    .commands = {HoldSeed(SpellSeedType::Shield, 1.0f),
	                 {.kind = Kind::DrawGesture,
	                  .delaySeconds = 1.0f,
	                  .point = {0.3f, 0.0f},
	                  .value = static_cast<size_t>(GestureType::Circle)}},
	    .repeatFrom = 0,
	});

	all.push_back({
	    .id = "miracles.gesture_power_up_storm",
	    .name = "Power up a storm",
	    .facet = Facet::Miracles,
	    .description = "The worship town of the player, with the storm, the extreme storm and the tornado. A tap puts the "
	                   "storm from the icon of the worship site into the hand (only a seed from an icon can be powered "
	                   "up), and its first power-up gesture, the inverse spiral, is drawn, then its second, the spiral.",
	    .expected =
	        "Each spiral is recognised as a power-up, level 0 then level 1, and the storm icon charges the seed to it "
	        "(the band and the announcer's voice). A circle drawn with the Action button held would size the storm instead.",
	    .environment = {.dispenserGrid = false, .landNumber = k_WorshipTownLand},
	    .framing = {.shot = Shot::Testbed},
	    .objects = WorshipTown(),
	    .temples = {{.offset = k_WorshipTemple}},
	    .commands = {{.kind = Kind::BuildWorshipSites, .delaySeconds = 1.0f},
	                 {.kind = Kind::TakeIconSeed,
	                  .delaySeconds = k_DrawSeconds,
	                  .value = static_cast<size_t>(SpellSeedType::Storm),
	                  .amount = 1.0f},
	                 Draw(GestureType::InverseSpiral, 1.0f),
	                 Draw(GestureType::Spiral)},
	    .repeatFrom = 1,
	    .townBeliefInPlayer = k_WorshipTownBelief,
	    .townMiracles = {MagicType::StormWindRain, MagicType::StormWindRainLightning, MagicType::Tornado},
	});

	all.push_back({
	    .id = "interface.hint_power_up_rings",
	    .name = "The seeds and rings over the hints of the power-ups",
	    .facet = Facet::Interface,
	    .description = "The worship town of the player, with the storm, the extreme storm and the tornado. A tap puts the "
	                   "storm from the icon of the worship site into the hand, and the hand waits. Then the hand draws "
	                   "the inverse spiral, the first power-up.",
	    .expected = "While the seed is in the hand, the hints show the inverse spiral and the spiral. Over each, the "
	                "storm's seed turns slowly with the effect of its holder, and rings in the player's colour turn "
	                "round it: one ring over the inverse spiral, two over the spiral. After the inverse spiral, its hint "
	                "goes, and the scribble shows the storm's seed without rings.",
	    .environment = {.dispenserGrid = false, .landNumber = k_WorshipTownLand},
	    .framing = {.shot = Shot::Testbed},
	    .objects = WorshipTown(),
	    .temples = {{.offset = k_WorshipTemple}},
	    .commands = {{.kind = Kind::BuildWorshipSites, .delaySeconds = 1.0f},
	                 {.kind = Kind::TakeIconSeed,
	                  .delaySeconds = k_DrawSeconds,
	                  .value = static_cast<size_t>(SpellSeedType::Storm),
	                  .amount = 1.0f},
	                 Draw(GestureType::InverseSpiral, 6.0f)},
	    .townBeliefInPlayer = k_WorshipTownBelief,
	    .townMiracles = {MagicType::StormWindRain, MagicType::StormWindRainLightning, MagicType::Tornado},
	});

	all.push_back({
	    .id = "hand.tooltips_held_miracle",
	    .name = "The name of the miracle in the hand",
	    .facet = Facet::Hand,
	    .description = "The worship town of the player, with the storm, the extreme storm and the tornado. A tap puts the "
	                   "storm from the icon of the worship site into the hand. The hand draws the inverse spiral, then the "
	                   "spiral.",
	    .expected = "When the seed comes into the hand, the tooltip \"Storm Miracle\" shows at once. When the storm icon has "
	                "charged the seed after the inverse spiral, \"Storm Miracle Increase\" shows, and after the spiral "
	                "\"Storm Miracle Extreme\". Each name shows with no mouse button, before the other tooltips of the hand "
	                "come back.",
	    .environment = {.dispenserGrid = false, .landNumber = k_WorshipTownLand},
	    .framing = {.shot = Shot::Testbed},
	    .objects = WorshipTown(),
	    .temples = {{.offset = k_WorshipTemple}},
	    .commands = {{.kind = Kind::BuildWorshipSites, .delaySeconds = 1.0f},
	                 {.kind = Kind::TakeIconSeed,
	                  .delaySeconds = k_DrawSeconds,
	                  .value = static_cast<size_t>(SpellSeedType::Storm),
	                  .amount = 1.0f},
	                 Draw(GestureType::InverseSpiral, 1.0f),
	                 Draw(GestureType::Spiral)},
	    .townBeliefInPlayer = k_WorshipTownBelief,
	    .townMiracles = {MagicType::StormWindRain, MagicType::StormWindRainLightning, MagicType::Tornado},
	});

	all.push_back({
	    .id = "miracles.gesture_scribble_cancel",
	    .name = "Cancel a held miracle with a scribble",
	    .facet = Facet::Miracles,
	    .description = "A fireball seed is put in the hand and a scribble is drawn: quick strokes from side to side.",
	    .expected = "The scribble is recognised; the miracles drop the seed back where it came from, refunding its "
	                "prayer power (or call off a power-up asked for first).",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Testbed},
	    // The next seed waits until the scribble has been drawn to its end
	    .commands = {AfterDrawing(HoldSeed(SpellSeedType::Fire, 1.0f)), Draw(GestureType::Scribble, 1.0f)},
	    .repeatFrom = 0,
	});

	all.push_back({
	    .id = "statistics.help_gestures",
	    .name = "The help record counts the gestures and the hand",
	    .facet = Facet::Miracles,
	    .description = "The worship town of the player, with the storm, the extreme storm and the tornado. A tap puts the "
	                   "storm from the icon of the worship site into the hand. The hand draws the inverse spiral, then a "
	                   "scribble, then a second scribble. Then pick up a thing with the hand and throw it. Read the counts "
	                   "with the inspector (help.profile), or open the Tech Stats tab of the Game Statistics box.",
	    .expected = "The spiral asks the storm icon for the power-up and counts one power-up (event 18). The first "
	                "scribble calls the power-up off: the icon takes the seed back to its plain storm, the seed stays in "
	                "the hand, and the scribble counts event 20. The second scribble drops "
	                "the seed and counts event 22. Each gesture also counts in Gestures Done (event 24). Each pick-up "
	                "counts one in Total Things Picked Up, and each throw or drop one in Total Objects Thrown.",
	    .environment = {.dispenserGrid = false, .landNumber = k_WorshipTownLand},
	    .framing = {.shot = Shot::Testbed},
	    .objects = WorshipTown(),
	    .temples = {{.offset = k_WorshipTemple}},
	    .commands = {{.kind = Kind::BuildWorshipSites, .delaySeconds = 1.0f},
	                 {.kind = Kind::TakeIconSeed,
	                  .delaySeconds = k_DrawSeconds,
	                  .value = static_cast<size_t>(SpellSeedType::Storm),
	                  .amount = 1.0f},
	                 Draw(GestureType::InverseSpiral, 1.0f),
	                 Draw(GestureType::Scribble),
	                 Draw(GestureType::Scribble)},
	    .townBeliefInPlayer = k_WorshipTownBelief,
	    .townMiracles = {MagicType::StormWindRain, MagicType::StormWindRainLightning, MagicType::Tornado},
	});

	all.push_back({
	    .id = "miracles.gesture_sounds",
	    .name = "The sounds of drawing gestures",
	    .facet = Facet::Miracles,
	    .description = "A shield seed is put in the hand and a circle is drawn round the middle of the screen with the "
	                   "Action button held; then a tap puts the storm from the icon of the worship site of the player's "
	                   "worship town into the hand, its two power-up gestures are drawn (the inverse spiral, then the "
	                   "spiral) and a scribble calls the power-up off. The failure on letting go of a storm with no circle is "
	                   "in miracles.hand_circle_casts, the leash "
	                   "gesture's in leash.gesture_picker.",
	    .expected = "While the circle is drawn with the Action button held, the hand hums (the gesture loop, heard from "
	                "the hand) until it lets go and the shield is cast. Each recognised gesture but the scribble "
	                "plays the recognition chime, heard without position, as its trail appears. Each power-up adds "
	                "the band's sound and the announcer's power-up voice (first, then second). The scribble has no "
	                "chime: the hand shakes with the shake sound. Drawing without the Action button makes no sound of "
	                "its own.",
	    .environment = {.dispenserGrid = false, .landNumber = k_WorshipTownLand},
	    .framing = {.shot = Shot::Testbed},
	    .objects = WorshipTown(),
	    .temples = {{.offset = k_WorshipTemple}},
	    .commands = {{.kind = Kind::BuildWorshipSites, .delaySeconds = 1.0f},
	                 AfterDrawing(HoldSeed(SpellSeedType::Shield, 1.0f)),
	                 Draw(GestureType::Circle, 1.0f),
	                 AfterDrawing({.kind = Kind::TakeIconSeed,
	                               .delaySeconds = k_DrawSeconds,
	                               .value = static_cast<size_t>(SpellSeedType::Storm),
	                               .amount = 1.0f}),
	                 Draw(GestureType::InverseSpiral, 1.0f),
	                 Draw(GestureType::Spiral),
	                 Draw(GestureType::Scribble)},
	    .repeatFrom = 1,
	    // The hand held over the middle of the land, inside the player's influence, where the circle is drawn
	    .hand = HandHold {.offset = {0.0f, 0.0f}, .height = 6.0f},
	    .townBeliefInPlayer = k_WorshipTownBelief,
	    .townMiracles = {MagicType::StormWindRain, MagicType::StormWindRainLightning, MagicType::Tornado},
	});

	all.push_back({
	    .id = "leash.gesture_picker",
	    .name = "The leash gesture and the leash picker",
	    .facet = Facet::Leash,
	    .description = "Your tiger is taught all three leashes, its leash is taken off, and the leash gesture (a square "
	                   "spiral) is drawn. With the picker open the learning leash's gesture (an E) is drawn; then the "
	                   "leash gesture again and the aggression leash's (a vertical scribble); then a scribble with the "
	                   "empty hand.",
	    .expected = "The square spiral puts the picked leash on and opens the picker, as the tiger knows more than one "
	                "leash. The E changes it to the learning rope and closes the picker. The square spiral opens the "
	                "picker again, the vertical scribble changes to the aggression leash, and the scribble shakes it "
	                "off. The picker closes by itself after 25 seconds.",
	    .framing = {.shot = Shot::Testbed},
	    .creatures = {Tiger()},
	    .commands = {{.kind = Kind::PutOnLeash, .creature = 0, .delaySeconds = 1.0f, .leash = LeashType::Evil},
	                 {.kind = Kind::PutOnLeash, .creature = 0, .delaySeconds = 1.0f, .leash = LeashType::Good},
	                 {.kind = Kind::TakeOffLeash, .creature = 0, .delaySeconds = 1.0f},
	                 Draw(GestureType::SquareSpirial, 1.0f),
	                 Draw(GestureType::EShape),
	                 Draw(GestureType::SquareSpirial),
	                 Draw(GestureType::VerticalScribble),
	                 Draw(GestureType::Scribble)},
	    .repeatFrom = 0,
	});

	all.push_back({
	    .id = "leash.gesture_scribbles",
	    .name = "Scribbles with the leash picker open",
	    .facet = Facet::Leash,
	    .description = "Your tiger, which knows every leash, has its rope taken off. The leash gesture (a square spiral) "
	                   "is drawn, then a scribble with the empty hand, then another.",
	    .expected = "The square spiral puts the rope on and opens the picker. The first scribble only closes the picker, "
	                "as in the game, and the rope stays on; the second shakes it off.",
	    .framing = {.shot = Shot::Testbed},
	    .creatures = {Tiger()},
	    // Each repeat waits until the hand has drawn the last scribble
	    .commands = {AfterDrawing({.kind = Kind::PutOnLeash, .creature = 0, .delaySeconds = 1.0f, .leash = LeashType::Rope}),
	                 {.kind = Kind::TakeOffLeash, .creature = 0, .delaySeconds = 1.0f},
	                 Draw(GestureType::SquareSpirial, 1.0f),
	                 Draw(GestureType::Scribble),
	                 // After the rest that follows the first scribble
	                 Draw(GestureType::Scribble, 4.0f)},
	    .repeatFrom = 0,
	});

	all.push_back({
	    .id = "leash.picker_hand_effect",
	    .name = "The hand's effect while the leash picker is open",
	    .facet = Facet::Leash,
	    .description = "Your tiger knows every leash and its leash comes off. The hand draws the leash gesture (a square "
	                   "spiral), which puts the leash on and opens the leash picker. Eight seconds later the hand draws "
	                   "a scribble.",
	    .expected = "While the picker is open, small white sparks come out of the hand and fade in half a second. The "
	                "scribble closes the picker: the shake sound plays, a band flies off the hand to the camera, and the "
	                "sparks stop. The leash stays on.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Testbed},
	    .creatures = {Tiger()},
	    .commands = {{.kind = Kind::PutOnLeash, .creature = 0, .delaySeconds = 1.0f, .leash = LeashType::Evil},
	                 {.kind = Kind::PutOnLeash, .creature = 0, .delaySeconds = 1.0f, .leash = LeashType::Good},
	                 {.kind = Kind::TakeOffLeash, .creature = 0, .delaySeconds = 1.0f},
	                 Draw(GestureType::SquareSpirial, 1.0f),
	                 Draw(GestureType::Scribble, 8.0f)},
	    // The hand held over the middle of the land, near the camera, where the sparks show
	    .hand = HandHold {.offset = {0.0f, 0.0f}, .height = 6.0f},
	});

	all.push_back({
	    .id = "miracles.gesture_chain_any_seed",
	    .name = "The chain behind a hand that holds a miracle",
	    .facet = Facet::Miracles,
	    .description = "A water miracle is put in the hand, as from a globe: it can not be powered up and no circle sizes "
	                   "it. Two seconds later the hand draws a circle across the middle of the screen.",
	    .expected = "When the miracle comes into the hand, and again after its first turn there, the hand starts its "
	                "path again. From then on, the faint chain in the player's colour trails behind the hand while it "
	                "holds the miracle, also while the hand draws the circle. The circle does nothing, because no "
	                "miracle in the hand waits for it.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Testbed},
	    .commands = {HoldSeed(SpellSeedType::Water, 1.0f), Draw(GestureType::Circle, 2.0f)},
	    .repeatFrom = 0,
	});
	// The worship town stands where the camera of the gestures demonstration looks: the demonstration puts the camera
	// where its recording had it on the second land
	constexpr glm::vec2 k_DemoView {488.0f, 788.0f};
	auto demoTown = WorshipTown();
	for (auto& object : demoTown)
	{
		object.offset += k_DemoView;
	}
	all.push_back({
	    .id = "story.l2_gesture_demo",
	    .name = "Story: the hand demonstration of the miracle gestures",
	    .facet = Facet::Story,
	    .description = "The worship town of the player, counted as the second land, has the grain miracle. Its worship "
	                   "site gets the prayer power for two grain miracles, as the second land's gesture lesson gives it. "
	                   "Then the gestures hand demonstration of that lesson plays through the player's hand and camera. "
	                   "The camera goes where the recording had it, which looks at the town.",
	    .expected = "The hand draws a spiral, and the miracle selection opens. The hand draws the gestures of the grain "
	                "miracle: the grain comes from the worship site into the hand, with its sound and its effect in the "
	                "hand. Then the hand draws a scribble: the hand shakes, and the grain goes back to the site.",
	    .environment = {.dispenserGrid = false, .landNumber = 2},
	    .framing = {.shot = Shot::Testbed},
	    .objects = demoTown,
	    .temples = {{.offset = k_WorshipTemple + k_DemoView}},
	    .commands = {{.kind = Kind::BuildWorshipSites, .delaySeconds = 1.0f},
	                 // The tap fills the site's battery; the start of the demonstration stops the charge of the icon
	                 {.kind = Kind::TapSiteIcon,
	                  .delaySeconds = 1.0f,
	                  .value = static_cast<size_t>(SpellSeedType::Food),
	                  .amount = 2.0f},
	                 {.kind = Kind::PlayHandDemo, .delaySeconds = 0.1f, .handDemo = "Gestures"}},
	    .townBeliefInPlayer = k_WorshipTownBelief,
	    .townMiracles = {MagicType::Food},
	});
	// Where the shield demonstration of the second land looks: its camera stays still at its recorded place, which
	// looks at the hut of Khazar's shield challenge
	using Argument = ScriptArgument;
	constexpr glm::vec2 k_ShieldDemoView {1052.0f, 899.0f};
	constexpr float k_ShieldInfluenceRadius = 40.0f;
	constexpr float k_OneShotSpellInHand = 31.0f;
	constexpr float k_ShieldStrength = 7.0f;
	constexpr float k_SeedPowerProperty = 17.0f;
	all.push_back({
	    .id = "story.l2_shield_demo",
	    .name = "Story: Khazar's hand demonstration of the shield",
	    .facet = Facet::Story,
	    .description = "The start of the hand demonstration of Khazar's shield challenge on the second land. A circle of "
	                   "influence of radius 40 goes about the place of the hut, as the challenge makes it. The shield "
	                   "demonstration starts. Then a script puts a shield miracle in the hand with the strength of seven "
	                   "shields. The camera goes where the recording had it. The Magic debug window is open under the "
	                   "place where the recorded hand presses the Action button.",
	    .expected = "The hand holds the Action button and draws a circle about the place of the hut. When it lets go of "
	                "the button, the shield goes up there and the hand is empty. At the end of the demonstration the "
	                "hand holds no miracle.",
	    .environment = {.dispenserGrid = false, .landNumber = 2},
	    .framing = {.shot = Shot::Testbed},
	    .commands = {{.kind = Kind::CallNative,
	                  .delaySeconds = 1.0f,
	                  .script = "INFLUENCE_POSITION",
	                  .scriptArguments = {{.kind = Argument::Kind::Position, .point = k_ShieldDemoView},
	                                      {.number = k_ShieldInfluenceRadius},
	                                      {.number = 0.0f},
	                                      {.number = 0.0f}}},
	                 {.kind = Kind::PlayHandDemo, .delaySeconds = 0.5f, .handDemo = "shield"},
	                 {.kind = Kind::CallNative,
	                  .delaySeconds = 0.1f,
	                  .script = "CREATE",
	                  .scriptArguments = {{.kind = Argument::Kind::Integer, .number = k_OneShotSpellInHand},
	                                      {.kind = Argument::Kind::Integer,
	                                       .number = static_cast<float>(SpellSeedType::PhysicalShield)},
	                                      {.kind = Argument::Kind::Position, .point = {0.0f, 0.0f}}}},
	                 {.kind = Kind::CallNative,
	                  .delaySeconds = 0.1f,
	                  .script = "SET_PROPERTY",
	                  .scriptArguments = {{.kind = Argument::Kind::Integer, .number = k_SeedPowerProperty},
	                                      {.kind = Argument::Kind::LastResult},
	                                      {.number = k_ShieldStrength}}}},
	    .debugWindow = "Magic",
	    .checks = {{.label = "the hand holds the shield miracle at the start of the demonstration",
	                .quantity = checks::Quantity::HandMiracle,
	                .afterTurns = 25,
	                .atLeast = 1.0},
	               {.label = "the demonstration casts the shield",
	                .quantity = checks::Quantity::PhysicalShields,
	                .afterTurns = 25,
	                .byTurn = 200,
	                .atLeast = 1.0},
	               {.label = "the hand holds no miracle after the demonstration",
	                .quantity = checks::Quantity::HandMiracle,
	                .afterTurns = 200,
	                .atMost = 0.0}},
	});
	// The same demonstration on the second land of the story, where the land around the hut is not flat to the
	// horizon. On a wide screen a cursor that goes outside the screen's 4:3 middle points at other places.
	{
		auto onTheLand = all.back();
		onTheLand.id = "story.l2_shield_demo_land";
		onTheLand.name = "Story: Khazar's hand demonstration of the shield on the second land";
		onTheLand.description =
		    "The second land of the story. A circle of influence of radius 40 goes about the hut of Khazar's "
		    "shield challenge, as the challenge makes it. The shield demonstration starts. Then a script puts "
		    "a shield miracle in the hand with the strength of seven shields. The camera goes where the recording had "
		    "it. Use it with a window of any shape, such as 5120 by 1190.";
		onTheLand.expected = "The hand holds the Action button and draws a circle about the hut, as on a 4:3 screen. "
		                     "When it lets go of the button, the shield goes up over the hut and the hand is empty. At "
		                     "the end of the demonstration the hand holds no miracle.";
		onTheLand.environment = {.land = Land::Story, .storyLand = "Land2.txt", .dispenserGrid = false};
		// The ring of influence goes about the hut of the challenge, which stands this far from the middle of the land
		constexpr glm::vec2 k_HutOnTheLand {1379.57f, 737.72f};
		onTheLand.commands.front().scriptArguments.front().point = k_HutOnTheLand;
		onTheLand.debugWindow = {};
		all.push_back(std::move(onTheLand));
	}
	all.push_back({
	    .id = "miracles.hand_demo_shakes_off_seed",
	    .name = "A hand demonstration shakes the miracle off the hand",
	    .facet = Facet::Miracles,
	    .description = "A water miracle is put in the hand. Two seconds later the gestures hand demonstration starts. It "
	                   "starts as a script starts it, without the option that keeps the hand as it is.",
	    .expected = "The hand holds the water miracle, with its bracelet and glow. When the demonstration starts, the "
	                "miracle goes from the hand at once: the bracelet and the glow go. The hand does not shake: no shake "
	                "sound plays and no band flies off the hand. Then the hand draws the gestures of the demonstration.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Testbed},
	    .commands = {HoldSeed(SpellSeedType::Water, 1.0f),
	                 {.kind = Kind::PlayHandDemo, .delaySeconds = 2.0f, .handDemo = "Gestures"}},
	    .checks = {{.label = "the hand holds the miracle before the demonstration",
	                .quantity = checks::Quantity::HandMiracle,
	                .afterTurns = 20,
	                .atLeast = 1.0},
	               {.label = "the start of the demonstration shakes the miracle off the hand",
	                .quantity = checks::Quantity::HandMiracle,
	                .afterTurns = 40,
	                .atMost = 0.0}},
	});
	all.push_back({
	    .id = "story.hidden_phone_box",
	    .name = "The square wave starts the hidden phone box",
	    .facet = Facet::Story,
	    .description = "The land counts as Land 1 of the story. Your tiger learns the aggression and compassion leashes, "
	                   "and its leash comes off. The hand draws the leash gesture (a square spiral), which opens the "
	                   "leash picker. Then the hand draws the square wave.",
	    .expected = "The square wave shows no trail, makes no sound and shows no hint. The hidden script starts. The two "
	                "advisors appear and say four lines. After 20 seconds the phone plays its first messages. The script "
	                "makes the phone box at its fixed point on Land 1, which is in the sea of the testbed. To see the "
	                "box, do the same steps on Land 1.",
	    .environment = {.dispenserGrid = false, .landNumber = 1},
	    .framing = {.shot = Shot::Testbed},
	    .creatures = {Tiger()},
	    .commands = {{.kind = Kind::PutOnLeash, .creature = 0, .delaySeconds = 1.0f, .leash = LeashType::Evil},
	                 {.kind = Kind::PutOnLeash, .creature = 0, .delaySeconds = 1.0f, .leash = LeashType::Good},
	                 {.kind = Kind::TakeOffLeash, .creature = 0, .delaySeconds = 1.0f},
	                 Draw(GestureType::SquareSpirial, 1.0f),
	                 Draw(GestureType::SquareWave)},
	});
}
