/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "FireFlies.h"

#include <cmath>

#include <algorithm>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/vec3.hpp>
#include <spdlog/spdlog.h>

#include "3D/DayNightClock.h"
#include "3D/FrameAnim.h"
#include "3D/MapCoords.h"
#include "Common/GUtilsDistance.h"
#include "Common/GameRandom.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Sprite.h"
#include "ECS/Components/StreetLantern.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Registry.h"
#include "ECS/Rocks.h"
#include "ECS/Systems/WorldEffectsInterface.h"
#include "FileSystem/FileSystemInterface.h"
#include "GameClock.h"
#include "Graphics/Texture2D.h"
#include "Locator.h"
#include "Resources/Loaders.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"
#include "Resources/SharedAssets.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;

namespace
{
constexpr size_t k_MaxFireFlies = 50;
constexpr float k_SearchRadius = 300.0f; // for both the lights and the perches
constexpr float k_HalfSize = 0.3f;
constexpr uint32_t k_Frame = 37; // S_SpriteSheet3, 8 x 8: column 5, row 4

enum class State
{
	Asleep,     // 0: invisible at its tree or rock
	Hovering,   // 1: at the house
	FlyingHome, // 2
	FlyingOut,  // 3
};

struct FireFly
{
	entt::entity sprite {entt::null};
	State state {State::Asleep};
	bool resting {true}; // sleeping at a tree or rock, can be woken
	glm::vec3 from {0.0f};
	glm::vec3 to {0.0f};
	glm::vec3 position {0.0f};
	glm::vec3 previous {0.0f};    // the position of the last turn (drawing interpolates)
	map_coords::MapCoords coords; // at rest: where it was made or where its last flight went
	float progress {0.0f};
	float duration {0.5f};
	float speedA {1.0f};
	float speedB {1.0f};
	float phaseA1 {0.0f};
	float phaseA2 {0.0f};
	float phaseB1 {0.0f};
	float phaseB2 {0.0f};
	float clock {0.0f}; // accumulated seconds
};

/// What this module keeps between calls (Locator::worldEffects)
struct FireFliesState
{
	std::vector<FireFly> fireFlies {}; // the game's firefly list, head first
	bool canSpawn {true};              // set again every morning
};

FireFliesState& FireFliesData()
{
	return openblack::Locator::worldEffects::value().Get<FireFliesState>();
}

graphics::TextureHandle SheetTexture()
{
	using namespace resources::shared_assets;
	if (LoadSpriteSheet3a("Fireflies") == LoadResult::Failed)
	{
		return graphics::TextureHandle {};
	}
	return Locator::resources::value().GetTextures().Handle(k_SpriteSheet3a.value())->GetNativeHandle();
}

/// A rock or any kind of tree
bool IsRockOrTree(entt::entity object)
{
	return Rocks::IsRock(object) || Locator::entitiesRegistry::value().AnyOf<Tree, DeadTree>(object);
}

/// An abode or a street light ((inferred) the street lanterns)
bool IsAbodeOrStreetLight(entt::entity object)
{
	return Locator::entitiesRegistry::value().AnyOf<Abode, StreetLantern>(object);
}

/// The firefly's map coordinates: at rest (asleep or hovering) where its flight went to; (approximate) in flight the
/// interpolated position (the port keeps it in metres)
map_coords::MapCoords CoordsOf(const FireFly& fly)
{
	if (fly.state == State::Asleep || fly.state == State::Hovering)
	{
		return fly.coords;
	}
	return map_coords::FromWorld(fly.position);
}

/// The game lists the spawn picks from: (inferred) newest first, by the creation index (the lists' order is not read)
std::vector<entt::entity> NewestFirst(std::vector<entt::entity> list)
{
	std::stable_sort(list.begin(), list.end(),
	                 [](entt::entity a, entt::entity b) { return object_index::Of(a) > object_index::Of(b); });
	return list;
}

/// The game's tree list: the Trees (a DeadTree counts as a rock)
std::vector<entt::entity> TreeList()
{
	std::vector<entt::entity> list;
	Locator::entitiesRegistry::value().Each<const Tree>(
	    [&list](entt::entity entity, const Tree& /*unused*/) { list.push_back(entity); });
	return NewestFirst(std::move(list));
}

/// The game's multi-map fixed list: the objects of that class
std::vector<entt::entity> MultiMapFixedList()
{
	std::vector<entt::entity> list;
	Locator::entitiesRegistry::value().Each<const Transform>([&list](entt::entity entity, const Transform& /*unused*/) {
		if (map_cells::IsMultiCellStaticClass(entity))
		{
			list.push_back(entity);
		}
	});
	return NewestFirst(std::move(list));
}

/// The search for a light or a perch: a spiral over ceil(2r / 10)^2 cells (ceil on the double) from the start's cell.
/// Each InBounds cell is searched only when GameRand(2) != 0; in its FindType(ANY) walk an object pred accepts is taken
/// when its GetDistanceInMetres from the start is below the best (or unordered) or there is no best yet, and then
/// GameRand(3) == 0 ends that cell. The radius only sizes the spiral: no cut by distance. Null when nothing was taken
entt::entity SpiralSearch(const map_coords::MapCoords& from, float radius, bool (*pred)(entt::entity))
{
	const float twice = radius + radius;
	const float cells = twice / 10.0f;
	const auto side = static_cast<int32_t>(std::ceil(static_cast<double>(cells)));
	int32_t count = side * side;
	map_coords::MapCoords coords = from;
	map_coords::Spiral spiral;
	entt::entity best = entt::null;
	float bestDistance = 0.0f;
	// one read filter for the whole search: nothing in it takes an object in the hand or into physics
	const map_cells::ReadBatch batch;
	for (; count > 0; --count)
	{
		if (map_coords::InBounds(coords) && game_random::GameRand(2) != 0)
		{
			const auto cell = map_coords::Cell(coords);
			for (auto candidate = map_cells::FindType(cell, ObjectType::Any); candidate != entt::null;
			     candidate = map_cells::FindType(cell, ObjectType::Any, candidate))
			{
				if (!pred(candidate))
				{
					continue;
				}
				const float distance = gutils::GetDistanceInMetres(from, object::MapCoordsOf(candidate));
				if (!(distance >= bestDistance) || best == entt::null)
				{
					bestDistance = distance;
					best = candidate;
					if (game_random::GameRand(3) == 0)
					{
						break;
					}
				}
			}
		}
		map_coords::AddCells(coords, spiral.Next());
	}
	return best;
}

/// The fallback, 15 m along x (truncated back to map coordinates) with this altitude
map_coords::MapCoords Aside(map_coords::MapCoords coords, float altitude)
{
	coords.x = map_coords::ToFixedGUtils(map_coords::ToMetres(coords.x) + 15.0f);
	coords.altitude = altitude;
	return coords;
}

/// A new firefly: eight synced GameFloatRand in this order: speedA = GFR(0.8) + 0.6, speedB the same, then GFR(2 pi)
/// for phaseA1, phaseA2, an unused one, phaseB1, phaseB2 and another unused one (drawn and never read). The new
/// firefly goes to the HEAD of the list
void Create(const map_coords::MapCoords& coords, graphics::TextureHandle texture)
{
	auto& state = FireFliesData();
	using game_random::GameFloatRand;
	FireFly fly;
	fly.coords = coords;
	fly.position = fly.previous = fly.from = fly.to = map_coords::ToWorld(coords);
	const float speedA = GameFloatRand(0.8f);
	fly.speedA = speedA + 0.6f;
	const float speedB = GameFloatRand(0.8f);
	fly.speedB = speedB + 0.6f;
	fly.phaseA1 = GameFloatRand(glm::two_pi<float>());
	fly.phaseA2 = GameFloatRand(glm::two_pi<float>());
	static_cast<void>(GameFloatRand(glm::two_pi<float>())); // unused
	fly.phaseB1 = GameFloatRand(glm::two_pi<float>());
	fly.phaseB2 = GameFloatRand(glm::two_pi<float>());
	static_cast<void>(GameFloatRand(glm::two_pi<float>())); // unused
	auto& registry = Locator::entitiesRegistry::value();
	fly.sprite = registry.Create();
	registry.Assign<Sprite>(fly.sprite, texture, graphics::frame_anim::SpriteCellUv(static_cast<int>(k_Frame), 8)[0],
	                        glm::vec2(1.0f / 8.0f), glm::vec4(0.0f), true);
	registry.Assign<Transform>(fly.sprite, fly.position, glm::mat3(1.0f), glm::vec3(k_HalfSize));
	state.fireFlies.insert(state.fireFlies.begin(), fly);
}

/// (max - count) attempts (the max re-read each time). Each GameRand(2): nonzero, the GameRand(count) tree of the tree
/// list, an empty list ends the spawn; zero, the GameRand(count) object of the multi-map fixed list, then on to the
/// first rock, an empty list ends the spawn and no rock from there makes nothing this attempt. A firefly at its map
/// coordinates
void Spawn()
{
	auto& state = FireFliesData();
	if (state.fireFlies.size() >= k_MaxFireFlies)
	{
		return;
	}
	const auto trees = TreeList();
	const auto fixed = MultiMapFixedList();
	const auto texture = SheetTexture();
	for (size_t attempt = state.fireFlies.size(); attempt < k_MaxFireFlies; ++attempt)
	{
		entt::entity at = entt::null;
		if (game_random::GameRand(2) != 0)
		{
			if (trees.empty())
			{
				break;
			}
			at = trees[game_random::GameRand(static_cast<uint32_t>(trees.size()))];
		}
		else
		{
			if (fixed.empty())
			{
				break;
			}
			for (size_t i = game_random::GameRand(static_cast<uint32_t>(fixed.size())); i < fixed.size(); ++i)
			{
				if (Rocks::IsRock(fixed[i]))
				{
					at = fixed[i];
					break;
				}
			}
			if (at == entt::null)
			{
				continue;
			}
		}
		Create(object::MapCoordsOf(at), texture);
	}
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Fireflies: {} ({} trees, {} multi map fixed)", state.fireFlies.size(),
	                   trees.size(), fixed.size());
}

