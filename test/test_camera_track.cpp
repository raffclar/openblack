/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstring>

#include <array>
#include <string>
#include <vector>

#include <EDTFile.h>
#include <gtest/gtest.h>

#include "3D/CameraTrack.h"

using namespace openblack;
using namespace openblack::camera_track;

namespace
{
/// The runner finds its Bezier parameter from a table of 127 steps along the segment, so it lands near, not on, the even
/// share
constexpr float k_Tolerance = 0.02f;

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

void AppendBlock(std::vector<uint8_t>& file, const std::string& name, const std::vector<uint8_t>& data)
{
	std::array<char, 32> padded {};
	std::memcpy(padded.data(), name.data(), name.size());
	Append(file, padded.data(), padded.size());
	Append(file, static_cast<uint32_t>(data.size()));
	Append(file, data.data(), data.size());
}

/// A straight way from `from` to `to` in a second at an even 10 metres a second, its handles a third and two thirds
/// along
void AppendWay(std::vector<uint8_t>& bytes, std::array<float, 3> from, std::array<float, 3> to)
{
	constexpr uint32_t k_Count = 2;
	Append(bytes, static_cast<uint16_t>(0x24 + (44 * k_Count)));
	Append(bytes, static_cast<uint16_t>(0x63));
	Append(bytes, k_Count);
	Append(bytes, 0.26f);
	Append(bytes, 0.0f); // length, made again on load
	Append(bytes, 1000); // duration
	for (int i = 0; i < 4; ++i)
	{
		Append(bytes, 0u); // the game's pointers
	}
	const auto along = [&](float share) {
		return std::array<float, 3> {from[0] + ((to[0] - from[0]) * share), from[1] + ((to[1] - from[1]) * share),
		                             from[2] + ((to[2] - from[2]) * share)};
	};
	for (const auto& point : {from, to})
	{
		Append(bytes, point.data(), sizeof(point));
	}
	for (int i = 0; i < 2; ++i)
	{
		const auto first = along(1.0f / 3.0f);
		const auto second = along(2.0f / 3.0f);
		Append(bytes, first.data(), sizeof(first));
		Append(bytes, second.data(), sizeof(second));
	}
	Append(bytes, 0.0f);
	Append(bytes, 1000.0f);
	Append(bytes, 10.0f);
	Append(bytes, 10.0f);
}

std::vector<uint8_t> File()
{
	std::vector<uint8_t> file {'L', 'i', 'O', 'n', 'H', 'e', 'A', 'd'};
	AppendBlock(file, "EDITOR", {1, 2, 3});
	std::vector<uint8_t> camera;
	for (const float value : {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 0.0f, -1.0f})
	{
		Append(camera, value);
	}
	AppendBlock(file, "Cam5", camera);
	std::vector<uint8_t> track;
	Append(track, 0u);
	// The camera moves along x while it looks along z
	AppendWay(track, {0.0f, 0.0f, 0.0f}, {10.0f, 0.0f, 0.0f});
	AppendWay(track, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 20.0f});
	AppendBlock(file, "Track7", track);
	return file;
}
} // namespace

TEST(CameraTrack, TheFileHoldsItsNumberedCamerasAndTracks)
{
	edt::EDTFile file;
	ASSERT_EQ(file.Open(File()), edt::EDTResult::Success);
	ASSERT_EQ(file.GetCameras().size(), 1);
	const auto& camera = file.GetCameras().at(5);
	EXPECT_EQ(camera.position, (std::array {1.0f, 2.0f, 3.0f}));
	EXPECT_EQ(camera.focus, (std::array {4.0f, 5.0f, 6.0f}));
	ASSERT_EQ(file.GetTracks().size(), 1);
	const auto& track = file.GetTracks().at(7);
	EXPECT_EQ(track.position.duration, 1000);
	EXPECT_EQ(track.position.points.size(), 2);
	EXPECT_EQ(track.focus.points[1], (std::array {0.0f, 0.0f, 20.0f}));
	EXPECT_EQ(track.focus.speeds[0], 10.0f);
}

TEST(CameraTrack, AWayCutShortIsRefused)
{
	auto bytes = File();
	bytes.resize(bytes.size() - 4);
	edt::EDTFile file;
	EXPECT_NE(file.Open(bytes), edt::EDTResult::Success);
}

TEST(CameraTrack, AnEvenSpeedRunsTheWayEvenly)
{
	edt::EDTFile file;
	ASSERT_EQ(file.Open(File()), edt::EDTResult::Success);
	const auto& way = file.GetTracks().at(7).position;
	WayRunner runner(way);
	EXPECT_NEAR(runner.Get(way, 0).x, 0.0f, 1e-5f);
	EXPECT_NEAR(runner.Get(way, 250).x, 2.5f, k_Tolerance);
	EXPECT_NEAR(runner.Get(way, 500).x, 5.0f, k_Tolerance);
	// At its end it is at its last point
	EXPECT_EQ(runner.Get(way, 1000).x, 10.0f);
	EXPECT_EQ(runner.Get(way, 5000).x, 10.0f);
}

TEST(CameraTrack, AWalkGoesWhereTheCameraLooks)
{
	edt::EDTFile file;
	ASSERT_EQ(file.Open(File()), edt::EDTResult::Success);
	const auto& track = file.GetTracks().at(7);
	auto walk = StartWalk(track, true, 0.0f, 1.0f);
	std::vector<float> zs;
	while (const auto at = WalkTurn(walk, track))
	{
		EXPECT_NEAR(at->x, 0.0f, k_Tolerance);
		zs.push_back(at->y);
	}
	// 100 milliseconds of the track a turn, from its start, and it stops once it has walked the whole way
	ASSERT_EQ(zs.size(), 10);
	EXPECT_NEAR(zs[0], 0.0f, k_Tolerance);
	EXPECT_NEAR(zs[1], 2.0f, k_Tolerance);
	EXPECT_NEAR(zs[9], 18.0f, k_Tolerance);
	EXPECT_EQ(Percentage(walk, track), 1.0f);
}

TEST(CameraTrack, AWalkCanStopPartWay)
{
	edt::EDTFile file;
	ASSERT_EQ(file.Open(File()), edt::EDTResult::Success);
	const auto& track = file.GetTracks().at(7);
	auto walk = StartWalk(track, true, 0.5f, 0.7f);
	EXPECT_NEAR(WalkTurn(walk, track)->y, 10.0f, k_Tolerance);
	EXPECT_NEAR(WalkTurn(walk, track)->y, 12.0f, k_Tolerance);
	EXPECT_FALSE(WalkTurn(walk, track).has_value());
}

TEST(CameraTrack, ABackwardWalkStartsWhereTheRunnerWas)
{
	edt::EDTFile file;
	ASSERT_EQ(file.Open(File()), edt::EDTResult::Success);
	const auto& track = file.GetTracks().at(7);
	auto walk = StartWalk(track, false, 0.0f, 1.0f);
	// At the track's end the runner keeps the parameter it had, the start, as the game's does
	EXPECT_NEAR(WalkTurn(walk, track)->y, 0.0f, k_Tolerance);
	EXPECT_NEAR(WalkTurn(walk, track)->y, 18.0f, k_Tolerance);
	EXPECT_NEAR(WalkTurn(walk, track)->y, 16.0f, k_Tolerance);
}
