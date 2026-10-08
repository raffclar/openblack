/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "FishShoals.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

#include <algorithm>
#include <bit>

#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <spdlog/spdlog.h>

#include "3D/FrameAnim.h"
#include "Common/GameRandom.h"
#include "Debug/DebugEnv.h"
#include "ECS/Components/FishFarm.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Unavailable.h"
#include "ECS/FishFarms.h"
#include "ECS/FishPuzzle.h"
#include "ECS/MapCells.h"
#include "ECS/Registry.h"
#include "ECS/SeaCells.h"
#include "ECS/Systems/DebugHooksInterface.h"
#include "ECS/Systems/WorldEffectsInterface.h"
#include "ECS/WaterRings.h"
#include "Locator.h"

using namespace openblack::ecs::components;
namespace map_coords = openblack::map_coords;

namespace
{
/// Set by the splashes of this frame, cleared after the shoals have seen it
struct FishShoalsSplashState
{
	std::optional<glm::vec3> splash;
};

/// This frame's splash (Locator::worldEffects)
FishShoalsSplashState& Splash()
{
	if (!openblack::Locator::worldEffects::has_value())
	{
		std::fputs("ecs::fish_shoals: no world effects in the locator (Locator::worldEffects)\n", stderr);
		std::abort();
	}
	return openblack::Locator::worldEffects::value().Get<FishShoalsSplashState>();
}

bool Trace()
{
	static const bool k_Trace = openblack::debug_env::HandTrace();
	return k_Trace;
}

/// What the fish puzzle's trace keeps between calls, in the debug hooks' store (Locator::debugHooks)
struct FishShoalsDebugHooksState
{
	uint32_t lastInside {~0u}; // OPENBLACK_HAND_TRACE: the last "inside" count logged
};

FishShoalsDebugHooksState& FishShoalsDebugHooksData()
{
	if (!openblack::Locator::debugHooks::has_value())
	{
		std::fputs("ecs::fish_shoals: no debug hooks in the locator (Locator::debugHooks)\n", stderr);
		std::abort();
	}
	return openblack::Locator::debugHooks::value().Get<FishShoalsDebugHooksState>();
}

/// One fish's step: its animation frame, its swim and its turn towards the target
void UpdateFish(Fish& fish, const glm::vec3& target, float dt)
{
	// the frame (dt at most 0.1 s), the cell before the wrap, then frame -= 15 truncate(frame / 15)
	fish.cell = openblack::graphics::frame_anim::FishFrame(fish.frame, dt, fish.speed);
	// a fish scared by a splash swims 4 times faster, easing back to normal over its last second
	float boost = 1.0f;
	if (fish.fleeTime > 0.0f)
	{
		boost = fish.fleeTime >= 1.0f ? 4.0f : 1.0f + 3.0f * fish.fleeTime;
		fish.fleeTime = std::max(0.0f, fish.fleeTime - dt);
	}
	fish.position += glm::vec3(std::cos(fish.heading), 0.0f, std::sin(fish.heading)) * fish.speed * dt * boost;
	// turn towards the shoal's target
	const float dx = target.x - fish.position.x;
	const float dz = target.z - fish.position.z;
	const float side = std::sin(fish.heading) * dx - std::cos(fish.heading) * dz < 0.0f ? 1.0f : -1.0f;
	fish.heading += side * fish.turnRate * dt;
	if (fish.heading > glm::pi<float>())
	{
		fish.heading -= glm::two_pi<float>();
	}
	else if (fish.heading <= -glm::pi<float>())
	{
		fish.heading += glm::two_pi<float>();
	}
}
} // namespace

void openblack::ecs::InitFishShoal(FishShoal& shoal, const glm::vec3& centre)
{
	using openblack::game_random::crt::Random;
	/// About 0.2 pi
	const auto turnScale = std::bit_cast<float>(0x3F20D97Cu);
	shoal.centre = centre;
	shoal.target = centre;
	// ten CRT draws per fish, in this order
	for (auto& fish : shoal.fish)
	{
		// the size, at least 1e-4
		fish.halfSize = std::max(Random(0.8f, 1.2f), 1e-4f);
		// the sprite's cell, truncate(r) & 0x3F
		fish.cell = static_cast<uint8_t>(static_cast<int32_t>(Random(0.0f, 15.5f)) & 0x3F);
		// the sprite's angle; every fish step sets it to the heading, the drawing uses the heading, so only the draw is
		// kept
		static_cast<void>(Random(0.0f, glm::pi<float>()));
		// the animation phase
		fish.frame = Random(0.0f, 15.0f);
		// z, y, x in that order, then the centre added
		const float z = Random(-5.0f, 5.0f);
		const float y = Random(-1.0f, 0.0f);
		const float x = Random(-5.0f, 5.0f);
		fish.position = glm::vec3(centre.x + x, centre.y + y, centre.z + z);
		fish.heading = Random(-glm::pi<float>(), glm::pi<float>());
		fish.speed = Random(0.5f, 1.5f);
		// ((r + 1) x speed) x about 0.2 pi
		const float r = Random(-0.1f, 0.1f);
		const float r1 = r + 1.0f;
		const float rs = r1 * fish.speed;
		fish.turnRate = rs * turnScale;
		fish.fleeTime = 0.0f;
	}
}

