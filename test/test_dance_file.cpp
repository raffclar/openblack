/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <bit>
#include <string>
#include <vector>

#include <DanceFile.h>
#include <gtest/gtest.h>

using namespace openblack;

namespace
{
void Put(std::vector<uint8_t>& bytes, uint32_t value)
{
	for (int i = 0; i < 4; ++i)
	{
		bytes.push_back(static_cast<uint8_t>(value >> (8 * i)));
	}
}

void PutAction(std::vector<uint8_t>& bytes, std::vector<uint32_t> groups, uint32_t type, uint32_t first, uint32_t second)
{
	Put(bytes, static_cast<uint32_t>(groups.size()));
	for (const auto group : groups)
	{
		Put(bytes, group);
	}
	Put(bytes, type);
	Put(bytes, first);
	Put(bytes, second);
	for (int i = 2; i < 14; ++i)
	{
		Put(bytes, 0);
	}
}

/// A dance of two groups named M and W, sharing their dancers half and half from its first key frame
std::vector<uint8_t> TwoGroupDance()
{
	std::vector<uint8_t> bytes;
	Put(bytes, 1); // version
	Put(bytes, 2); // key frames
	Put(bytes, std::bit_cast<uint32_t>(0.0f));
	Put(bytes, 1);
	Put(bytes, 2); // actions
	PutAction(bytes, {0}, 6, 0, 50);
	PutAction(bytes, {1}, 6, 0, 50);
	Put(bytes, std::bit_cast<uint32_t>(20.0f));
	Put(bytes, 0);
	Put(bytes, 1);
	PutAction(bytes, {1}, 16, 2, 0);
	Put(bytes, std::bit_cast<uint32_t>(0.0f)); // beat
	Put(bytes, 2);                             // groups
	Put(bytes, 14);
	Put(bytes, 0);
	Put(bytes, 1); // loops
	for (const char name : {'M', 'W'})
	{
		Put(bytes, 1);
		bytes.push_back(static_cast<uint8_t>(name));
	}
	return bytes;
}

} // namespace

TEST(DanceFile, ReadsTheKeyFramesSettingsAndGroupNames)
{
	dance::DanceFile file;
	ASSERT_EQ(file.Open(TwoGroupDance()), dance::DanceResult::Success);
	EXPECT_EQ(file.version, 1u);
	ASSERT_EQ(file.keyFrames.size(), 2u);
	EXPECT_FLOAT_EQ(file.keyFrames[1].time, 20.0f);
	EXPECT_EQ(file.keyFrames[0].actions[1].groups, std::vector<uint32_t> {1});
	EXPECT_EQ(file.keyFrames[0].actions[1].arguments[1], 50u);
	EXPECT_EQ(file.groupCount, 2u);
	EXPECT_EQ(file.loops, 1u);
	EXPECT_FALSE(file.angle.has_value());
	EXPECT_EQ(file.groupNames, (std::vector<std::string> {"M", "W"}));
	// Cut short, or with more after its end, it isn't read
	auto bytes = TwoGroupDance();
	bytes.pop_back();
	EXPECT_EQ(dance::DanceFile {}.Open(bytes), dance::DanceResult::ErrTruncated);
	bytes = TwoGroupDance();
	bytes.push_back(0);
	EXPECT_EQ(dance::DanceFile {}.Open(bytes), dance::DanceResult::ErrTrailingBytes);
}
