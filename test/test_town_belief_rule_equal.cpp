/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The rule of the symbols over the village centres gives the same symbols as the copy of the rule before it became
// faster. The tests give random villages to the two rules and compare each value of each symbol, bit for bit.

#include <cmath>
#include <cstdint>
#include <cstring>

#include <array>
#include <limits>
#include <memory>
#include <numeric>
#include <optional>
#include <random>
#include <span>
#include <string>
#include <vector>

#include <ParticleFile.h>
#include <entt/meta/context.hpp>
#include <gtest/gtest.h>

#include "Common/GameRandom.h"
#include "Common/PortableMath.h"
#include "ECS/Reflection/BinaryArchive.h"
#include "ECS/Reflection/ComponentReflection.h"
#include "Particles/ParticleArchive.h"
#include "Particles/ParticleClassRegistry.h"
#include "Particles/ParticleEffect.h"
#include "Particles/ParticleMaths.h"
#include "Particles/TownBeliefMaths.h"
#include "TownBeliefReferenceRule.h"

using namespace openblack;
using namespace openblack::particles;
namespace reflection = openblack::ecs::reflection;

namespace
{
constexpr entt::entity k_Centre = static_cast<entt::entity>(5);

/// A village centre that the test changes between the steps
class FakeWorld final: public ParticleWorldInterface
{
public:
	[[nodiscard]] float LandHeight(glm::vec2 /*xz*/) const override { return 0.0f; }
	[[nodiscard]] uint32_t PlayerColour(int /*player*/) const override { return 0xFF0000u; }
	[[nodiscard]] glm::vec3 CameraRight() const override { return {1.0f, 0.0f, 0.0f}; }
	[[nodiscard]] glm::vec3 CameraUp() const override { return {0.0f, 1.0f, 0.0f}; }
	[[nodiscard]] glm::vec3 CameraPosition() const override { return camera; }
	[[nodiscard]] std::optional<TownCentreBelief> TownCentreBeliefOf(entt::entity centre) const override
	{
		return centre == k_Centre && present ? std::optional(belief) : std::nullopt;
	}
	[[nodiscard]] std::vector<int> PlayersInGame() const override { return players; }

