/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <array>
#include <bit>
#include <optional>
#include <string_view>
#include <vector>

#include <entt/core/hashed_string.hpp>
#include <glm/vec3.hpp>

#include "Animals/FishShoal.h"

/// The fish puzzle of the fourth land: a ring of seven floats in the sea, with a shoal of fifteen fish on each side.
/// The player scares the fish into the ring. When all thirty fish stay in the ring for half a second, the fish are
/// caught: they go in rings on the water, and the floats close in.
namespace openblack::ecs::fish_puzzle
{

/// The ring of floats catches the fish that are this near its middle, across the sea
inline constexpr float k_NetRadius = 11.0f;
/// The two shoals have their centres this far east and west of the middle, and this far north of it
inline constexpr float k_ShoalOffset = 12.0f;
/// The fish are caught when this many are in the ring for this long
inline constexpr uint32_t k_FishToCatch = 30;
inline constexpr uint32_t k_HoldMilliseconds = 500;
/// The floats of the ring, and the angle from one to the next (two pi divided by seven, as the game keeps it)
inline constexpr size_t k_FloatCount = 7;
inline constexpr float k_FloatAngleStep = std::bit_cast<float>(0x3F65C8FAu);
/// When they close in, the floats go from this far beyond the smallest ring to the smallest ring
inline constexpr float k_ClosedRadius = 1.0f;
inline constexpr float k_ClosingReach = 10.0f;
/// The floats bob up and down by this much
inline constexpr float k_BobHeight = 0.5f;
/// The ring takes one step for each shoal in each turn, and each step moves its time by two times the step's seconds
inline constexpr float k_TimePerStep = 2.0f;
/// Each fish makes a ring on the water as it is caught, which grows at this rate
inline constexpr float k_CaughtRingGrowth = 2.0f;
/// The model of a float, from the misc folder
inline constexpr std::string_view k_FloatModelFile = "Fishplot.l3d";
inline constexpr entt::hashed_string k_FloatMeshId = entt::hashed_string("misc/fishplot");

/// The ring of floats
struct Net
{
	/// The middle of the ring, at the sea's level
	glm::vec3 middle {0.0f};
	/// How many of the fish that show were in the ring in the last turn
	uint32_t fishInRing {0};
	/// How long all the fish have been in the ring, in milliseconds
	uint32_t heldMilliseconds {0};
	/// The fish are caught. From then on the shoals do not swim and are not drawn.
	bool caught {false};
	/// How far the floats still have to close in: 1 at the start, 0 when they are closed
	float closing {1.0f};
	/// The time that the floats bob by
	float bobTime {0.0f};
};

/// The ring and its two shoals, the east shoal first
struct FishNet
{
	Net net;
	std::array<fish_shoal::Shoal, 2> shoals {};
};

/// The ring and the shoals about a middle point, the east shoal made first
[[nodiscard]] FishNet Make(glm::vec3 middle, const fish_shoal::Random& random);

/// How many of the fish that show of a shoal are in the ring, across the sea
[[nodiscard]] uint32_t FishInNet(const fish_shoal::Shoal& shoal, const Net& net);

/// One step of the ring: its floats bob, and they close in after the fish are caught
void StepNet(Net& net, float seconds);

/// One turn of the puzzle. Each shoal swims and is scared, the west shoal first, and the ring takes a step after each
/// shoal. The fish are caught when all of them stay in the ring long enough. Gives the places of all the fish of both
/// shoals in the turn when they are caught, for their rings on the water, and none in other turns.
[[nodiscard]] std::vector<glm::vec3> StepTurn(FishNet& puzzle, uint32_t milliseconds, std::optional<glm::vec3> scare,
                                              const fish_shoal::Random& random);

/// Where the floats are, with their bob
[[nodiscard]] std::array<glm::vec3, k_FloatCount> FloatPoints(const Net& net);

} // namespace openblack::ecs::fish_puzzle
