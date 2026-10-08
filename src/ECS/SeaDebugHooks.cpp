/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SeaDebugHooks.h"

#include <cstdio>
#include <cstdlib>

#include <array>
#include <string>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <spdlog/spdlog.h>

#include "3D/LandIslandInterface.h"
#include "ECS/Archetypes/AnimalArchetype.h"
#include "ECS/Archetypes/MobileStaticArchetype.h"
#include "ECS/Archetypes/PotArchetype.h"
#include "ECS/Archetypes/TreeArchetype.h"
#include "ECS/Archetypes/VillagerArchetype.h"
#include "ECS/Components/CutByPlane.h"
#include "ECS/FeatureBuild.h"
#include "ECS/FishPuzzle.h"
#include "ECS/MissionaryBoat.h"
#include "ECS/Physics/PhysicsObjects.h"
#include "ECS/Registry.h"
#include "ECS/SeaCells.h"
#include "Enums.h"
#include "Locator.h"

namespace openblack::ecs
{

void RunSeaDebugHooks()
{
	RunFishPuzzleDebugHook();        // OPENBLACK_TEST_FISH_PUZZLE
	missionary_boat::RunDebugHook(); // OPENBLACK_TEST_JC_SPECIAL
	feature_build::RunDebugHook();   // OPENBLACK_TEST_BUILT_PERCENTAGE
	const char* test = std::getenv("OPENBLACK_TEST_SEA");
	if (test == nullptr || !Locator::terrainSystem::has_value())
	{
		return;
	}
	float x = 0.0f;
	float z = 0.0f;
	std::array<char, 32> kind {};
	float height = 2.0f;
	if (std::sscanf(test, "%f,%f,%31[^,],%f", &x, &z, kind.data(), &height) < 3)
	{
		SPDLOG_LOGGER_WARN(spdlog::get("game"), "Sea test: OPENBLACK_TEST_SEA=\"x,z,type[,height]\", got \"{}\"", test);
		return;
	}
	const std::string type(kind.data());
	const auto& island = Locator::terrainSystem::value();
	const glm::vec3 at(x, island.GetHeightAt(glm::vec2(x, z)) + height, z);
	using namespace archetypes;
	entt::entity entity = entt::null;
	if (type == "villager")
	{
		entity = VillagerArchetype::Create(at, at, VillagerInfo::CelticFarmerMale, 20, false);
	}
	else if (type == "animal")
	{
		entity = AnimalArchetype::Create(at, AnimalInfo::Cow, 0, 0);
	}
	else if (type == "tree")
	{
		entity = TreeArchetype::Create(0, at, TreeInfo::Beech, true, 0.0f, 1.0f, 1.0f);
	}
	else if (type == "pot")
	{
		entity = PotArchetype::Create(at, 0.0f, PotInfo::HandFood, 300);
	}
	else if (type == "rock")
	{
		entity = MobileStaticArchetype::Create(at, MobileStaticInfo::Boulder1Lime, 0.0f, 0.0f, 0.0f, 0.0f, 0.5f);
	}
	auto& registry = Locator::entitiesRegistry::value();
	registry.SetDirty();
	if (std::getenv("OPENBLACK_TEST_CUT") != nullptr && entity != entt::null && registry.Valid(entity))
	{
		// the part under the water drawn cut by the plane in the sharks' 0xFF303070 (Renderer::DrawCutBelowWater)
		registry.Assign<components::CutByPlane>(entity);
	}
	const auto* po = entity != entt::null && registry.Valid(entity)
	                     ? physics::PhysicsObjects::AddObject(entity, glm::vec3(0.0f), glm::vec3(0.0f))
	                     : nullptr;
	const auto cell = sea_cells::CellOf(at);
	SPDLOG_LOGGER_INFO(
	    spdlog::get("game"),
	    "Sea test: {} {} at ({:.1f}, {:.1f}, {:.1f}) in physics {} (density {:.4f}, radius {:.2f}); cell ({}, {}) "
	    "altitude {} water {} dry land {}",
	    type, static_cast<uint32_t>(entity), at.x, at.y, at.z, po != nullptr, po != nullptr ? po->body.density : 0.0f,
	    po != nullptr ? po->body.Radius() : 0.0f, cell.x, cell.y, sea_cells::AltitudeAt(island, cell),
	    sea_cells::IsWater(island, cell), sea_cells::IsDryLand(island, cell));
	SPDLOG_LOGGER_INFO(spdlog::get("game"),
	                   "Sea test: GET_LAND_HEIGHT ({:.1f}, {:.1f}) = {:.3f}, dry reference (1788.4, 2710) = {:.3f}", x, z,
	                   sea_cells::ScriptLandHeight(island, glm::vec3(x, 0.0f, z)),
	                   sea_cells::ScriptLandHeight(island, glm::vec3(1788.4f, 0.0f, 2710.0f)));
}

} // namespace openblack::ecs
