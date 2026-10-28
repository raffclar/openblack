/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>
#include <cstdint>

#include <optional>
#include <utility>
#include <vector>

#include <glm/gtc/constants.hpp>
#include <gtest/gtest.h>

#include "3D/TempleMapSpells.h"

using namespace openblack;

namespace
{
/// Records what the seeds do with their effects
class FakeEffects final: public TempleMapSpells::Effects
{
public:
	struct Started
	{
		ParticleType type;
		glm::vec3 point;
		float scale;
		PlayerNames player;
	};
	std::vector<Started> started;
	std::vector<std::pair<uint32_t, float>> steps;
	std::vector<glm::vec3> stepPoints;
	std::vector<uint32_t> stopped;
	uint32_t next {1};

	std::optional<uint32_t> Start(ParticleType type, glm::vec3 point, float scale, PlayerNames player) override
	{
		started.push_back({type, point, scale, player});
		return next++;
	}
	void Step(uint32_t effect, glm::vec3 point, float seconds) override
	{
		steps.emplace_back(effect, seconds);
		stepPoints.push_back(point);
	}
	void Stop(uint32_t effect) override { stopped.push_back(effect); }
};

TempleMapSpells::Cast FireballCast(entt::entity spell)
{
	return {.spell = spell,
	        .seed = SpellSeedType::Fire,
	        .mapPoint = glm::vec3(1.0f, 2.0f, 3.0f),
	        .player = PlayerNames::PLAYER_ONE,
	        .ownerAlignment = 0.5f,
	        .holderParticle = ParticleType::Fireball,
	        .holderHeight = 2.0f,
	        .phialReceiveType = std::nullopt};
}
} // namespace

TEST(TempleMapSpells, ShowsTheSeedHalfAUnitOverTheMap)
{
	TempleMapSpells spells;
	FakeEffects effects;
	const std::vector casts {FireballCast(entt::entity {1})};
	spells.Update(casts, 0.0f, 0.0f, effects);
	ASSERT_EQ(spells.GetSeeds().size(), 1u);
	const auto& seed = spells.GetSeeds().front();
	EXPECT_EQ(seed.seed, SpellSeedType::Fire);
	EXPECT_EQ(seed.middle, glm::vec3(1.0f, 2.5f, 3.0f));
	EXPECT_FLOAT_EQ(seed.spin, 0.0f);
	EXPECT_FLOAT_EQ(seed.ownerAlignment, 0.5f);
	// The holder's effect starts once, at a tenth of its size and height, in the caster's colours
	ASSERT_EQ(effects.started.size(), 1u);
	EXPECT_EQ(effects.started.front().type, ParticleType::Fireball);
	EXPECT_FLOAT_EQ(effects.started.front().scale, 0.1f);
	EXPECT_NEAR(effects.started.front().point.y, 2.5f + 0.2f, 1e-6f);
	EXPECT_EQ(effects.started.front().player, PlayerNames::PLAYER_ONE);
}

TEST(TempleMapSpells, TurnsTheSeedsByTheGameTimeOnly)
{
	TempleMapSpells spells;
	FakeEffects effects;
	const std::vector casts {FireballCast(entt::entity {1})};
	// The world stops in the temple: no game time, no turn
	spells.Update(casts, 0.0f, 0.0f, effects);
	spells.Update(casts, 0.0f, 0.0f, effects);
	EXPECT_FLOAT_EQ(spells.GetSeeds().front().spin, 0.0f);
	// Two radians a second, round the circle
	spells.Update(casts, 0.5f, 0.5f, effects);
	EXPECT_NEAR(spells.GetSeeds().front().spin, 1.0f, 1e-6f);
	spells.Update(casts, 3.0f, 3.0f, effects);
	EXPECT_NEAR(spells.GetSeeds().front().spin, std::fmod(7.0f, glm::two_pi<float>()), 1e-5f);
	// The effect steps by the time it is given, in each frame that steps it
	ASSERT_EQ(effects.steps.size(), 4u);
	EXPECT_FLOAT_EQ(effects.steps.back().second, 3.0f);
	EXPECT_EQ(effects.started.size(), 1u);
}

TEST(TempleMapSpells, TheEffectsDoNotStepInAFrameWithoutTheirStep)
{
	TempleMapSpells spells;
	FakeEffects effects;
	const std::vector casts {FireballCast(entt::entity {1})};
	spells.Update(casts, 0.5f, std::nullopt, effects);
	// The seed turns, but its effect does not step
	EXPECT_NEAR(spells.GetSeeds().front().spin, 1.0f, 1e-6f);
	EXPECT_TRUE(effects.steps.empty());
	spells.Update(casts, 0.0f, 0.25f, effects);
	ASSERT_EQ(effects.steps.size(), 1u);
	EXPECT_FLOAT_EQ(effects.steps.front().second, 0.25f);
}

TEST(TempleMapSpells, FollowsTheMiracleAndDropsItWhenItStops)
{
	TempleMapSpells spells;
	FakeEffects effects;
	auto first = FireballCast(entt::entity {1});
	const auto second = FireballCast(entt::entity {2});
	spells.Update(std::vector {first, second}, 0.0f, 0.0f, effects);
	EXPECT_EQ(spells.GetSeeds().size(), 2u);
	first.mapPoint = glm::vec3(4.0f, 0.0f, 4.0f);
	spells.Update(std::vector {first}, 0.0f, 0.0f, effects);
	ASSERT_EQ(spells.GetSeeds().size(), 1u);
	EXPECT_EQ(spells.GetSeeds().front().middle, glm::vec3(4.0f, 0.5f, 4.0f));
	EXPECT_EQ(effects.stepPoints.back(), glm::vec3(4.0f, 0.7f, 4.0f));
	// The second miracle stopped: its effect stops with its seed
	ASSERT_EQ(effects.stopped.size(), 1u);
	EXPECT_EQ(effects.stopped.front(), 2u);
	spells.Clear(effects);
	EXPECT_TRUE(spells.GetSeeds().empty());
	EXPECT_EQ(effects.stopped.size(), 2u);
}

TEST(TempleMapSpells, PulsesPhialsByTheGameTime)
{
	TempleMapSpells spells;
	FakeEffects effects;
	auto phial = FireballCast(entt::entity {3});
	phial.seed = SpellSeedType::CreatureSpellFreeze;
	phial.holderParticle = ParticleType::None;
	phial.phialReceiveType = 1;
	spells.Update(std::vector {phial}, 0.0f, 0.0f, effects);
	EXPECT_FLOAT_EQ(spells.GetSeeds().front().phialPhase, 0.0f);
	EXPECT_FLOAT_EQ(spells.GetSeeds().front().phialFrame, 0.0f);
	spells.Update(std::vector {phial}, 1.0f, 1.0f, effects);
	EXPECT_GT(spells.GetSeeds().front().phialPhase, 0.0f);
	EXPECT_NE(spells.GetSeeds().front().phialFrame, 0.0f);
	// A seed without a holder's effect starts none
	EXPECT_TRUE(effects.started.empty());
}
