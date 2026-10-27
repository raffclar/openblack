/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstddef>

#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#define LOCATOR_IMPLEMENTATIONS
#include <3D/GripLandscapeEffect.h>
#include <Common/RandomNumberManagerTesting.h>
#include <ECS/Components/GripLandscapeParticle.h>
#include <ECS/Components/Sprite.h>
#include <ECS/Components/Transform.h>
#include <ECS/Registry.h>
#include <Locator.h>

using namespace openblack;
using namespace openblack::ecs::components;

namespace
{
constexpr glm::vec3 k_Centre {100.0f, 10.0f, 100.0f};
constexpr glm::vec3 k_Offset {0.5f, 0.25f, -0.5f};
// The steps of the turns that the test's sprite has
constexpr int32_t k_Turns = 3;

class GripLandscapeEffectTest: public testing::Test
{
protected:
	void SetUp() override
	{
		auto& registry = Locator::entitiesRegistry::emplace<ecs::Registry>();
		_sprite = registry.Create();
		registry.Assign<GripLandscapeParticle>(_sprite, k_Centre, k_Offset, 0.0f, 3.0f, k_Turns);
		registry.Assign<Sprite>(_sprite, 0u, glm::vec2(0.0f), glm::vec2(0.125f), glm::vec4(1.0f), false);
		registry.Assign<Transform>(_sprite, k_Centre, glm::mat3(1.0f), glm::vec3(0.0f));
	}

	void TearDown() override
	{
		Locator::rng::reset();
		Locator::entitiesRegistry::reset();
	}

	entt::entity _sprite {entt::null};
};
} // namespace

TEST_F(GripLandscapeEffectTest, TheDustStandsStillWhileTheGameIsPaused)
{
	auto& registry = Locator::entitiesRegistry::value();
	GripLandscapeEffect::Update(0.25f);
	const auto particle = registry.Get<GripLandscapeParticle>(_sprite);
	const auto transform = registry.Get<Transform>(_sprite);
	const auto sprite = registry.Get<Sprite>(_sprite);
	// Frames of no game time
	for (int frame = 0; frame < 100; ++frame)
	{
		GripLandscapeEffect::Update(0.0f);
	}
	const auto& still = registry.Get<GripLandscapeParticle>(_sprite);
	EXPECT_EQ(still.age, particle.age);
	EXPECT_EQ(still.frame, particle.frame);
	EXPECT_EQ(registry.Get<Transform>(_sprite).position, transform.position);
	EXPECT_EQ(registry.Get<Sprite>(_sprite).tint, sprite.tint);
}

TEST_F(GripLandscapeEffectTest, TheDustGrowsAndFadesByTheGameTime)
{
	auto& registry = Locator::entitiesRegistry::value();
	GripLandscapeEffect::Update(0.5f);
	const auto& particle = registry.Get<GripLandscapeParticle>(_sprite);
	EXPECT_FLOAT_EQ(particle.age, 0.5f);
	EXPECT_GT(registry.Get<Transform>(_sprite).scale.x, 0.0f);
	EXPECT_LT(registry.Get<Sprite>(_sprite).tint.a, 78.0f / 255.0f);
	// The frames do not remove it, also when it is older than the age at which it goes
	GripLandscapeEffect::Update(3.0f);
	EXPECT_TRUE(registry.Valid(_sprite));
}

TEST_F(GripLandscapeEffectTest, TheTurnsRemoveTheDustWhenItsTurnsAreDone)
{
	auto& registry = Locator::entitiesRegistry::value();
	for (int turn = 1; turn < k_Turns; ++turn)
	{
		GripLandscapeEffect::ProcessTurn();
		ASSERT_TRUE(registry.Valid(_sprite)) << turn;
	}
	GripLandscapeEffect::ProcessTurn();
	EXPECT_FALSE(registry.Valid(_sprite));
}

TEST(GripLandscapeEffect, APuffLivesForTheTurnOfItsGripThenUntilTheTurnsReachItsAge)
{
	// 2.65752 seconds is between 26 and 27 turns of a tenth of a second: the step of the grip's turn, then 27 more
	EXPECT_EQ(GripLandscapeEffect::LifeTurns(GripLandscapeEffect::k_DieSeconds, 0.1f), 28);
	EXPECT_EQ(GripLandscapeEffect::LifeTurns(1.0f, 0.5f), 3);
	EXPECT_EQ(GripLandscapeEffect::LifeTurns(1.1f, 0.5f), 4);
	EXPECT_EQ(GripLandscapeEffect::LifeTurns(0.0f, 0.1f), 1);
}

TEST(GripLandscapeEffect, AGripMakesEightSpritesWithNoPicturesLoaded)
{
	// The game server loads no pictures, and it must make the same entities as the clients
	auto& registry = Locator::entitiesRegistry::emplace<ecs::Registry>();
	Locator::rng::emplace<RandomNumberManagerTesting>();
	GripLandscapeEffect::Spawn(k_Centre);
	size_t sprites = 0;
	registry.Each<const GripLandscapeParticle>([&sprites](const GripLandscapeParticle& particle) {
		++sprites;
		EXPECT_GT(particle.centre.y, k_Centre.y);
		EXPECT_EQ(particle.centre.x, k_Centre.x);
		EXPECT_EQ(particle.turnsLeft, GripLandscapeEffect::LifeTurns(GripLandscapeEffect::k_DieSeconds, 0.1f));
	});
	EXPECT_EQ(sprites, 8u);
	Locator::rng::reset();
	Locator::entitiesRegistry::reset();
}
