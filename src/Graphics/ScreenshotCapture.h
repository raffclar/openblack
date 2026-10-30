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
#include <functional>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <glm/vec2.hpp>

#include "GraphicsHandle.h"
#include "Visual/RenderPass.h"
#include "Visual/ScreenView.h"

namespace openblack::graphics
{

/// How a picture of the screen is read
enum class ScreenshotRoute : uint8_t
{
	/// From the window's own buffers, as it is presented
	Screen,
	/// The frame's screen passes drawn into a target of the window's size and read back from it, for a window whose
	/// buffers the renderer cannot read (Vulkan drops them while the window is minimised, and OpenGL gives no pixels
	/// where other windows cover the window)
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

/// What the renderer keeps in the window's buffers, which a picture of the screen reads
enum class WindowBuffers : uint8_t
{
	/// All the pixels of the frame, at all times
	Kept,
	/// All the pixels of the frame, but no buffers while the window is minimised (Vulkan)
	DroppedWhenMinimised,
	/// Only the pixels that no other window covers; the other pixels have no defined value (OpenGL)
	UncoveredOnly,
	/// No buffer after the browser shows the frame: the browser gives the canvas a new texture for each frame (WebGPU)
	ReleasedAfterPresent,
};

/// How to read a picture of the frame being made: from the screen when the window's buffers hold the whole frame now,
/// else the renderer draws the frame aside and reads it back, if it can do that at that size
[[nodiscard]] ScreenshotPlan PlanScreenshot(WindowBuffers buffers, bool minimised, bool canReadBack, glm::u16vec2 size);

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

/// The pictures that the game asks for in memory, by the name it asks under (a name that starts with k_Prefix). The
/// renderer makes each picture from the frame when it reads the frame. Thus it does not copy the full frame, and the
/// game thread does not read it.
class ScreenPictures
{
public:
	/// The renderer keeps a picture in memory when it reads the frame for a path that starts with this
	static constexpr std::string_view k_Prefix = k_ScreenPicturePrefix;
	[[nodiscard]] static bool InMemory(std::string_view path) { return path.starts_with(k_Prefix); }

	/// Keeps the function that makes the picture for a name, until the frame for it is read
	void Ask(std::string name, ScreenPicturer picturer);
	/// Makes the picture for a name from the frame. Returns false if nobody asked for a picture under the name.
	bool Make(const std::string& name, const ScreenView& screen);
	/// The picture made for a name, one time; none while it is not made
	[[nodiscard]] std::optional<std::vector<uint8_t>> Take(const std::string& name);
	/// Removes a picture that the renderer gave up
	void Forget(const std::string& name);

private:
	std::mutex _mutex;
	std::unordered_map<std::string, ScreenPicturer> _asked;
	std::unordered_map<std::string, std::vector<uint8_t>> _made;
};

/// RGBA8 pixels made into BGRA8 pixels: the red and blue of each pixel change places
[[nodiscard]] std::vector<uint8_t> RgbaToBgra(std::span<const uint8_t> rgba);

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
	static constexpr std::array k_ScreenPasses = {RenderPass::Sky,
	                                              RenderPass::Land,
	                                              RenderPass::Main,
	                                              RenderPass::Translucent,
	                                              RenderPass::WorldLabels,
	                                              RenderPass::GestureHintCollar0,
	                                              RenderPass::GestureHintCollar1,
	                                              RenderPass::GestureHintCollar2,
	                                              RenderPass::GestureHintSeed0,
	                                              RenderPass::GestureHintSeed1,
	                                              RenderPass::GestureHintSeed2,
	                                              RenderPass::GestureHintSeed3,
	                                              RenderPass::GestureHintSeed4,
	                                              RenderPass::GestureHints,
	                                              RenderPass::CinemaBars,
	                                              RenderPass::Advisors,
	                                              RenderPass::Interface,
	                                              RenderPass::FilmCreature,
	                                              RenderPass::FilmOverlay,
	                                              RenderPass::MenuCreature,
	                                              RenderPass::MenuCreatureOverlay,
	                                              RenderPass::InterfaceCreatures,
	                                              RenderPass::ImGui,
	                                              RenderPass::Cursor};

	OffscreenScreenshots() = default;
	OffscreenScreenshots(const OffscreenScreenshots&) = delete;
	OffscreenScreenshots& operator=(const OffscreenScreenshots&) = delete;
	~OffscreenScreenshots();

	/// Draws the frame being made aside, at the window's size, for a picture written to path once read back
	void Begin(const std::filesystem::path& path, glm::u16vec2 size);
	/// Call this when bgfx has the frame with the number frame. It gives the passes back to the window. It writes each
	/// picture whose pixels are back, or makes it when the game asked for it in memory. It records each picture that
	/// it cannot write.
	void FrameEnded(uint32_t frame, ScreenshotFailures& failures, ScreenPictures& pictures);

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
