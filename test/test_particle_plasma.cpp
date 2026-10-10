/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The temple heart's plasma beam: its maths, and the rule with the citadel's spell file

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <ParticleFile.h>
#include <glm/geometric.hpp>
#include <gtest/gtest.h>

#include "Common/GameRandom.h"
#include "Particles/BeamMaths.h"
#include "Particles/ParticleClassRegistry.h"
#include "Particles/ParticleCreators.h"
#include "Particles/ParticleEffect.h"
#include "Particles/PlasmaCommand.h"

using namespace openblack;
using namespace openblack::particles;

namespace
{
constexpr float k_Epsilon = 1e-4f;
constexpr float k_Step = 0.1f;

class FakeWorld final: public ParticleWorldInterface
{
public:
	[[nodiscard]] float LandHeight(glm::vec2 /*xz*/) const override { return 0.0f; }
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

/// Every whole-number draw is 0, and each draw of a random fraction gives the next of a script of draws: a centred
/// value, or the most, once the script reaches a marked span. It counts the fractions drawn.
class ScriptedRandom final: public GameRandomInterface
{
public:
	uint32_t GameRand(uint32_t n) override { return Draw(n); }
	float GameFloatRand(float /*x*/) override { return 0.0f; }
	uint32_t LocalRand(int32_t n) override { return Draw(static_cast<uint32_t>(n)); }
	float LocalFloatRand(float /*x*/) override { return 0.0f; }
	int32_t CrtRand() override { return 0; }
	void CrtSrand(uint32_t /*seed*/) override {}
	[[nodiscard]] GameRandomSeeds GetSeeds() const override { return {}; }
	void SetSeeds(GameRandomSeeds /*seeds*/) override {}
	[[nodiscard]] ParticleRandomStream GetParticleStream() const override { return _stream; }
	void SetParticleStream(ParticleRandomStream stream) override { _stream = stream; }

	/// The fractions numbered from `first`, up to but not including `last`, put a point in the ball at x = 1/2
	void Mark(size_t first, size_t last)
	{
		_first = first;
		_last = last;
	}
	[[nodiscard]] size_t Fractions() const { return _fractions; }

private:
	uint32_t Draw(uint32_t n)
	{
		if (n != game_random::k_FloatRandRange)
		{
			return 0;
		}
		const size_t i = _fractions++;
		// A point's x, y and z are drawn in turn; a centred draw puts its axis at the middle
		constexpr uint32_t k_Centre = game_random::k_FloatRandRange / 2;
		constexpr uint32_t k_Half = (game_random::k_FloatRandRange / 4) * 3;
		const bool marked = i >= _first && i < _last;
		return marked && (i - _first) % 3 == 0 ? k_Half : k_Centre;
	}