	glm::vec3 camera {0.0f};
	TownCentreBelief belief;
	bool present {true};
	std::vector<int> players;
};

class FakeRandom final: public GameRandomInterface
{
public:
	uint32_t SyncedRand(uint32_t n, DrawSite /*site*/) override { return n == 0 ? 0 : game_random::LHRand(n, _seeds.synced); }
	float SyncedFloatRand(float x, DrawSite /*site*/) override { return game_random::FloatRand(x, _seeds.synced); }
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

/// The effect of the symbols with the sparks of a fight in group 1, for the rule of this class name
std::string SymbolEffect(std::string_view className, bool fixedScale)
{
	const std::string properties =
	    std::string("PROPERTY Group INTEGER 0\nPROPERTY HeightAt1 FLOAT 2\n"
	                "PROPERTY NextGroups ARRAY SIZE 1 1\nPROPERTY GroupFightSparkle INTEGER 1\n"
	                "PROPERTY TimeBetweenFightsAt0 FLOAT 0.3\nPROPERTY TimeBetweenFightsAt1 FLOAT "
	                "0.2\nPROPERTY FightDurationAt0 FLOAT 0.4\nPROPERTY FightDurationAt1 FLOAT 0.5\n") +
	    (fixedScale ? "PROPERTY UseFixedScale BOOL 1\nPROPERTY FixedScale FLOAT 0.02\n" : "");
	return "BEGINPROPERTIES\nPROPERTY DeleteOnCloseDown BOOL 0\nPROPERTY Hierarchies ARRAY SIZE 3 0 0 0\n"
	       "PROPERTY InitiallyCreated ARRAY SIZE 3 1 0 0\nPROPERTY MaxSpellAge FLOAT -1\nENDPROPERTIES\n"
	       "BEGINCLASS " +
	       std::string(className) + " Belief0\nBEGINPROPERTIES\n" + properties + "ENDPROPERTIES\nENDCLASS\n";
}

/// One rule's effect with its own world and random numbers
struct RuleRun
{
	FakeWorld world;
	FakeRandom random;
	std::unique_ptr<Effect> effect;
};

template <typename T>
[[nodiscard]] bool SameBits(const T& a, const T& b)
{
	// NOLINTNEXTLINE(bugprone-suspicious-memory-comparison): the compare must be bit exact
	return std::memcmp(&a, &b, sizeof(T)) == 0;
}

const entt::meta_ctx& Context()
{
	static const auto k_Context = []() {
		auto made = std::make_unique<entt::meta_ctx>();
		reflection::RegisterComponents(*made);
		return made;
	}();
	return *k_Context;
}

/// The bytes of a save of the effect: each value of each symbol and of its sparks
std::vector<uint8_t> Saved(const Effect& effect)
{
	SharedTables tables;
	reflection::BinaryWriter bytes(Context());
	EffectWriter writer(bytes, tables);
	effect.Save(writer);
	return std::move(bytes.Bytes());
}

/// The symbols as the effect draws them
std::vector<glm::vec4> Drawn(const Effect& effect)
{
	Effect::DrawWalk walk;
	effect.Walk(1.0f, walk);
	std::vector<glm::vec4> drawn;
	drawn.reserve(walk.atoms.size());
	for (const auto& atom : walk.atoms)
	{
		drawn.emplace_back(atom.position, atom.scale);
	}
	return drawn;
}

/// A belief from a few values, so that many players tie
float RandomBelief(std::mt19937& draw)
{
	constexpr std::array<float, 9> k_Beliefs = {0.0f, 0.0f, 0.0f, 0.3f, 0.5f, 1.0f, 1.5f, -0.25f, 0.75f};
	if (std::uniform_int_distribution<int>(0, 3)(draw) == 0)
	{
		return std::uniform_real_distribution<float>(-0.5f, 1.5f)(draw);
	}
	return k_Beliefs.at(std::uniform_int_distribution<size_t>(0, k_Beliefs.size() - 1)(draw));
}

void RandomiseTown(std::mt19937& draw, TownCentreBelief& belief, bool notANumber)
{
	for (size_t player = 0; player < belief.players; ++player)
	{
		belief.beliefs.at(player) = RandomBelief(draw);
	}
	if (notANumber)
	{
		belief.beliefs.at(std::uniform_int_distribution<size_t>(0, belief.players - 1)(draw)) =
		    std::numeric_limits<float>::quiet_NaN();
	}
}

class TownBeliefRuleEqual: public ::testing::Test
{
protected:
	void Start(RuleRun& run, std::string_view className, bool fixedScale, const std::vector<int>& players, uint32_t seed)
	{
		auto file = psys::ParticleFile::Parse(SymbolEffect(className, fixedScale));
		ASSERT_TRUE(file.has_value());
		run.world.players = players;
		run.random.SetSeeds({.synced = seed, .local = seed * 3u + 1u});
		run.effect =
		    std::make_unique<Effect>(std::make_shared<const psys::ParticleFile>(std::move(*file)),
		                             EffectServices {_classes, run.world, run.random, _noise}, glm::vec3(0.0f), 1.0f, false);
		run.effect->SetPlayer(0);
		run.effect->AddTarget(k_Centre);
	}

