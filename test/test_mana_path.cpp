/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The mana path the hand lets out past the border: its maths, and the rule with the game's file

#include <cmath>

#include <memory>
#include <numbers>
#include <optional>
#include <string>
#include <utility>

#include <ParticleFile.h>
#include <gtest/gtest.h>

#include "Common/GameRandom.h"
#include "Particles/ManaPathMaths.h"
#include "Particles/ParticleClassRegistry.h"
#include "Particles/ParticleEffect.h"
#include "Particles/ParticleMaths.h"

using namespace openblack;
using namespace openblack::particles;
using namespace openblack::particles::mana_path;

namespace
{
constexpr Settings k_Steady {.speed = 40.0f,
                             .timeToTravel = 3.0f,
                             .noiseFrequency = 0.5f,
                             .noiseAmplitude = 6.0f,
                             .height = 0.4f,
                             .constantSpeed = true};
constexpr Settings k_Curved {.speed = 40.0f,
                             .timeToTravel = 3.0f,
                             .noiseFrequency = 0.5f,
                             .noiseAmplitude = 6.0f,
                             .height = 0.4f,
                             .constantSpeed = false};
constexpr Draws k_Draws {.frequency = 0.4f, .amplitude = 0.2f, .phase = 0.5f};
constexpr float k_Step = 0.1f;
constexpr float k_Ground = 7.0f;

class FakeWorld final: public ParticleWorldInterface
{
public:
	[[nodiscard]] float LandHeight(glm::vec2 /*xz*/) const override { return k_Ground; }
	[[nodiscard]] uint32_t PlayerColour(int /*player*/) const override { return 0xFF102030u; }
	[[nodiscard]] glm::vec3 CameraRight() const override { return {1.0f, 0.0f, 0.0f}; }
	[[nodiscard]] glm::vec3 CameraUp() const override { return {0.0f, 1.0f, 0.0f}; }
	[[nodiscard]] std::optional<TargetInfo> Target(entt::entity /*target*/, bool /*centre*/) const override
	{
		return std::nullopt;
	}
};

class FakeRandom final: public GameRandomInterface
{
public:
	uint32_t GameRand(uint32_t n) override { return n == 0 ? 0 : game_random::LHRand(n, _seeds.synced); }
	float GameFloatRand(float x) override { return game_random::FloatRand(x, _seeds.synced); }
	uint32_t LocalRand(int32_t n) override { return n == 0 ? 0 : game_random::LHRand(static_cast<uint32_t>(n), _seeds.local); }
	float LocalFloatRand(float x) override { return game_random::FloatRand(x, _seeds.local); }
	int32_t CrtRand() override { return game_random::CrtRand(_crt); }
	void CrtSrand(uint32_t seed) override { _crt = seed; }
	[[nodiscard]] GameRandomSeeds GetSeeds() const override { return _seeds; }
	void SetSeeds(GameRandomSeeds seeds) override { _seeds = seeds; }
	[[nodiscard]] ParticleRandomStream GetParticleStream() const override { return _stream; }
	void SetParticleStream(ParticleRandomStream stream) override { _stream = stream; }

private:
	GameRandomSeeds _seeds;
	ParticleRandomStream _stream {ParticleRandomStream::None};
	uint32_t _crt {1};
};

/// The mana path's file as the game has it
std::string ManaPathFile()
{
	return "BEGINPROPERTIES\nPROPERTY DeleteOnCloseDown BOOL 1\nPROPERTY Hierarchies ARRAY SIZE 2 0 0\n"
	       "PROPERTY InitiallyCreated ARRAY SIZE 2 1 0\nPROPERTY MaxSpellAge FLOAT -1\nENDPROPERTIES\n"
	       "BEGINCLASS UR_ManaPathNew UR_ManaPathNew0\nBEGINPROPERTIES\n"
	       "PROPERTY Condition PERSIS_PNTR NULL_STRING\nPROPERTY Gain FLOAT 0.827434\nPROPERTY GlowLength FLOAT 5.24734\n"
	       "PROPERTY GlowPosSpeed FLOAT 40\nPROPERTY Group INTEGER 0\nPROPERTY Height FLOAT 0.4\n"
	       "PROPERTY NextGroups ARRAY SIZE 0\nPROPERTY NoiseAmplitude FLOAT 5.9885\n"
	       "PROPERTY NoiseFrequency FLOAT 0.0893805\nPROPERTY PCreator PERSIS_PNTR ParticleSpriteCreatorGood\n"
	       "PROPERTY RemoveOnCloseDown BOOL 0\nPROPERTY TimeToTravel FLOAT 3\nPROPERTY UseConstantSpeed BOOL 1\n"
	       "ENDPROPERTIES\nENDCLASS\n"
	       "BEGINCLASS ParticleSpriteCreator ParticleSpriteCreatorGood\nBEGINPROPERTIES\n"
	       "PROPERTY CentreAtBase BOOL 0\nPROPERTY ColorA INTEGER 15\nPROPERTY ColorB INTEGER 255\n"
	       "PROPERTY ColorG INTEGER 46\nPROPERTY ColorR INTEGER 255\nPROPERTY FrameRate FLOAT 24\n"
	       "PROPERTY InitFrame INTEGER 0\nPROPERTY InitialScale FLOAT 2\nPROPERTY LoopAnim BOOL 1\n"
	       "PROPERTY MaterialUpdateZBuffer BOOL 0\nPROPERTY NumFrames INTEGER 8\nPROPERTY NumSpritesPerRow INTEGER 8\n"
	       "PROPERTY PlayAnim BOOL 1\nPROPERTY RandomiseFrameDirection BOOL 1\nPROPERTY RandomiseInitFrame BOOL 1\n"
	       "PROPERTY RandomiseScale BOOL 0\nPROPERTY ScaleAlpha INTEGER 255\nPROPERTY SetHorozontal BOOL 1\n"
	       "PROPERTY SetVertical BOOL 0\nPROPERTY SpriteOriginX FLOAT 0\nPROPERTY SpriteOriginY FLOAT 0\n"
	       "PROPERTY StretchVertically FLOAT 3\nPROPERTY FileOffset INTEGER 40\n"
	       "PROPERTY TextureFileName STRING .\\Data\\Textures\\S_SpriteSheet2.raw\nPROPERTY UseAdditiveAlpha BOOL 1\n"
	       "PROPERTY UsePlayerColor BOOL 0\nENDPROPERTIES\nENDCLASS\n";
}

class ManaPathRuleTest: public ::testing::Test
{
protected:
	std::unique_ptr<Effect> Make()
	{
		auto file = psys::ParticleFile::Parse(ManaPathFile());
		EXPECT_TRUE(file.has_value());
		return std::make_unique<Effect>(std::make_shared<const psys::ParticleFile>(std::move(*file)),
		                                EffectServices {classes, world, random, noise}, glm::vec3(0.0f), 1.0f, false);
	}

