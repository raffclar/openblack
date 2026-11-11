/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <vector>

#include <gtest/gtest.h>

#include "Common/GameRandom.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureSkin.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/MagicLiving.h"
#include "Locator.h"
#include "Magic/MagicWorldInterface.h"
#include "Magic/SpellRules.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;

namespace
{

/// The game's random numbers: each draw gives 0 and is kept with its range, in the order of the draws. A float draw
/// keeps its limit as a negative range.
class RecordedGameRandom final: public GameRandomInterface
{
public:
	uint32_t SyncedRand(uint32_t range, DrawSite /*site*/) override
	{
		draws.push_back(static_cast<float>(range));
		return 0;
	}
	float SyncedFloatRand(float limit, DrawSite /*site*/) override
	{
		draws.push_back(-limit);
		return limit;
	}
	uint32_t LocalRand(int32_t /*n*/) override { return 0; }
	float LocalFloatRand(float /*x*/) override { return 0.0f; }
	int32_t CrtRand() override { return 0; }
	void CrtSrand(uint32_t /*seed*/) override {}
	[[nodiscard]] GameRandomSeeds GetSeeds() const override { return {}; }
	void SetSeeds(GameRandomSeeds /*seeds*/) override {}
	[[nodiscard]] ParticleRandomStream GetParticleStream() const override { return ParticleRandomStream::None; }
	void SetParticleStream(ParticleRandomStream /*stream*/) override {}

	std::vector<float> draws;
};

/// A creature of player one with no posed body, standing at a place on the map
class CreatureScarDraws: public ::testing::Test
{
protected:
	void SetUp() override
	{
		Locator::entitiesRegistry::emplace<Registry>();
		random = &static_cast<RecordedGameRandom&>(Locator::gameRandom::emplace<RecordedGameRandom>());
		auto& registry = Locator::entitiesRegistry::value();
		creature = registry.Create();
		registry.Assign<Creature>(creature, Creature {.owner = PlayerNames::PLAYER_ONE, .species = CreatureType::Cow});
		registry.Assign<Transform>(creature, Transform {.position = {100.0f, 0.0f, 100.0f}});
	}

	void TearDown() override
	{
		Locator::gameRandom::reset();
		Locator::entitiesRegistry::reset();
	}

	RecordedGameRandom* random {nullptr};
	entt::entity creature {entt::null};
};

} // namespace

// A crush from another player draws the cut's column also when the body has no pose. Nothing is marked then.
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(CreatureScarDraws, ACrushDrawsTheCutWithNoPoseAndMarksNothing)
{
	magic::EffectValues values;
	values[magic::EffectKind::Crush] = 0.5f;
	const magic::EffectSource source {.player = PlayerNames::PLAYER_TWO, .point = glm::vec3 {150.0f, 0.0f, 100.0f}};
	EXPECT_FALSE(systems::magic_living::TakesEffectItsOwnWay(creature, values, source));
	EXPECT_EQ(random->draws, (std::vector<float> {5.0f}));
	EXPECT_FALSE(Locator::entitiesRegistry::value().AllOf<CreatureMarks>(creature));
}

// A burn draws its kind and its chance, then the place it comes from, with no pose too
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables): external macro
TEST_F(CreatureScarDraws, ABurnDrawsItsKindChanceAndPlaceWithNoPose)
{
	magic::EffectValues values;
	values[magic::EffectKind::Burn] = 0.5f;
	values[magic::EffectKind::Crush] = 0.25f;
	const magic::EffectSource source {.player = PlayerNames::PLAYER_TWO};
	EXPECT_FALSE(systems::magic_living::TakesEffectItsOwnWay(creature, values, source));
	ASSERT_GE(random->draws.size(), 5U);
	EXPECT_EQ(random->draws[0], 3.0f);
	EXPECT_GT(random->draws[1], 0.0f);
	EXPECT_LT(random->draws[2], 0.0f);
	EXPECT_LT(random->draws[3], 0.0f);
	EXPECT_LT(random->draws[4], 0.0f);
	EXPECT_FALSE(Locator::entitiesRegistry::value().AllOf<CreatureMarks>(creature));
}
