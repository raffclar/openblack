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

#include <string_view>

namespace openblack::ecs::systems
{

/// The debug inspector: a local server that agents and tools query for the game's state (entities, components, the
/// moon, splashes and the like) and control the run through (pause, resume, step, scenarios). Built only with the
/// inspector option and started only by the --inspect-port flag; without them it is not there at all.
class InspectorSystemInterface
{
public:
	virtual ~InspectorSystemInterface() = default;

	/// Once a frame, before the game's turn: answers the requests that came in since the last frame, then holds or
	/// releases the game for any stepping asked for
	virtual void Service() = 0;
	/// The port it listens on, on 127.0.0.1
	[[nodiscard]] virtual uint16_t GetPort() const = 0;
	/// The game is about to load something that takes long (a land, the testbed), during which it serves no frames:
	/// meanwhile the inspector answers on its own, saying it is loading (game.state is not ready, other queries are
	/// refused as loading), so tools wait rather than time out. Nested loads count as one.
	virtual void BeginLoading(std::string_view what) = 0;
	/// The load has finished: requests are answered by the game's frames again
	virtual void EndLoading() = 0;
	/// Once a frame, just before it is drawn, after everything the game does with its camera: the frame is drawn from
	/// where the inspector shows the camera (an override, a picture's camera or framing), over whatever holds it
	virtual void PlaceCamera() = 0;
	/// Once the frame is drawn, and after the pointer's work: the camera gets its own place back, so that the rest of
	/// the game (the sound, the scripts, the camera's own movement) goes by the game's own camera, never by the one shown
	virtual void GiveCameraBack() = 0;
	/// While the game goes by its pointer (the hand's ray, picking under the cursor): an override is the view that is
	/// seen, so the pointer goes through it, until GiveCameraBack. A picture's camera is never shown here.
	virtual void ShowOverrideToPointer() = 0;
};

} // namespace openblack::ecs::systems
