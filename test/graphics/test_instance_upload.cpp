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
#include <span>
#include <vector>

#include <gtest/gtest.h>

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