	ParticleClassRegistry _classes {[] {
		auto registry = ParticleClassRegistry::WithAllClasses();
		registry.AddModifier("UR_TownCentreBeliefReference",
		                     ParticleClassRegistry::Make<town_belief_reference::TownCentreBeliefRule>);
		return registry;
	}()};
	maths::ValueNoise _noise;
};
} // namespace

TEST_F(TownBeliefRuleEqual, RandomVillagesGiveTheSameSymbolsAsTheRuleBefore)
{
	constexpr std::array<size_t, 8> k_PlayerCounts = {1, 2, 3, 8, 9, 17, 33, 34};
	constexpr int k_Villages = 48;
	constexpr int k_Steps = 240;
	size_t drawnSymbols = 0;
	for (int village = 0; village < k_Villages; ++village)
	{
		std::mt19937 draw(static_cast<uint32_t>(village) * 7919u + 17u);
		const size_t count = k_PlayerCounts.at(static_cast<size_t>(village) % k_PlayerCounts.size());
		std::vector<int> players(count);
		std::iota(players.begin(), players.end(), 0);
		// Some games give a symbol to some players only, and some to a player that the beliefs do not hold
		if (village % 5 == 3 && count > 2)
		{
			players.erase(players.begin() + 1);
			players.push_back(static_cast<int>(count));
		}
		const bool fixedScale = village % 2 == 1;
		const bool notANumber = village % 7 == 6;
		RuleRun before;
		RuleRun after;
		Start(before, "UR_TownCentreBeliefReference", fixedScale, players, static_cast<uint32_t>(village) + 1u);
		Start(after, "UR_TownCentreBelief", fixedScale, players, static_cast<uint32_t>(village) + 1u);
		TownCentreBelief belief {.totemTop = {std::uniform_real_distribution<float>(-500.0f, 500.0f)(draw), 30.0f,
		                                      std::uniform_real_distribution<float>(-500.0f, 500.0f)(draw)},
		                         .players = count};
		RandomiseTown(draw, belief, notANumber);
		for (int step = 0; step < k_Steps; ++step)
		{
			// The beliefs and the camera change now and then, and the village is sometimes away for a step
			if (std::uniform_int_distribution<int>(0, 9)(draw) == 0)
			{
				RandomiseTown(draw, belief, notANumber);
			}
			const glm::vec3 camera(std::uniform_real_distribution<float>(-3000.0f, 3000.0f)(draw), 200.0f,
			                       std::uniform_real_distribution<float>(-3000.0f, 3000.0f)(draw));
			const bool present = std::uniform_int_distribution<int>(0, 29)(draw) != 0;
			const float dt = std::uniform_real_distribution<float>(0.005f, 0.2f)(draw);
			for (auto* run : {&before, &after})
			{
				run->world.belief = belief;
				run->world.camera = camera;
				run->world.present = present;
				run->effect->Step(dt);
			}
			const auto where = "village " + std::to_string(village) + " step " + std::to_string(step);
			ASSERT_EQ(Saved(*before.effect), Saved(*after.effect)) << where;
			const auto expected = Drawn(*before.effect);
			const auto actual = Drawn(*after.effect);
			ASSERT_EQ(expected.size(), actual.size()) << where;
			ASSERT_EQ(std::memcmp(expected.data(), actual.data(), expected.size() * sizeof(glm::vec4)), 0) << where;
			ASSERT_EQ(before.random.GetSeeds().local, after.random.GetSeeds().local) << where;
			ASSERT_EQ(before.random.GetSeeds().synced, after.random.GetSeeds().synced) << where;
			drawnSymbols += actual.size();
		}
	}
	// The villages show many symbols, so that the comparison is not empty
	EXPECT_GT(drawnSymbols, static_cast<size_t>(k_Villages * k_Steps));
}

// The ranks of all the players, found with no list on the heap, are the ranks of the rule before
TEST(TownBeliefRanks, RandomBeliefsGiveTheSameRanksAsTheRanksBefore)
{
	std::mt19937 draw(99u);
	for (int round = 0; round < 2000; ++round)
	{
		const size_t count = std::uniform_int_distribution<size_t>(0, town_belief::k_MostPlayers)(draw);
		TownCentreBelief belief {.players = count};
		if (count > 0)
		{
			RandomiseTown(draw, belief, round % 50 == 49);
		}
		const auto beliefs = belief.OfPlayers();
		const auto expected = town_belief_reference::RanksBefore(beliefs);
		const auto actual = town_belief::Ranks(beliefs);
		for (size_t player = 0; player < count; ++player)
		{
			EXPECT_EQ(actual.at(player), expected.at(player)) << "round " << round << " player " << player;
		}
		for (size_t player = count; player < actual.size(); ++player)
		{
			EXPECT_EQ(actual.at(player), 0) << "round " << round;
		}
	}
}

// The sine and the cosine of one angle are the bits of the sine and the cosine alone
TEST(TownBeliefSinCos, TheSineAndCosineTogetherHaveTheBitsOfEachAlone)
{
	std::mt19937 draw(7u);
	std::vector<float> angles = {0.0f,
	                             -0.0f,
	                             1e-10f,
	                             -1e-10f,
	                             1e-4f,
	                             0.785f,
	                             0.7854f,
	                             1.5707964f,
	                             3.1415927f,
	                             4.712389f,
	                             6.2831855f,
	                             -6.2831855f,
	                             100.0f,
	                             1e6f,
	                             std::numeric_limits<float>::infinity(),
	                             std::numeric_limits<float>::quiet_NaN()};
	for (int i = 0; i < 20000; ++i)
	{
		angles.push_back(std::uniform_real_distribution<float>(-20.0f, 20.0f)(draw));
	}
	for (const float angle : angles)
	{
		const auto both = portable_math::SinCos(angle);
		const float sin = portable_math::Sin(angle);
		const float cos = portable_math::Cos(angle);
		EXPECT_TRUE(SameBits(both.sin, sin)) << angle;
		EXPECT_TRUE(SameBits(both.cos, cos)) << angle;
		const auto wide = portable_math::SinCos(static_cast<double>(angle) * 1.0000001);
		EXPECT_TRUE(SameBits(wide.sin, portable_math::Sin(static_cast<double>(angle) * 1.0000001))) << angle;
		EXPECT_TRUE(SameBits(wide.cos, portable_math::Cos(static_cast<double>(angle) * 1.0000001))) << angle;
	}
}
