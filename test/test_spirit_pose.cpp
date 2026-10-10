/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The advisors' poses (src/Help/SpiritPose.h) on a made-up skeleton of three bones in a line: the layers' rules, the
// head's turn and the fingertip.

#include <cmath>
#include <cstdint>

#include <array>
#include <numbers>
#include <vector>

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <gtest/gtest.h>

#include "Help/SpiritPose.h"
#include "Help/SpiritView.h"

using namespace openblack;
using namespace openblack::help::spirits;

namespace
{
constexpr float k_Near = 1e-5f;

/// Three bones up the y axis, one unit apart
SpiritRig MakeRig()
{
	const std::array<uint32_t, 3> parents {skeletal_animation::k_NoParent, 0, 1};
	const std::array<glm::mat4, 3> rest {glm::mat4(1.0f), glm::translate(glm::mat4(1.0f), {0.0f, 1.0f, 0.0f}),
	                                     glm::translate(glm::mat4(1.0f), {0.0f, 2.0f, 0.0f})};
	return MakeSpiritRig(parents, rest);
}

/// A still animation turning the given bone about z by an angle and moving it to a place
skeletal_animation::Animation Turning(uint32_t bone, float angle, glm::vec3 place)
{
	skeletal_animation::Animation clip {.duration = 1000,
	                                    .looping = true,
	                                    .rotatedJoints = {bone},
	                                    .translatedJoints = {bone},
	                                    .frames = {},
	                                    .displacement = glm::vec3(0.0f)};
	clip.frames.push_back({.eulerAngles = {{0.0f, 0.0f, angle}}, .translations = {place}});
	clip.frames.push_back({.eulerAngles = {{0.0f, 0.0f, angle}}, .translations = {place}});
	return clip;
}

void ExpectNear(const glm::vec3& a, const glm::vec3& b)
{
	EXPECT_NEAR(a.x, b.x, k_Near);
	EXPECT_NEAR(a.y, b.y, k_Near);
	EXPECT_NEAR(a.z, b.z, k_Near);
}
} // namespace

TEST(SpiritPose, RestPoseComesBackWithoutLayers)
{
	const auto rig = MakeRig();
	const DudeData data;
	const auto world = EvaluatePose(rig, data, {}, glm::mat4(1.0f));
	ASSERT_EQ(world.size(), 3u);
	ExpectNear(glm::vec3(world[2][3]), {0.0f, 2.0f, 0.0f});
}

TEST(SpiritPose, SetWithoutFillLeavesOtherBones)
{
	const auto rig = MakeRig();
	DudeData data;
	data.clips[anim::k_Stand] = Turning(1, 0.0f, {0.0f, 3.0f, 0.0f});
	data.clips[anim::k_Hover] = Turning(2, 0.0f, {0.0f, 5.0f, 0.0f});
	// The hover alone moves only the last bone: the middle one keeps its rest place
	std::vector<AnimLayer> layers {{.kind = AnimLayer::Kind::Set, .clip = anim::k_Hover}};
	auto world = EvaluatePose(rig, data, layers, glm::mat4(1.0f));
	ExpectNear(glm::vec3(world[1][3]), {0.0f, 1.0f, 0.0f});
	ExpectNear(glm::vec3(world[2][3]), {0.0f, 6.0f, 0.0f});
	// With the stand under it as the fill, the middle bone takes the stand's place
	layers.insert(layers.begin(), {.kind = AnimLayer::Kind::Set, .clip = anim::k_Stand, .fill = true});
	world = EvaluatePose(rig, data, layers, glm::mat4(1.0f));
	ExpectNear(glm::vec3(world[1][3]), {0.0f, 3.0f, 0.0f});
	ExpectNear(glm::vec3(world[2][3]), {0.0f, 8.0f, 0.0f});
}

TEST(SpiritPose, BlendRunsFromTheFirstToTheSecond)
{
	const auto rig = MakeRig();
	DudeData data;
	data.clips[anim::k_Hover] = Turning(1, 0.0f, {0.0f, 2.0f, 0.0f});
	data.clips[anim::k_HoverStable] = Turning(1, 0.0f, {0.0f, 4.0f, 0.0f});
	AnimLayer blend {.kind = AnimLayer::Kind::SetBlend, .clip = anim::k_Hover, .clipB = anim::k_HoverStable};
	for (const float t : {0.0f, 0.25f, 1.0f})
	{
		blend.blend = t;
		const auto world = EvaluatePose(rig, data, std::span(&blend, 1), glm::mat4(1.0f));
		ExpectNear(glm::vec3(world[1][3]), {0.0f, 2.0f + 2.0f * t, 0.0f});
	}
}

TEST(SpiritPose, EyesScaleTheirBonesOnly)
{
	const auto rig = MakeRig();
	DudeData data;
	data.faceBones = {1, 0, 0, 2, 0, 0, 0};
	const AnimLayer scale {.kind = AnimLayer::Kind::ScaleEyes, .blend = 2.0f, .blendB = 0.5f};
	const auto world = EvaluatePose(rig, data, std::span(&scale, 1), glm::mat4(1.0f));
	EXPECT_NEAR(glm::length(glm::vec3(world[0][0])), 1.0f, k_Near);
	EXPECT_NEAR(glm::length(glm::vec3(world[1][0])), 2.0f, k_Near);
	// The second eye's bone is under the first's
	EXPECT_NEAR(glm::length(glm::vec3(world[2][0])), 1.0f, k_Near);
}

