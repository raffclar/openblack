/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <ANMFile.h>
#include <glm/mat4x4.hpp>
#include <gtest/gtest.h>

#include "3D/L3DAnim.h"
#include "ECS/VillagerClips.h"

using namespace openblack;
namespace villager_clips = openblack::ecs::villager_clips;

namespace
{
/// An animation file's header as the couple's kiss has it in the game's pack: 5133 ms of play in 56036 bytes of data
anm::ANMFile KissHeader()
{
	anm::ANMFile file;
	auto& header = file.GetHeader();
	header.unknown0x20 = 5133;
	header.frameCount = 0;
	header.animationDataSize = 56036;
	header.unknown0x50 = 0x416;
	return file;
}
} // namespace

TEST(VillagerClips, AClipLastsItsPlayTimeNotTheSizeOfItsData)
{
	L3DAnim clip;
	clip.Load(KissHeader());
	EXPECT_EQ(clip.GetPlayTime(), 5133u);
	EXPECT_EQ(clip.GetDataSize(), 56036u);
}

TEST(VillagerClips, AClipHasPlayedOnceItsTurnsCoverItsPlayTime)
{
	// The kiss of 5133 ms has played after 52 turns of 100 ms, not after 51
	EXPECT_FALSE(villager_clips::HasPlayed(51, 100, 5133, 1));
	EXPECT_TRUE(villager_clips::HasPlayed(52, 100, 5133, 1));
	// Twice through takes twice as long
	EXPECT_FALSE(villager_clips::HasPlayed(102, 100, 5133, 2));
	EXPECT_TRUE(villager_clips::HasPlayed(103, 100, 5133, 2));
	// A clip of no length has always played
	EXPECT_TRUE(villager_clips::HasPlayed(0, 100, 0, 1));
}

TEST(VillagerClips, SettingTheSameClipAgainPlaysOn)
{
	using villager_clips::ClipPlace;
	EXPECT_EQ(villager_clips::PlaceOnSetClip(true, true, false), ClipPlace::Keep);
	EXPECT_EQ(villager_clips::PlaceOnSetClip(true, false, false), ClipPlace::Keep);
	EXPECT_EQ(villager_clips::PlaceOnSetClip(true, true, true), ClipPlace::Keep);
}

TEST(VillagerClips, ADifferentClipStartsOverOnlyWhenAskedAndNotDancing)
{
	using villager_clips::ClipPlace;
	EXPECT_EQ(villager_clips::PlaceOnSetClip(false, true, false), ClipPlace::Restart);
	EXPECT_EQ(villager_clips::PlaceOnSetClip(false, false, false), ClipPlace::Keep);
	// A dancer's new clip takes over in time with the dance
	EXPECT_EQ(villager_clips::PlaceOnSetClip(false, true, true), ClipPlace::Keep);
}

TEST(VillagerClips, ADanceMoveStartsTheClipOverOnceItHasPlayedThrough)
{
	// A 1366 ms dance clip, 20 turns of 100 ms since the dancer's state changed: it starts over and the count restarts
	const auto first = villager_clips::OnDanceMove(20, 100, 1366);
	EXPECT_TRUE(first.restart);
	EXPECT_EQ(first.turnsSinceStateChange, 0u);
	// Halfway through it plays on, keeping its count
	const auto halfway = villager_clips::OnDanceMove(7, 100, 1366);
	EXPECT_FALSE(halfway.restart);
	EXPECT_EQ(halfway.turnsSinceStateChange, 7u);
}

TEST(VillagerClips, AMoveStartedOnFiveTurnsStartsTheClipOverOnce)
{
	// The dance starts a move on five turns running; the clip starts over on the first only, then plays on
	uint32_t turns = 30;
	int restarts = 0;
	for (int turn = 0; turn < 5; ++turn)
	{
		const auto move = villager_clips::OnDanceMove(turns, 100, 1366);
		restarts += move.restart ? 1 : 0;
		turns = move.turnsSinceStateChange + 1;
	}
	EXPECT_EQ(restarts, 1);
	EXPECT_EQ(turns, 5u);
}
