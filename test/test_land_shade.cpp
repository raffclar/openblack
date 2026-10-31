/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <optional>
#include <span>
#include <vector>

#include <entt/core/hashed_string.hpp>
#include <glm/mat4x4.hpp>
#include <gtest/gtest.h>

#include "3D/LandShade.h"
#include "3D/VillageLights.h"
#include "ECS/Components/Cloud.h"
#include "ECS/Components/Mist.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/VillageLight.h"
#include "ECS/Registry.h"

#define LOCATOR_IMPLEMENTATIONS
#include "ECS/Systems/Implementations/LandShadeSystem.h"

using namespace openblack;
using ecs::systems::LandShadeFrame;
using ecs::systems::LandShadeSystem;

namespace
{

constexpr uint8_t k_Laid = 200;

land_shade::LaidBlock Block(glm::ivec2 at, uint8_t luminosity = k_Laid)
{
	land_shade::LaidBlock block {.at = at, .luminosity = {}};
	block.luminosity.fill(luminosity);
	return block;
}

uint8_t CellOf(const land_shade::ShadedLand& land, glm::ivec2 block, int32_t x, int32_t z)
{
	return land.Find(block)->at(static_cast<size_t>((x * land_shade::k_BlockSide) + z));
}

/// A camera looking straight down on the first 300 units of x: the first two blocks along x are in view
glm::mat4 TopDown()
{
	glm::mat4 m(0.0f);
	m[0][0] = 1.0f / 150.0f;
	m[3][0] = -1.0f;
	m[2][1] = 1.0f / 160.0f;
	m[3][1] = -1.0f;
	m[3][3] = 1.0f;
	return m;
}

/// A land of three blocks along x, the last out of view, with fake images, its own registry and its own drawn registry
struct World
{
	ecs::Registry registry;
	ecs::Registry drawn;
	std::vector<uint8_t> black = std::vector<uint8_t>(40 * 40, 0);
	std::vector<uint8_t> bright = std::vector<uint8_t>(14 * 14, 255);
	LandShadeSystem system {LandShadeSystem::Sources {
	    .image = [this](entt::id_type id) -> std::span<const uint8_t> {
		    if (id == entt::hashed_string("raw/sclouds").value() || id == entt::hashed_string("raw/sstorm").value())
		    {
			    return black;
		    }
		    return bright;
	    },
	    .landIdentity = []() -> std::optional<uint64_t> { return 1; },
	    .lay =
	        []() {
		        LandShadeSystem::LaidLand land;
		        for (int32_t x = 0; x < 3; ++x)
		        {
			        land.blocks.push_back(Block({x, 0}));
			        land.highest.push_back(0);
		        }
		        land.firstBlock = {0, 0};
		        land.mapSize = {3 * 16 + 1, 16 + 1};
		        return land;
	        },
	    .registry = [this]() { return &registry; },
	    .drawn = [this]() -> const ecs::Registry* { return &drawn; },
	}};

	/// A cloud of the sky, in the drawn registry or else in the land's registry
	void AddCloud(glm::vec2 at, uint32_t alpha, bool inDrawn = true)
	{
		auto& into = inDrawn ? drawn : registry;
		const auto cloud = into.Create();
		into.Assign<ecs::components::Cloud>(cloud, ecs::components::Cloud {.track = {}, .pinned = false});
		into.Assign<ecs::components::Mist>(cloud, ecs::components::Mist {.size = 1.0f, .colour = alpha << 24u});
		into.Assign<ecs::components::Transform>(cloud, ecs::components::Transform {.position = {at.x, 0.0f, at.y}});
	}

	void AddVillageLight(glm::vec2 at)
	{
		const auto light = registry.Create();
		registry.Assign<ecs::components::VillageLight>(light, ecs::components::VillageLight {});
		registry.Assign<ecs::components::Transform>(light, ecs::components::Transform {.position = {at.x, 0.0f, at.y}});
	}