void openblack::ecs::SplashWater(const glm::vec3& point)
{
	Splash().splash = point;
}

void openblack::ecs::ProcessFishFarmsTurn(uint32_t turn)
{
	if (turn % FishFarm::k_GrowthTurns != 0)
	{
		return;
	}
	Locator::entitiesRegistry::value().Each<FishFarm>(
	    [](FishFarm& farm) { farm.food = std::clamp(farm.food + 1.0f, 0.0f, FishFarm::k_FoodValue); });
}

bool openblack::ecs::IsOkToCreateFishFarmAt(const glm::vec3& point)
{
	// coastal, then no FISH_FARM on the point's map cell: a fish farm is only in its own cell (it is inserted at its
	// position only)
	return sea_cells::IsCoastal(point) && map_cells::FindType(map_coords::CellOf(point), ObjectType::FishFarm) == entt::null;
}

std::optional<entt::entity> openblack::ecs::FindFishFarmAt(const glm::vec3& point)
{
	std::optional<entt::entity> found;
	Locator::entitiesRegistry::value().Each<const FishFarm>(
	    [&](entt::entity entity, const FishFarm& farm) {
		    // the shoals with a bait (the fish puzzle's) are skipped
		    if (found || !farm.shoal.has_value() || farm.shoal->bait != entt::null)
		    {
			    return;
		    }
		    const size_t count = std::min(farm.VisibleFish(), farm.shoal->fish.size());
		    for (size_t i = 0; i < count; ++i)
		    {
			    const auto& fish = farm.shoal->fish[i].position;
			    const float dx = fish.x - point.x;
			    const float dz = fish.z - point.z;
			    if (dx * dx + dz * dz < 4.0f)
			    {
				    found = entity;
				    return;
			    }
		    }
	    },
	    entt::exclude<Unavailable>);
	return found;
}

uint32_t openblack::ecs::RemoveFishFarmFood(entt::entity farm, uint32_t amount)
{
	// the farm takes a signed amount: the hand's amounts are small and positive, the same values
	return static_cast<uint32_t>(fish_farms::RemoveFood(farm, static_cast<int32_t>(amount)));
}