TEST(SpiritPose, ModelMatrixPutsTheBonesInTheWorld)
{
	const auto rig = MakeRig();
	const DudeData data;
	const glm::mat3 rows(glm::vec3(2.0f, 0.0f, 0.0f), glm::vec3(0.0f, 2.0f, 0.0f), glm::vec3(0.0f, 0.0f, 2.0f));
	const auto world = EvaluatePose(rig, data, {}, ModelMatrix(rows, {10.0f, 0.0f, 0.0f}));
	ExpectNear(glm::vec3(world[2][3]), {10.0f, 4.0f, 0.0f});
}

TEST(SpiritPose, HeadTurnsTowardsTheTarget)
{
	// The head is the last bone, the eyes the first two: between them is (0, 0.5, 0)
	const std::array<glm::mat4, 3> world {glm::mat4(1.0f), glm::translate(glm::mat4(1.0f), {0.0f, 1.0f, 0.0f}),
	                                      glm::mat4(1.0f)};
	const std::array<uint32_t, helpdude::k_FaceBones> bones {0, 0, 0, 1, 0, 0, 2};
	// Straight along the head's y: no turn
	EXPECT_EQ(HeadAngles(world, bones, {0.0f, 10.0f, 0.0f}), glm::vec2(0.0f));
	// Behind it: none either
	EXPECT_EQ(HeadAngles(world, bones, {5.0f, -10.0f, 0.0f}), glm::vec2(0.0f));
	// Far to the side: as far as it goes, a sixth of a turn
	const auto side = HeadAngles(world, bones, {0.0f, 0.5f, 100.0f});
	EXPECT_NEAR(side.x, 1.04719758f * 0.47746482f, k_Near);
	EXPECT_NEAR(side.y, 0.0f, k_Near);
	const auto up = HeadAngles(world, bones, {1.0f, 1.5f, 0.0f});
	EXPECT_NEAR(up.y, std::asin(1.0f / std::sqrt(2.0f)) * 0.47746482f, k_Near);
}

TEST(SpiritPose, FingertipFollowsTheRootsAxes)
{
	DudeData data;
	data.modelSize = 10.0f;
	data.fingertipOffsetRow0 = 0.5f;
	data.fingertipOffsetRow2 = 0.25f;
	// The root turned a quarter about y and moved: its first axis is -z, its third +x
	glm::mat4 root = glm::rotate(glm::mat4(1.0f), std::numbers::pi_v<float> * 0.5f, {0.0f, 1.0f, 0.0f});
	root[3] = glm::vec4(1.0f, 2.0f, 3.0f, 1.0f);
	const std::array world {root};
	ExpectNear(Fingertip(world, data, glm::vec3(0.0f)), {1.0f + 2.5f, 2.0f, 3.0f - 5.0f});
	EXPECT_EQ(Fingertip({}, data, glm::vec3(7.0f)), glm::vec3(7.0f));
}

namespace
{
/// A camera at the origin looking down +z, a quarter turn wide and 4:3, as glm makes it (left-handed: +x is right)
SpiritView MakeView()
{
	SpiritView view;
	view.eye = glm::vec3(0.0f);
	view.right = {1.0f, 0.0f, 0.0f};
	view.up = {0.0f, 1.0f, 0.0f};
	view.forward = {0.0f, 0.0f, 1.0f};
	view.nearClip = 1.0f;
	view.screen = {.width = 640, .height = 480};
	const glm::mat4 projection = glm::perspective(std::numbers::pi_v<float> * 0.5f, 4.0f / 3.0f, 1.0f, 1000.0f);
	view.halfExtent = {1.0f / projection[0][0], 1.0f / projection[1][1]};
	view.viewProjection = projection * glm::lookAt(view.eye, view.forward, view.up);
	return view;
}
} // namespace

TEST(SpiritView, PointsOnTheScreenAndBack)
{
	const auto view = MakeView();
	// The middle of the screen is straight ahead, at the near plane for a depth of 0
	ExpectNear(PointFromScreen(view, {320.0f, 240.0f}, 10.0f), {0.0f, 0.0f, 10.0f});
	ExpectNear(PointFromScreen(view, {320.0f, 240.0f}, 0.0f), {0.0f, 0.0f, 1.0f});
	// A negative depth is behind the camera, mirrored through the eye
	const auto left = PointFromScreen(view, {0.0f, 240.0f}, 10.0f);
	ExpectNear(PointFromScreen(view, {0.0f, 240.0f}, -10.0f), -left);
	// And back onto the screen
	const auto projected = ProjectPoint(view, left);
	ASSERT_TRUE(projected.has_value());
	EXPECT_NEAR(projected->x, 0, 1);
	EXPECT_EQ(projected->y, 240);
	EXPECT_NEAR(projected->depth, 10.0f, 1e-3f);
	const auto pixel = WorldToPixel(view, PointFromScreen(view, {160.0f, 120.0f}, 20.0f), false);
	ASSERT_TRUE(pixel.has_value());
	EXPECT_NEAR(pixel->x, 160.0f, 1e-2f);
	EXPECT_NEAR(pixel->y, 120.0f, 1e-2f);
}

TEST(SpiritView, PointsOffTheScreenNeedForcing)
{
	const auto view = MakeView();
	// Closer than the near plane: never
	EXPECT_FALSE(ProjectPoint(view, {0.0f, 0.0f, 0.5f}).has_value());
	EXPECT_FALSE(WorldToPixel(view, {0.0f, 0.0f, 0.5f}, true).has_value());
	// Off to the side: only forced, and not kept on the screen
	const glm::vec3 aside = PointFromScreen(view, {-320.0f, 240.0f}, 10.0f);
	EXPECT_FALSE(WorldToPixel(view, aside, false).has_value());
	const auto forced = WorldToPixel(view, aside, true);
	ASSERT_TRUE(forced.has_value());
	EXPECT_NEAR(forced->x, -320.0f, 1e-2f);
}
