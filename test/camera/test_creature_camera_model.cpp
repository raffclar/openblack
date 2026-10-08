/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstdint>

#include <chrono>
#include <initializer_list>
#include <memory>
#include <optional>
#include <utility>

#include <glm/geometric.hpp>
#include <gtest/gtest.h>

#include "Camera/Camera.h"
#include "Camera/CreatureCameraModel.h"
#include "Camera/CreatureFollow.h"
#include "Camera/ScriptCamera.h"
#include "scenarios/Mock.h"
#include "support/RestoreService.h"

using namespace openblack;
using input::BindableActionMap;

namespace
{
constexpr glm::vec3 k_CameraOrigin {1000.0f, 200.0f, 900.0f};
constexpr glm::vec3 k_CameraFocus {1000.0f, 0.0f, 1100.0f};
constexpr glm::vec3 k_Creature {1000.0f, 0.0f, 1000.0f};
constexpr float k_CreatureHeight = 15.0f;
constexpr float k_FrameSeconds = 0.016f;
constexpr auto k_Step = std::chrono::milliseconds(10);

/// The keys held this frame, and no others
class HeldKeys final: public MockAction
{
public:
	explicit HeldKeys(std::initializer_list<BindableActionMap> held)
	{
		for (const auto action : held)
		{
			_held |= static_cast<uint64_t>(action);
		}
	}
	[[nodiscard]] bool GetBindable(BindableActionMap action) const override
	{
		return (_held & static_cast<uint64_t>(action)) != 0;
	}

private:
	uint64_t _held {0};
};

/// The model on the creature, its time moving on by a steady frame
std::unique_ptr<CreatureCameraModel> MakeModel(float frameSeconds = k_FrameSeconds)
{
	return std::make_unique<CreatureCameraModel>(k_CameraOrigin, k_CameraFocus, k_Creature, k_CreatureHeight,
	                                             [frameSeconds]() { return frameSeconds; });
}

/// The keys, the screen and the level land the model reads, put back as they were afterwards
class CreatureCameraModelTest: public ::testing::Test
{
protected:
	void Hold(std::initializer_list<BindableActionMap> held) { Locator::gameActionSystem::emplace<HeldKeys>(held); }
	void SetUp() override
	{
		Locator::windowing::emplace<MockWindowingSystem>();
		Locator::terrainSystem::emplace<MockTerrain>();
		Hold({});
	}

private:
	test::RestoreService<Locator::gameActionSystem> _actions;
	test::RestoreService<Locator::windowing> _windowing;
	test::RestoreService<Locator::terrainSystem> _terrain;
};
} // namespace

TEST(CreatureCameraModel, StartsOnTheCreaturesMiddleWithTheFollowsView)
{
	const auto model = MakeModel();
	const auto start = creature_follow::Start(k_CameraOrigin, k_CameraFocus, k_Creature, k_CreatureHeight);
	EXPECT_EQ(model->GetTargetFocus(), creature_follow::Focus(k_Creature, k_CreatureHeight));
	EXPECT_FLOAT_EQ(model->GetView().yaw, start.yaw);
	EXPECT_FLOAT_EQ(model->GetView().pitch, start.pitch);
	EXPECT_FLOAT_EQ(model->GetView().distance, start.distance);
	EXPECT_EQ(model->GetTargetOrigin(), creature_follow::Origin(model->GetTargetFocus(), model->GetView()));
	EXPECT_FALSE(model->WantsToLeave());
	EXPECT_FLOAT_EQ(model->GetSecondsInMode(), 0.0f);
}

TEST(CreatureCameraModel, HasNoLensOfItsOwnSoTheWorldDiscKeepsIt)
{
	// It looks at the open world: the configured projection, and the camera's world-disc clamp as for the player
	const auto model = MakeModel();
	EXPECT_FALSE(model->GetLens().has_value());
	EXPECT_EQ(model->GetIdleTime(), std::chrono::seconds::zero());
}

TEST(CreatureCameraModel, EachUpdateSetsOffForTheViewArrivingAfterTheFollowsEase)
{
	Camera camera;
	const auto model = MakeModel();
	const auto info = model->Update(k_Step, camera);
	ASSERT_TRUE(info.has_value());
	EXPECT_EQ(info->focus, creature_follow::Focus(k_Creature, k_CreatureHeight));
	EXPECT_EQ(info->origin, creature_follow::Origin(info->focus, creature_follow::Clamped(model->GetView())));
	// Two seconds as the mode starts, the script camera's follow at its own pace
	EXPECT_EQ(info->duration, std::chrono::seconds(2));
	EXPECT_FLOAT_EQ(script_camera::FollowSeconds(0.0f, 1.0f), 2.0f);
}

