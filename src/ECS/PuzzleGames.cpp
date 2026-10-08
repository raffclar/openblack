/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "PuzzleGames.h"

#include <array>
#include <vector>

#include <glm/vec2.hpp>
#include <spdlog/spdlog.h>

#include "3D/LandIslandInterface.h"
#include "ECS/Components/FishFarm.h"
#include "ECS/Components/PuzzleGame.h"
#include "ECS/Components/Transform.h"
#include "ECS/FishPuzzle.h"
#include "ECS/Registry.h"
#include "ECS/Systems/WorldEffectsInterface.h"
#include "ECS/ToBeDeleted.h"
#include "Locator.h"

namespace openblack::ecs
{
namespace
{
using namespace components;

/// The puzzle list, with what each puzzle made: the parts outlive a puzzle the scripts delete in no other way here
struct Tracked
{
	entt::entity puzzle;
	entt::entity bait;
	std::array<entt::entity, 2> shoals;
};
/// What this module keeps between calls (Locator::worldEffects)
struct PuzzleGamesState
{
	std::vector<Tracked> puzzles {};
};

PuzzleGamesState& PuzzleGamesData()
{
	return openblack::Locator::worldEffects::value().Get<PuzzleGamesState>();
}

void DestroyParts(const Tracked& tracked)
{
	auto& registry = Locator::entitiesRegistry::value();
	// the bait (with its fish plot) and the two shoals (off the shoal list, their 15 fish, then the shoal)
	if (tracked.bait != entt::null && registry.Valid(tracked.bait))
	{
		registry.Destroy(tracked.bait);
	}
	for (const auto shoal : tracked.shoals)
	{
		if (shoal != entt::null && registry.Valid(shoal))
		{
			registry.Destroy(shoal);
		}
	}
	registry.SetDirty();
}
} // namespace

entt::entity CreatePuzzleGame(const glm::vec3& position, script::PuzzleGameType type, float yAngleRadians, float scale)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = registry.Create();
	auto& puzzle = registry.Assign<PuzzleGame>(entity);
	puzzle.type = type;
	puzzle.position = position;
	if (Locator::terrainSystem::has_value())
	{
		puzzle.position.y += Locator::terrainSystem::value().GetHeightAt(glm::vec2(position.x, position.z));
	}
	// (int)(angle x 2048 x (1 / 2 pi))
	puzzle.angle = static_cast<int32_t>(yAngleRadians * 2048.0f * 0.159155f);
	puzzle.scale = scale;
	registry.Assign<Transform>(entity, puzzle.position, glm::mat3(1.0f), glm::vec3(scale));
	PuzzleGamesData().puzzles.push_back({entity, entt::null, {entt::null, entt::null}});
	SPDLOG_LOGGER_INFO(spdlog::get("scripting"), "PuzzleGame {} at ({:.2f}, {:.2f}, {:.2f})", static_cast<int>(type),
	                   puzzle.position.x, puzzle.position.y, puzzle.position.z);
	return entity;
}

bool IsPuzzleGamePlayed(entt::entity entity)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* puzzle = registry.TryGet<PuzzleGame>(entity);
	if (puzzle == nullptr)
	{
		return false;
	}
	if (puzzle->type != script::PuzzleGameType::Fishes1)
	{
		return false; // not ported
	}
	// no bait yet -> 0; bait done -> played, 1
	const auto* bait = puzzle->bait != entt::null ? registry.TryGet<const FishBait>(puzzle->bait) : nullptr;
	if (bait == nullptr || !bait->done)
	{
		return false;
	}
	puzzle->played = true;
	return true;
}

void ProcessPuzzleGame(entt::entity entity)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* puzzle = registry.TryGet<PuzzleGame>(entity);
	// nothing once played; the played test marks it
	if (puzzle == nullptr || puzzle->played)
	{
		return;
	}
	if (IsPuzzleGamePlayed(entity))
	{
		SPDLOG_LOGGER_INFO(spdlog::get("scripting"), "PuzzleGame {} played", static_cast<int>(puzzle->type));
		return;
	}
	if (puzzle->type == script::PuzzleGameType::Fishes1 && puzzle->bait == entt::null)
	{
		const auto parts = CreateFishPuzzle(puzzle->position);
		puzzle = registry.TryGet<PuzzleGame>(entity);
		puzzle->bait = parts.bait;
		puzzle->shoals = parts.shoals;
		for (auto& tracked : PuzzleGamesData().puzzles)
		{
			if (tracked.puzzle == entity)
			{
				tracked.bait = parts.bait;
				tracked.shoals = parts.shoals;
			}
		}
	}
}

void ProcessPuzzleGamesTurn()
{
	auto& state = PuzzleGamesData();
	if (state.puzzles.empty())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto puzzles = state.puzzles;
	state.puzzles.clear();
	for (const auto& tracked : puzzles)
	{
		if (!ecs::IsAvailable(tracked.puzzle) || !registry.AllOf<PuzzleGame>(tracked.puzzle))
		{
			DestroyParts(tracked);
			continue;
		}
		state.puzzles.push_back(tracked);
	}
	for (const auto& tracked : std::vector<Tracked>(state.puzzles))
	{
		ProcessPuzzleGame(tracked.puzzle);
	}
}

entt::entity PuzzleGameBait(entt::entity entity)
{
	const auto* puzzle = Locator::entitiesRegistry::value().TryGet<const PuzzleGame>(entity);
	return puzzle != nullptr ? puzzle->bait : entt::null;
}

} // namespace openblack::ecs
