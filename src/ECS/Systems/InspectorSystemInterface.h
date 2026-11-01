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

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "Debug/SimRun.h"

namespace openblack::fight_autopilot
{
class AutopilotInterface;
} // namespace openblack::fight_autopilot

namespace openblack::inspector
{
class ProviderInterface;
} // namespace openblack::inspector

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
	/// The fight autopilot, which fights for the player with the inspector's input
	[[nodiscard]] virtual fight_autopilot::AutopilotInterface* GetFightAutopilot() { return nullptr; }
	/// Each land or scenario loaded from now on starts paused, before any of its frames, for a tool that steps it
	virtual void PauseOnLoad([[maybe_unused]] bool pause) {}
	/// Tools: the next land or scenario that starts makes this recorded input from its first frame on. The file is a list of
	/// {frame, event}, as input.record answers it, or an object with that list as "events" and a "camera" that camera.set
	/// takes, which puts the camera there before the first frame. An error, or none.
	[[nodiscard]] virtual std::optional<std::string> ReplayOnLoad([[maybe_unused]] const std::filesystem::path& path)
	{
		return "this inspector makes no input";
	}
	/// Tools: the next land or scenario that starts records the input made from its first frame on, with the frames as
	/// ReplayOnLoad reads them. The recording goes to the file as the inspector stops.
	virtual void RecordInputOnLoad([[maybe_unused]] const std::filesystem::path& path) {}
	/// Starts the run record of a simulation run in a file, with a record every so many turns, and with the hash of each
	/// component when asked. The record reads every field of the creature on the turns that fieldsEvery divides. An
	/// error, or none.
	[[nodiscard]] virtual std::optional<std::string>
	StartSimRecord([[maybe_unused]] const std::filesystem::path& path, [[maybe_unused]] uint32_t every,
	               [[maybe_unused]] const std::optional<std::filesystem::path>& limits,
	               [[maybe_unused]] sim_run::HashDetail hashParts, [[maybe_unused]] uint32_t fieldsEvery)
	{
		return "this inspector writes no run record";
	}
	/// Adds the queries of a provider, which replaces a provider of the same name. The game server adds its own queries.
	virtual void AddProvider(std::unique_ptr<inspector::ProviderInterface> provider) = 0;
	/// Removes the provider of this name, before the state that its queries read stops
	virtual void RemoveProvider(std::string_view name) = 0;
	/// The game server hosts a network game. Then the inspector refuses the queries that change the shared world, as it
	/// does in the game of a player in a network game. Each client must play the same world as the server.
	virtual void SetHostsNetworkGame(bool hosts) = 0;
};

} // namespace openblack::ecs::systems
