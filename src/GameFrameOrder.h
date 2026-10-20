/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <array>
#include <optional>
#include <span>

/// The order of the steps of one frame. The game does its frame in a fixed order of stages: the input, the turns, the
/// camera, the hand, the draw and then the keys of the camera. What one stage changes, the next stages see in the same
/// frame. Each step of openblack's frame has the stage of the game that it does. A step whose stage is not known keeps
/// its place between its neighbours. The order keeps no game state, so that the game and the tests use it both.
namespace openblack::game_frame
{

/// The stages of the game's frame, in the game's order. The game reads the mouse buttons and does the frame updates of
/// the interface after it draws a frame. Nothing changes between that point and the input of the next frame, so
/// openblack does them after the input, before the turns.
enum class Stage : uint8_t
{
	/// The keys and the mouse buttons that the player pressed
	Input,
	/// The menus and the dialogs of the interface take the input of the frame
	FrameUpdates,
	/// The turns of the game that are due, zero or one in a frame
	Turns,
	/// The game time of the frame, which is zero while the game is paused
	FrameTime,
	/// The camera moves, with the movement of the mouse in this frame
	Camera,
	/// The hand goes to the point under the cursor, as the camera now sees it
	PreDraw,
	/// The world is drawn
	Draw,
	/// The keys of the camera. Their steps move the camera in the next frame.
	CameraKeys,
	/// The end of the frame: the alignment of the player goes into the profile
	EndFrame,
};

/// The steps of openblack's frame
enum class Step : uint8_t
{
	FrameClock,
	Input,
	ScreenMarks,
	DebugGui,
	Interface,
	GripEffect,
	Turns,
	FrameTime,
	Video,
	PauseText,
	CameraMouse,
	CameraPaths,
	CameraControllers,
	Camera,
	Temple,
	Bookmarks,
	HighDetail,
	FrameEffects,
	Hand,
	Entities,
	Audio,
	CameraKeys,
	ProfileAlignment,

	_Count
};

/// A step of the frame and the stage of the game that it does. No stage: the place of the step in the game's frame is
/// not known, and the step keeps its place between its neighbours.
struct Place
{
	Step step;
	std::optional<Stage> stage;
};

/// The frame, step by step
inline constexpr std::array k_Frame = std::to_array<Place>({
    // The clock of the frame and the debug inspector, before the input
    {.step = Step::FrameClock, .stage = std::nullopt},
    // The keys, the mouse buttons and the pointer
    {.step = Step::Input, .stage = Stage::Input},
    // The numbers and the marks that rise over things, by the real time
    {.step = Step::ScreenMarks, .stage = std::nullopt},
    {.step = Step::DebugGui, .stage = std::nullopt},
    {.step = Step::Interface, .stage = Stage::FrameUpdates},
    {.step = Step::GripEffect, .stage = std::nullopt},
    {.step = Step::Turns, .stage = Stage::Turns},
    {.step = Step::FrameTime, .stage = Stage::FrameTime},
    {.step = Step::Video, .stage = std::nullopt},
    {.step = Step::PauseText, .stage = std::nullopt},
    // The movement of the mouse turns, tilts and zooms the camera in the same frame
    {.step = Step::CameraMouse, .stage = Stage::Camera},
    // A miracle's camera path, the editor's camera and the creature's camera
    {.step = Step::CameraPaths, .stage = Stage::Camera},
    {.step = Step::CameraControllers, .stage = Stage::Camera},
    {.step = Step::Camera, .stage = Stage::Camera},
    // The temple's camera can take the player out of the temple
    {.step = Step::Temple, .stage = Stage::Camera},
    // The markers of the bookmarks throb by the game time of this frame
    {.step = Step::Bookmarks, .stage = std::nullopt},
    {.step = Step::HighDetail, .stage = std::nullopt},
    // The effects of the frame. Each has its own place in the game, which is not known for all of them.
    {.step = Step::FrameEffects, .stage = std::nullopt},
    {.step = Step::Hand, .stage = Stage::PreDraw},
    {.step = Step::Entities, .stage = Stage::Draw},
    {.step = Step::Audio, .stage = std::nullopt},
    // The keys of the camera act after the camera moved, so their steps move the camera in the next frame
    {.step = Step::CameraKeys, .stage = Stage::CameraKeys},
    {.step = Step::ProfileAlignment, .stage = Stage::EndFrame},
});

/// True when the steps with a stage follow the stages of the game
[[nodiscard]] constexpr bool InGameOrder(std::span<const Place> frame)
{
	std::optional<Stage> last;
	for (const auto& place : frame)
	{
		if (!place.stage.has_value())
		{
			continue;
		}
		if (last.has_value() && *place.stage < *last)
		{
			return false;
		}
		last = place.stage;
	}
	return true;
}

/// Does each step of the frame, in the order of the frame. A step that returns false stops the frame.
template <typename Do>
bool PlayFrame(Do&& step)
{
	for (const auto& place : k_Frame)
	{
		if (!step(place.step))
		{
			return false;
		}
	}
	return true;
}

static_assert(k_Frame.size() == static_cast<size_t>(Step::_Count), "Each step has one place in the frame");
static_assert(InGameOrder(k_Frame), "The steps of the frame follow the game's stages");

} // namespace openblack::game_frame
