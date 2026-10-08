/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <functional>

#include <gtest/gtest.h>

#include "3D/CreatureCaveEffects.h"
#include "3D/TempleSounds.h"

using namespace openblack;

namespace
{
/// The bounds' middle, counting the draws
struct FakeRandom
{
	int draws {0};
	float operator()(float min, float max)
	{
		++draws;
		return (min + max) * 0.5f;
	}
};
} // namespace

TEST(CreatureCaveEffects, SmokeFadesAsItAges)
{
	// Steady at 79 until a quarter of its life, then fading to nothing at its end
	EXPECT_EQ(CreatureCaveEffects::SmokeAlpha(0, false), 0x4f);
	EXPECT_EQ(CreatureCaveEffects::SmokeAlpha(225, false), 0x4f);
	EXPECT_EQ(CreatureCaveEffects::SmokeAlpha(900, false), 0);
	// Smoke thrown up fades in first
	EXPECT_EQ(CreatureCaveEffects::SmokeAlpha(0, true), 0);
	EXPECT_EQ(CreatureCaveEffects::SmokeAlpha(50, true), 39);
	EXPECT_EQ(CreatureCaveEffects::SmokeAlpha(100, true), 0x4f);
}

TEST(CreatureCaveEffects, FlamesRunBackThroughTheirFramesAQuarterApart)
{
	EXPECT_EQ(CreatureCaveEffects::FlameFrame(0, 0), 31u);
	EXPECT_EQ(CreatureCaveEffects::FlameFrame(32, 0), 30u);
	EXPECT_EQ(CreatureCaveEffects::FlameFrame(0, 1), 23u);
	EXPECT_EQ(CreatureCaveEffects::FlameFrame(32 * 32, 0), 31u);
}

TEST(CreatureCaveEffects, SmokeDriftsUpAndStartsOverAtItsPlace)
{
	CreatureCaveEffects::Smoke smoke;
	smoke.place = {10.0f, 0.0f, 0.0f};
	for (auto& particle : smoke.particles)
	{
		particle.age = 0;
		particle.position = smoke.place;
	}
	// A second: 255 steps of age, rising at 2.55 a second
	FakeRandom random;
	CreatureCaveEffects::StepSmoke(smoke, 1000, std::ref(random));
	EXPECT_EQ(smoke.particles[0].age, 255);
	EXPECT_NEAR(smoke.particles[0].position.y, 2.55f, 1e-4f);
	EXPECT_TRUE(smoke.particles[0].hidden);
	// Past its life it starts over, seen, from the smoke's place, as far on as it went past
	smoke.particles[0].age = 890;
	smoke.particles[0].position = {50.0f, 50.0f, 50.0f};
	CreatureCaveEffects::StepSmoke(smoke, 100, std::ref(random));
	EXPECT_EQ(smoke.particles[0].age, (890 + 25) % 900);
	EXPECT_FALSE(smoke.particles[0].hidden);
	EXPECT_NEAR(smoke.particles[0].position.x, 10.0f, 1e-4f);
	EXPECT_NEAR(smoke.particles[0].position.y, 15.0f / 255.0f * 2.55f, 1e-4f);
	// Smoke that is not thrown up draws nothing as it starts over
	EXPECT_EQ(random.draws, 0);
}

TEST(CreatureCaveEffects, AgeKeepsWhatAFrameLeavesOver)
{
	CreatureCaveEffects::Smoke smoke;
	for (auto& particle : smoke.particles)
	{
		particle.age = 0;
	}
	FakeRandom random;
	// 2 ms is half a step of age: two frames of it make one
	CreatureCaveEffects::StepSmoke(smoke, 2, std::ref(random));
	EXPECT_EQ(smoke.particles[0].age, 0);
	CreatureCaveEffects::StepSmoke(smoke, 2, std::ref(random));
	EXPECT_EQ(smoke.particles[0].age, 1);
}

TEST(CreatureCaveEffects, RisingSmokeIsThrownUpAsItStartsOver)
{
	CreatureCaveEffects::Smoke smoke;
	smoke.rising = true;
	smoke.drift = glm::vec3(0.0f);
	for (auto& particle : smoke.particles)
	{
		particle.age = 0;
	}
	smoke.particles[0].age = 899;
	FakeRandom random;
	CreatureCaveEffects::StepSmoke(smoke, 100, std::ref(random));
	// Three draws for the one particle that started over, thrown up at the middle of 3 to 4
	EXPECT_EQ(random.draws, 3);
	EXPECT_FALSE(smoke.particles[0].hidden);
	EXPECT_TRUE(smoke.particles[1].hidden);
	const float time = 24.0f / 255.0f;
	EXPECT_NEAR(smoke.particles[0].position.y, (3.5f * time) - (0.5f * 2.5f * 1.5f * time * time), 1e-4f);
}

TEST(CreatureCaveEffects, TheRoomsSoundsAtTheFireAndTheWaterfall)
{
	const glm::vec3 fire {1.0f, 2.0f, 3.0f};
	const auto fireSound = temple_sounds::CreatureCaveFire(fire);
	EXPECT_EQ(fireSound.sound, static_cast<entt::id_type>(audio::SoundId::G_FireCreatureCave_01));
	EXPECT_TRUE(fireSound.is3D);
	EXPECT_FALSE(fireSound.track);
	EXPECT_EQ(fireSound.position, fire);
	const auto water = temple_sounds::CreatureCaveWater();
	EXPECT_EQ(water.sound, static_cast<entt::id_type>(audio::SoundId::G_WaterCreatureCave_01));
	EXPECT_TRUE(water.is3D);
	EXPECT_EQ(water.position, glm::vec3(160.0f, -45.0f, -30.0f));
	// Played every frame the room is drawn, they carry on while they play
	EXPECT_EQ(fireSound.mode, 2);
	EXPECT_EQ(water.mode, 2);
}