/// The flight (out or home) to those map coordinates
void StartFlight(FireFly& fly, const map_coords::MapCoords& to, State state)
{
	fly.from = fly.position;
	fly.coords = to;
	fly.to = map_coords::ToWorld(to);
	fly.progress = 0.0f;
	fly.state = state;
	fly.duration = std::max(0.5f, glm::distance(fly.from, fly.to) / (3.0f * fly.speedA));
}

/// The list head to the tail
void HeadToTail()
{
	auto& state = FireFliesData();
	std::rotate(state.fireFlies.begin(), state.fireFlies.begin() + 1, state.fireFlies.end());
}

/// Only the list head, and only when it is resting. Its tree or rock must still be there: an object of the fixed list
/// of its cell that is a rock or tree and whose map coordinates equal its own (x and z), else it is deleted. (The same
/// walk deletes it when another firefly there has its map coordinates; a firefly is a mobile object, not in the fixed
/// list, so that never happens.) Then it flies to the nearest-ish abode or lantern, raised by its height + 2, none:
/// 15 m along x, altitude 4; it is awake and goes to the tail
void WakeOne()
{
	auto& state = FireFliesData();
	if (state.fireFlies.empty() || !state.fireFlies.front().resting)
	{
		return;
	}
	auto& fly = state.fireFlies.front();
	const auto at = CoordsOf(fly);
	bool perched = false;
	map_cells::ForEachFixed(map_coords::Cell(at), [&perched, &at](entt::entity candidate) {
		if (IsRockOrTree(candidate))
		{
			const auto coords = object::MapCoordsOf(candidate);
			perched = perched || (coords.x == at.x && coords.z == at.z);
		}
		return true;
	});
	if (!perched)
	{
		auto& registry = Locator::entitiesRegistry::value();
		if (registry.Valid(fly.sprite))
		{
			registry.Destroy(fly.sprite);
		}
		state.fireFlies.erase(state.fireFlies.begin());
		return;
	}
	map_coords::MapCoords destination;
	if (const auto light = SpiralSearch(at, k_SearchRadius, IsAbodeOrStreetLight); light != entt::null)
	{
		destination = object::MapCoordsOf(light);
		const float height = object::GetHeight(light) + 2.0f;
		destination.altitude = height + destination.altitude;
	}
	else
	{
		destination = Aside(at, 4.0f);
	}
	StartFlight(fly, destination, State::FlyingOut);
	fly.resting = false;
	HeadToTail();
}

