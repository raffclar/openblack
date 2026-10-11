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
#include <string>
#include <string_view>

#include <glm/vec3.hpp>

namespace openblack::ecs::systems
{

/// The tutorial's hand demonstrations: while the advisors teach a move, a recording of the player's interface plays
/// through the player's own hand and camera, the player's mouse kept out. The script that starts one keeps it in step
/// with the advisors' lines through the marks in the recording, at which it can hold until the script reads the mark.
class HandDemoSystemInterface
{
public:
	/// Where the demonstration has the camera
	struct CameraPose
	{
		glm::vec3 origin {0.0f};
		glm::vec3 focus {0.0f};
	};
	/// For the debug inspector
	struct Status
	{
		std::string name;
		uint32_t task {0};
		size_t record {0};
		size_t records {0};
		bool pauseOnTrigger {false};
		bool triggerReached {false};
		bool holding {false};
	};

	virtual ~HandDemoSystemInterface() = default;

	/// A script task starts the demonstration of a name (a file of Data/HandDemo). Unless `withoutHandModify`, the hand
	/// first lets go of what it holds. A demonstration that can't be read doesn't play.
	virtual void Play(std::string_view name, uint32_t task, bool pauseOnTrigger, bool withoutHandModify) = 0;
	/// Whether a demonstration plays: any, or only the one a task started (task 0 for any)
	[[nodiscard]] virtual bool IsPlaying(uint32_t task) const = 0;
	/// Whether the demonstration reached a mark since the last asked, which also lets a held one go on
	virtual bool TakeTrigger() = 0;
	/// Once a frame, before the hand and the camera move: the records the game's time has reached play
	virtual void Update() = 0;
	/// Ends the demonstration where it is
	virtual void Stop() = 0;
	/// A script task stopped: a demonstration it started ends with it
	virtual void TaskStopped(uint32_t task) = 0;
	/// The scripts start again: no demonstration plays
	virtual void Reset() = 0;

	/// Where the demonstration has the camera, none while none plays
	[[nodiscard]] virtual std::optional<CameraPose> GetCamera() const = 0;
	/// The camera hints the hand shows, as the demonstration recorded them
	[[nodiscard]] virtual uint32_t GetHints() const = 0;
	/// The angle the rotate hint is turned by, as the demonstration recorded it, in radians
	[[nodiscard]] virtual float GetHintAngle() const = 0;
	/// The mouse buttons the demonstration holds down: the one that moves the hand and the action button
	struct HeldButtons
	{
		bool move {false};
		bool action {false};
	};
	[[nodiscard]] virtual HeldButtons GetHeldButtons() const = 0;
	[[nodiscard]] virtual std::optional<Status> GetStatus() const = 0;
};

} // namespace openblack::ecs::systems
