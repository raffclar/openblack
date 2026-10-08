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
#include <optional>

namespace openblack::ecs::systems
{
/// The Game's frame count and its one screenshot request slot, made with the Game and kept until it goes
/// (Locator::screenshotRequest). --screenshot-frame, the text shot and the test hooks share the slot: a new request
/// replaces the pending one. A request is for the frame it was made in (or the frame given), is taken by that frame's
/// logic and is dropped at the end of that frame or of any later one. Absent without a Game (unit tests), so the test
/// hooks request nothing there
class ScreenshotRequestSystemInterface
{
public:
	virtual ~ScreenshotRequestSystemInterface() = default;

	/// Frames finished since the frame count was last reset (the start of Game::Run)
	[[nodiscard]] virtual uint32_t Frame() const noexcept = 0;
	virtual void ResetFrame() noexcept = 0;
	/// A screenshot of the current frame, to path
	virtual void Request(const std::filesystem::path& path) noexcept = 0;
	/// A screenshot of the given frame, to path (--screenshot-frame)
	virtual void RequestAt(uint32_t frame, const std::filesystem::path& path) noexcept = 0;
	/// The output path when the pending request is for the current frame
	[[nodiscard]] virtual std::optional<std::filesystem::path> Due() const = 0;
	/// The end of a frame: drops a request for this or an earlier frame, then counts the frame
	virtual void FinishFrame() noexcept = 0;
};
} // namespace openblack::ecs::systems
