/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS 1

#include <cstdint>

#include <array>
#include <memory>
#include <optional>
#include <span>

#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "3D/MapCoords.h"
#include "Common/GameRandom.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/ForestMember.h"
#include "ECS/Components/LandForest.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/AlignmentSystem.h"
#include "ECS/Systems/Implementations/FieldSystem.h"
#include "ECS/Systems/Implementations/ForestSystem.h"
#include "ECS/Systems/Implementations/TimeSystem.h"
#include "ECS/Systems/InfluenceSystemInterface.h"
#include "ECS/ThingMapPlace.h"
#include "InfoConstants.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::components;

namespace
{
/// A map position far from the corner of the map, which metres cannot keep: its metres give the next map position
constexpr int32_t k_Far = 0x01000001;
/// A z that metres keep
constexpr int32_t k_Z = 0x00100000;
constexpr uint32_t k_ForestId = 3;

/// No influence anywhere. It keeps the last map position where a question asked for the influence.
class AskedInfluence final: public ecs::systems::InfluenceSystemInterface
{
public:
	void Reset() override {}
	void ProcessTurn(uint32_t) override {}
	void BordersChanged() override {}
	void Update(std::chrono::duration<float, std::milli>) override {}
	void ShowHandInfluence(std::chrono::duration<float, std::milli>) override {}
	[[nodiscard]] float PlayerInfluence(PlayerNames, const map_coords::MapCoords& position) const override
	{
		askedAt = position;
		return 0.0f;
	}
	[[nodiscard]] float HandPointInfluence(PlayerNames, const map_coords::MapCoords&) const override { return 0.0f; }
	[[nodiscard]] float PlayerRawInfluence(PlayerNames, const map_coords::MapCoords&) const override { return 0.0f; }
	void HeldThingUsedOnLand(PlayerNames) override {}
	void SetInGameTurn(bool) override {}
	void FixCitadelReaches() override {}
	[[nodiscard]] std::span<const influence::Circle> GetCircles() const override { return {}; }
	[[nodiscard]] bool IsBorderShown(PlayerNames) const override { return false; }
	[[nodiscard]] glm::vec2 GetScrollOffset() const override { return {}; }
	[[nodiscard]] std::span<const influence::Ripple> GetRipples() const override { return {}; }

	mutable std::optional<map_coords::MapCoords> askedAt;
};

/// Each draw gives 0
class ZeroRandom final: public GameRandomInterface
{
public:
	uint32_t SyncedRand(uint32_t, DrawSite) override { return 0; }
	float SyncedFloatRand(float, DrawSite) override { return 0.0f; }
	uint32_t LocalRand(int32_t) override { return 0; }
	float LocalFloatRand(float) override { return 0.0f; }
	int32_t CrtRand() override { return 0; }
	void CrtSrand(uint32_t) override {}
	[[nodiscard]] GameRandomSeeds GetSeeds() const override { return {}; }
	void SetSeeds(GameRandomSeeds) override {}
	[[nodiscard]] ParticleRandomStream GetParticleStream() const override { return ParticleRandomStream::None; }
	void SetParticleStream(ParticleRandomStream) override {}
};

/// A tree and a field grow by the weather and the alignment of the land. Both come from the map position of the thing,
/// which can be one unit away from the map position that its metres give.
class GrowthMapPlaces: public ::testing::Test
{
protected:
	void SetUp() override
	{
		Locator::entitiesRegistry::emplace<ecs::Registry>();
		_info = std::make_unique<InfoConstants>();
		auto& beech = _info->tree.at(static_cast<size_t>(TreeInfo::Beech));
		beech.growsAfterNumGameTurns = 5;
		beech.growthAmount = 0.1f;
		beech.rainingAcceleratorMultiplier = 1.0f;
		auto& wheat = _info->fieldType.at(static_cast<size_t>(FieldTypeInfo::Wheat));
		wheat.timesToSow = 1.0f;
		wheat.ageGrowth = 5.0f;
		wheat.ageRecolt = 10.0f;
		wheat.totalFoodInField = 200.0f;
		wheat.ratioBeforeRipe = 0.5f;
		Locator::infoConstants::emplace(*_info);
		_influence = &static_cast<AskedInfluence&>(Locator::influenceSystem::emplace<AskedInfluence>());
		Locator::alignmentSystem::emplace<ecs::systems::AlignmentSystem>();
		Locator::time::emplace<ecs::systems::TimeSystem>([] { return uint32_t {0}; });
		Locator::gameRandom::emplace<ZeroRandom>();
	}
	void TearDown() override
	{
		Locator::gameRandom::reset();
		Locator::time::reset();
		Locator::alignmentSystem::reset();
		Locator::influenceSystem::reset();
		Locator::infoConstants::reset();
		Locator::entitiesRegistry::reset();
	}

	/// A thing at the metres of a map position that it keeps
	static entt::entity At(const map_coords::MapCoords& place)
	{
		auto& registry = Locator::entitiesRegistry::value();
		const auto thing = registry.Create();
		const auto xz = map_coords::ToMetres(place);
		registry.Assign<Transform>(thing, glm::vec3(xz.x, 0.0f, xz.y), glm::mat3(1.0f), glm::vec3(0.5f));
		ecs::thing_map_place::Keep(registry, thing, place);
		return thing;
	}

	std::unique_ptr<InfoConstants> _info;
	AskedInfluence* _influence {nullptr};
};

TEST_F(GrowthMapPlaces, AGrowingTreeTakesTheAlignmentAtItsMapPosition)
{
	auto& registry = Locator::entitiesRegistry::value();
	const map_coords::MapCoords place {.x = k_Far, .z = k_Z};
	ASSERT_EQ(map_coords::FromMetres(map_coords::ToMetres(place)).x, k_Far + 1);
	const auto forest = At({.x = k_Far + 3, .z = k_Z});
	registry.Assign<LandForest>(forest, LandForest {.id = k_ForestId});
	registry.Assign<ForestTurns>(forest, ForestTurns {.id = k_ForestId});
	const auto tree = At(place);
	// Its clock runs out this turn, and it is short of its largest size
	registry.Assign<Tree>(tree, Tree {.type = TreeInfo::Beech, .maxSize = 1.0f, .growthCountdown = 1, .madeToGrow = true});
	registry.Assign<ForestMember>(tree, ForestMember {.forest = k_ForestId, .growing = true, .listed = 0});

	ecs::systems::ForestSystem forests;
	forests.GrowForests();
	EXPECT_FLOAT_EQ(registry.Get<const Transform>(tree).scale.x, 0.6f);
	ASSERT_TRUE(_influence->askedAt.has_value());
	EXPECT_EQ(_influence->askedAt->x, k_Far);
	EXPECT_EQ(_influence->askedAt->z, k_Z);
}

TEST_F(GrowthMapPlaces, AGrowingFieldTakesTheAlignmentAtItsMapPosition)
{
	auto& registry = Locator::entitiesRegistry::value();
	const map_coords::MapCoords place {.x = k_Far, .z = k_Z};
	const auto field = At(place);
	auto& data = registry.Assign<Field>(field);
	data.town = -1;
	data.type = FieldTypeInfo::Wheat;
	data.crop = {.timesSown = 1, .age = 1.0f, .food = 0.0f};

	ecs::systems::FieldSystem fields;
	// The crop grows in the passes of the turns that are a multiple of the turns between its growth
	fields.ProcessField(field, 0);
	ASSERT_TRUE(_influence->askedAt.has_value());
	EXPECT_EQ(_influence->askedAt->x, k_Far);
	EXPECT_EQ(_influence->askedAt->z, k_Z);
}
} // namespace
