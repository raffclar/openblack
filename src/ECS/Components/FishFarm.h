/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <array>
#include <optional>
#include <vector>

#include <entt/core/fwd.hpp>
#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

namespace openblack::ecs::components
{

/// A fish of the shoal: a sprite of misc0.raw lying flat on the water
struct Fish
{
	glm::vec3 position;
	float halfSize;
	float heading;    ///< radians, the direction (cos, 0, sin)
	float speed;      ///< units per second
	float turnRate;   ///< radians per second
	float frame;      ///< animation phase 0..15 (frame_anim::FishFrame)
	float fleeTime;   ///< seconds left fleeing a splash
	uint8_t cell {8}; ///< the sprite's cell, 8..23, set from the frame before it wraps
};

/// The shoal of a fish farm
struct FishShoal
{
	static constexpr size_t k_FishCount = 15; ///< 15 x the visible share (= 1)
	static constexpr float k_Range = 7.0f;    ///< the target wanders this far from the centre

	glm::vec3 centre;
	glm::vec3 target;
	float timer {0.0f}; ///< seconds until a new target
	std::array<Fish, k_FishCount> fish;
	/// The fish puzzle's bait (components::FishBait) the shoal swims for, or null (the fish farms). Such a shoal is not
	/// fished with the hand and is no longer drawn once its bait is done
	entt::entity bait {entt::null};
	uint8_t alpha {255}; ///< this frame, from the camera distance
	bool visible {false};
	size_t shown {k_FishCount}; ///< fish shown this frame (FishFarm::VisibleFish)
};

/// The fish puzzle's net of floats: Data\MISC\Fishplot.l3d (one float, a static object with dynamic lighting) drawn at
/// 7 points on a circle round the bait
struct FishPlot
{
	static constexpr size_t k_Floats = 7;

	glm::vec3 centre;
	std::array<glm::vec3, k_Floats> points; ///< centre + r (cos(i 2pi/7), 0, sin(i 2pi/7))
	float phase {0.0f};                     ///< the bobbing, += 2 dt per draw under the water
	float closure {1.0f};                   ///< 1 open .. 0 closed, the radius is 1 + 10 closure
	bool closing {false};
	entt::id_type mesh {0}; ///< "misc/Fishplot" in the mesh manager (0: not loaded)
};

/// The fish puzzle's bait (puzzle game type 14): the net is done when `need` fish of the shoals swimming for it are all
/// within `radius` (x, z) at once for `holdMs` of game time
struct FishBait
{
	glm::vec3 position;
	float radius {11.0f};
	uint32_t need {30};
	uint32_t holdMs {500};
	bool done {false};
	uint32_t inside {0}; ///< the fish inside this frame
	uint32_t timerMs {0};
	FishPlot net;
};

/// A fish farm (CREATE_FISH_FARM / CREATE_TOWN_FISH_FARM, fish farm info 0). No shoal when no sea was found around it.
struct FishFarm
{
	static constexpr float k_FoodValue = 1400.0f; ///< The fish farm info's foodValue: the full stock
	static constexpr uint32_t k_GrowthTurns = 16; ///< numGameTurnsAfterWhichFoodIsIncreased: +1 food

	std::optional<FishShoal> shoal;
	float food {k_FoodValue}; ///< full when created
	uint32_t info {0};        ///< the script's fish farm info index (info.dat only has 0)
	/// always the nearest town (any tribe), whatever town the script gave
	entt::entity town {entt::null};
	/// the fishermen, newest first; the count is the size (fish_farms::)
	std::vector<entt::entity> fishermen {};

	/// the shoal's share = food / foodValue; the first 15 x that fish are shown (and swim, and can be caught)
	[[nodiscard]] size_t VisibleFish() const
	{
		return static_cast<size_t>(static_cast<float>(FishShoal::k_FishCount) * food / k_FoodValue);
	}
};

} // namespace openblack::ecs::components
