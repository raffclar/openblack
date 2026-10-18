/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The testbed's scenarios of how the hand looks for its player's alignment: evil, neutral and good, and turning from
// one to the other. Also how the hand holds a thing across the screen, and how it hovers over the icons of miracles,
// over the help's text bubble, at the edge of the screen and while it offers a thing to a creature.

#include <cstddef>

#include <vector>

#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

namespace
{
using Kind = Command::Kind;

/// The hand hovers over the land in the middle of the window
constexpr glm::vec2 k_Cursor {0.5f, 0.5f};

Environment Looking(float alignment)
{
	return {.dispenserGrid = false, .playerAlignment = alignment, .cursor = k_Cursor};
}

/// Looking down at the middle of the map from close by, where the hand hovers
Framing Close()
{
	return {.shot = Shot::Overview, .include = {{0.0f, 0.0f}}, .distance = 0.2f};
}

Command Align(float alignment, float delay)
{
	return {.kind = Kind::SetAlignment, .delaySeconds = delay, .alignment = alignment};
}

/// The hand takes the scenario's object, is put at the middle of the screen, at its left side and at its right side,
/// sweeps across the screen and back, and throws the object away to the side
void HoldAcrossTheScreen(std::vector<Command>& commands, size_t object)
{
	constexpr float k_Rest = 2.0f;
	constexpr float k_SweepSeconds = 3.0f;
	commands.push_back({.kind = Kind::PointerTo, .delaySeconds = 1.0f, .point = {0.5f, 0.5f}});
	commands.push_back({.kind = Kind::HandTake, .delaySeconds = 0.5f, .object = object});
	commands.push_back({.kind = Kind::PointerTo, .delaySeconds = k_Rest, .point = {0.1f, 0.5f}});
	commands.push_back({.kind = Kind::PointerTo, .delaySeconds = k_Rest, .point = {0.9f, 0.5f}});
	commands.push_back({.kind = Kind::PointerTo, .delaySeconds = k_Rest, .point = {0.5f, 0.5f}});
	commands.push_back({.kind = Kind::PointerSweep, .delaySeconds = k_Rest, .point = {-0.4f, 0.0f}, .amount = 1.5f});
	commands.push_back({.kind = Kind::PointerSweep, .delaySeconds = 1.5f, .point = {0.8f, 0.0f}, .amount = k_SweepSeconds});
	commands.push_back({.kind = Kind::PointerSweep, .delaySeconds = k_SweepSeconds, .point = {-0.4f, 0.0f}, .amount = 1.5f});
	// Thrown far to the side, so that it does not stay in the view
	commands.push_back({.kind = Kind::HandRelease, .delaySeconds = 2.5f, .velocity = {-40.0f, 15.0f, 0.0f}});
}
} // namespace

