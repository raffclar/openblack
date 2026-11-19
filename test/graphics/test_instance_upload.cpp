/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <array>
#include <random>
#include <span>
#include <vector>

#include <gtest/gtest.h>

#include "Common/SameBits.h"
#include "Graphics/InstanceUpload.h"

namespace instance_upload = openblack::graphics::instance_upload;
using instance_upload::ChangedRuns;
using Runs = std::vector<instance_upload::Run>;
using Changed = instance_upload::Run;

namespace
{

/// An instance of two words, as an instance buffer holds
struct Instance
{
	std::array<float, 4> values {};
};

std::vector<Instance> Instances(size_t count)
{
	std::vector<Instance> instances(count);
	for (size_t i = 0; i < count; ++i)
	{
		instances[i].values[0] = static_cast<float>(i);
	}
	return instances;
}

std::vector<instance_upload::Run> Found(const std::vector<Instance>& now, const std::vector<Instance>& uploaded)
{
	return ChangedRuns(std::span<const Instance>(now), std::span<const Instance>(uploaded));
}

/// The runs found by a test of each instance, with no block test first
Runs FoundOneByOne(const std::vector<Instance>& now, const std::vector<Instance>& uploaded)
{
	Runs runs;
	for (uint32_t i = 0; i < static_cast<uint32_t>(now.size()); ++i)
	{
		if (openblack::same_bits::SameBits(now[i], uploaded[i]))
		{
			continue;
		}
		if (!runs.empty() && i - (runs.back().first + runs.back().count) < instance_upload::k_JoinGap)
		{
			runs.back().count = i + 1 - runs.back().first;
		}
		else
		{
			runs.push_back({.first = i, .count = 1});
		}
	}
	if (runs.size() > instance_upload::k_MaxRuns)
	{
		const auto first = runs.front().first;
		const auto end = runs.back().first + runs.back().count;
		runs = {{.first = first, .count = end - first}};
	}
	return runs;
}

/// A value of three words, which a 64-bit word does not divide
struct OddValue
{
	std::array<uint32_t, 3> words {};
};

} // namespace

TEST(InstanceUpload, NothingChangedUploadsNothing)
{
	const auto instances = Instances(500);
	EXPECT_TRUE(Found(instances, instances).empty());
}

TEST(InstanceUpload, AnotherCountUploadsAll)
{
	const auto uploaded = Instances(10);
	const auto now = Instances(12);
	EXPECT_EQ(Found(now, uploaded), (Runs {Changed {.first = 0, .count = 12}}));
	EXPECT_TRUE(Found({}, uploaded).empty());
}

TEST(InstanceUpload, ChangesFarApartGoUpApart)
{
	const auto uploaded = Instances(1000);
	auto now = uploaded;
	now[3].values[1] = 1.0f;
	now[4].values[2] = 1.0f;
	now[900].values[3] = 1.0f;
	EXPECT_EQ(Found(now, uploaded), (Runs {Changed {.first = 3, .count = 2}, Changed {.first = 900, .count = 1}}));
}

TEST(InstanceUpload, ChangesCloseTogetherGoUpAsOne)
{
	const auto uploaded = Instances(1000);
	auto now = uploaded;
	now[10].values[1] = 1.0f;
	now[10 + instance_upload::k_JoinGap].values[1] = 1.0f;
	EXPECT_EQ(Found(now, uploaded), (Runs {Changed {.first = 10, .count = instance_upload::k_JoinGap + 1}}));
}

TEST(InstanceUpload, ManyRunsGoUpAsOne)
{
	const auto uploaded = Instances(10000);
	auto now = uploaded;
	for (size_t i = 0; i <= instance_upload::k_MaxRuns; ++i)
	{
		now[100 + (i * 200)].values[1] = 1.0f;
	}
	const auto last = static_cast<uint32_t>(100 + (instance_upload::k_MaxRuns * 200));
	EXPECT_EQ(Found(now, uploaded), (Runs {Changed {.first = 100, .count = last - 99}}));
}

TEST(InstanceUpload, ASignOfZeroIsAChange)
{
	const auto uploaded = Instances(4);
	auto now = uploaded;
	now[0].values[0] = -0.0f;
	EXPECT_EQ(Found(now, uploaded), (Runs {Changed {.first = 0, .count = 1}}));
}

TEST(InstanceUpload, TheMirrorKeepsWhatWasUploaded)
{
	instance_upload::Mirror<Instance> mirror;
	auto now = Instances(100);
	EXPECT_EQ(mirror.Take(std::span<const Instance>(now)), (Runs {Changed {.first = 0, .count = 100}}));
	EXPECT_TRUE(mirror.Take(std::span<const Instance>(now)).empty());
	now[50].values[1] = 2.0f;
	EXPECT_EQ(mirror.Take(std::span<const Instance>(now)), (Runs {Changed {.first = 50, .count = 1}}));
	EXPECT_TRUE(mirror.Take(std::span<const Instance>(now)).empty());
	mirror.Forget();
	EXPECT_EQ(mirror.Take(std::span<const Instance>(now)), (Runs {Changed {.first = 0, .count = 100}}));
}

TEST(InstanceUpload, TheBlockTestFindsTheRunsOfATestOfEachInstance)
{
	std::mt19937 random(5);
	for (const size_t count : {0u, 1u, 7u, 8u, 9u, 63u, 1000u, 4099u})
	{
		const auto uploaded = Instances(count);
		for (int round = 0; round < 40 && count > 0; ++round)
		{
			auto now = uploaded;
			const auto changes = random() % 30;
			for (uint32_t change = 0; change < changes; ++change)
			{
				now[random() % count].values[random() % 4] += 1.0f;
			}
			EXPECT_EQ(Found(now, uploaded), FoundOneByOne(now, uploaded)) << count << " " << round;
		}
	}
}

TEST(InstanceUpload, ARunIsTheSameOnlyWhenEachBitIsTheSame)
{
	using openblack::same_bits::SameRun;
	std::vector<OddValue> a(5);
	for (uint32_t i = 0; i < a.size(); ++i)
	{
		a[i].words = {i, i + 1, i + 2};
	}
	for (size_t length = 0; length <= a.size(); ++length)
	{
		const auto run = std::span<const OddValue>(a).first(length);
		EXPECT_TRUE(SameRun(run, run));
		for (size_t value = 0; value < length; ++value)
		{
			for (size_t word = 0; word < 3; ++word)
			{
				auto b = a;
				b[value].words.at(word) ^= 0x80000000u;
				EXPECT_FALSE(SameRun(run, std::span<const OddValue>(b).first(length))) << length << value << word;
			}
		}
	}
	EXPECT_FALSE(SameRun(std::span<const OddValue>(a).first(2), std::span<const OddValue>(a).first(3)));
}