	[[nodiscard]] static LandShadeFrame Day()
	{
		return {.viewProjection = TopDown(), .nearPlane = 0.5f, .landReflection = false, .landColour = 0xFFFFFF};
	}
};

} // namespace

TEST(LandShade, TheCornersOfACellComeFromTheBlockThatHoldsIt)
{
	land_shade::ShadedLand land;
	auto first = Block({0, 0}, 10);
	first.luminosity.at(16 * 17) = 1;
	land.Lay(std::vector {first, Block({1, 0}, 20)});
	// The last cell of the first block reads its own copy of the next block's first row
	const auto corners = land.CornersAt(15, 0);
	ASSERT_TRUE(corners.has_value());
	EXPECT_EQ(corners->at(2), 1);
	EXPECT_EQ(land.CornersAt(16, 0)->at(0), 20);
	EXPECT_FALSE(land.CornersAt(0, 16).has_value());
	EXPECT_FALSE(land.CornersAt(-1, 0).has_value());
	EXPECT_FALSE(land.CornersAt(512, 0).has_value());
}

TEST(LandShade, AShadowDarkensOnlyTheBlocksInViewEachInItsOwnCells)
{
	land_shade::ShadedLand land;
	land.Lay(std::vector {Block({0, 0}), Block({1, 0})});
	land.SetInView({0, 0}, true);
	const std::vector<uint8_t> black(40 * 40, 0);
	// Its first texel a whole cell from the map's corner, weights of 255: each cell takes the texel after it
	land_shade::CastShadow(land, {.corner = {0.0f, 0.0f}, .image = black, .side = 40, .alpha = 255});
	EXPECT_EQ(CellOf(land, {0, 0}, 0, 0), land_shade::k_DarkestShadow);
	// The first block's copy of the shared row is dark, the second block's own is not
	EXPECT_EQ(CellOf(land, {0, 0}, 16, 3), land_shade::k_DarkestShadow);
	EXPECT_EQ(CellOf(land, {1, 0}, 0, 3), k_Laid);
	ASSERT_EQ(land.Changed().size(), 1u);
	land.Restore();
	EXPECT_EQ(CellOf(land, {0, 0}, 0, 0), k_Laid);
	EXPECT_TRUE(land.Changed().empty());
}

TEST(LandShade, AShadowsAlphaSetsHowDarkItGets)
{
	land_shade::ShadedLand land;
	land.Lay(std::vector {Block({0, 0}, 250)});
	land.SetInView({0, 0}, true);
	const std::vector<uint8_t> black(4, 0);
	land_shade::CastShadow(land, {.corner = {0.0f, 0.0f}, .image = black, .side = 2, .alpha = 51});
	// 255 less 255 times 51 of 255
	EXPECT_EQ(CellOf(land, {0, 0}, 0, 0), 204);
	EXPECT_EQ(land_shade::Alpha(0.2f), 51);
	EXPECT_EQ(land_shade::Alpha(-1.0f), 0);
	EXPECT_EQ(land_shade::Alpha(3.0f), 255);
}

TEST(LandShade, ALightOnABlocksFirstCellAlsoLightsTheCopiesBeforeIt)
{
	land_shade::ShadedLand land;
	land.Lay(std::vector {Block({0, 0}, 20), Block({1, 0}, 20), Block({0, 1}, 20), Block({1, 1}, 20)});
	const std::vector<uint8_t> bright(4, 40);
	// The image's first texel lands on the cell (16, 16), the first of the last block
	land_shade::CastLight(land, {.corner = {160.0f, 160.0f}, .image = bright, .side = 2, .strength = 1.0f}, 255);
	EXPECT_EQ(CellOf(land, {1, 1}, 0, 0), 40);
	EXPECT_EQ(CellOf(land, {1, 0}, 0, 16), 40);
	EXPECT_EQ(CellOf(land, {0, 1}, 16, 0), 40);
	EXPECT_EQ(CellOf(land, {0, 0}, 16, 16), 40);
	EXPECT_EQ(CellOf(land, {1, 1}, 1, 1), 20);
}

