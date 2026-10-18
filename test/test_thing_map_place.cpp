/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdlib>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <vector>

#include <VanillaSaveFile.h>
#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/CreatureLeash.h"
#include "ECS/Components/Firefly.h"
#include "ECS/Components/Flock.h"
#include "ECS/Components/MapPlace.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/WallHug.h"
#include "ECS/Registry.h"
#include "ECS/TempleHeartOf.h"
#include "ECS/ThingMapPlace.h"

namespace
{
using namespace openblack;
using namespace openblack::ecs::components;
using map_coords::MapCoords;

/// A map position far from the corner of the map, which metres cannot keep: through metres and back it is one unit more
constexpr int32_t k_FarX = 0x01000001;
constexpr int32_t k_FarZ = 0x00FC131F;

/// The saved games of the original game are the players' own files. The test that reads them runs only on a copy of a
/// Profiles folder that OPENBLACK_VANILLA_SAVES gives, and only reads it.
std::vector<std::filesystem::path> VanillaWorldFiles()
{
	std::vector<std::filesystem::path> files;
	const char* folder = std::getenv("OPENBLACK_VANILLA_SAVES"); // NOLINT(concurrency-mt-unsafe)
	if (folder == nullptr || !std::filesystem::is_directory(folder))
	{
		return files;
	}
	for (const auto& entry : std::filesystem::recursive_directory_iterator(folder))
	{
		if (entry.is_regular_file() && entry.path().extension() == ".sav" &&
		    !entry.path().filename().string().starts_with("Script_"))
		{
			files.push_back(entry.path());
		}
	}
	return files;
}

void Stand(ecs::Registry& registry, entt::entity thing, glm::vec3 position)
{
	registry.Assign<Transform>(thing, Transform {.position = position, .rotation = glm::mat3(1.0f), .scale = glm::vec3(1.0f)});
}
} // namespace

TEST(ThingMapPlace, MetresCannotKeepAPlaceFarFromTheCorner)
{
	EXPECT_NE(map_coords::ToFixed(map_coords::ToMetres(k_FarX)), k_FarX);
}

TEST(ThingMapPlace, AThingThatStandsIsWhereItsMetresAre)
{
	ecs::Registry registry;
	const auto rock = registry.Create();
	Stand(registry, rock, {2520.0f, 7.0f, 2490.0f});
	EXPECT_EQ(ecs::thing_map_place::Of(registry, rock),
	          (MapCoords {.x = map_coords::ToFixed(2520.0f), .z = map_coords::ToFixed(2490.0f)}));
}

TEST(ThingMapPlace, AThingWithNoPlaceIsAtTheCorner)
{
	ecs::Registry registry;
	EXPECT_EQ(ecs::thing_map_place::Of(registry, registry.Create()), MapCoords {});
}

TEST(ThingMapPlace, AWalkerIsWhereItsWalkHoldsItUnlessItWasMovedSince)
{
	ecs::Registry registry;
	const auto villager = registry.Create();
	const glm::vec3 metres {map_coords::ToMetres(k_FarX), 0.0f, map_coords::ToMetres(k_FarZ)};
	Stand(registry, villager, metres);
	auto& walk = registry.Assign<WallHug>(villager);
	walk.position = {k_FarX, k_FarZ};
	walk.placedAt = {metres.x, metres.z};
	EXPECT_EQ(ecs::thing_map_place::Of(registry, villager), (MapCoords {.x = k_FarX, .z = k_FarZ}));
	// Something else moved it: it is where its metres are
	registry.Get<Transform>(villager).position.x += 1.0f;
	EXPECT_EQ(ecs::thing_map_place::Of(registry, villager).x, map_coords::ToFixed(metres.x + 1.0f));
}

TEST(ThingMapPlace, AnAnimalIsWhereItsTurnPutItNotWhereItIsDrawn)
{
	ecs::Registry registry;
	const auto sheep = registry.Create();
	Stand(registry, sheep, {100.0f, 0.0f, 200.0f});
	registry.Assign<Animal>(sheep).move.position = {k_FarX, k_FarZ};
	EXPECT_EQ(ecs::thing_map_place::Of(registry, sheep), (MapCoords {.x = k_FarX, .z = k_FarZ}));
}

TEST(ThingMapPlace, AFlockAndAFireflyKeepTheirOwnPlaces)
{
	ecs::Registry registry;
	const auto flock = registry.Create();
	registry.Assign<Flock>(flock).place = {.x = k_FarX, .z = k_FarZ, .altitude = 3.0f};
	EXPECT_EQ(ecs::thing_map_place::Of(registry, flock), (MapCoords {.x = k_FarX, .z = k_FarZ}));
	const auto firefly = registry.Create();
	registry.Assign<Firefly>(firefly).at = {.x = k_FarZ, .z = k_FarX, .altitude = 1.0f};
	EXPECT_EQ(ecs::thing_map_place::Of(registry, firefly), (MapCoords {.x = k_FarZ, .z = k_FarX}));
}