	ParticleClassRegistry classes {ParticleClassRegistry::WithAllClasses()};
	FakeWorld world;
	FakeRandom random;
	maths::ValueNoise noise;
};
} // namespace

TEST(ManaPath, APathKnowsItsLengthAcrossTheGroundAndTheWayAcross)
{
	const auto path = MakePath({10.0f, 5.0f, 20.0f}, {13.0f, 50.0f, 24.0f}, k_Steady, k_Draws);
	EXPECT_FLOAT_EQ(path.length, 5.0f);
	// A quarter turn from the way along (0.6, 0.8)
	EXPECT_FLOAT_EQ(path.side.x, 0.8f);
	EXPECT_FLOAT_EQ(path.side.y, 0.0f);
	EXPECT_FLOAT_EQ(path.side.z, -0.6f);
	// Each spark weaves by its own share of 0.6 to 1.4 of the file's frequency and size, starting along the noise by
	// its draw out of 256
	EXPECT_FLOAT_EQ(path.frequency, 0.5f);
	EXPECT_FLOAT_EQ(path.amplitude, 4.8f);
	EXPECT_FLOAT_EQ(path.phase, 128.0f);
}

TEST(ManaPath, ASparkIsTurnedAlongItsPathAQuarterTurnRound)
{
	const auto path = MakePath({0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 10.0f}, k_Steady, k_Draws);
	EXPECT_FLOAT_EQ(Heading(path), std::numbers::pi_v<float>);
}

TEST(ManaPath, ASteadySparkCrossesItsPathAtItsSpeed)
{
	const auto path = MakePath({0.0f, 0.0f, 0.0f}, {80.0f, 0.0f, 0.0f}, k_Steady, k_Draws);
	EXPECT_FLOAT_EQ(Progress(path, k_Steady, 1.0f), 0.5f);
	// The point is held to the path's ends
	EXPECT_FLOAT_EQ(PointOnPath(path, k_Steady, 0.25f).x, 20.0f);
	EXPECT_FLOAT_EQ(PointOnPath(path, k_Steady, 1.5f).x, 80.0f);
	EXPECT_FLOAT_EQ(PointOnPath(path, k_Steady, -1.0f).x, 0.0f);
}

TEST(ManaPath, ACurvedSparkTakesItsTimeAndLeavesAndArrivesAlongThePath)
{
	const auto path = MakePath({0.0f, 0.0f, 0.0f}, {120.0f, 0.0f, 0.0f}, k_Curved, k_Draws);
	EXPECT_FLOAT_EQ(Progress(path, k_Curved, 1.5f), 0.5f);
	// Its ends, and with tangents as long as the path the curve runs evenly
	EXPECT_NEAR(PointOnPath(path, k_Curved, 0.0f).x, 0.0f, 1e-4f);
	EXPECT_NEAR(PointOnPath(path, k_Curved, 1.0f).x, 120.0f, 1e-3f);
	EXPECT_NEAR(PointOnPath(path, k_Curved, 0.25f).x, 30.0f, 1e-3f);
	// The curve is not held to its ends
	EXPECT_GT(PointOnPath(path, k_Curved, 1.2f).x, 120.0f);
}

