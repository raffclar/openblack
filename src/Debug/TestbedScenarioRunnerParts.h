/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <string_view>
#include <vector>

#include <glm/vec2.hpp>

#include "BenchmarkRecorder.h"
#include "ECS/Systems/CreatureLocomotionSystemInterface.h"
#include "ECS/Systems/MagicSystemInterface.h"
#include "TestbedScenarioRegistry.h"

// The shared values and helpers of the files of the testbed runner. Only the runner files
// include this header.

namespace openblack::testbed_scenarios::runner_parts
{
using Kind = Command::Kind;
using Pace = ecs::systems::CreatureLocomotionSystemInterface::Pace;

/// The size of a creature whose scenario doesn't give one
inline constexpr float k_DefaultSize = 1.0f;
/// The town a scenario's buildings and fields belong to, its id kept clear of the crowds' towns, and what its buildings
/// hold
inline constexpr uint32_t k_ScenarioTownId = 900;
inline constexpr uint32_t k_ScenarioTownFood = 500;
inline constexpr uint32_t k_ScenarioTownWood = 500;
/// The hand looks at its point from this high over itself, and at a creature this high over its feet
inline constexpr float k_HandEyeHeight = 20.0f;
inline constexpr float k_HandAimHeight = 2.0f;

/// The town of the scenario, made when it is first necessary
uint32_t ScenarioTown(glm::vec2 middle);
/// The name of the result of a hand cast, for the log
std::string_view HandResultName(ecs::systems::MagicSystemInterface::HandResult result);
/// The stages of the profiler, as the benchmarks name them
std::vector<benchmark::StageInfo> BenchmarkStages();
/// Keeps the debug windows off the mouse, or lets them take it again
void KeepDebugWindowsOffTheMouse(bool off);
/// The word for the log that tells if a command started
std::string_view Started(bool started);
} // namespace openblack::testbed_scenarios::runner_parts
