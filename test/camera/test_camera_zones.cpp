/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>
#include <cstdint>
#include <cstring>

#include <array>
#include <string>
#include <vector>

#include <EXCFile.h>
#include <glm/vec3.hpp>
#include <gtest/gtest.h>

#include "Camera/CameraZones.h"

using namespace openblack;
using namespace openblack::camera_zones;

namespace
{
/// A square fence 100 across, from (0, 0) to (100, 100)
const std::vector<glm::vec3> k_Square {
    {0.0f, 0.0f, 0.0f},
    {100.0f, 0.0f, 0.0f},
    {100.0f, 0.0f, 100.0f},
    {0.0f, 0.0f, 100.0f},
};

void Append(std::vector<uint8_t>& bytes, const void* data, size_t size)
{
	const auto* begin = static_cast<const uint8_t*>(data);
	bytes.insert(bytes.end(), begin, begin + size);
}

template <typename T>
void Append(std::vector<uint8_t>& bytes, T value)
{
	Append(bytes, &value, sizeof(T));
}

std::vector<uint8_t> ZoneFile(uint32_t exclusionSize)
{
	std::vector<uint8_t> data;
	Append(data, uint32_t {1});
	Append(data, uint32_t {0});
	Append(data, uint32_t {0});
	Append(data, uint32_t {1});
	Append(data, uint32_t {0});
	Append(data, 62.5f);
	Append(data, 500.0f);
	Append(data, int32_t {3});
	for (const auto& corner : {glm::vec3(1.0f, 2.0f, 3.0f), glm::vec3(4.0f, 5.0f, 6.0f), glm::vec3(7.0f, 8.0f, 9.0f)})
	{
		Append(data, corner.x);
		Append(data, corner.y);
		Append(data, corner.z);
	}
	Append(data, int32_t {1});
	Append(data, exclusionSize);
	std::vector<uint8_t> exclusion(exclusionSize, 0);
	const std::array<float, 5> numbers {10.0f, 20.0f, 30.0f, 7.0f, 4.0f};
	std::memcpy(exclusion.data() + 12, numbers.data(), sizeof(numbers));
	const uint32_t cylinder = 1;
	std::memcpy(exclusion.data() + 32, &cylinder, sizeof(cylinder));
	Append(data, exclusion.data(), exclusion.size());

	std::vector<uint8_t> file {'L', 'i', 'O', 'n', 'H', 'e', 'A', 'd'};
	std::array<char, 32> name {};
	std::memcpy(name.data(), "cameraexc", 9);
	Append(file, name.data(), name.size());
	Append(file, static_cast<uint32_t>(data.size()));
	Append(file, data.data(), data.size());
	return file;
}
} // namespace

TEST(CameraZones, AZoneFileHoldsItsFenceLimitsAndExclusions)
{
	exc::EXCFile file;
	ASSERT_EQ(file.Open(ZoneFile(0x28)), exc::EXCResult::Success);
	const auto& zones = file.GetZones();
	EXPECT_EQ(zones.version, 1u);
	EXPECT_FALSE(zones.fenceOn);
	EXPECT_TRUE(zones.useMaxAltitude);
	EXPECT_FALSE(zones.useHeightAboveLand);
	EXPECT_FLOAT_EQ(zones.maxAltitude, 62.5f);
	ASSERT_EQ(zones.fence.size(), 3u);
	EXPECT_FLOAT_EQ(zones.fence[2][1], 8.0f);
	ASSERT_EQ(zones.exclusions.size(), 1u);
	EXPECT_FLOAT_EQ(zones.exclusions[0].position[2], 30.0f);
	EXPECT_FLOAT_EQ(zones.exclusions[0].radius, 7.0f);
	EXPECT_EQ(zones.exclusions[0].kind, exc::EXCExclusion::Kind::Cylinder);

	// Exclusions of another size are read past and dropped
	exc::EXCFile other;
	ASSERT_EQ(other.Open(ZoneFile(0x24)), exc::EXCResult::Success);
	EXPECT_TRUE(other.GetZones().exclusions.empty());
	EXPECT_EQ(other.GetZones().fence.size(), 3u);
}

