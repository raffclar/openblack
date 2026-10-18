/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The testbed's scenarios of how the hand looks for its player's alignment: evil, neutral and good, and turning from
// one to the other

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
}