TEST(LandShade, ALightTakesBrightLandToItsLevelOnlyWhenItOutshinesIt)
{
	land_shade::ShadedLand land;
	land.Lay(std::vector {Block({0, 0}, 250)});
	const std::vector<uint8_t> dim(4, 40);
	// The threshold for a full green is 47 of 256: 250 needs more than 45
	land_shade::CastLight(land, {.corner = {0.0f, 0.0f}, .image = dim, .side = 2, .strength = 1.0f}, 255);
	EXPECT_EQ(CellOf(land, {0, 0}, 0, 0), 250);
	const std::vector<uint8_t> bright(4, 46);
	land_shade::CastLight(land, {.corner = {0.0f, 0.0f}, .image = bright, .side = 2, .strength = 1.0f}, 255);
	EXPECT_EQ(CellOf(land, {0, 0}, 0, 0), 46);
	// No strength, no light
	land_shade::ShadedLand dark;
	dark.Lay(std::vector {Block({0, 0}, 10)});
	land_shade::CastLight(dark, {.corner = {0.0f, 0.0f}, .image = bright, .side = 2, .strength = 0.0f}, 255);
	EXPECT_TRUE(dark.Changed().empty());
}

TEST(LandShade, ALightsTexelsAreBroughtDownToTheWarmLevels)
{
	EXPECT_EQ(land_shade::WarmLevel(0), 0);
	EXPECT_EQ(land_shade::WarmLevel(255), 47);
	EXPECT_EQ(land_shade::WarmImage(std::vector<uint8_t> {0, 255}), (std::vector<uint8_t> {0, 47}));
}

TEST(LandShadeSystem, TheSkysCloudsShadeTheBlocksInViewForOneFrame)
{
	World world;
	world.AddCloud({50.0f, 10.0f}, 255);
	world.system.Shade(World::Day());
	EXPECT_EQ(world.system.ShadowCount(), 1u);
	EXPECT_EQ(CellOf(world.system.Land(), {0, 0}, 6, 2), land_shade::k_DarkestShadow);
	// The block out of view keeps its luminosity
	EXPECT_EQ(CellOf(world.system.Land(), {2, 0}, 5, 2), k_Laid);
	// The map of the GPU shows the frame, all of it new on the first frame
	auto map = world.system.Map();
	EXPECT_EQ(map.texels[(2 * map.size.x) + 6], land_shade::k_DarkestShadow);
	EXPECT_EQ(map.firstChangedRow, 0);
	EXPECT_EQ(map.lastChangedRow, map.size.y);

	// The cloud fades out: the next frame shows the land as it was laid, in the rows it changed
	world.drawn.Each<ecs::components::Mist>([](ecs::components::Mist& mist) { mist.colour = 0; });
	world.system.Shade(World::Day());
	EXPECT_EQ(world.system.ShadowCount(), 0u);
	EXPECT_EQ(CellOf(world.system.Land(), {0, 0}, 6, 2), k_Laid);
	map = world.system.Map();
	EXPECT_EQ(map.texels[(2 * map.size.x) + 6], k_Laid);
	EXPECT_EQ(map.firstChangedRow, 0);
	EXPECT_EQ(map.lastChangedRow, 17);
}

TEST(LandShadeSystem, OnlyTheCloudsOfTheDrawnRegistryShadeTheLand)
{
	World world;
	world.AddCloud({50.0f, 10.0f}, 255, false);
	world.system.Shade(World::Day());
	EXPECT_EQ(world.system.ShadowCount(), 0u);
	EXPECT_EQ(CellOf(world.system.Land(), {0, 0}, 6, 2), k_Laid);
	world.AddCloud({50.0f, 10.0f}, 255);
	world.system.Shade(World::Day());
	EXPECT_EQ(world.system.ShadowCount(), 1u);
}