void openblack::ecs::UpdateFishShoals(float seconds, const glm::vec3& camera)
{
	// the frame's game time in seconds, at most 0.1
	const float dt = std::min(seconds, 0.1f);
	auto& registry = Locator::entitiesRegistry::value();
	const auto splash = Splash().splash;
	Splash().splash.reset();
	// no fish inside any bait yet this frame
	registry.Each<FishBait>([](FishBait& bait) { bait.inside = 0; });
	// moves the shoal and returns how many of its fish are inside its bait (0 when the bait is done: the
	// shoal is no longer moved nor drawn)
	const auto updateShoal = [&](FishFarm& farm, const FishBait* bait) -> uint32_t {
		auto& shoal = *farm.shoal;
		const size_t shown = std::min(farm.VisibleFish(), shoal.fish.size());
		shoal.shown = shown;
		if (bait != nullptr && bait->done)
		{
			shoal.visible = false;
			return 0;
		}
		// nothing when d^2 > 90000 (300 units); when d^2 > 40000 the alpha byte is the low byte of
		// truncate((1 - (d^2 - 40000) * 0.0001) * 255), which reaches 0 at d^2 = 50000 (223.6 units) and then wraps
		// around (the truncation goes negative), exactly as the original stores it
		const float d2 = glm::dot(shoal.centre - camera, shoal.centre - camera);
		shoal.visible = d2 <= 90000.0f;
		if (!shoal.visible)
		{
			return 0;
		}
		shoal.alpha = d2 > 40000.0f ? static_cast<uint8_t>(static_cast<int>((1.0f - (d2 - 40000.0f) * 1e-4f) * 255.0f) & 0xFF)
		                            : uint8_t {255};
		// before the splash: timer -= the game time step x 0.001 (whole milliseconds; (approximate) the port's game
		// time, at most 0.1 s); below 0 a new target: CRT Random(-range, range) for z then x, y the centre's, and timer =
		// 0.5 x the distance from the previous target
		const float elapsed = static_cast<float>(static_cast<uint32_t>(dt * 1000.0f)) * 0.001f;
		shoal.timer -= elapsed;
		if (shoal.timer < 0.0f)
		{
			const auto previous = shoal.target;
			const float z = game_random::crt::Random(-FishShoal::k_Range, FishShoal::k_Range);
			const float x = game_random::crt::Random(-FishShoal::k_Range, FishShoal::k_Range);
			shoal.target = glm::vec3(x + shoal.centre.x, shoal.centre.y, z + shoal.centre.z);
			shoal.timer = 0.5f * glm::distance(shoal.target, previous);
		}
		size_t fled = 0;
		if (splash.has_value())
		{
			// the shoal darts 2 units in a random direction (CRT Random(0, 2 pi)); the fish within 8 units of the splash
			// flee from it for 2 s, and only then the shoal's timer is 2
			const float r = game_random::crt::Random(0.0f, glm::two_pi<float>());
			shoal.target = shoal.centre + 2.0f * glm::vec3(std::cos(r), 0.0f, -std::sin(r));
			for (size_t i = 0; i < shown; ++i)
			{
				auto& fish = shoal.fish[i];
				const auto away = fish.position - *splash;
				if (glm::dot(away, away) < 64.0f)
				{
					fish.fleeTime = 2.0f;
					shoal.timer = 2.0f;
					fish.heading = std::atan2(away.z, away.x);
					++fled;
				}
			}
		}
		if (fled > 0 && Trace())
		{
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Fish trace: splash, {} fish flee", fled);
		}
		if (dt <= 0.0f)
		{
			return 0;
		}
		// the hidden fish (the stock is low) stay where they are until they come back
		uint32_t inside = 0;
		for (size_t i = 0; i < shown; ++i)
		{
			UpdateFish(shoal.fish[i], shoal.target, dt);
			// inside the bait's radius in (x, z)
			if (bait != nullptr)
			{
				const float dx = shoal.fish[i].position.x - bait->position.x;
				const float dz = shoal.fish[i].position.z - bait->position.z;
				inside += dx * dx + dz * dz < bait->radius * bait->radius ? 1 : 0;
			}
		}
		return inside;
	};
	// the game time step, whole milliseconds
	const auto ms = static_cast<uint32_t>(dt * 1000.0f);
	registry.Each<FishFarm>([&](FishFarm& farm) {
		if (!farm.shoal.has_value())
		{
			return;
		}
		auto* bait = farm.shoal->bait != entt::null ? registry.TryGet<FishBait>(farm.shoal->bait) : nullptr;
		const uint32_t inside = updateShoal(farm, bait);
		if (bait == nullptr)
		{
			return;
		}
		// the net's part under the water is drawn here, once per shoal of the bait: its update too
		AdvanceFishPlot(bait->net, dt);
		bait->inside += inside;
		if (bait->inside < bait->need || bait->done)
		{
			return;
		}
		bait->timerMs += ms;
		if (bait->timerMs < bait->holdMs)
		{
			return;
		}
		bait->done = true;
		bait->net.closing = true;
		if (Trace())
		{
			SPDLOG_LOGGER_INFO(spdlog::get("game"), "Fish puzzle: net closed ({}/{} inside for {} ms)", bait->inside,
			                   bait->need, bait->timerMs);
		}
		// every fish of every shoal of that bait (all 15, shown or not) leaves a white ring: growth 2, rate 1, cell 0x30
		const auto baitEntity = farm.shoal->bait;
		registry.Each<const FishFarm>([baitEntity](const FishFarm& other) {
			if (!other.shoal.has_value() || other.shoal->bait != baitEntity)
			{
				return;
			}
			for (const auto& fish : other.shoal->fish)
			{
				const WaterRing ring {.position = fish.position, .growth = 2.0f, .argb = 0xFFFFFFFFu};
				AddWaterRing(ring);
			}
		});
	});
	// a bait without all its fish inside starts its 500 ms again
	registry.Each<FishBait>([](FishBait& bait) {
		if (bait.inside < bait.need)
		{
			bait.timerMs = 0;
		}
		if (Trace() && !bait.done)
		{
			auto& last = FishShoalsDebugHooksData().lastInside;
			if (bait.inside != last)
			{
				last = bait.inside;
				SPDLOG_LOGGER_INFO(spdlog::get("game"), "Fish puzzle: inside {}/{}", bait.inside, bait.need);
			}
		}
	});
}