/// Only the list head, and only when it is awake. It flies to the tree or rock the search finds, none: 15 m along x,
/// altitude 0; it is resting from now on and goes to the tail
void SleepOne()
{
	auto& state = FireFliesData();
	if (state.fireFlies.empty() || state.fireFlies.front().resting)
	{
		return;
	}
	auto& fly = state.fireFlies.front();
	const auto at = CoordsOf(fly);
	const auto perch = SpiralSearch(at, k_SearchRadius, IsRockOrTree);
	StartFlight(fly, perch != entt::null ? object::MapCoordsOf(perch) : Aside(at, 0.0f), State::FlyingHome);
	fly.resting = true;
	HeadToTail();
}

float Smooth(float p)
{
	return p * p * (3.0f - 2.0f * p);
}
} // namespace

void ecs::ProcessFireFliesTurn(const DayNightClock& clock)
{
	auto& fireFliesState = FireFliesData();
	const float visual = clock.GetVisualTime();
	const float skyType = clock.SkyType();
	if (visual > 12.0f && skyType > 1.0f)
	{
		if (fireFliesState.canSpawn)
		{
			Spawn();
		}
		WakeOne();
		fireFliesState.canSpawn = false;
	}
	else if (visual < 12.0f && skyType < 1.0f)
	{
		SleepOne();
		fireFliesState.canSpawn = true;
	}

	// the turn's length in seconds, read every turn
	const float turnSeconds = static_cast<float>(game_clock::MsPerTurn()) * game_clock::k_SecondsPerMs;
	for (auto& fly : fireFliesState.fireFlies)
	{
		fly.previous = fly.position;
		switch (fly.state)
		{
		case State::Asleep:
			fly.progress = 0.0f;
			break;
		case State::Hovering:
			fly.progress = 1.0f;
			break;
		case State::FlyingHome:
		case State::FlyingOut:
			fly.progress = std::min(1.0f, fly.progress + turnSeconds / fly.duration);
			fly.position = glm::mix(fly.from, fly.to, Smooth(fly.progress));
			if (fly.progress >= 1.0f)
			{
				fly.state = fly.state == State::FlyingOut ? State::Hovering : State::Asleep;
			}
			break;
		}
	}
}

