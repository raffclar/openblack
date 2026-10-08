/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>
#include <cstring>

#include <stdexcept>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "3D/CameraTracks.h"
#include "Resources/Loaders.h"

namespace
{
/// A fake segment file: the "LiOnHeAd" magic, then each segment's 32-byte name, u32 size and data
std::vector<uint8_t> SegmentFile(const std::vector<std::pair<std::string, std::vector<uint8_t>>>& segments)
{
	std::vector<uint8_t> file = {'L', 'i', 'O', 'n', 'H', 'e', 'A', 'd'};
	for (const auto& [name, data] : segments)
	{
		std::vector<uint8_t> header(36, 0);
		std::memcpy(header.data(), name.data(), name.size());
		const auto size = static_cast<uint32_t>(data.size());
		std::memcpy(header.data() + 32, &size, 4);
		file.insert(file.end(), header.begin(), header.end());
		file.insert(file.end(), data.begin(), data.end());
	}
	return file;
}
} // namespace

TEST(CameraSegments, FindsASegmentByName)
{
	const auto file = SegmentFile({{"Cam1", {1, 2, 3}}, {"Track7", {4, 5}}});
	const auto track = openblack::camera_tracks::FindSegment(file, "Track7");
	ASSERT_EQ(track.size(), 2u);
	EXPECT_EQ(track[0], 4);
	EXPECT_EQ(track[1], 5);
	EXPECT_TRUE(openblack::camera_tracks::FindSegment(file, "Track8").empty());
}

TEST(CameraSegments, NotASegmentFile)
{
	const std::vector<uint8_t> file = {'n', 'o', 't', ' ', 'a', ' ', 'f', 'i', 'l', 'e'};
	EXPECT_TRUE(openblack::camera_tracks::FindSegment(file, "Cam1").empty());
}

TEST(CameraSegments, AShortSegmentIsNotATrack)
{
	const std::vector<uint8_t> segment = {0, 0, 0, 0, 0x24, 0};
	EXPECT_FALSE(openblack::camera_tracks::ParseTrack(segment).has_value());
	using openblack::resources::CameraTrackLoader;
	EXPECT_THROW((void)CameraTrackLoader {}(CameraTrackLoader::FromBufferTag {}, segment), std::runtime_error);
}
