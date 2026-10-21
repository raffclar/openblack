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
/// camera, the poses of the creatures, the hand, the draw and then the keys of the camera. What one stage changes, the next
/// stages see in the same frame. Each step of openblack's frame has the stage of the game that it does. A step whose stage is
/// not known keeps its place between its neighbours. The order keeps no game state, so that the game and the tests use it both.
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
	/// The camera moves, with the movement of the mouse in this frame, and shakes
	Camera,
	/// The draw starts. A film shows its frames, and the creatures get their poses between their last two turns, before
	/// the hand moves.
	DrawStart,
	/// The hand goes to the point under the cursor, as the camera now sees it
	PreDraw,
	/// The world is drawn. The effects move on as the draw shows them, after the hand moved.
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
	DebugGui,
	Interface,
	GripEffect,
	Turns,
	FrameTime,
	CameraMouse,
	CameraPaths,
	CameraControllers,
	Camera,
	CameraShake,
	Temple,
	HighDetail,
	Video,
	PauseText,
	CreaturePoses,
	Hand,
	Bookmarks,
	FrameEffects,
	ScreenMarks,
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
    // The clock of the frame and the debug inspector, before the input. They are openblack's own and have no place in the
    // game's frame.
    {.step = Step::FrameClock, .stage = std::nullopt},
    // The keys, the mouse buttons and the pointer
    {.step = Step::Input, .stage = Stage::Input},
    // The debug windows are openblack's own and have no place in the game's frame
    {.step = Step::DebugGui, .stage = std::nullopt},
    {.step = Step::Interface, .stage = Stage::FrameUpdates},
    {.step = Step::Turns, .stage = Stage::Turns},
    {.step = Step::FrameTime, .stage = Stage::FrameTime},
    // The movement of the mouse turns, tilts and zooms the camera in the same frame
    {.step = Step::CameraMouse, .stage = Stage::Camera},
    // A miracle's camera path, the editor's camera and the creature's camera
    {.step = Step::CameraPaths, .stage = Stage::Camera},
    {.step = Step::CameraControllers, .stage = Stage::Camera},
    {.step = Step::Camera, .stage = Stage::Camera},
    // The camera shakes near a blast, before the hand looks through it
    {.step = Step::CameraShake, .stage = Stage::Camera},
    // The temple's camera can take the player out of the temple
    {.step = Step::Temple, .stage = Stage::Camera},
    // A full-screen film decodes the frames that are due, as the draw starts
    {.step = Step::Video, .stage = Stage::DrawStart},
    // The creatures' fights, walks, animations, hair and the sounds of their animations
    {.step = Step::CreaturePoses, .stage = Stage::DrawStart},
    {.step = Step::Hand, .stage = Stage::PreDraw},
    // The land's draw lets the villagers that a script draws in high detail go once the script's cinema bars are gone
    {.step = Step::HighDetail, .stage = Stage::Draw},
    // The dust where the hand grips the land moves on as the draw shows it, as the other spot effects do
    {.step = Step::GripEffect, .stage = Stage::Draw},
    // The markers of the bookmarks throb by the game time of this frame, as the draw shows them
    {.step = Step::Bookmarks, .stage = Stage::Draw},
    // The effects that the draw moves on: the weather, the fires, the villagers and the animals, the marks and the texts
    {.step = Step::FrameEffects, .stage = Stage::Draw},
    // The numbers and the marks that rise over things, by the game time of the frame
    {.step = Step::ScreenMarks, .stage = Stage::Draw},
    {.step = Step::Entities, .stage = Stage::Draw},
    // The words of the pause and the speed fade as the interface finishes the draw of the frame
    {.step = Step::PauseText, .stage = Stage::Draw},
    // The sound library's own work: the voices that finished are let go and the sounds follow the listener. The game's
    // sound library does this work itself, outside the game's frame.
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

/// What the player can do with the camera keys in this frame
struct CameraKeyState
{
	/// The player is in the temple
	bool inTemple;
	/// The interface takes the player's input, which a script can stop
	bool interfaceActive;
	/// A box of the menu or another dialog is on the screen
	bool boxShows;
};

/// True when the camera reads its keys after its step. In the temple, without the interface or with a box on the screen,
/// the camera does not read its keys and does not push. Its next step then gets no key steps.
[[nodiscard]] constexpr bool CameraReadsKeys(const CameraKeyState& state)
{
	return !state.inTemple && state.interfaceActive && !state.boxShows;
}

/// Whether a step is part of the frame's graphics: it moves the camera, puts the hand under the cursor or draws the
/// world. The game does none of these steps while its window is minimised. The turns, the keys and the other steps
/// continue.
[[nodiscard]] constexpr bool IsGraphics(Step step)
{
	for (const auto& place : k_Frame)
	{
		if (place.step == step)
		{
			return place.stage == Stage::Camera || place.stage == Stage::PreDraw || place.stage == Stage::Draw;
		}
	}
	return false;
}

/// Whether the frame does its graphics: not while the window is minimised, unless a picture of the frame is necessary
[[nodiscard]] constexpr bool DoesGraphics(bool minimised, bool pictureWanted)
{
	return !minimised || pictureWanted;
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
