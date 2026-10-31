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
#include <string>
#include <vector>

#include "ScreenView.h"

namespace openblack::graphics
{

/// The requests that the world, the dedicated server and the debug tools send to the renderer: the end of a frame, the
/// pictures of the screen and the state of the renderer. The renderer of the game and the renderer that draws nothing
/// both take these requests (Locator::rendererRequests).
class RendererRequestsInterface
{
public:
	RendererRequestsInterface() = default;
	RendererRequestsInterface(const RendererRequestsInterface&) = delete;
	RendererRequestsInterface& operator=(const RendererRequestsInterface&) = delete;
	RendererRequestsInterface(RendererRequestsInterface&&) = delete;
	RendererRequestsInterface& operator=(RendererRequestsInterface&&) = delete;
	virtual ~RendererRequestsInterface() noexcept = default;

	virtual void Frame() noexcept = 0;
	/// Why a picture asked for was given up, once; none while it may still come
	[[nodiscard]] virtual std::optional<std::string> TakeScreenshotFailure(const std::filesystem::path& filepath) noexcept = 0;
	/// Asks for a picture of the frame that the game makes now, kept in memory under a name. The name must start with
	/// k_ScreenPicturePrefix. The renderer calls picturer on its own thread when it reads the frame.
	virtual void RequestScreenPicture(const std::string& name, ScreenPicturer picturer) noexcept = 0;
	/// The picture made for a name, one time; none while it is not made
	[[nodiscard]] virtual std::optional<std::vector<uint8_t>> TakeScreenPicture(const std::string& name) noexcept = 0;
	/// How many draws the last frame had. A renderer that draws nothing gives 0.
	[[nodiscard]] virtual uint32_t GetDrawsOfLastFrame() const noexcept = 0;
	[[nodiscard]] virtual bool GetDebug() const noexcept = 0;
	[[nodiscard]] virtual bool GetProfile() const noexcept = 0;
};

} // namespace openblack::graphics
