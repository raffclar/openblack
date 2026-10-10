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