TEST(CameraZones, APointIsInsideTheFenceWhenALineFromItCrossesItAnOddNumberOfTimesAhead)
{
	const auto inside = CrossFence(k_Square, true, {50.0f, 10.0f, 50.0f}, {1.0f, 0.0f, 0.0f});
	EXPECT_TRUE(inside.inside);
	// The crossing ahead is the nearest, unless a corner is nearer
	EXPECT_FLOAT_EQ(inside.closest.x, 100.0f);
	EXPECT_FLOAT_EQ(inside.closest.z, 50.0f);

	const auto outside = CrossFence(k_Square, true, {150.0f, 10.0f, 50.0f}, {-1.0f, 0.0f, 0.0f});
	EXPECT_FALSE(outside.inside);
	EXPECT_FLOAT_EQ(outside.closest.x, 100.0f);
	EXPECT_FLOAT_EQ(outside.closest.y, 10.0f);

	// A line that crosses nowhere ahead takes the nearest crossing behind
	const auto behind = CrossFence(k_Square, true, {150.0f, 0.0f, 50.0f}, {1.0f, 0.0f, 0.0f});
	EXPECT_FALSE(behind.inside);
	EXPECT_FLOAT_EQ(behind.closest.x, 100.0f);
	EXPECT_FLOAT_EQ(behind.closest.z, 50.0f);
	// One that crosses nowhere at all takes the nearest corner
	const auto away = CrossFence(k_Square, true, {150.0f, 0.0f, 150.0f}, {1.0f, 0.0f, -1.0f});
	EXPECT_FALSE(away.inside);
	EXPECT_FLOAT_EQ(away.closest.x, 100.0f);
	EXPECT_FLOAT_EQ(away.closest.z, 100.0f);

	// Off, or with fewer than three corners, everywhere is inside
	EXPECT_TRUE(CrossFence(k_Square, false, {500.0f, 0.0f, 500.0f}, {1.0f, 0.0f, 0.0f}).inside);
	EXPECT_TRUE(CrossFence(std::span(k_Square).first(2), true, {500.0f, 0.0f, 500.0f}, {1.0f, 0.0f, 0.0f}).inside);
	// On a corner it is inside
	EXPECT_TRUE(CrossFence(k_Square, true, {100.0f, 0.0f, 100.0f}, {1.0f, 0.0f, 0.0f}).inside);
}

TEST(CameraZones, ACameraOutsideTheFenceIsPutBackPastWhereItsLineCrossesIt)
{
	// Moved out across the right side from where it started: back along the way it came, 20 past the fence
	const auto pushed =
	    PushInsideFence(k_Square, true, {90.0f, 30.0f, 50.0f}, {110.0f, 30.0f, 50.0f}, {120.0f, 0.0f, 50.0f}, k_SearchRadius);
	ASSERT_TRUE(pushed.has_value());
	EXPECT_FLOAT_EQ(pushed->x, -30.0f);
	EXPECT_FLOAT_EQ(pushed->y, 0.0f);
	EXPECT_NEAR(pushed->z, 0.0f, 1e-4f);

	// Hardly moved, it is tested towards what it looks at
	const auto still =
	    PushInsideFence(k_Square, true, {110.0f, 30.0f, 50.0f}, {110.0f, 30.0f, 50.0f}, {50.0f, 0.0f, 50.0f}, k_SearchRadius);
	ASSERT_TRUE(still.has_value());
	EXPECT_FLOAT_EQ(still->x, -30.0f);

	// Inside, it stays
	EXPECT_FALSE(
	    PushInsideFence(k_Square, true, {50.0f, 30.0f, 50.0f}, {60.0f, 30.0f, 50.0f}, {120.0f, 0.0f, 50.0f}, k_SearchRadius)
	        .has_value());
}

TEST(CameraZones, TheCameraIsHeldUnderItsHeightAboveTheLandOrItsHighest)
{
	Zones zones;
	EXPECT_FLOAT_EQ(HeightLimit(zones, 10.0f), 30000.0f);
	zones.useHeightAboveLand = true;
	zones.heightAboveLand = 37.76f;
	EXPECT_FLOAT_EQ(HeightLimit(zones, 10.0f), 47.76f);
	zones.useMaxAltitude = true;
	zones.maxAltitude = 40.0f;
	EXPECT_FLOAT_EQ(HeightLimit(zones, 10.0f), 40.0f);

	// Above it, the camera and what it looks at slide down their line until the camera is at the limit
	const auto slide = SlideUnderLimit({0.0f, 50.0f, 0.0f}, {0.0f, 10.0f, 40.0f}, 40.0f);
	ASSERT_TRUE(slide.has_value());
	EXPECT_FLOAT_EQ(slide->y, -10.0f);
	EXPECT_FLOAT_EQ(slide->z, 10.0f);
	// Looking all but level, it can't slide down
	EXPECT_FALSE(SlideUnderLimit({0.0f, 50.0f, 0.0f}, {0.0f, 49.0f, 40.0f}, 40.0f).has_value());
	EXPECT_FALSE(SlideUnderLimit({0.0f, 30.0f, 0.0f}, {0.0f, 10.0f, 40.0f}, 40.0f).has_value());
}
