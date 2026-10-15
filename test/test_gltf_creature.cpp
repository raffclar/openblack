/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// EXPERIMENT: the conversion of a glTF model into a creature body, on a small model made here

#include <cmath>

#include <array>
#include <numbers>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "3D/CreatureBody.h"
#include "Creature/CreatureMorph.h"
#include "Creature/GltfCreature.h"

using namespace openblack;
using namespace openblack::gltf_creature;

namespace
{
constexpr float k_Tolerance = 1e-4f;

/// A root node that is no joint, a hip one up from it and a knee two up from the hip. The skin lists the knee first.
/// One animation turns the knee a quarter turn round z over a second.
glb::Model TwoBones()
{
	glb::Model model;
	model.nodes = {
	    glb::Node {.name = "root"},
	    glb::Node {.name = "bone_0_hip", .parent = 0u, .translation = {0.0f, 1.0f, 0.0f}},
	    glb::Node {.name = "bone_1", .parent = 1u, .translation = {0.0f, 2.0f, 0.0f}},
	};
	model.skin = glb::Skin {.joints = {2, 1}, .inverseBindMatrices = {}};
	const auto half = std::numbers::sqrt2_v<float> / 2.0f;
	model.animations.push_back({
	    .name = "move/walk",
	    .channels = {glb::Channel {.node = 2,
	                               .target = glb::Target::Rotation,
	                               .interpolation = glb::Interpolation::Linear,
	                               .times = {0.0f, 1.0f},
	                               .values = {{0.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, half, half}}}},
	});
	return model;
}
} // namespace

TEST(GltfCreature, NamedJointsKeepTheirBoneNumbers)
{
	EXPECT_EQ(BonesOfJoints(TwoBones()), (std::vector<uint32_t> {1, 0}));
}

TEST(GltfCreature, JointsWithoutBoneNamesKeepTheSkinOrder)
{
	auto model = TwoBones();
	model.nodes[2].name = "knee";
	EXPECT_EQ(BonesOfJoints(model), (std::vector<uint32_t> {0, 1}));
}

TEST(GltfCreature, TheRestPoseFollowsTheNodes)
{
	const auto model = TwoBones();
	const auto bones = BonesOfJoints(model);
	const auto rest = RestOf(model, bones);
	ASSERT_TRUE(rest.has_value());
	EXPECT_EQ(rest->parents.at(0), std::numeric_limits<uint32_t>::max());
	EXPECT_EQ(rest->parents.at(1), 0u);
	const auto world = rest->World();
	EXPECT_NEAR(world.at(1)[3].y, 3.0f, k_Tolerance);
}

TEST(GltfCreature, TheKeyframesPoseTheBonesAsTheFileDoes)
{
	const auto model = TwoBones();
	const auto bones = BonesOfJoints(model);
	const auto rest = RestOf(model, bones);
	ASSERT_TRUE(rest.has_value());
	const auto skeleton = skeletal_animation::Skeleton::FromRestMatrices(rest->parents, rest->World());
	const auto animation =
	    ToAnimation(model, model.animations.front(), bones, skeleton, {.durationMs = 1000, .frames = 3, .looping = false});
	ASSERT_EQ(animation.frames.size(), 3u);
	// Half way, the knee has turned an eighth of a turn round z; turning z round the axes turns it the other way
	const auto poses = skeletal_animation::SampleCycle(animation, animation, 500, skeleton);
	const auto angle = std::numbers::pi_v<float> / 4.0f;
	const auto& knee = poses.at(1).rotation;
	EXPECT_NEAR(knee[0][0], std::cos(angle), k_Tolerance);
	EXPECT_NEAR(std::abs(knee[0][1]), std::sin(angle), k_Tolerance);
	EXPECT_NEAR(knee[2][2], 1.0f, k_Tolerance);
	EXPECT_NEAR(poses.at(1).translation.y, 2.0f, k_Tolerance);
}

TEST(GltfCreature, ClipsMatchByNameWithoutRegardToCase)
{
	const auto model = TwoBones();
	const std::vector<std::string> names {"move/stand", "MOVE/WALK"};
	const auto matches = MatchClips(names, model.animations);
	ASSERT_EQ(matches.size(), 2u);
	EXPECT_FALSE(matches[0].has_value());
	ASSERT_TRUE(matches[1].has_value());
	EXPECT_EQ(*matches[1], 0u);
}

