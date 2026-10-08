/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// An animation of a .cbn or .hbn file as the skeletal animation maths takes it, and the hand's clip fields under their
// new names

#include <MorphFile.h>
#include <gtest/gtest.h>

#include "3D/SkeletalAnimation.h"
#include "creature/SyntheticCreatureBlock.h"

using namespace openblack;
using namespace openblack::test::creature_block;

TEST(SkeletalFromMorph, CopiesTheHeaderJointsAndFrames)
{
	morph::Animation source;
	source.header.duration = 1234;
	source.header.looping = 1;
	source.header.displacement = {0.5f, 0.0f, -2.0f};
	source.rotatedJointIndices = {1, 3};
	source.translatedJointIndices = {0};
	source.keyframes = {
	    {.eulerAngles = {{0.1f, 0.2f, 0.3f}, {0.4f, 0.5f, 0.6f}}, .translations = {{1.0f, 2.0f, 3.0f}}},
	    {.eulerAngles = {{-0.1f, -0.2f, -0.3f}, {0.0f, 0.0f, 0.0f}}, .translations = {{4.0f, 5.0f, 6.0f}}},
	};
	const auto animation = skeletal_animation::FromMorph(source);
	EXPECT_EQ(animation.duration, 1234u);
	EXPECT_TRUE(animation.looping);
	EXPECT_EQ(animation.displacement, glm::vec3(0.5f, 0.0f, -2.0f));
	EXPECT_EQ(animation.rotatedJoints, (std::vector<uint32_t> {1, 3}));
	EXPECT_EQ(animation.translatedJoints, (std::vector<uint32_t> {0}));
	ASSERT_EQ(animation.frames.size(), 2u);
	EXPECT_EQ(animation.frames[0].eulerAngles[1], glm::vec3(0.4f, 0.5f, 0.6f));
	EXPECT_EQ(animation.frames[1].translations[0], glm::vec3(4.0f, 5.0f, 6.0f));

	source.header.looping = 0;
	EXPECT_FALSE(skeletal_animation::FromMorph(source).looping);
}

TEST(SkeletalFromMorph, TheHandsClipFieldsReadTheSameBytes)
{
	// The hand's animator reads a clip's length and whether it loops from the first two fields of its header
	const test::creature_block::TempFolder folder(std::string("openblack_test_skeletal_from_morph_hand"));
	WriteSpec(folder.Path(), false, 7, {{"standard", {"Cwiggle", "Lgrip"}}});
	Block block;
	block.version = 0;
	block.specVersion = 7;
	block.clips = {Clip {.durationMs = 2500, .looping = 1}, Clip {.durationMs = 800, .looping = 0}};
	morph::MorphFile file;
	ASSERT_EQ(file.Open(Build(block), folder.Path()), morph::MorphResult::Success);
	const auto& clips = file.GetBaseAnimationSet();
	ASSERT_EQ(clips.size(), 2u);
	EXPECT_EQ(clips[0].name, "Cwiggle");
	EXPECT_EQ(clips[0].header.duration, 2500u);
	EXPECT_EQ(clips[0].header.looping, 1u);
	EXPECT_EQ(clips[1].name, "Lgrip");
	EXPECT_EQ(clips[1].header.duration, 800u);
	EXPECT_EQ(clips[1].header.looping, 0u);
	// the same bits as the header's first and second words
	const auto bytes = Build(block);
	uint32_t first = 0;
	std::memcpy(&first, bytes.data() + sizeof(morph::MorphHeader) + (3 * sizeof(uint32_t)), sizeof(first));
	EXPECT_EQ(first, 2500u);
}
