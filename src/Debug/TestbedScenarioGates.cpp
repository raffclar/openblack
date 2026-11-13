/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The testbed's scenarios of the creatures' gate on the first land: the Norse gate and the gate stone plinth beside it,
// placed as the land's script places them, the three carved stones laid in the plinth as the hand gives them, and the
// plinth and the gate opened and closed as the choosing script does, and the sparkle that a script puts on a gate stone

#include <vector>

#include "TestbedScenarioRegistry.h"

using namespace openblack;
using namespace openblack::testbed_scenarios;

namespace
{
using Kind = Command::Kind;

/// The gate stands in the middle; the plinth is where the first land puts it from the gate, and both are turned as
/// there (5.953 radians)
constexpr glm::vec2 k_Plinth {-6.53f, -17.33f};
constexpr float k_Turn = 341.08f;
/// The objects by their place in the scenario
constexpr size_t k_GateObject = 0;
constexpr size_t k_PlinthObject = 1;
constexpr size_t k_TigerStone = 2;
constexpr size_t k_ApeStone = 3;
constexpr size_t k_CowStone = 4;

std::vector<ObjectSetup> GateAndStones()
{
	return {
	    {.type = AnimatedStaticInfo::NorseGate, .yawDegrees = k_Turn},
	    {.type = AnimatedStaticInfo::GateStonePlinth, .offset = k_Plinth, .yawDegrees = k_Turn},
	    {.type = MobileStaticInfo::GateTotemTiger, .offset = k_Plinth + glm::vec2(-8.0f, -4.0f)},
	    {.type = MobileStaticInfo::GateTotemApe, .offset = k_Plinth + glm::vec2(-4.0f, -8.0f)},
	    {.type = MobileStaticInfo::GateTotemCow, .offset = k_Plinth + glm::vec2(4.0f, -8.0f)},
	};
}

Framing GateView()
{
	return {.shot = Shot::Overview, .include = {{0.0f, 0.0f}, k_Plinth}, .distance = 1.5f};
}

Command Lay(size_t stone, float delay)
{
	return {.kind = Kind::LayGateStone, .delaySeconds = delay, .value = k_PlinthObject, .object = stone};
}

Command OpenClose(size_t object, bool open, float delay)
{
	return {.kind = Kind::SetOpenClose, .delaySeconds = delay, .value = open ? size_t {1} : size_t {0}, .object = object};
}

/// The land's script starts the sparkle on a gate stone as the first gold scroll does: the effect that marks an object,
/// on the stone, with no end
Command SparkleOn(size_t stone, float delay)
{
	constexpr float k_HighlightOnObject = 34.0f;
	return {.kind = Kind::CallNative,
	        .delaySeconds = delay,
	        .script = "SPECIAL_EFFECT_OBJECT",
	        .scriptArguments = {{.kind = ScriptArgument::Kind::Integer, .number = k_HighlightOnObject},
	                            {.kind = ScriptArgument::Kind::Object, .index = stone},
	                            {.kind = ScriptArgument::Kind::Number, .number = -1.0f}}};
}
} // namespace