	ParticleRandomStream _stream {ParticleRandomStream::None};
	size_t _fractions {0};
	size_t _first {0};
	size_t _last {0};
};

/// The citadel's beam file as the game has it
std::string PlasmaFile(const std::string& randomTangents = "0")
{
	return "BEGINPROPERTIES\nPROPERTY DeleteOnCloseDown BOOL 0\nPROPERTY Hierarchies ARRAY SIZE 2 0 0\n"
	       "PROPERTY InitiallyCreated ARRAY SIZE 2 1 0\nPROPERTY MaxSpellAge FLOAT -1\nENDPROPERTIES\n"
	       "BEGINCLASS ParticlePointCreator ParticlePointCreator0\nBEGINPROPERTIES\n"
	       "PROPERTY InitialScale FLOAT 1\nENDPROPERTIES\nENDCLASS\n"
	       "BEGINCLASS UR_Plasma UR_Plasma0\nBEGINPROPERTIES\n"
	       "PROPERTY BeamGroup INTEGER 1\nPROPERTY ChainCreator PERSIS_PNTR ParticleChainCreator0\n"
	       "PROPERTY Condition PERSIS_PNTR NULL_STRING\nPROPERTY ForkScaleMax FLOAT 0.8\nPROPERTY ForkScaleMin FLOAT 0.1\n"
	       "PROPERTY Group INTEGER 0\nPROPERTY JointsPerArc INTEGER 20\nPROPERTY MaxAlpha INTEGER 255\n"
	       "PROPERTY NextGroups ARRAY SIZE 0\nPROPERTY NumBeams INTEGER 2\nPROPERTY NumSplinePoints INTEGER 6\n"
	       "PROPERTY PCreator PERSIS_PNTR ParticlePointCreator0\nPROPERTY RandomFrac FLOAT 3\n"
	       "PROPERTY RandomTangents FLOAT " +
	       randomTangents +
	       "\nPROPERTY RemoveOnCloseDown BOOL 0\nPROPERTY ScaleTangents FLOAT 3.0531\n"
	       "PROPERTY SpeedV FLOAT 0.3\nPROPERTY WiggleFreq FLOAT 3\nPROPERTY WiggleSpeed FLOAT -4\nENDPROPERTIES\nENDCLASS\n"
	       "BEGINCLASS ParticleChainCreator ParticleChainCreator0\nBEGINPROPERTIES\n"
	       "PROPERTY ColorA INTEGER 200\nPROPERTY FrameHeight INTEGER 256\nPROPERTY FrameWidth INTEGER 64\n"
	       "PROPERTY InitialScale FLOAT 3\nPROPERTY NumTexturesForWholeChain INTEGER 4\n"
	       "PROPERTY TextureFileName STRING .\\Data\\Textures\\S_Beam.raw\nPROPERTY UseAdditiveAlpha BOOL 1\n"
	       "ENDPROPERTIES\nENDCLASS\n";
}

PlasmaCommand HeartCommand(glm::vec3 start, glm::vec3 end)
{
	return {.start = start,
	        .end = end,
	        .startTangent = k_HeartPlasmaStartTangent,
	        .endTangent = k_HeartPlasmaEndTangent,
	        .life = k_HeartPlasmaLife,
	        .speed = k_HeartPlasmaSpeed,
	        .alpha = k_HeartPlasmaAlpha};
}

class ParticlePlasmaTest: public ::testing::Test
{
protected:
	std::unique_ptr<Effect> Make(glm::vec3 origin)
	{
		auto file = psys::ParticleFile::Parse(PlasmaFile());
		EXPECT_TRUE(file.has_value());
		return std::make_unique<Effect>(std::make_shared<const psys::ParticleFile>(std::move(*file)),
		                                EffectServices {classes, world, random, noise}, origin, 1.0f, false);
	}

