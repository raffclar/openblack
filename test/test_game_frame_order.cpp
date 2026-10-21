/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cstddef>

#include <algorithm>
#include <array>
#include <optional>
#include <vector>

#include <gtest/gtest.h>

#include "GameFrameOrder.h"

using namespace openblack::game_frame;

namespace
{

/// Records the steps that the frame does, in the order that it does them
class FrameRecorder
{
public:
	bool operator()(Step step)
	{
		_steps.push_back(step);
		return !_stopAt.has_value() || step != *_stopAt;
	}

	/// The frame stops after this step
	void StopAt(Step step) { _stopAt = step; }

	[[nodiscard]] const std::vector<Step>& Steps() const { return _steps; }

	/// Where the step is in the record
	[[nodiscard]] std::ptrdiff_t IndexOf(Step step) const
	{
		return std::distance(_steps.begin(), std::ranges::find(_steps, step));
	}

	[[nodiscard]] bool Before(Step first, Step second) const { return IndexOf(first) < IndexOf(second); }

private:
	std::vector<Step> _steps;
	std::optional<Step> _stopAt;
};

FrameRecorder PlayOneFrame()
{
	FrameRecorder recorder;
	EXPECT_TRUE(PlayFrame([&recorder](Step step) { return recorder(step); }));
	return recorder;
}

} // namespace

TEST(GameFrameOrder, EachStepHappensOnceInAFrame)
{
	const auto recorder = PlayOneFrame();
	ASSERT_EQ(recorder.Steps().size(), static_cast<size_t>(Step::_Count));
	for (size_t i = 0; i < static_cast<size_t>(Step::_Count); ++i)
	{
		const auto step = static_cast<Step>(i);
		EXPECT_EQ(std::ranges::count(recorder.Steps(), step), 1) << "step " << i;
	}
}

TEST(GameFrameOrder, TheFrameDoesItsStepsInTheOrderOfTheList)
{
	const auto recorder = PlayOneFrame();
	ASSERT_EQ(recorder.Steps().size(), k_Frame.size());
	for (size_t i = 0; i < k_Frame.size(); ++i)
	{
		EXPECT_EQ(recorder.Steps().at(i), k_Frame.at(i).step) << "place " << i;
	}
}

TEST(GameFrameOrder, TheStepsFollowTheStagesOfTheGame)
{
	EXPECT_TRUE(InGameOrder(k_Frame));
}

TEST(GameFrameOrder, AStepOfAnEarlierStageAfterALaterOneIsOutOfOrder)
{
	constexpr std::array k_Wrong = std::to_array<Place>({
	    {.step = Step::Camera, .stage = Stage::Camera},
	    {.step = Step::Turns, .stage = Stage::Turns},
	});
	EXPECT_FALSE(InGameOrder(k_Wrong));
	// A step with no stage keeps its place and does not count
	constexpr std::array k_Unknown = std::to_array<Place>({
	    {.step = Step::Turns, .stage = Stage::Turns},
	    {.step = Step::DebugGui, .stage = std::nullopt},
	    {.step = Step::Camera, .stage = Stage::Camera},
	});
	EXPECT_TRUE(InGameOrder(k_Unknown));
}

TEST(GameFrameOrder, AStepThatStopsTheFrameEndsIt)
{
	FrameRecorder recorder;
	recorder.StopAt(Step::Turns);
	EXPECT_FALSE(PlayFrame([&recorder](Step step) { return recorder(step); }));
	EXPECT_EQ(recorder.Steps().back(), Step::Turns);
	EXPECT_EQ(std::ranges::count(recorder.Steps(), Step::Camera), 0);
}

TEST(GameFrameOrder, TheInputAndTheInterfaceComeBeforeTheTurns)
{
	const auto recorder = PlayOneFrame();
	EXPECT_TRUE(recorder.Before(Step::Input, Step::Interface));
	EXPECT_TRUE(recorder.Before(Step::Interface, Step::Turns));
}

TEST(GameFrameOrder, TheCameraMovesAfterTheTurnsAndTheFrameTime)
{
	const auto recorder = PlayOneFrame();
	EXPECT_TRUE(recorder.Before(Step::Turns, Step::FrameTime));
	EXPECT_TRUE(recorder.Before(Step::FrameTime, Step::CameraMouse));
	EXPECT_TRUE(recorder.Before(Step::CameraMouse, Step::Camera));
	EXPECT_TRUE(recorder.Before(Step::Turns, Step::Temple));
}

TEST(GameFrameOrder, TheHandGoesUnderTheCursorAfterTheCameraMovesAndBeforeTheDraw)
{
	const auto recorder = PlayOneFrame();
	EXPECT_TRUE(recorder.Before(Step::Camera, Step::Hand));
	EXPECT_TRUE(recorder.Before(Step::Hand, Step::Entities));
}

