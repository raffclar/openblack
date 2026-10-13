/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <optional>
#include <vector>

#include <gtest/gtest.h>

#include "Common/GameRandom.h"
#include "ScriptHeaders/ScriptRandom.h"

using namespace openblack;
using namespace openblack::script::random;

namespace
{

/// Gives the highest number each draw allows, or a set answer, and keeps the widths it was asked for
class WidthRecordingRandom final: public GameRandomInterface
{
public:
	uint32_t GameRand(uint32_t n) override
	{
		widths.push_back(n);
		return n == 0 ? 0 : (answer.value_or(n - 1));
	}
	float GameFloatRand(float x) override
	{
		floatWidths.push_back(x);
		return floatAnswer.value_or(0.0f);
	}
	uint32_t LocalRand(int32_t /*n*/) override
	{
		++localDraws;
		return 0;
	}
	float LocalFloatRand(float /*x*/) override { return 0.0f; }
	int32_t CrtRand() override { return 0; }
	void CrtSrand(uint32_t /*seed*/) override {}
	[[nodiscard]] GameRandomSeeds GetSeeds() const override { return {}; }
	void SetSeeds(GameRandomSeeds /*seeds*/) override {}
	[[nodiscard]] ParticleRandomStream GetParticleStream() const override { return ParticleRandomStream::None; }
	void SetParticleStream(ParticleRandomStream /*stream*/) override {}

	std::optional<uint32_t> answer;
	std::vector<uint32_t> widths;
	std::optional<float> floatAnswer;
	std::vector<float> floatWidths;
	int localDraws {0};
};

} // namespace

TEST(ScriptRandom, BothEndsOfTheRangeCanComeUp)
{
	// The opening's "lost the mother" lines: texts 1257 to 1260
	WidthRecordingRandom random;
	random.answer = 0;
	EXPECT_EQ(WholeNumberBetween(1257, 1260, random), 1257u);
	random.answer.reset();
	EXPECT_EQ(WholeNumberBetween(1257, 1260, random), 1260u);
	EXPECT_EQ(random.widths, (std::vector<uint32_t> {4, 4}));
}

TEST(ScriptRandom, DrawsOnceFromTheSyncedNumbers)
{
	WidthRecordingRandom random;
	random.answer = 2;
	EXPECT_EQ(WholeNumberBetween(1249, 1253, random), 1251u);
	EXPECT_EQ(random.widths, (std::vector<uint32_t> {5}));
	EXPECT_EQ(random.localDraws, 0);
}

TEST(ScriptRandom, ASingleNumberRangeGivesThatNumber)
{
	WidthRecordingRandom random;
	EXPECT_EQ(WholeNumberBetween(5219, 5219, random), 5219u);
	EXPECT_EQ(random.widths, (std::vector<uint32_t> {1}));
}

TEST(ScriptRandom, AReversedRangeWrapsRoundUnsigned)
{
	// High one below low makes a width of 0, which draws nothing and gives low
	WidthRecordingRandom random;
	EXPECT_EQ(WholeNumberBetween(10, 9, random), 10u);
	EXPECT_EQ(random.widths, (std::vector<uint32_t> {0}));

	// High further below low wraps to a huge width; the draw is added to low unsigned as well
	random.widths.clear();
	random.answer = 5;
	EXPECT_EQ(WholeNumberBetween(10, 4, random), 15u);
	EXPECT_EQ(random.widths, (std::vector<uint32_t> {0xFFFFFFFBu}));
}

TEST(ScriptRandom, NumberFromIsAWholeNumberInTheRange)
{
	// The hidden phone box: "number from 1 to 10" picks one of ten calls
	WidthRecordingRandom random;
	random.floatAnswer = 0.0f;
	EXPECT_EQ(NumberFrom(1.0f, 10.0f, random), 1.0f);
	random.floatAnswer = 3.7f;
	EXPECT_EQ(NumberFrom(1.0f, 10.0f, random), 4.0f);
	random.floatAnswer = 9.999f;
	EXPECT_EQ(NumberFrom(1.0f, 10.0f, random), 10.0f);
	EXPECT_EQ(random.floatWidths, (std::vector<float> {10.0f, 10.0f, 10.0f}));
	EXPECT_TRUE(random.widths.empty());
	EXPECT_EQ(random.localDraws, 0);
}

TEST(ScriptRandom, NumberFromCutsTheFractionTowardsZero)
{
	WidthRecordingRandom random;
	// -3 + 1.5 is -1.5, which becomes -1, not -2
	random.floatAnswer = 1.5f;
	EXPECT_EQ(NumberFrom(-3.0f, 3.0f, random), -1.0f);
	EXPECT_EQ(random.floatWidths, (std::vector<float> {7.0f}));
}