TEST(ThingMapPlace, AThingPutAtAMapPositionKeepsItWhileItStaysThere)
{
	ecs::Registry registry;
	const auto temple = registry.Create();
	Stand(registry, temple, {0.0f, 0.0f, 0.0f});
	ecs::thing_map_place::Put(registry, temple, {.x = k_FarX, .z = k_FarZ}, 12.0f);
	const auto& at = registry.Get<Transform>(temple).position;
	EXPECT_EQ(at, glm::vec3(map_coords::ToMetres(k_FarX), 12.0f, map_coords::ToMetres(k_FarZ)));
	EXPECT_EQ(ecs::thing_map_place::Of(registry, temple), (MapCoords {.x = k_FarX, .z = k_FarZ}));
	// Its height does not change its map position
	registry.Get<Transform>(temple).position.y = 30.0f;
	EXPECT_EQ(ecs::thing_map_place::Of(registry, temple), (MapCoords {.x = k_FarX, .z = k_FarZ}));
	// Moved in metres, it is where its metres are
	registry.Get<Transform>(temple).position.z += 1.0f;
	EXPECT_EQ(ecs::thing_map_place::Of(registry, temple).z, map_coords::ToFixed(map_coords::ToMetres(k_FarZ) + 1.0f));
}

TEST(ThingMapPlace, AWalkerAndAnAnimalStartFromTheMapPositionTheyKeep)
{
	ecs::Registry registry;
	const glm::vec3 metres {map_coords::ToMetres(k_FarX), 0.0f, map_coords::ToMetres(k_FarZ)};
	const auto villager = registry.Create();
	Stand(registry, villager, metres);
	registry.Assign<WallHug>(villager);
	ecs::thing_map_place::Keep(registry, villager, {.x = k_FarX, .z = k_FarZ});
	const auto& walk = registry.Get<const WallHug>(villager);
	EXPECT_EQ(walk.position, glm::ivec2(k_FarX, k_FarZ));
	EXPECT_EQ(walk.placedAt, glm::vec2(metres.x, metres.z));
	const auto sheep = registry.Create();
	Stand(registry, sheep, metres);
	registry.Assign<Animal>(sheep);
	ecs::thing_map_place::Keep(registry, sheep, {.x = k_FarX, .z = k_FarZ});
	EXPECT_EQ(registry.Get<const Animal>(sheep).move.position, glm::ivec2(k_FarX, k_FarZ));
}

TEST(ThingMapPlace, AThingWithNoMetresKeepsNothing)
{
	ecs::Registry registry;
	const auto thing = registry.Create();
	ecs::thing_map_place::Keep(registry, thing, {.x = k_FarX, .z = k_FarZ});
	EXPECT_FALSE(registry.AllOf<MapPlace>(thing));
}

TEST(ThingMapPlace, AScriptPlaceIsReadAsTheGameReadsIt)
{
	// The script's 1280.04 is one unit less than ToFixed of the same float, because 6553.6 is not 65536 / 10 as a float
	EXPECT_EQ(map_coords::ToFixed(1280.04f), 8388871);
	EXPECT_EQ(map_coords::ScriptPlace(1280.04, 1280.04), (MapCoords {.x = 8388870, .z = 8388870}));
	EXPECT_EQ(map_coords::ScriptPlace(2520.5, 0.0), (MapCoords {.x = map_coords::ToFixedGUtils(2520.5f), .z = 0}));
}

