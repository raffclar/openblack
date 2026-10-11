/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <array>
#include <functional>

#include <glm/vec3.hpp>

/// The players' symbols over a village centre's totem, showing how much the village believes in each player. The
/// player it believes in most has its symbol sitting over the totem; each of the others circles round it, a level higher
/// for each player believed in more, and the more it is believed in the bigger, wider and quicker it goes. The one
/// believed in second most now and then comes down to fight the first, throwing sparks while the fight is at its height.
namespace openblack::particles::town_belief
{

inline constexpr size_t k_PlayerCount = 8;
/// A symbol's way round is kept in step with the camera's distance by this much at the most
inline constexpr float k_MaximumDistanceScale = 10.0f;
/// The tilt of a symbol's way round goes on at this share of its speed, swaying with its sway's phase
inline constexpr float k_TiltShare = 0.846f;

/// The effect file's numbers, each pair at no belief (At0) and full belief (At1)
struct Params
{
	float radiusAt0 {10.0f};
	float radiusAt1 {2.0f};
	float speedAt0 {2.0f};
	float speedAt1 {10.0f};
	float scaleAt0 {0.0f};
	float scaleAt1 {1.0f};
	/// How high over the totem's top the symbols sit
	float heightAt1 {0.0f};
	/// Each player believed in more raises a symbol by this
	float heightPerLevel {2.0f};
	/// Whether the symbols' way round grows with the camera's distance, and by how much a unit of distance
	bool useFixedScale {true};
	float fixedScale {1.0f};
	float timeBetweenFightsAt0 {10.0f};
	float timeBetweenFightsAt1 {5.0f};
	float fightDurationAt0 {1.0f};
	float fightDurationAt1 {2.0f};
	/// How much faster a fighting symbol circles at the fight's height
	float speedUpDuringFight {2.0f};
	/// The sparks fly once the fight is past this
	float fightFracWhenEmitting {0.2f};
	/// How fast the tilt's sway goes round
	float angleSpeedPhaseSpeed {0.2f};
};

/// A symbol's own motion: its way round, the tilt of that way, the tilt's sway, and its fight's clock
struct Symbol
{
	/// Radians: round the totem, and up from level
	float around {0.0f};
	float tilt {0.0f};
	float sway {0.0f};
	/// Seconds until the next fight; seconds into the fight and how long it lasts
	float untilFight {0.0f};
	float intoFight {0.0f};
	float fightLength {0.0f};
};

/// How a symbol stands after a step
struct Look
{
	/// From the point over the totem
	glm::vec3 offset {0.0f};
	float scale {0.0f};
	/// How far into its fight it is: 0 out of a fight, rising to 1 at its height and back to 0
	float fight {0.0f};
};

/// A belief as the symbols show it: between none and all
[[nodiscard]] float Shown(float belief);

/// How many players a village believes in more than this one: those believed in more, or as much with a later number
[[nodiscard]] int Rank(const std::array<float, k_PlayerCount>& beliefs, size_t player);

/// How much the symbols' way round grows at this distance from the camera
[[nodiscard]] float DistanceScale(const Params& params, float distance);

/// A frame's step of a symbol for a belief and rank, dt seconds on; random gives a number between two others
[[nodiscard]] Look Step(Symbol& symbol, const Params& params, float belief, int rank, float dt, float distanceScale,
                        const std::function<float(float, float)>& random);

/// How strong the sparks of a fight are, of 255
[[nodiscard]] uint8_t SparkAlpha(float fight);
/// Whether a fight throws sparks
[[nodiscard]] bool Sparking(const Params& params, float fight);

} // namespace openblack::particles::town_belief