TEST(CreatureCameraModel, TheModesTimeMovesOnByTheCamerasFrameNotTheStep)
{
	Camera camera;
	const auto model = MakeModel(0.25f);
	// The step it is given is not what moves its time on: the camera's frame seconds are
	(void)model->Update(std::chrono::seconds(1), camera);
	EXPECT_FLOAT_EQ(model->GetSecondsInMode(), 0.25f);
	// The ease is read before the time moves on: this frame's is the one for a quarter of a second in
	const auto info = model->Update(std::chrono::seconds(1), camera);
	ASSERT_TRUE(info.has_value());
	EXPECT_FLOAT_EQ(model->GetSecondsInMode(), 0.5f);
	const auto expected = std::chrono::duration_cast<std::chrono::microseconds>(
	    std::chrono::duration<float>(creature_follow::EaseSeconds(0.25f)));
	EXPECT_EQ(info->duration, expected);
}

TEST(CreatureCameraModel, TheEaseSettlesToASecondAfterTwo)
{
	Camera camera;
	const auto model = MakeModel(0.5f);
	std::optional<CameraModel::CameraInterpolationUpdateInfo> info;
	for (int frame = 0; frame < 6; ++frame)
	{
		info = model->Update(k_Step, camera);
	}
	ASSERT_TRUE(info.has_value());
	EXPECT_FLOAT_EQ(model->GetSecondsInMode(), 3.0f);
	EXPECT_EQ(info->duration, std::chrono::seconds(1));
}

TEST(CreatureCameraModel, FollowsTheCreatureAsItMoves)
{
	Camera camera;
	const auto model = MakeModel();
	const glm::vec3 moved {1200.0f, 30.0f, 1000.0f};
	model->SetTarget(moved, 30.0f);
	EXPECT_EQ(model->GetTargetFocus(), creature_follow::Focus(moved, 30.0f));
	const auto info = model->Update(k_Step, camera);
	ASSERT_TRUE(info.has_value());
	EXPECT_EQ(info->focus, creature_follow::Focus(moved, 30.0f));
	EXPECT_NEAR(glm::distance(info->origin, info->focus), model->GetView().distance, 1e-2f);
}

TEST(CreatureCameraModel, TheCameraTakesTheUpdateAsItIs)
{
	// The camera's zoomers set off for the model's origin and focus, over its duration
	Camera camera;
	auto owned = MakeModel();
	auto* model = owned.get();
	(void)camera.SetModel(std::move(owned));
	const auto expected = creature_follow::Origin(model->GetTargetFocus(), creature_follow::Clamped(model->GetView()));
	camera.Update(k_Step);
	EXPECT_EQ(camera.GetOriginZoomer().GetDestination(), expected);
	EXPECT_EQ(camera.GetFocusZoomer().GetDestination(), creature_follow::Focus(k_Creature, k_CreatureHeight));
}

TEST(CreatureCameraModel, SentSomewhereItLooksFromTheFlightsWay)
{
	auto model = MakeModel();
	const glm::vec3 origin {1000.0f, 100.0f, 900.0f};
	const glm::vec3 focus {1000.0f, 0.0f, 1000.0f};
	model->SetFlight(origin, focus);
	const auto turned = creature_follow::HeadingPitchOf(origin, focus);
	EXPECT_FLOAT_EQ(model->GetView().yaw, turned.heading);
	EXPECT_FLOAT_EQ(model->GetView().pitch, turned.pitch);
	EXPECT_FLOAT_EQ(model->GetView().distance, glm::distance(origin, focus));
	// and within the follow's bounds: from straight below it is kept at the least pitch
	model->SetFlight(glm::vec3(1000.0f, -50.0f, 1000.0f), focus);
	EXPECT_FLOAT_EQ(model->GetView().pitch, creature_follow::Clamped(model->GetView()).pitch);
	EXPECT_GE(model->GetView().pitch, creature_follow::k_MinPitch);
}

TEST_F(CreatureCameraModelTest, TheCursorKeysAloneGiveTheCameraBack)
{
	auto model = MakeModel();
	const auto before = model->GetView();
	Hold({BindableActionMap::MOVE_LEFT});
	model->HandleActions(k_Step);
	EXPECT_TRUE(model->WantsToLeave());
	EXPECT_FLOAT_EQ(model->GetView().yaw, before.yaw);
	EXPECT_FLOAT_EQ(model->GetView().pitch, before.pitch);
}