TEST(ThingMapPlace, ACreatureOfARealSaveKeepsItsExactMapPosition)
{
	const auto files = VanillaWorldFiles();
	if (files.empty())
	{
		GTEST_SKIP() << "Set OPENBLACK_VANILLA_SAVES to a copy of a Profiles folder of the game to check its saved games";
	}
	size_t creatures = 0;
	for (const auto& path : files)
	{
		std::ifstream stream(path, std::ios::binary);
		const std::vector<uint8_t> bytes {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
		for (const auto& creature : openblack::gamesave::FindCreatures(bytes))
		{
			const auto& saved = creature.object.position.position;
			ecs::Registry registry;
			const auto thing = registry.Create();
			Stand(registry, thing, glm::vec3(0.0f));
			ecs::thing_map_place::Put(registry, thing, {.x = saved.x, .z = saved.z}, 0.0f);
			EXPECT_EQ(ecs::thing_map_place::Of(registry, thing), (MapCoords {.x = saved.x, .z = saved.z}))
			    << path << " creature " << creature.number;
			++creatures;
		}
	}
	EXPECT_GT(creatures, 0u) << "No saved game of the folder has a creature";
}

TEST(ThingMapPlace, ACreatureMadeAtItsHomeGetsTheExactMapPositionAsItsHome)
{
	ecs::Registry registry;
	const glm::vec3 metres {map_coords::ToMetres(8388870), 0.0f, map_coords::ToMetres(8388870)};
	const auto creature = registry.Create();
	Stand(registry, creature, metres);
	auto& leash = registry.Assign<CreatureLeash>(creature);
	leash.home = metres;
	leash.homeOnMap = map_coords::FromMetres({metres.x, metres.z});
	ASSERT_EQ(leash.homeOnMap->x, 8388871);
	ecs::thing_map_place::Keep(registry, creature, {.x = 8388870, .z = 8388870});
	EXPECT_EQ(registry.Get<const CreatureLeash>(creature).homeOnMap, (MapCoords {.x = 8388870, .z = 8388870}));
	// A later map position does not change its home
	ecs::thing_map_place::Keep(registry, creature, {.x = 8388869, .z = 8388869});
	EXPECT_EQ(registry.Get<const CreatureLeash>(creature).homeOnMap, (MapCoords {.x = 8388870, .z = 8388870}));
}

TEST(ThingMapPlace, ACreatureWithAHomeElsewhereKeepsItsHome)
{
	ecs::Registry registry;
	const auto creature = registry.Create();
	Stand(registry, creature, {map_coords::ToMetres(8388870), 0.0f, map_coords::ToMetres(8388870)});
	auto& leash = registry.Assign<CreatureLeash>(creature);
	leash.home = glm::vec3(10.0f, 0.0f, 10.0f);
	leash.homeOnMap = map_coords::FromMetres({10.0f, 10.0f});
	ecs::thing_map_place::Keep(registry, creature, {.x = 8388870, .z = 8388870});
	EXPECT_EQ(registry.Get<const CreatureLeash>(creature).homeOnMap, map_coords::FromMetres({10.0f, 10.0f}));
}

TEST(ThingMapPlace, ATemplesHeartIsAThingOfItsOwnThatStandsForTheTemple)
{
	ecs::Registry registry;
	const auto temple = registry.Create();
	Stand(registry, temple, {0.0f, 0.0f, 0.0f});
	registry.Assign<Temple>(temple);
	EXPECT_EQ(ecs::temple_heart::Of(registry, temple), entt::entity {entt::null});
	const auto other = registry.Create();
	registry.Assign<TempleHeart>(other, registry.Create());
	const auto heart = registry.Create();
	registry.Assign<TempleHeart>(heart, temple);
	EXPECT_EQ(ecs::temple_heart::Of(registry, temple), heart);
	// The creature acts on the temple for its heart, and on any other thing for itself
	EXPECT_EQ(ecs::temple_heart::Acted(registry, heart), temple);
	EXPECT_EQ(ecs::temple_heart::Acted(registry, temple), temple);
	EXPECT_EQ(ecs::temple_heart::Acted(registry, entt::null), entt::entity {entt::null});
}

TEST(ThingMapPlace, TheThingInTheMapsCellsForATempleIsItsHeart)
{
	ecs::Registry registry;
	const auto temple = registry.Create();
	registry.Assign<Temple>(temple);
	// Without a heart, the temple stands for itself
	EXPECT_EQ(ecs::temple_heart::MapThingOf(registry, temple), temple);
	const auto heart = registry.Create();
	registry.Assign<TempleHeart>(heart, temple);
	EXPECT_EQ(ecs::temple_heart::MapThingOf(registry, temple), heart);
	// Any other thing, and the heart, stand for themselves
	const auto rock = registry.Create();
	EXPECT_EQ(ecs::temple_heart::MapThingOf(registry, rock), rock);
	EXPECT_EQ(ecs::temple_heart::MapThingOf(registry, heart), heart);
	EXPECT_EQ(ecs::temple_heart::MapThingOf(registry, entt::null), entt::entity {entt::null});
}

TEST(ThingMapPlace, AThingPutAtThePointsMapPositionKeepsThatPositionNotOneThroughMetres)
{
	// A point in metres whose map position, through metres and back, is one unit more
	constexpr float k_Point = 1792.6961669921875f;
	constexpr int32_t k_Place = 11748614;
	ASSERT_EQ(map_coords::ToFixed(k_Point), k_Place);
	ASSERT_EQ(map_coords::ToFixed(map_coords::Quantise(k_Point)), k_Place + 1);
	// A thing that a script or a track puts at the point goes to the point's map position, as the game puts it
	ecs::Registry registry;
	const auto villager = registry.Create();
	Stand(registry, villager, {0.0f, 0.0f, 0.0f});
	registry.Assign<WallHug>(villager);
	ecs::thing_map_place::Put(registry, villager, map_coords::FromMetres({k_Point, k_Point}), 4.0f);
	EXPECT_EQ(ecs::thing_map_place::Of(registry, villager), (MapCoords {.x = k_Place, .z = k_Place}));
	EXPECT_EQ(registry.Get<const WallHug>(villager).position, glm::ivec2(k_Place, k_Place));
}
