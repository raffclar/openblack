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
#include <filesystem>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <glm/vec2.hpp>

#include "GraphicsHandle.h"
#include "RenderPass.h"

namespace openblack::graphics
{

/// How a picture of the screen is read
enum class ScreenshotRoute : uint8_t
{
	/// From the window's own buffers, as it is presented
	Screen,
	/// The frame's screen passes drawn into a target of the window's size and read back from it, for a window with no
	/// buffers to read (Vulkan drops them while the window is minimised)
	Offscreen,
	/// Not at all, for the reason given
	Refused,
};

struct ScreenshotPlan
{
	ScreenshotRoute route;
	/// Why a refused picture can't be taken
	std::string why;
};

/// How to read a picture of the frame being made: from the screen, unless the renderer keeps no screen while the window
/// is minimised and it is, when the frame is drawn aside and read back if the renderer can do that at that size
[[nodiscard]] ScreenshotPlan PlanScreenshot(bool screenDroppedWhenMinimised, bool minimised, bool canReadBack,
                                            glm::u16vec2 size);

/// The picture's path in bgfx's note that it couldn't read the screen for it; none for any other note
[[nodiscard]] std::optional<std::string_view> UncapturedScreenshotPath(std::string_view note);

/// The pictures the renderer gave up, by path, with why. Recorded from the render thread, taken from the game's.
class ScreenshotFailures
{
public:
	void Record(std::string path, std::string why);
	/// Why the picture at a path was given up, once; none while it may still come
	[[nodiscard]] std::optional<std::string> Take(const std::string& path);

private:
	std::mutex _mutex;
	std::unordered_map<std::string, std::string> _failures;
};

/// Writes BGRA8 pixels, rows pitch bytes apart, to a PNG without their alpha. The file is written aside and renamed once
/// whole, so that whoever waits for it never reads half of it. Why not, if it couldn't.
[[nodiscard]] std::string WriteScreenshotPng(const std::filesystem::path& path, uint32_t width, uint32_t height, uint32_t pitch,
                                             std::span<const uint8_t> bgra, bool yflip);

/// Pictures of frames drawn aside: the frame's screen passes are drawn into a target of the window's size instead of
/// the window, copied into a texture the processor can read and written out a couple of frames later, when the pixels
/// are back. The window shows nothing new for that frame, which is why it is only for a window nobody sees.
class OffscreenScreenshots
{
public:
	/// The passes that draw to the window
	static constexpr std::array k_ScreenPasses = {RenderPass::Sky,      RenderPass::Main,      RenderPass::Translucent,
	                                              RenderPass::Advisors, RenderPass::Interface, RenderPass::ImGui,
	                                              RenderPass::Cursor};

	OffscreenScreenshots() = default;
	OffscreenScreenshots(const OffscreenScreenshots&) = delete;
	OffscreenScreenshots& operator=(const OffscreenScreenshots&) = delete;
	~OffscreenScreenshots();

	/// Draws the frame being made aside, at the window's size, for a picture written to path once read back
	void Begin(const std::filesystem::path& path, glm::u16vec2 size);
	/// Once bgfx has taken the frame it numbers frame: gives the window its passes back and writes the pictures whose
	/// pixels are back, recording those that couldn't be written
	void FrameEnded(uint32_t frame, ScreenshotFailures& failures);

private:
	struct Capture
	{
		std::filesystem::path path;
		glm::u16vec2 size;
		FrameBufferHandle frameBuffer;
		TextureHandle readBack;
		std::vector<uint8_t> pixels;
		/// The frame bgfx has the pixels back by
		uint32_t readyAt;
	};
	static void Release(const Capture& capture);

	std::vector<Capture> _captures;
	/// Whether the window's passes draw aside this frame
	bool _redirected {false};
};

} // namespace openblack::graphics
