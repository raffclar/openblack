/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>
#include <cstdlib>

#include <vector>

#include <gtest/gtest.h>

#include "3D/LandIslandInterface.h"
#include "ECS/Components/MagicPile.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "Locator.h"

// The test builds its own locator
#define LOCATOR_IMPLEMENTATIONS
#include "ECS/Systems/Implementations/MiracleFxSystem.h"

using namespace openblack;

namespace
{
/// A land that rises one metre for each metre across x, plus a step that the test can raise
class SlopedLand final: public LandIslandInterface
{
public:
	[[nodiscard]] float GetHeightAt(glm::vec2 point) const final { return point.x + step; }
	[[nodiscard]] glm::vec3 GetNormalAt(glm::vec2) const final { return {0.0f, 1.0f, 0.0f}; }
	[[nodiscard]] const lnd::LNDCell& GetCell(const glm::u16vec2&) const final { std::abort(); }
	[[nodiscard]] const lnd::LNDCell* FindCell(const glm::u16vec2&) const final { return nullptr; }
	void DumpTextures() const final {}
	void DumpMaps() const final {}
	[[nodiscard]] std::vector<LandBlock>& GetBlocks() final { std::abort(); }
	[[nodiscard]] const std::vector<LandBlock>& GetBlocks() const final { std::abort(); }
	[[nodiscard]] const std::vector<lnd::LNDCountry>& GetCountries() const final { std::abort(); }
	[[nodiscard]] const graphics::Texture2D& GetHeightMap() const final { std::abort(); }
	[[nodiscard]] const graphics::Texture2D& GetLuminosityMap() const final { std::abort(); }
	[[nodiscard]] const graphics::Texture2D& GetCellColourMap() const final { std::abort(); }
	[[nodiscard]] const graphics::Texture2D& GetBlockTextures() const final { std::abort(); }
	[[nodiscard]] U16Extent2 GetIndexExtent() const final { std::abort(); }
	[[nodiscard]] glm::mat4 GetOrthoView() const final { std::abort(); }
	[[nodiscard]] glm::mat4 GetOrthoProj() const final { std::abort(); }
	[[nodiscard]] Extent2 GetExtent() const final { std::abort(); }
	uint8_t GetNoise(glm::u8vec2) final { return 0; }

	float step {0.0f};
};
} // namespace

class MagicPileHeight: public ::testing::Test
{
protected:
	void SetUp() override
	{
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		_land = &static_cast<SlopedLand&>(Locator::terrainSystem::emplace<SlopedLand>());
	}
	void TearDown() override
	{
		Locator::terrainSystem::reset();
		Locator::entitiesRegistry::reset();
	}

	entt::entity Thing(glm::vec3 at, bool magicPile)
	{
		auto& registry = Locator::entitiesRegistry::value();
		const auto entity = registry.Create();
		registry.Assign<ecs::components::Transform>(entity, at, glm::mat3(1.0f), glm::vec3(1.0f));
		if (magicPile)
		{
			registry.Assign<ecs::components::MagicPile>(entity);
		}
		return entity;
	}
	[[nodiscard]] static float HeightOf(entt::entity entity)
	{
		return Locator::entitiesRegistry::value().Get<const ecs::components::Transform>(entity).position.y;
	}

	SlopedLand* _land {nullptr};
	ecs::systems::MiracleFxSystem _fx;
};

TEST_F(MagicPileHeight, TheTurnPutsAMiraclesPileOnTheLandAsTheLandIsNow)
{
	const auto pile = Thing({12.0f, 0.0f, 5.0f}, true);
	_fx.PlacePilesOnLand();
	EXPECT_EQ(HeightOf(pile), 12.0f);
	// The land rises under it: the next turn has the pile on the new land
	_land->step = 3.0f;
	_fx.PlacePilesOnLand();
	EXPECT_EQ(HeightOf(pile), 15.0f);
}

TEST_F(MagicPileHeight, OtherThingsKeepTheirHeight)
{
	const auto pot = Thing({12.0f, 4.0f, 5.0f}, false);
	_fx.PlacePilesOnLand();
	EXPECT_EQ(HeightOf(pot), 4.0f);
}