TEST_F(CreatureCameraModelTest, NoKeysLeaveTheViewAsItIs)
{
	auto model = MakeModel();
	const auto before = model->GetView();
	model->HandleActions(k_Step);
	EXPECT_FALSE(model->WantsToLeave());
	EXPECT_FLOAT_EQ(model->GetView().yaw, before.yaw);
	EXPECT_FLOAT_EQ(model->GetView().pitch, before.pitch);
	EXPECT_FLOAT_EQ(model->GetView().distance, before.distance);
}

TEST_F(CreatureCameraModelTest, ShiftAndTheCursorKeysTurnAndTiltIt)
{
	auto model = MakeModel();
	auto expected = model->GetView();
	Hold({BindableActionMap::MOVE_LEFT, BindableActionMap::MOVE_FORWARDS, BindableActionMap::ROTATE_ON});
	model->HandleActions(k_Step);
	EXPECT_FALSE(model->WantsToLeave());
	// as the keys the follow reads, a hundredth of a second of the left and up cursor keys on an 800 wide screen
	auto keys = creature_follow::KeysFor(-1, -1, 0.01f);
	keys.shift = true;
	ASSERT_EQ(creature_follow::Apply(expected, keys, static_cast<float>(k_Width)), creature_follow::KeyOutcome::Stay);
	EXPECT_FLOAT_EQ(model->GetView().yaw, expected.yaw);
	EXPECT_FLOAT_EQ(model->GetView().pitch, expected.pitch);
	EXPECT_FLOAT_EQ(model->GetView().distance, expected.distance);
}

TEST_F(CreatureCameraModelTest, CtrlAndTheCursorKeysTurnItAndDrawItOut)
{
	auto model = MakeModel();
	const auto before = model->GetView();
	Hold({BindableActionMap::MOVE_RIGHT, BindableActionMap::MOVE_BACKWARDS, BindableActionMap::ZOOM_ON});
	model->HandleActions(k_Step);
	EXPECT_FALSE(model->WantsToLeave());
	EXPECT_NE(model->GetView().yaw, before.yaw);
	EXPECT_FLOAT_EQ(model->GetView().pitch, before.pitch);
	EXPECT_GT(model->GetView().distance, before.distance);
}

TEST_F(CreatureCameraModelTest, TheWheelDrawsItInAndOut)
{
	auto model = MakeModel();
	const auto before = model->GetView().distance;
	Hold({BindableActionMap::ZOOM_IN});
	model->HandleActions(k_Step);
	const auto in = model->GetView().distance;
	EXPECT_LT(in, before);
	Hold({BindableActionMap::ZOOM_OUT});
	model->HandleActions(k_Step);
	EXPECT_GT(model->GetView().distance, in);
	EXPECT_FALSE(model->WantsToLeave());
}

TEST_F(CreatureCameraModelTest, CtrlAndShiftTogetherSwingItClearOfTheLand)
{
	auto model = MakeModel();
	auto expected = model->GetView();
	Hold({BindableActionMap::ROTATE_ON, BindableActionMap::ZOOM_ON});
	model->HandleActions(k_Step);
	// The keys first (no cursor key: only the distance's bounds), then the clear view over the fake's level land
	auto keys = creature_follow::KeysFor(0, 0, 0.01f);
	keys.shift = true;
	keys.ctrl = true;
	(void)creature_follow::Apply(expected, keys, static_cast<float>(k_Width));
	expected = creature_follow::Clamped(creature_follow::ClearView(
	    expected, model->GetTargetFocus(), glm::vec3(0.0f, 1.0f, 0.0f), [](glm::vec2 /*point*/) { return 0.0f; }));
	EXPECT_FLOAT_EQ(model->GetView().yaw, expected.yaw);
	EXPECT_FLOAT_EQ(model->GetView().pitch, expected.pitch);
	EXPECT_FLOAT_EQ(model->GetView().distance, expected.distance);
	EXPECT_FALSE(model->WantsToLeave());
}

TEST_F(CreatureCameraModelTest, WithoutKeysToReadItDoesNothing)
{
	auto model = MakeModel();
	const auto before = model->GetView();
	Locator::gameActionSystem::reset();
	model->HandleActions(k_Step);
	EXPECT_FALSE(model->WantsToLeave());
	EXPECT_FLOAT_EQ(model->GetView().yaw, before.yaw);
	EXPECT_FLOAT_EQ(model->GetView().distance, before.distance);
}