TEST(LandShadeSystem, NoShadowsWithoutTheDetailOrRoomForThem)
{
	World world;
	world.AddCloud({50.0f, 10.0f}, 255);
	auto frame = World::Day();
	frame.cloudShadows = false;
	world.system.Shade(frame);
	EXPECT_EQ(world.system.ShadowCount(), 0u);
	frame.cloudShadows = true;
	frame.particleStamps.assign(land_shade::k_MostStamps - 1, std::nullopt);
	frame.flashes = 1;
	world.system.Shade(frame);
	EXPECT_EQ(world.system.ShadowCount(), 0u);
	EXPECT_EQ(CellOf(world.system.Land(), {0, 0}, 6, 2), k_Laid);
}

TEST(LandShadeSystem, AParticlesShadowDarkensTheLandWithoutTheCloudsDetail)
{
	World world;
	const std::vector<uint8_t> black(4, 0);
	auto frame = World::Day();
	frame.cloudShadows = false;
	frame.particleStamps.emplace_back(land_shade::ParticleShadow({60.0f, 0.0f, 30.0f}, black, 2, 1.0f));
	world.system.Shade(frame);
	EXPECT_EQ(world.system.ShadowCount(), 1u);
	// Centred on (60, 30): its first texel is at (55, 25), the cell (5, 2)
	EXPECT_EQ(CellOf(world.system.Land(), {0, 0}, 5, 2), land_shade::k_DarkestShadow);
	EXPECT_EQ(CellOf(world.system.Land(), {0, 0}, 4, 2), k_Laid);
	// It shows for one frame only
	world.system.Shade(World::Day());
	EXPECT_EQ(world.system.ShadowCount(), 0u);
	EXPECT_EQ(CellOf(world.system.Land(), {0, 0}, 5, 2), k_Laid);
}

TEST(LandShadeSystem, TheParticlesStampsComeFirstAndLeaveTheCloudsNoRoom)
{
	World world;
	world.AddCloud({50.0f, 10.0f}, 255);
	const std::vector<uint8_t> black(4, 0);
	auto frame = World::Day();
	frame.particleStamps.assign(land_shade::k_MostStamps + 5, std::nullopt);
	// A shadow past the list's room is not stamped
	frame.particleStamps.back() = land_shade::ParticleShadow({60.0f, 0.0f, 30.0f}, black, 2, 1.0f);
	world.system.Shade(frame);
	EXPECT_EQ(world.system.ShadowCount(), 0u);
	// A shadow in the last place is, and then there is no room for the cloud
	frame.particleStamps.resize(land_shade::k_MostStamps);
	frame.particleStamps.back() = land_shade::ParticleShadow({60.0f, 0.0f, 30.0f}, black, 2, 1.0f);
	world.system.Shade(frame);
	EXPECT_EQ(world.system.ShadowCount(), 1u);
	EXPECT_EQ(CellOf(world.system.Land(), {0, 0}, 5, 2), land_shade::k_DarkestShadow);
}

TEST(LandShadeSystem, TheVillagesLightTheLandOnlyAtNight)
{
	World world;
	world.AddVillageLight({100.0f, 80.0f});
	auto frame = World::Day();
	frame.scriptHour = 23.0f;
	world.system.Shade(frame);
	EXPECT_EQ(world.system.LightCount(), 0u);
	frame.landColour = 0x202020;
	world.system.Shade(frame);
	EXPECT_EQ(world.system.LightCount(), 1u);
	// A light's image starts 50 units before it: its middle is lit to the last warm level
	EXPECT_EQ(CellOf(world.system.Land(), {0, 0}, 10, 8), village_lights::k_WarmLevels - 1);
	// By day the light goes out
	frame.scriptHour = 12.0f;
	world.system.Shade(frame);
	EXPECT_EQ(CellOf(world.system.Land(), {0, 0}, 10, 8), k_Laid);
}