void ecs::UpdateFireFlies(float seconds, const glm::vec3& camera)
{
	auto& fireFliesState = FireFliesData();
	if (fireFliesState.fireFlies.empty())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	// the fraction of the turn
	const float interpolation = game_clock::TurnFraction();
	for (auto& fly : fireFliesState.fireFlies)
	{
		if (!registry.Valid(fly.sprite))
		{
			continue;
		}
		auto& sprite = registry.Get<Sprite>(fly.sprite);
		fly.clock += seconds;
		const float t1 = fly.clock * fly.speedA;
		const float t2 = fly.clock * fly.speedB;
		const float a1 = std::fmod(t1 * 0.1f + fly.phaseA1, glm::two_pi<float>());
		const float a2 = std::fmod(t1 * 0.1f + fly.phaseA2, glm::two_pi<float>());
		const float b1 = std::fmod(t2 + fly.phaseB1, glm::two_pi<float>());
		const float b2 = std::fmod(t2 * 1.21f + fly.phaseB2, glm::two_pi<float>());
		float amplitude = 0.0f;
		switch (fly.state)
		{
		case State::Asleep:
			amplitude = 0.0f;
			break;
		case State::Hovering:
			amplitude = 1.0f;
			break;
		case State::FlyingHome:
			amplitude = fly.progress < 0.8f ? 1.0f : 1.0f - (fly.progress - 0.8f) * 5.0f;
			break;
		case State::FlyingOut:
			amplitude = fly.progress < 0.2f ? fly.progress * 5.0f : 1.0f;
			break;
		}
		const glm::vec3 big =
		    amplitude * 8.0f * glm::vec3(std::cos(a2) * std::cos(a1), 0.5f * std::sin(a2), std::cos(a2) * std::sin(a1));
		const glm::vec3 small =
		    amplitude * glm::vec3(std::cos(b2) * std::cos(b1), 0.5f * std::sin(b2), std::cos(b2) * std::sin(b1));
		const glm::vec3 p = fly.previous + (fly.position - fly.previous) * interpolation + big + small;
		registry.Get<Transform>(fly.sprite).position = p;

		// nothing asleep or 300 m away; alpha 190, fading out between 100 and 300 m
		const glm::vec3 d = camera - p;
		const float d2 = glm::dot(d, d);
		float alpha = 0.0f;
		if (fly.state != State::Asleep && d2 < 90000.0f)
		{
			alpha = d2 < 10000.0f ? 190.0f : 190.0f * (1.0f - (d2 - 10000.0f) / 80000.0f);
		}
		sprite.tint = glm::vec4(1.0f, 1.0f, 1.0f, alpha / 255.0f);
	}
}

void ecs::ClearFireFlies()
{
	auto& state = FireFliesData();
	if (Locator::entitiesRegistry::has_value())
	{
		auto& registry = Locator::entitiesRegistry::value();
		for (const auto& fly : state.fireFlies)
		{
			if (registry.Valid(fly.sprite))
			{
				registry.Destroy(fly.sprite);
			}
		}
	}
	state.fireFlies.clear();
	state.canSpawn = true;
}

bool ecs::TakeFireFlyAt(const glm::vec3& position)
{
	auto& state = FireFliesData();
	// map coordinates compare equal on the fixed-point x and z (1/65536 of a 10 m cell)
	constexpr float k_Tolerance = 10.0f / 65536.0f;
	const auto it = std::find_if(state.fireFlies.begin(), state.fireFlies.end(), [&](const FireFly& fly) {
		return std::abs(fly.position.x - position.x) < k_Tolerance && std::abs(fly.position.z - position.z) < k_Tolerance;
	});
	if (it == state.fireFlies.end())
	{
		return false;
	}
	if (Locator::entitiesRegistry::has_value() && Locator::entitiesRegistry::value().Valid(it->sprite))
	{
		Locator::entitiesRegistry::value().Destroy(it->sprite);
	}
	state.fireFlies.erase(it);
	return true;
}
