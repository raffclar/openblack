/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Living things keep their height above the land when the land changes under them, as a villager standing where a
// vortex levels the ground goes down with it

#include <stdexcept>
#include <vector>

#include <LNDFile.h>
#include <gtest/gtest.h>

#include "3D/LandIslandInterface.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/HandGrab.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/VillagerPose.h"
#include "ECS/LandFollowers.h"
#include "ECS/Registry.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;
using namespace openblack::ecs::land_followers;

namespace
{
/// Land as high everywhere, whose height can be changed
class LevelledIsland final: public LandIslandInterface
{
public:
	float height {13.36f};

	[[nodiscard]] float GetHeightAt(glm::vec2) const override { return height; }
	[[nodiscard]] glm::vec3 GetNormalAt(glm::vec2) const override { return {0.0f, 1.0f, 0.0f}; }
	[[nodiscard]] const lnd::LNDCell& GetCell(const glm::u16vec2&) const override { return _cell; }
	[[nodiscard]] const lnd::LNDCell* FindCell(const glm::u16vec2&) const override { return &_cell; }
	void DumpTextures() const override {}
	void DumpMaps() const override {}
	[[nodiscard]] std::vector<LandBlock>& GetBlocks() override { throw std::logic_error("no blocks"); }
	[[nodiscard]] const std::vector<LandBlock>& GetBlocks() const override { throw std::logic_error("no blocks"); }
	[[nodiscard]] const std::vector<lnd::LNDCountry>& GetCountries() const override { throw std::logic_error("no countries"); }
	[[nodiscard]] const graphics::Texture2D& GetHeightMap() const override { throw std::logic_error("no maps"); }
	[[nodiscard]] const graphics::Texture2D& GetLuminosityMap() const override { throw std::logic_error("no maps"); }
	[[nodiscard]] const graphics::Texture2D& GetCellColourMap() const override { throw std::logic_error("no maps"); }
	[[nodiscard]] const graphics::Texture2D& GetBlockTextures() const override { throw std::logic_error("no maps"); }
	[[nodiscard]] const graphics::FrameBuffer& GetFootprintFramebuffer() const override { throw std::logic_error("no maps"); }
	[[nodiscard]] const graphics::FrameBuffer& GetLandAlphaFramebuffer() const override { throw std::logic_error("no maps"); }
	[[nodiscard]] U16Extent2 GetIndexExtent() const override { return {}; }
	[[nodiscard]] glm::mat4 GetOrthoView() const override { return glm::mat4(1.0f); }
	[[nodiscard]] glm::mat4 GetOrthoProj() const override { return glm::mat4(1.0f); }
	[[nodiscard]] Extent2 GetExtent() const override { return {}; }
	uint8_t GetNoise(glm::u8vec2) override { return 0; }

private:
	lnd::LNDCell _cell {};
};

entt::entity Standing(Registry& registry, glm::vec3 position)
{
	const auto entity = registry.Create();
	registry.Assign<Transform>(entity, position, glm::mat3(1.0f), glm::vec3(1.0f));
	return entity;
}
} // namespace

TEST(LandFollowers, AThingKeepsItsHeightAboveTheLand)
{
	EXPECT_FLOAT_EQ(FollowedHeight(13.36f, 13.36f, 12.06f), 12.06f);
	EXPECT_FLOAT_EQ(FollowedHeight(15.0f, 13.0f, 12.0f), 14.0f);
	EXPECT_FLOAT_EQ(FollowedHeight(5.0f, 5.0f, 5.0f), 5.0f);
}

TEST(LandFollowers, OnlyTheLivingOnTheLandFollowIt)
{
	EXPECT_TRUE(FollowsTheLand(true, false, false));
	EXPECT_FALSE(FollowsTheLand(false, false, false));
	EXPECT_FALSE(FollowsTheLand(true, true, false));
	EXPECT_FALSE(FollowsTheLand(true, false, true));
}

TEST(LandFollowers, AVillagerStandingWhereTheLandIsLevelledStaysOnIt)
{
	Registry registry;
	LevelledIsland land;
	// The fisherman's wife waits on the cliff top the vortex levels from 13.36 down to 12.06
	const auto wife = Standing(registry, {1745.47f, 13.36f, 2513.73f});
	registry.Assign<Villager>(wife);
	auto& pose = registry.Assign<VillagerPose>(wife);
	pose.turnStart = glm::vec3(1745.47f, 13.36f, 2513.73f);
	pose.drawnAt = glm::vec3(1745.47f, 13.36f, 2513.73f);
	const auto cow = Standing(registry, {1740.0f, 13.36f, 2510.0f});
	auto& grazing = registry.Assign<Animal>(cow);
	grazing.previousPosition = glm::vec3(1740.0f, 13.36f, 2510.0f);
	grazing.position = grazing.previousPosition;
	// A thrown villager flies, a held one is in the hand, and a rock is not alive: none of them follows
	const auto thrown = Standing(registry, {1741.0f, 20.0f, 2511.0f});
	registry.Assign<Villager>(thrown);
	registry.Assign<InPhysics>(thrown);
	const auto held = Standing(registry, {1742.0f, 30.0f, 2512.0f});
	registry.Assign<Villager>(held);
	registry.Assign<InHand>(held);
	const auto rock = Standing(registry, {1743.0f, 13.36f, 2513.0f});
	// One outside the levelled square is left alone
	const auto faraway = Standing(registry, {1900.0f, 13.36f, 2513.0f});
	registry.Assign<Villager>(faraway);

	const auto followers = Capture(registry, land, {1690.0f, 2460.0f}, {1810.0f, 2580.0f});
	land.height = 12.06f;
	Follow(registry, land, followers);

	EXPECT_FLOAT_EQ(registry.Get<Transform>(wife).position.y, 12.06f);
	EXPECT_FLOAT_EQ(pose.turnStart->y, 12.06f);
	EXPECT_FLOAT_EQ(pose.drawnAt->y, 12.06f);
	EXPECT_FLOAT_EQ(registry.Get<Transform>(cow).position.y, 12.06f);
	EXPECT_FLOAT_EQ(grazing.previousPosition.y, 12.06f);
	EXPECT_FLOAT_EQ(grazing.position.y, 12.06f);
	EXPECT_FLOAT_EQ(registry.Get<Transform>(thrown).position.y, 20.0f);
	EXPECT_FLOAT_EQ(registry.Get<Transform>(held).position.y, 30.0f);
	EXPECT_FLOAT_EQ(registry.Get<Transform>(rock).position.y, 13.36f);
	EXPECT_FLOAT_EQ(registry.Get<Transform>(faraway).position.y, 13.36f);
}