TEST(GameFrameOrder, TheKeysOfTheCameraActAfterTheCameraMoves)
{
	const auto recorder = PlayOneFrame();
	EXPECT_TRUE(recorder.Before(Step::Camera, Step::CameraKeys));
	EXPECT_TRUE(recorder.Before(Step::Entities, Step::CameraKeys));
	// So the steps of this frame's keys move the camera in the next frame, after the next frame's turns
	EXPECT_EQ(recorder.Steps().back(), Step::ProfileAlignment);
}

TEST(GameFrameOrder, TheCameraReadsItsKeysOnlyOutsideTheTempleWithTheInterfaceAndNoBox)
{
	EXPECT_TRUE(CameraReadsKeys({.inTemple = false, .interfaceActive = true, .boxShows = false}));
	EXPECT_FALSE(CameraReadsKeys({.inTemple = true, .interfaceActive = true, .boxShows = false}));
	EXPECT_FALSE(CameraReadsKeys({.inTemple = false, .interfaceActive = false, .boxShows = false}));
	EXPECT_FALSE(CameraReadsKeys({.inTemple = false, .interfaceActive = true, .boxShows = true}));
}

TEST(GameFrameOrder, TheCameraShakesBeforeTheHandLooksThroughIt)
{
	const auto recorder = PlayOneFrame();
	EXPECT_TRUE(recorder.Before(Step::Camera, Step::CameraShake));
	EXPECT_TRUE(recorder.Before(Step::CameraShake, Step::Hand));
}

TEST(GameFrameOrder, TheCreaturesGetTheirPosesBeforeTheHandAndTheEffectsMoveAfterIt)
{
	const auto recorder = PlayOneFrame();
	EXPECT_TRUE(recorder.Before(Step::Camera, Step::CreaturePoses));
	EXPECT_TRUE(recorder.Before(Step::CreaturePoses, Step::Hand));
	EXPECT_TRUE(recorder.Before(Step::Hand, Step::FrameEffects));
	EXPECT_TRUE(recorder.Before(Step::Hand, Step::ScreenMarks));
	// The draw shows what the effects did in this frame
	EXPECT_TRUE(recorder.Before(Step::FrameEffects, Step::Entities));
	EXPECT_TRUE(recorder.Before(Step::ScreenMarks, Step::Entities));
}

TEST(GameFrameOrder, TheScreenMarksRiseByTheGameTimeOfTheFrame)
{
	const auto recorder = PlayOneFrame();
	EXPECT_TRUE(recorder.Before(Step::FrameTime, Step::ScreenMarks));
}

TEST(GameFrameOrder, AFilmShowsItsFramesAfterTheCameraAndBeforeTheCreaturesArePosed)
{
	const auto recorder = PlayOneFrame();
	EXPECT_TRUE(recorder.Before(Step::Camera, Step::Video));
	EXPECT_TRUE(recorder.Before(Step::Video, Step::PauseText));
	EXPECT_TRUE(recorder.Before(Step::Video, Step::CreaturePoses));
}

TEST(GameFrameOrder, TheBookmarksThrobAsTheDrawShowsThem)
{
	const auto recorder = PlayOneFrame();
	EXPECT_TRUE(recorder.Before(Step::Hand, Step::Bookmarks));
	EXPECT_TRUE(recorder.Before(Step::Bookmarks, Step::Entities));
}

// While the window is minimised the game does not move the camera, put the hand under the cursor or draw the world
TEST(GameFrameOrder, TheGraphicsAreTheCameraThePreDrawAndTheDraw)
{
	for (const auto step : {Step::CameraMouse, Step::CameraPaths, Step::CameraControllers, Step::Camera, Step::Temple,
	                        Step::Hand, Step::Entities})
	{
		EXPECT_TRUE(IsGraphics(step)) << static_cast<int>(step);
	}
	for (const auto step : {Step::FrameClock, Step::Input, Step::Interface, Step::Turns, Step::FrameTime, Step::CameraKeys,
	                        Step::ProfileAlignment, Step::Audio})
	{
		EXPECT_FALSE(IsGraphics(step)) << static_cast<int>(step);
	}
}

// A minimised window skips the graphics, except in a frame that a picture is necessary from
TEST(GameFrameOrder, AMinimisedWindowSkipsTheGraphicsUnlessAPictureIsWanted)
{
	EXPECT_TRUE(DoesGraphics(false, false));
	EXPECT_FALSE(DoesGraphics(true, false));
	EXPECT_TRUE(DoesGraphics(true, true));
}