void testbed_scenarios::AddHandLookScenarios(std::vector<Scenario>& all)
{
	all.push_back({
	    .id = "hand.look_evil",
	    .name = "The hand of an evil player",
	    .facet = Facet::Hand,
	    .description = "The player's alignment is set to -1, fully evil, with the hand over the land in the middle of the "
	                   "window.",
	    .expected = "The hand is dark red and veined, its fingers and knuckles ridged and spiked: wholly the evil hand's shape "
	                "and skin.",
	    .environment = Looking(-1.0f),
	    .framing = Close(),
	});

	all.push_back({
	    .id = "hand.look_neutral",
	    .name = "The hand of a neutral player",
	    .facet = Facet::Hand,
	    .description = "The player's alignment is set to 0, with the hand over the land in the middle of the window.",
	    .expected = "The hand is the plain orange-brown hand with smooth fingers: the base hand untouched.",
	    .environment = Looking(0.0f),
	    .framing = Close(),
	});

	all.push_back({
	    .id = "hand.look_good",
	    .name = "The hand of a good player",
	    .facet = Facet::Hand,
	    .description = "The player's alignment is set to 1, fully good, with the hand over the land in the middle of the "
	                   "window.",
	    .expected = "The hand is pale gold, the same smooth shape as the neutral hand: the good hand only changes its skin.",
	    .environment = Looking(1.0f),
	    .framing = Close(),
	});

	// From neutral, jumps to either end and back, then a slow sweep from evil to good and back again
	std::vector<Command> turning {Align(-1.0f, 3.0f), Align(0.0f, 3.0f),  Align(1.0f, 3.0f),
	                              Align(0.0f, 3.0f),  Align(-0.5f, 3.0f), Align(0.5f, 3.0f)};
	constexpr int k_SweepSteps = 200;
	constexpr float k_SweepDelay = 0.05f;
	for (int i = 0; i <= k_SweepSteps; ++i)
	{
		turning.push_back(Align(-1.0f + (2.0f * static_cast<float>(i) / k_SweepSteps), i == 0 ? 3.0f : k_SweepDelay));
	}
	for (int i = k_SweepSteps - 1; i >= 0; --i)
	{
		turning.push_back(Align(-1.0f + (2.0f * static_cast<float>(i) / k_SweepSteps), k_SweepDelay));
	}
	turning.push_back(Align(0.0f, 3.0f));
	all.push_back({
	    .id = "hand.look_turning",
	    .name = "The hand turning evil and good",
	    .facet = Facet::Hand,
	    .description = "Every three seconds the player's alignment jumps: to -1, 0, 1, 0, -0.5 and 0.5. Then it sweeps "
	                   "from -1 to 1 and back by a hundredth every twentieth of a second, and returns to 0.",
	    .expected = "Each jump changes the hand at once, with no fading: red and ridged at -1, plain at 0, pale gold at 1, "
	                "half way at -0.5 and 0.5. In the sweep the hand changes in steps of three or four hundredths, its "
	                "ridges smoothing away as it nears neutral and its skin turning from red through orange-brown to gold.",
	    .environment = Looking(0.0f),
	    .framing = Close(),
	    .commands = turning,
	    .repeatFrom = 0,
	});

	std::vector<Command> holding;
	for (size_t object = 0; object < 3; ++object)
	{
		HoldAcrossTheScreen(holding, object);
	}
	all.push_back({
	    .id = "hand.held_frame_heading",
	    .name = "A held rock, tree and villager at the middle and the sides of the screen",
	    .facet = Facet::Hand,
	    .description = "The hand takes a rock, then a birch, then a villager, which wait out of sight at the sides. With "
	                   "each one, the hand stays at the middle, at the left side and at the right side of "
	                   "the screen for two seconds. Then it moves slowly across the screen and back, and throws the "
	                   "thing away to the side.",
	    .expected = "The held thing turns with the hand about the vertical. It faces back along the line from the camera "
	                "through the hand, so at each side of the screen it turns away from the middle by the same angle as "
	                "the hand. At the middle of the screen it faces the camera as before.",
	    .environment = {.dispenserGrid = false, .cursor = glm::vec2 {0.5f, 0.5f}},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 9.0f, -14.0f}, .look = {0.0f, 0.0f, 0.0f}},
	    // Out of sight at the sides, so that the view behind the hand stays clear
	    .objects = {{.type = MobileStaticInfo::Rock, .offset = {-25.0f, 5.0f}},
	                {.type = TreeInfo::Birch, .offset = {25.0f, 5.0f}},
	                {.type = VillagerInfo::CelticFarmerMale, .offset = {-25.0f, 10.0f}, .inScript = true}},
	    .commands = holding,
	    .repeatFrom = 0,
	});

	// The hand over the stone hands of a village centre: it rests nearer than each icon by its own height and opens
	constexpr glm::vec2 k_Centre {0.0f, 40.0f};
	constexpr float k_IconRest = 3.0f;
	all.push_back({
	    .id = "hand.icon_rest",
	    .name = "The hand over the stone hands of a village centre",
	    .facet = Facet::Hand,
	    .description = "A Norse town of the player on the first land, given the heal and the water miracles, so that "
	                   "its village centre shows two stone hands. The pointer moves onto the left stone hand, onto the "
	                   "land before the centre, onto the right stone hand and back to the land, three seconds at each.",
	    .expected = "On a stone hand, the hand does not go into the icon: it stays nearer to the camera than the icon by "
	                "its own height, turns to the slope of the land, and opens its fingers to take the miracle. On the "
	                "land it hangs over the land in its ordinary pose.",
	    .environment = {.dispenserGrid = false, .landNumber = 1},
	    .framing = {.shot = Shot::Placed, .eye = {-28.0f, 9.5f, 37.0f}, .look = {-11.0f, 1.5f, 37.0f}},
	    .objects = {{.type = AbodeInfo::NorseTownCentre, .offset = k_Centre},
	                {.type = VillagerInfo::NorseFarmerMale, .offset = k_Centre + glm::vec2(5.0f, -15.0f), .joinTown = true}},
	    .temples = {{.offset = {0.0f, -60.0f}}},
	    .commands = {{.kind = Kind::PointerTo, .delaySeconds = 2.0f, .point = {0.267f, 0.393f}},
	                 {.kind = Kind::PointerTo, .delaySeconds = k_IconRest, .point = {0.25f, 0.8f}},
	                 {.kind = Kind::PointerTo, .delaySeconds = k_IconRest, .point = {0.661f, 0.358f}},
	                 {.kind = Kind::PointerTo, .delaySeconds = k_IconRest, .point = {0.75f, 0.8f}}},
	    .repeatFrom = 0,
	    .townBeliefInPlayer = 1000.0f,
	    .townMiracles = {MagicType::Heal, MagicType::Water},
	});

	// At the edge of the screen the empty hand offers to turn the camera and stands up towards where the camera looks
	all.push_back({
	    .id = "hand.focus_lean",
	    .name = "The hand standing towards the camera's focus at the edge of the screen",
	    .facet = Facet::Hand,
	    .description = "The pointer rests on open land in the middle of the screen, then at the right edge, then at the "
	                   "left edge, three seconds at each.",
	    .expected = "At each edge the hand shows the turning pose and stands up towards the point the camera looks at, "
	                "leaning up by three tenths of the distance to that point. In the middle it hangs over the land.",
	    .environment = {.dispenserGrid = false, .cursor = k_Cursor},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 9.0f, -14.0f}, .look = {0.0f, 0.0f, 0.0f}},
	    .commands = {{.kind = Kind::PointerTo, .delaySeconds = 2.0f, .point = {0.98f, 0.6f}},
	                 {.kind = Kind::PointerTo, .delaySeconds = 3.0f, .point = {0.5f, 0.6f}},
	                 {.kind = Kind::PointerTo, .delaySeconds = 3.0f, .point = {0.02f, 0.6f}},
	                 {.kind = Kind::PointerTo, .delaySeconds = 3.0f, .point = {0.5f, 0.6f}}},
	    .repeatFrom = 0,
	});

	// Offering a thing to a creature, the hand eases away from the camera over two seconds, not 0.28 seconds
	CreatureSetup hungry {.species = CreatureType::Tiger, .offset = {0.0f, 0.0f}, .needs = {.energy = 0.3f}, .hold = true};
	all.push_back({
	    .id = "hand.offer_slow_ease",
	    .name = "The hand easing away slowly while it offers food to a creature",
	    .facet = Facet::Hand,
	    .description = "The player's hand takes a pile of food. The pointer moves onto a hungry tiger for a second, so that "
	                   "the hand offers the food, then onto the land far behind the tiger. Three seconds later it moves to "
	                   "the land in front of the tiger, and after two more seconds to the far land again.",
	    .expected = "Moved off the tiger while it offers the food, the hand eases away to the far land over two seconds. "
	                "When the offer has ended, it eases away to the far land in about a quarter of a second, as usual.",
	    .environment = {.dispenserGrid = false, .cursor = glm::vec2(0.5f, 0.8f)},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 25.0f, -40.0f}, .look = {0.0f, 5.0f, 0.0f}},
	    .creatures = {hungry},
	    .objects = {{.type = MobileObjectInfo::MagicFood, .offset = {15.0f, -25.0f}}},
	    .commands = {{.kind = Kind::HandTake, .delaySeconds = 1.0f, .object = 0},
	                 {.kind = Kind::PointerOnCreature, .creature = 0, .delaySeconds = 1.0f},
	                 {.kind = Kind::PointerTo, .delaySeconds = 1.0f, .point = {0.5f, 0.25f}},
	                 {.kind = Kind::PointerTo, .delaySeconds = 3.0f, .point = {0.5f, 0.85f}},
	                 {.kind = Kind::PointerTo, .delaySeconds = 2.0f, .point = {0.5f, 0.25f}},
	                 {.kind = Kind::HandRelease, .delaySeconds = 3.0f, .velocity = {0.0f, 0.0f, 0.0f}}},
	});

	// Over the help's text bubble, the hand stands straight up and eases to the bubble
	constexpr size_t k_ActionButton = 3;
	all.push_back({
	    .id = "hand.bubble_stand",
	    .name = "The hand over the help's text bubble",
	    .facet = Facet::Hand,
	    .description = "A \"did you know\" sign near the camera. The pointer taps the sign, so that its tip opens in the "
	                   "blue bubble. Then the pointer moves onto the bubble, onto the land beside the sign and onto the "
	                   "bubble again, three seconds at each.",
	    .expected = "On the bubble the hand stands straight up, not turned to the slope of the land, and stays at the "
	                "distance of the bubble from the camera. On the land it hangs over the land, turned to its slope.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 6.0f, -15.0f}, .look = {0.0f, 2.0f, 0.0f}},
	    .highlights = {{.kind = 1, .offset = {0.0f, 0.0f}, .challenge = 0, .tip = std::pair {4128u, 3u}}},
	    .commands = {{.kind = Kind::PointerTo, .delaySeconds = 1.0f, .point = {0.5f, 0.46f}},
	                 {.kind = Kind::PointerPress, .delaySeconds = 1.0f, .value = k_ActionButton},
	                 {.kind = Kind::PointerRelease, .delaySeconds = 0.1f, .value = k_ActionButton},
	                 {.kind = Kind::PointerTo, .delaySeconds = 2.5f, .point = {0.625f, 0.225f}},
	                 {.kind = Kind::PointerTo, .delaySeconds = 3.0f, .point = {0.2f, 0.8f}},
	                 {.kind = Kind::PointerTo, .delaySeconds = 3.0f, .point = {0.625f, 0.225f}}},
	});

	// A pour tips the hand that holds the miracle; once the seed goes, the empty hand keeps the tipped up until the
	// pointer moves across the screen
	all.push_back({
	    .id = "hand.pour_tilt_up",
	    .name = "The hand stays tipped after a pour",
	    .facet = Facet::Hand,
	    .description = "The hand pours food for three seconds and then lets go of the seed, with the pointer still. Three "
	                   "seconds later the pointer moves a little to the right.",
	    .expected = "While it pours, the hand rises and rolls about the line to the camera. When the seed goes, the empty "
	                "hand stays rolled over as the last pour left it. It stands up to the land again over 0.4 seconds "
	                "only when the pointer moves across the screen.",
	    .environment = {.dispenserGrid = false, .cursor = glm::vec2(0.5f, 0.6f)},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 20.0f, -30.0f}, .look = {0.0f, 5.0f, 20.0f}},
	    .miracles = {{.type = MagicType::Food,
	                  .point = {0.0f, 20.0f},
	                  .handOffset = {0.0f, 20.0f},
	                  .handHeight = 12.0f,
	                  .delaySeconds = 1.0f,
	                  .holdSeconds = 3.0f,
	                  .byHand = true}},
	    .commands = {{.kind = Kind::PointerTo, .delaySeconds = 8.0f, .point = {0.55f, 0.6f}}},
	});

	// The hand that holds a thing while the camera turns by the ring stands towards the focus, and keeps that up
	// once it lets go
	constexpr size_t k_LeftButton = 1;
	all.push_back({
	    .id = "hand.ring_turn_holding",
	    .name = "The hand that holds a thing while the camera turns by the ring",
	    .facet = Facet::Hand,
	    .description = "The hand takes a rock. The left button is pressed at the right edge of the screen and the mouse "
	                   "moves down slowly, which turns the camera by the ring. Then the button comes up, the hand puts "
	                   "the rock down with the pointer still, and two seconds later the pointer moves to the middle.",
	    .expected = "While the camera turns, the hand holds the rock. After it puts the rock down, the empty hand stands "
	                "up towards the point the camera looks at, leaning up by three tenths of the distance. It turns to "
	                "the land again only when the pointer moves across the screen.",
	    .environment = {.dispenserGrid = false, .cursor = glm::vec2(0.5f, 0.6f)},
	    .framing = {.shot = Shot::Testbed},
	    .objects = {{.type = FeatureInfo::FatPilarChalk, .offset = {0.0f, 30.0f}}},
	    .commands = {{.kind = Kind::HandTake, .delaySeconds = 1.0f, .object = 0},
	                 {.kind = Kind::PointerTo, .delaySeconds = 0.5f, .point = {0.98f, 0.55f}},
	                 {.kind = Kind::PointerPress, .delaySeconds = 1.0f, .value = k_LeftButton},
	                 {.kind = Kind::PointerSweep, .delaySeconds = 0.2f, .point = {0.0f, 0.3f}, .amount = 1.5f},
	                 {.kind = Kind::PointerRelease, .delaySeconds = 1.6f, .value = k_LeftButton},
	                 {.kind = Kind::HandRelease, .delaySeconds = 0.1f, .velocity = {0.0f, 0.0f, 0.0f}},
	                 {.kind = Kind::PointerTo, .delaySeconds = 2.0f, .point = {0.5f, 0.6f}}},
	});
}