	ParticleClassRegistry classes {ParticleClassRegistry::WithAllClasses()};
	FakeWorld world;
	FakeRandom random;
	maths::ValueNoise noise;
};
} // namespace

TEST(PlasmaMaths, TheAlphaRisesAndFallsOverTheBeamsLife)
{
	EXPECT_EQ(maths::PlasmaAlpha(0.0f, 2.0f, 70, 255), 0);
	EXPECT_EQ(maths::PlasmaAlpha(1.0f, 2.0f, 70, 255), 70);
	EXPECT_EQ(maths::PlasmaAlpha(2.0f, 2.0f, 70, 255), 0);
	// Held at the ends outside its life
	EXPECT_EQ(maths::PlasmaAlpha(-1.0f, 2.0f, 70, 255), 0);
	EXPECT_EQ(maths::PlasmaAlpha(5.0f, 2.0f, 70, 255), 0);
	// A quarter of the way: three quarters of the most, cut down
	EXPECT_EQ(maths::PlasmaAlpha(0.5f, 2.0f, 70, 255), 52);
}

TEST(PlasmaMaths, TheCurveStartsAndEndsAtItsPointsAlongItsTangents)
{
	const glm::vec3 start(0.0f, 10.0f, 0.0f);
	const glm::vec3 end(30.0f, 2.0f, 40.0f);
	const glm::vec3 startTangent(0.0f, 5.0f, 0.0f);
	const glm::vec3 endTangent(0.0f, -5.0f, 0.0f);
	const auto curve = maths::PlasmaCurve(start, end, startTangent, endTangent);
	EXPECT_LT(glm::distance(maths::At(curve, 0.0f), start), k_Epsilon);
	EXPECT_LT(glm::distance(maths::At(curve, 1.0f), end), k_Epsilon);
	// The slope at each end is its tangent: the curve's first coefficient at the start, the sum of three, two and one of
	// them at the end
	EXPECT_LT(glm::distance(curve.c1, startTangent), k_Epsilon);
	EXPECT_LT(glm::distance((curve.c3 * 3.0f) + (curve.c2 * 2.0f) + curve.c1, endTangent), k_Epsilon);
}

TEST(PlasmaMaths, TangentsAreNudgedThenMadeAsLongAsTheBeamTimesTheScale)
{
	const auto [first, second] = maths::PlasmaTangents({0.0f, 0.0f, 0.0f}, {3.0f, 0.0f, 4.0f}, {0.0f, 1.0f, 0.0f},
	                                                   {0.0f, -1.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, 0.5f, 2.0f);
	EXPECT_LT(glm::distance(first, glm::vec3(5.0f, 10.0f, 0.0f)), k_Epsilon);
	EXPECT_LT(glm::distance(second, glm::vec3(0.0f, -10.0f, 5.0f)), k_Epsilon);
}

TEST(PlasmaMaths, KeyPointsBetweenTheEndsArePushedByTheNoiseInItsOrder)
{
	const auto curve = maths::PlasmaCurve({0.0f, 0.0f, 0.0f}, {10.0f, 0.0f, 0.0f}, {10.0f, 0.0f, 0.0f}, {10.0f, 0.0f, 0.0f});
	std::vector<float> asked;
	const auto keys = maths::PlasmaKeyPoints(curve, 3, 3.0f, 2.0f, 1.0f, 4, [&asked](float x) {
		asked.push_back(x);
		return 0.5f;
	});
	ASSERT_EQ(keys.size(), 3u);
	// The ends are the curve's own
	EXPECT_LT(glm::distance(keys.front(), glm::vec3(0.0f)), k_Epsilon);
	EXPECT_LT(glm::distance(keys.back(), glm::vec3(10.0f, 0.0f, 0.0f)), k_Epsilon);
	// The middle across by noise times the amount, the other way the same, upwards by half of the noise plus one
	EXPECT_NEAR(keys[1].x, 5.0f + 1.0f, k_Epsilon);
	EXPECT_NEAR(keys[1].z, 1.0f, k_Epsilon);
	EXPECT_NEAR(keys[1].y, 1.5f, k_Epsilon);
	// Across, the other way across, then upwards, each drifting at its own share
	ASSERT_EQ(asked.size(), 3u);
	EXPECT_NEAR(asked[0], 1.5f + 4.0f + 1.0f, k_Epsilon);
	EXPECT_NEAR(asked[1], 1.5f + 0.7f + 4.0f, k_Epsilon);
	EXPECT_NEAR(asked[2], 1.5f + 1.3f + 4.0f, k_Epsilon);
}

TEST_F(ParticlePlasmaTest, EachBeamIsTwoRibbonsOfTwentyJointsFromStartToEnd)
{
	const glm::vec3 start(100.0f, 30.0f, 100.0f);
	const glm::vec3 end(140.0f, 5.0f, 120.0f);
	auto effect = Make(start);
	EXPECT_TRUE(effect->UnportedClasses().empty());
	effect->SetPlayer(0);
	effect->AddPlasma(HeartCommand(start, end));
	for (int i = 0; i < 10; ++i)
	{
		effect->Step(k_Step);
	}
	// The beam's own atom and the joints of its two ribbons
	EXPECT_EQ(effect->AtomCount(), 1u + (2u * 20u));
	Effect::DrawWalk walk;
	effect->Walk(1.0f, walk);
	ASSERT_EQ(walk.chains.size(), 2u);
	for (const auto& chain : walk.chains)
	{
		ASSERT_EQ(chain.jointCount, 20u);
		const auto first = walk.joints[chain.firstJoint];
		const auto last = walk.joints[chain.firstJoint + chain.jointCount - 1];
		// The first joint made ends at the end and the newest starts at the start, both at the smallest scale
		EXPECT_NEAR(glm::distance(first.position, end), 0.0f, 1e-3f);
		EXPECT_NEAR(glm::distance(last.position, start), 0.0f, 1e-3f);
		EXPECT_NEAR(first.scale, 0.1f * 3.0f, 1e-3f);
		// The texture slides at the beam's speed times the rule's
		EXPECT_NEAR(chain.textureScroll, effect->GetAge() * 1.5f * 0.3f, 1e-3f);
	}
}

TEST_F(ParticlePlasmaTest, TheBeamGoesAFifthOfASecondAfterItsLife)
{
	auto effect = Make({0.0f, 10.0f, 0.0f});
	effect->AddPlasma(HeartCommand({0.0f, 10.0f, 0.0f}, {20.0f, 0.0f, 0.0f}));
	effect->Step(k_Step);
	EXPECT_EQ(effect->AtomCount(), 41u);
	// Its life is two seconds; it goes once its age is past 2.2
	for (int i = 0; i < 21; ++i)
	{
		effect->Step(k_Step);
	}
	EXPECT_EQ(effect->AtomCount(), 41u);
	effect->Step(k_Step);
	effect->Step(k_Step);
	EXPECT_EQ(effect->AtomCount(), 0u);
}

namespace
{
/// The joints of each ribbon ending at `end`, after an effect is given each beam in turn and stepped once after each, then once
/// more
std::vector<std::vector<glm::vec3>> RibbonsEndingAt(ParticleClassRegistry& classes, ParticleWorldInterface& world,
                                                    ScriptedRandom& random, const std::vector<PlasmaCommand>& commands,
                                                    glm::vec3 end)
{
	auto file = psys::ParticleFile::Parse(PlasmaFile("0.5"));
	EXPECT_TRUE(file.has_value());
	maths::ValueNoise noise;
	Effect effect(std::make_shared<const psys::ParticleFile>(std::move(*file)), EffectServices {classes, world, random, noise},
	              commands.front().start, 1.0f, false);
	// Beams asked for in the same step are begun newest first, so each is asked for in a step of its own
	for (const auto& command : commands)
	{
		effect.AddPlasma(command);
		effect.Step(k_Step);
	}
	// And once more, so that the last beam has a pose to be drawn from
	effect.Step(k_Step);
	Effect::DrawWalk walk;
	effect.Walk(1.0f, walk);
	std::vector<std::vector<glm::vec3>> ribbons;
	for (const auto& chain : walk.chains)
	{
		if (chain.jointCount == 0 || glm::distance(walk.joints[chain.firstJoint].position, end) > 1e-3f)
		{
			continue;
		}
		auto& joints = ribbons.emplace_back();
		for (size_t j = 0; j < chain.jointCount; ++j)
		{
			joints.push_back(walk.joints[chain.firstJoint + j].position);
		}
	}
	return ribbons;
}

float Furthest(const std::vector<std::vector<glm::vec3>>& a, const std::vector<std::vector<glm::vec3>>& b)
{
	float furthest = 0.0f;
	for (size_t r = 0; r < std::min(a.size(), b.size()); ++r)
	{
		for (size_t j = 0; j < std::min(a[r].size(), b[r].size()); ++j)
		{
			furthest = std::max(furthest, glm::distance(a[r][j], b[r][j]));
		}
	}
	return furthest;
}
} // namespace

TEST_F(ParticlePlasmaTest, TheNewestBeamTakesTheFirstNudges)
{
	// A beam that ends where it starts is the same whatever its nudges; the other is bent by them
	const glm::vec3 still(0.0f, 10.0f, 0.0f);
	const PlasmaCommand point = HeartCommand(still, still);
	const glm::vec3 end(40.0f, 0.0f, 30.0f);
	const PlasmaCommand beam = HeartCommand({0.0f, 10.0f, 0.0f}, end);

	// Without any nudge, the bent beam asked for last and first; and how many fractions the whole run draws
	ScriptedRandom plainLast;
	const auto unbentLast = RibbonsEndingAt(classes, world, plainLast, {point, beam}, end);
	ASSERT_EQ(unbentLast.size(), 2u);
	ScriptedRandom plainFirst;
	const auto unbentFirst = RibbonsEndingAt(classes, world, plainFirst, {beam, point}, end);
	ASSERT_EQ(unbentFirst.size(), 2u);
	ASSERT_EQ(plainLast.Fractions(), plainFirst.Fractions());
	// Each beam's two ribbons draw a start and an end nudge of three fractions each, last of all in each step
	constexpr size_t k_PerBeam = 2 * 2 * 3;
	ASSERT_GE(plainLast.Fractions(), 2 * k_PerBeam);
	const size_t laying = plainLast.Fractions() - (2 * k_PerBeam);

	// In the last step the first beam laid takes the marked nudges: the newest, so the beam asked for last
	ScriptedRandom newest;
	newest.Mark(laying, laying + k_PerBeam);
	const auto bent = RibbonsEndingAt(classes, world, newest, {point, beam}, end);
	ASSERT_EQ(bent.size(), 2u);
	EXPECT_GT(Furthest(bent, unbentLast), 0.1f);

	ScriptedRandom oldest;
	oldest.Mark(laying, laying + k_PerBeam);
	const auto straight = RibbonsEndingAt(classes, world, oldest, {beam, point}, end);
	ASSERT_EQ(straight.size(), 2u);
	EXPECT_LT(Furthest(straight, unbentFirst), 1e-3f);
}