void testbed_scenarios::AddGateScenarios(std::vector<Scenario>& all)
{
	all.push_back({
	    .id = "objects.creature_gate_open",
	    .name = "The creatures' gate opens",
	    .facet = Facet::Objects,
	    .description = "The Norse gate and the gate stone plinth stand as on the first land, with the tiger, ape and cow "
	                   "gate stones beside the plinth. The stones are laid in the plinth one by one, then the plinth is "
	                   "opened and, two seconds later, the gate, as the choosing script does once all three are in.",
	    .expected = "Each stone vanishes from the ground, leaving a fading ghost, and appears on the plinth's top, stacked "
	                "one above the other. Opened, the plinth sinks over about ten seconds and the stones sink down into it "
	                "until they are gone. The gate's two doors swing open over about ten seconds and stay open.",
	    .framing = GateView(),
	    .objects = GateAndStones(),
	    .commands = {Lay(k_TigerStone, 2.0f), Lay(k_ApeStone, 2.0f), Lay(k_CowStone, 2.0f),
	                 OpenClose(k_PlinthObject, true, 3.0f), OpenClose(k_GateObject, true, 2.0f)},
	});

	all.push_back({
	    .id = "objects.creature_gate_lanterns",
	    .name = "The lanterns on the creatures' gate",
	    .facet = Facet::Objects,
	    .description = "The Norse gate stands as on the first land. The camera looks at the two town lanterns that the "
	                   "gate holds up, 15 units to each side of it and 30 units above it.",
	    .expected = "Each lantern has the same land light and land colour as the middle of the gate, not the light of "
	                "the land below the lantern. The lanterns show only while the gate shows. The cursor does not find "
	                "them: it finds the gate or the land behind them.",
	    .framing = {.shot = Shot::Placed, .eye = {-50.0f, 30.0f, 50.0f}, .look = {0.0f, 25.0f, 0.0f}},
	    .objects = {{.type = AnimatedStaticInfo::NorseGate, .yawDegrees = k_Turn}},
	});

	all.push_back({
	    .id = "objects.creature_gate_close",
	    .name = "The creatures' gate opens and closes again",
	    .facet = Facet::Objects,
	    .description = "The Norse gate and the gate stone plinth with one stone laid in it are opened, and closed again "
	                   "twelve seconds later, as the scripts may.",
	    .expected = "The gate swings open and the plinth sinks with its stone; once closed, each plays back to where it "
	                "began: the gate shut and the plinth risen, the stone showing on its top again, drawn both stacked "
	                "and sinking while the plinth comes back up.",
	    .framing = GateView(),
	    .objects = GateAndStones(),
	    .commands = {Lay(k_TigerStone, 1.0f), OpenClose(k_PlinthObject, true, 1.0f), OpenClose(k_GateObject, true, 0.0f),
	                 OpenClose(k_PlinthObject, false, 12.0f), OpenClose(k_GateObject, false, 0.0f)},
	});

	all.push_back({
	    .id = "objects.gate_stone_sparkle_thrown",
	    .name = "The sparkle on a gate stone that flies",
	    .facet = Facet::Objects,
	    .description = "A script starts the sparkle on the tiger gate stone, as the first gold scroll does. Then the hand "
	                   "throws the stone across the land from the west, again every six seconds.",
	    .expected = "The sparks rise from the foot of the stone in each frame, also while it flies. They do not stay "
	                "behind the stone or jump after it.",
	    .environment = {.dispenserGrid = false},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 18.0f, -40.0f}, .look = {0.0f, 6.0f, 0.0f}},
	    .objects = {{.type = MobileStaticInfo::GateTotemTiger, .offset = {-12.0f, 0.0f}}},
	    .commands = {SparkleOn(0, 1.0f)},
	    .throws = {{.object = 0,
	                .from = glm::vec2(-12.0f, 0.0f),
	                .height = 4.0f,
	                .velocity = {8.0f, 9.0f, 0.0f},
	                .delaySeconds = 3.0f,
	                .repeatSeconds = 6.0f}},
	});

	all.push_back({
	    .id = "objects.gate_stone_sparkle_held",
	    .name = "The sparkle on a gate stone in the hand",
	    .facet = Facet::Objects,
	    .description = "A script starts the sparkle on the tiger gate stone, as the first gold scroll does. Then the "
	                   "player's hand takes the stone. Move the hand to carry the stone across the land.",
	    .expected = "The sparks rise from the foot of the stone in the hand in each frame. They do not stay behind the "
	                "stone or jump after it while the hand moves.",
	    .environment = {.dispenserGrid = false, .cursor = glm::vec2(0.5f, 0.5f)},
	    .framing = {.shot = Shot::Placed, .eye = {0.0f, 25.0f, -45.0f}, .look = {0.0f, 4.0f, 0.0f}},
	    .objects = {{.type = MobileStaticInfo::GateTotemTiger}},
	    .commands = {SparkleOn(0, 1.0f), {.kind = Kind::HandTake, .delaySeconds = 2.0f, .object = 0}},
	});
}