TEST(GltfCreature, TheSkinAveragesTheTexelsEachCovers)
{
	// 512 by 512 texels: each skin texel covers 2 by 2, half white and half black
	constexpr uint32_t k_Side = 512;
	std::vector<uint8_t> rgba(static_cast<size_t>(k_Side) * k_Side * 4, 0);
	for (uint32_t y = 0; y < k_Side; ++y)
	{
		for (uint32_t x = 0; x < k_Side; ++x)
		{
			const auto at = (static_cast<size_t>(y) * k_Side + x) * 4;
			const uint8_t value = x % 2 == 0 ? 255 : 0;
			rgba[at] = value;
			rgba[at + 1] = value;
			rgba[at + 2] = value;
			rgba[at + 3] = 255;
		}
	}
	const auto skin = SkinFromRgba(rgba, k_Side, k_Side, 7);
	EXPECT_EQ(skin->id, 7u);
	EXPECT_EQ(skin->texels.at(0).r, 8u);
	EXPECT_EQ(skin->texels.at(0).a, 15u);
}

TEST(GltfCreature, ASecondJointGetsAPartnerVertexInNoTriangle)
{
	auto model = TwoBones();
	model.primitives.push_back({
	    .positions = {{0.0f, 1.0f, 0.0f}, {0.0f, 2.0f, 0.0f}, {0.0f, 3.0f, 0.0f}},
	    .normals = {{0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f}},
	    .texCoords = {},
	    // The skin's joint 0 is bone 1 (the knee), its joint 1 is bone 0 (the hip). The middle vertex is mostly the hip's.
	    .joints = {{1, 0, 0, 0}, {1, 0, 0, 0}, {0, 0, 0, 0}},
	    .weights = {{1.0f, 0.0f, 0.0f, 0.0f}, {0.7f, 0.3f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f, 0.0f}},
	    .indices = {0, 1, 2},
	});
	const auto bones = BonesOfJoints(model);
	const auto rest = RestOf(model, bones);
	ASSERT_TRUE(rest.has_value());
	const auto file = BuildMesh(model, bones, *rest, *SkinFromRgba(std::vector<uint8_t>(4, 255), 1, 1, k_SkinId));
	ASSERT_EQ(file.GetVertices().size(), 4u);
	ASSERT_EQ(file.GetBlends().size(), 1u);
	EXPECT_NEAR(file.GetBlends().front().weight, 0.3f, k_Tolerance);
	// The hip's vertices run first, then the knee's
	const auto groups = file.GetVertexGroupSpan(0);
	ASSERT_EQ(groups.size(), 2u);
	EXPECT_EQ(groups[0].boneIndex, 0u);
	EXPECT_EQ(groups[0].vertexCount, 2u);
	EXPECT_EQ(groups[1].boneIndex, 1u);
	// The partner is in no triangle
	const auto& indices = file.GetIndices();
	ASSERT_EQ(indices.size(), 3u);
	EXPECT_EQ(std::ranges::count(indices, file.GetBlends().front().indices[1]), 0);
	// The blended vertex is one up in the hip's space, its partner one down in the knee's
	const auto& blended = file.GetVertices().at(file.GetBlends().front().indices[0]);
	const auto& partner = file.GetVertices().at(file.GetBlends().front().indices[1]);
	EXPECT_NEAR(blended.position.y, 1.0f, k_Tolerance);
	EXPECT_NEAR(partner.position.y, -1.0f, k_Tolerance);
}

TEST(GltfCreature, TheUndeadCowHasACowsSettingsAndNoMorph)
{
	EXPECT_EQ(creature::InfoRow(CreatureType::UndeadCow), creature::InfoRow(CreatureType::Cow));
	EXPECT_FALSE(creature::HasBodyMorph(CreatureType::UndeadCow));
	EXPECT_TRUE(creature::HasBodyMorph(CreatureType::Cow));
	const creature_morph::Morph morph {.evilGood = -0.8f, .thinFat = 0.5f, .weakStrong = 0.3f};
	const auto undead = creature_morph::ForSpecies(CreatureType::UndeadCow, morph);
	EXPECT_EQ(undead.evilGood, 0.0f);
	EXPECT_EQ(undead.thinFat, 0.0f);
	EXPECT_EQ(undead.weakStrong, 0.0f);
	EXPECT_EQ(creature_morph::ForSpecies(CreatureType::Cow, morph).evilGood, -0.8f);
}
