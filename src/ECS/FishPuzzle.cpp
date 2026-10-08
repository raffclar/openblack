/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "FishPuzzle.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

#include <algorithm>
#include <exception>
#include <string>

#include <spdlog/spdlog.h>

#include "ECS/FishShoals.h"
#include "ECS/PuzzleGames.h"
#include "ECS/Registry.h"
#include "ECS/Systems/WorldEffectsInterface.h"
#include "FileSystem/FileSystemInterface.h"
#include "Locator.h"
#include "Resources/Loaders.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs::components;

namespace
{
// the bait
constexpr float k_BaitRadius = 11.0f;
constexpr uint32_t k_BaitNeed = 30;
constexpr uint32_t k_BaitHoldMs = 500;
// the shoals' centres, +12 in x and z for the first, then x - 24
constexpr glm::vec3 k_ShoalOffsetA {12.0f, 0.0f, 12.0f};
constexpr glm::vec3 k_ShoalOffsetB {-12.0f, 0.0f, 12.0f};
// 2 pi / 7
constexpr float k_FloatStep = 0.897598f;

/// What this module keeps between calls (Locator::worldEffects)
struct FishPuzzleState
{
	/// Fishplot.l3d failed to load (not tried again)
	bool fishPlotFailed {false};
};

FishPuzzleState& FishPuzzleData()
{
	return openblack::Locator::worldEffects::value().Get<FishPuzzleState>();
}

/// Data\MISC\Fishplot.l3d, loaded once into the mesh manager as "misc/Fishplot"
entt::id_type LoadFishPlotMesh()
{
	static const auto k_Id = resources::HashIdentifier(std::string("misc/Fishplot"));
	auto& meshes = Locator::resources::value().GetMeshes();
	if (FishPuzzleData().fishPlotFailed)
	{
		return 0;
	}
	if (!meshes.Contains(k_Id))
	{
		try
		{
			meshes.Load(k_Id, resources::L3DLoader::FromDiskTag {},
			            Locator::filesystem::value().GetPath<filesystem::Path::Misc>() / "Fishplot.l3d");
		}
		catch (const std::exception& e)
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("game"), "Fish puzzle: cannot load Fishplot.l3d: {}", e.what());
			FishPuzzleData().fishPlotFailed = true;
			return 0;
		}
	}
	return k_Id;
}

/// The 7 points round the centre (at construction and while closing): y is the centre's
void PlaceFloats(FishPlot& net, float radius)
{
	for (size_t i = 0; i < net.points.size(); ++i)
	{
		const float angle = static_cast<float>(i) * k_FloatStep;
		net.points[i] = net.centre + glm::vec3(std::cos(angle) * radius, 0.0f, std::sin(angle) * radius);
	}
}
} // namespace

void openblack::ecs::AdvanceFishPlot(FishPlot& net, float seconds)
{
	// dt = the game time increment * 0.001
	if (net.closing && net.closure != 0.0f)
	{
		net.closure = std::max(net.closure - 2.0f * seconds, 0.0f);
		PlaceFloats(net, 1.0f + 10.0f * net.closure);
	}
	net.phase += 2.0f * seconds;
}

std::array<glm::vec3, FishPlot::k_Floats> openblack::ecs::FishPlotFloats(const FishPlot& net)
{
	std::array<glm::vec3, FishPlot::k_Floats> floats {};
	for (size_t i = 0; i < floats.size(); ++i)
	{
		const auto squared = static_cast<float>(static_cast<int>(i * i)); // i * i as an integer
		floats[i] = net.points[i] + glm::vec3(0.0f, 0.5f * std::cos(squared + net.phase), 0.0f);
	}
	return floats;
}

openblack::ecs::FishPuzzleParts openblack::ecs::CreateFishPuzzle(const glm::vec3& position)
{
	FishPuzzleParts parts {entt::null, {entt::null, entt::null}};
	auto& registry = Locator::entitiesRegistry::value();
	const auto bait = registry.Create();
	auto& component = registry.Assign<FishBait>(bait);
	component.position = position;
	component.radius = k_BaitRadius;
	component.need = k_BaitNeed;
	component.holdMs = k_BaitHoldMs;
	// new FishPlot(pos, 11.0f): phase 0, closure 1, not closing
	component.net.centre = position;
	component.net.mesh = LoadFishPlotMesh();
	PlaceFloats(component.net, k_BaitRadius);
	// the shoals (range 7, count 1.0: all 15 shown), linked into the shoal list like the fish farms' ones. Here
	// they are fish farms of full stock, without a Transform (so no farm of the map counts them).
	for (size_t i = 0; i < parts.shoals.size(); ++i)
	{
		const auto& offset = i == 0 ? k_ShoalOffsetA : k_ShoalOffsetB;
		const auto entity = registry.Create();
		parts.shoals.at(i) = entity;
		auto& farm = registry.Assign<FishFarm>(entity);
		FishShoal shoal;
		InitFishShoal(shoal, position + offset);
		shoal.bait = bait;
		farm.shoal = shoal;
	}
	parts.bait = bait;
	registry.SetDirty();
	SPDLOG_LOGGER_INFO(spdlog::get("game"), "Fish puzzle: bait at ({:.2f}, {:.2f}, {:.2f}), net mesh {}", position.x,
	                   position.y, position.z, component.net.mesh != 0);
	return parts;
}

void openblack::ecs::RunFishPuzzleDebugHook()
{
	const char* test = std::getenv("OPENBLACK_TEST_FISH_PUZZLE");
	if (test == nullptr)
	{
		return;
	}
	float x = 0.0f;
	float z = 0.0f;
	int inside = 0;
	if (std::sscanf(test, "%f,%f,%d", &x, &z, &inside) < 2)
	{
		SPDLOG_LOGGER_WARN(spdlog::get("game"), "Fish puzzle test: OPENBLACK_TEST_FISH_PUZZLE=\"x,z[,inside]\", got \"{}\"",
		                   test);
		return;
	}
	// CREATE_WITH_ANGLE_AND_SCALE(32 = PuzzleGame, 14, (x, 0, z), 0, 1), then its first turn of the global lists
	const auto puzzle = CreatePuzzleGame(glm::vec3(x, 0.0f, z), script::PuzzleGameType::Fishes1, 0.0f, 1.0f);
	ProcessPuzzleGame(puzzle);
	const auto bait = PuzzleGameBait(puzzle);
	if (inside == 0 || bait == entt::null)
	{
		return;
	}
	// test only: every fish of the puzzle's shoals at the bait, so the 30 are inside at once
	Locator::entitiesRegistry::value().Each<FishFarm>([bait, x, z](FishFarm& farm) {
		if (farm.shoal.has_value() && farm.shoal->bait == bait)
		{
			for (auto& fish : farm.shoal->fish)
			{
				fish.position = glm::vec3(x, fish.position.y, z) + 0.2f * (fish.position - farm.shoal->centre);
			}
		}
	});
}
