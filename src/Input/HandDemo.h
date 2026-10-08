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

#include <optional>
#include <string_view>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

// The hand demos of the tutorial (Data\HandDemo\*.hnd): the interface's playback and the script's side
// (PLAY_HAND_DEMO, IS_PLAYING_HAND_DEMO, HAND_DEMO_TRIGGER). The recorded interface messages drive the real hand and
// the records set the camera.
// Wiki: docs/bw1-notes/hand-and-interface.md, "Hand demos".
namespace openblack::hand_demo
{

/// What the playback gives the frame: the mouse (normalised 0..1), the buttons as the recorded messages left them, and
/// the camera of the last record due
struct Frame
{
	glm::vec2 mouse {0.5f};
	bool grip {false};   ///< messages 1 GRAB_DOWN / 2 GRAB_UP
	bool action {false}; ///< messages 3 ACTION_DOWN / 4 ACTION_UP
	std::optional<glm::vec3> eye;
	std::optional<glm::vec3> focus;
};

/// PLAY_HAND_DEMO: starts the playback of ".\Data\HandDemo\<name>.hnd" for `task`, then sets the script's
/// wait-for-trigger flag to waitTrigger and clears its pending trigger. False when the file cannot be read.
bool Play(std::string_view name, uint32_t task, bool waitTrigger, bool keepHand);
/// Whether a demo plays, and when `task` is not 0, whether that task started it
[[nodiscard]] bool IsPlaying(uint32_t task = 0);
/// HAND_DEMO_TRIGGER: the script's pending trigger, which it clears
bool ConsumeTrigger();
/// Ends the playback
void End();
/// The task stop callback: ends the playback when the stopped task owns the demo
void EndIfTask(uint32_t task);
/// Once a frame before the hand's ray: the records due at the visual clock `nowMs`. Empty when no demo plays.
[[nodiscard]] std::optional<Frame> Update(uint32_t nowMs);

} // namespace openblack::hand_demo
