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
// plinth and the gate opened and closed as the choosing script does

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
}