TEST(ManaPath, TheWeaveGrowsHoldsAndDiesAway)
{
	EXPECT_FLOAT_EQ(WeaveShare(-0.1f), 0.0f);
	EXPECT_FLOAT_EQ(WeaveShare(0.0f), 0.0f);
	EXPECT_FLOAT_EQ(WeaveShare(0.1f), 0.5f);
	EXPECT_FLOAT_EQ(WeaveShare(0.5f), 1.0f);
	EXPECT_FLOAT_EQ(WeaveShare(0.8f), 1.0f);
	EXPECT_NEAR(WeaveShare(0.9f), 0.5f, 1e-5f);
	EXPECT_NEAR(WeaveShare(1.0f), 0.0f, 1e-5f);
	EXPECT_FLOAT_EQ(WeaveShare(1.1f), 0.0f);
}

TEST(ManaPath, ASparkWeavesAcrossItsPathByTheNoise)
{
	const auto path = MakePath({0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 10.0f}, k_Steady, k_Draws);
	// The noise is read along the path by its frequency over its length, from its phase
	EXPECT_FLOAT_EQ(WeaveNoiseAt(path, 0.5f), 0.5f * 10.0f * 0.5f + 128.0f);
	// Across a path along z is along x
	const auto woven = Woven(path, {0.0f, 0.0f, 5.0f}, 0.5f, 0.5f);
	EXPECT_FLOAT_EQ(woven.x, 0.5f * 4.8f);
	EXPECT_FLOAT_EQ(woven.y, 5.0f);
	// Not at all where it starts
	const auto start = Woven(path, {0.0f, 0.0f, 0.0f}, 0.0f, 1.0f);
	EXPECT_FLOAT_EQ(start.x, 0.0f);
}

TEST(ManaPath, TheGroundIsReadOnTheMapsFixedGrid)
{
	// 1 m is 6553.6 steps, truncated to 6553
	EXPECT_FLOAT_EQ(GroundPoint({1.0f, 0.0f}).x, 6553.0f * 10.0f / 65536.0f);
	EXPECT_FLOAT_EQ(GroundPoint({0.0f, 10.0f}).y, 10.0f);
}

TEST_F(ManaPathRuleTest, ASparkRunsFromTheHandOverTheLandAndGoesAfterArriving)
{
	auto effect = Make();
	EXPECT_TRUE(effect->UnportedClasses().empty());
	// 80 m at 40 m a second: two seconds
	effect->AddManaPathSpark({.from = {100.0f, 30.0f, 100.0f}, .to = {180.0f, 0.0f, 100.0f}, .rgb = 0x204060u});
	effect->Step(k_Step);
	ASSERT_EQ(effect->AtomCount(), 1u);
	for (int i = 0; i < 19; ++i)
	{
		effect->Step(k_Step);
	}
	// Still on its way, a little above the land
	ASSERT_EQ(effect->AtomCount(), 1u);
	Effect::DrawWalk walk;
	effect->Walk(1.0f, walk);
	ASSERT_EQ(walk.atoms.size(), 1u);
	EXPECT_NEAR(walk.atoms[0].position.y, k_Ground + 0.4f, 1e-3f);
	EXPECT_GT(walk.atoms[0].position.x, 150.0f);
	// Its colour is the spark's, its opacity the creator's
	EXPECT_EQ(walk.atoms[0].rgb[0], 0x20);
	EXPECT_EQ(walk.atoms[0].rgb[1], 0x40);
	EXPECT_EQ(walk.atoms[0].rgb[2], 0x60);
	// Past its end, then gone the step after
	effect->Step(k_Step);
	effect->Step(k_Step);
	effect->Step(k_Step);
	EXPECT_EQ(effect->AtomCount(), 0u);
}

TEST_F(ManaPathRuleTest, NoMoreThanAHundredSparksAtOnce)
{
	auto effect = Make();
	for (int i = 0; i < 120; ++i)
	{
		effect->AddManaPathSpark({.from = {0.0f, 0.0f, 0.0f}, .to = {100.0f, 0.0f, 0.0f}, .rgb = 0xFFFFFFu});
	}
	effect->Step(k_Step);
	EXPECT_EQ(effect->AtomCount(), 100u);
	// The ones let go are not kept for later
	effect->Step(k_Step);
	EXPECT_EQ(effect->AtomCount(), 100u);
}

TEST_F(ManaPathRuleTest, ASparkWithNoWayToGoIsNone)
{
	auto effect = Make();
	effect->AddManaPathSpark({.from = {5.0f, 0.0f, 5.0f}, .to = {5.005f, 3.0f, 5.0f}, .rgb = 0xFFFFFFu});
	effect->Step(k_Step);
	EXPECT_EQ(effect->AtomCount(), 0u);
}
