/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ScreenshotCapture.h"

#include <cstring>

#include <algorithm>
#include <system_error>
#include <utility>

#include <bgfx/bgfx.h>
#include <bimg/bimg.h>
#include <bx/file.h>
#include <spdlog/spdlog.h>

#include "GraphicsHandleBgfx.h"

namespace openblack::graphics
{

namespace
{
/// bgfx's note when it has no screen to read for a picture, before the picture's path and a full stop
constexpr std::string_view k_UncapturedNote = "Unable to capture screenshot ";
/// The first view after every pass: a copy asked for in it is made once the whole frame is drawn
constexpr auto k_CopyView = static_cast<bgfx::ViewId>(RenderPass::_count);
constexpr uint32_t k_BytesPerPixel = 4;
} // namespace

ScreenshotPlan PlanScreenshot(bool screenDroppedWhenMinimised, bool minimised, bool canReadBack, glm::u16vec2 size)
{
	if (!screenDroppedWhenMinimised || !minimised)
	{
		return {.route = ScreenshotRoute::Screen, .why = {}};
	}
	if (!canReadBack)
	{
		return {.route = ScreenshotRoute::Refused,
		        .why = "the window is minimised, which leaves this renderer nothing to read, and it can't read back a frame "
		               "drawn aside: restore the window"};
	}
	if (size.x == 0 || size.y == 0)
	{
		return {.route = ScreenshotRoute::Refused,
		        .why = "the window is minimised, which leaves this renderer nothing to read, and its size is unknown: "
		               "restore the window"};
	}
	return {.route = ScreenshotRoute::Offscreen, .why = {}};
}

std::optional<std::string_view> UncapturedScreenshotPath(std::string_view note)
{
	if (!note.starts_with(k_UncapturedNote))
	{
		return std::nullopt;
	}
	note.remove_prefix(k_UncapturedNote.size());
	if (note.ends_with('.'))
	{
		note.remove_suffix(1);
	}
	if (note.empty())
	{
		return std::nullopt;
	}
	return note;
}

void ScreenshotFailures::Record(std::string path, std::string why)
{
	const std::scoped_lock lock(_mutex);
	_failures.insert_or_assign(std::move(path), std::move(why));
}

std::optional<std::string> ScreenshotFailures::Take(const std::string& path)
{
	const std::scoped_lock lock(_mutex);
	const auto found = _failures.find(path);
	if (found == _failures.end())
	{
		return std::nullopt;
	}
	auto why = std::move(found->second);
	_failures.erase(found);
	return why;
}

std::string WriteScreenshotPng(const std::filesystem::path& path, uint32_t width, uint32_t height, uint32_t pitch,
                               std::span<const uint8_t> bgra, bool yflip)
{
	if (static_cast<uint64_t>(pitch) * height > bgra.size() || pitch < width * k_BytesPerPixel)
	{
		return "the pixels don't fill a " + std::to_string(width) + "x" + std::to_string(height) + " picture";
	}
	auto partPath = path;
	partPath += ".part";
	bx::FileWriter writer;
	bx::Error err;
	if (!bx::open(&writer, partPath.string().c_str(), false, &err))
	{
		return std::string(err.getMessage().getCPtr(), err.getMessage().getLength());
	}
	// The picture is opaque whatever the frame left in its alpha
	std::vector<uint8_t> opaque(bgra.begin(), bgra.end());
	for (uint32_t y = 0; y < height; ++y)
	{
		for (uint32_t x = 0; x < width; ++x)
		{
			opaque[static_cast<size_t>(y) * pitch + x * k_BytesPerPixel + 3] = 0xFF;
		}
	}
	bimg::imageWritePng(&writer, width, height, pitch, opaque.data(), bimg::TextureFormat::BGRA8, yflip, &err);
	bx::close(&writer);
	if (!err.isOk())
	{
		return std::string(err.getMessage().getCPtr(), err.getMessage().getLength());
	}
	std::error_code renameError;
	std::filesystem::rename(partPath, path, renameError);
	return renameError ? renameError.message() : std::string {};
}

OffscreenScreenshots::~OffscreenScreenshots()
{
	std::ranges::for_each(_captures, &OffscreenScreenshots::Release);
}

void OffscreenScreenshots::Begin(const std::filesystem::path& path, glm::u16vec2 size)
{
	const std::array textures = {
	    bgfx::createTexture2D(size.x, size.y, false, 1, bgfx::TextureFormat::BGRA8, BGFX_TEXTURE_RT),
	    bgfx::createTexture2D(size.x, size.y, false, 1, bgfx::TextureFormat::D24S8, BGFX_TEXTURE_RT_WRITE_ONLY),
	};
	const auto frameBuffer = bgfx::createFrameBuffer(static_cast<uint8_t>(textures.size()), textures.data(), true);
	const auto readBack = bgfx::createTexture2D(size.x, size.y, false, 1, bgfx::TextureFormat::BGRA8,
	                                            BGFX_TEXTURE_BLIT_DST | BGFX_TEXTURE_READ_BACK);
	bgfx::setName(frameBuffer, "Screenshot");
	bgfx::setName(readBack, "Screenshot Read Back");

	for (const auto pass : k_ScreenPasses)
	{
		bgfx::setViewFrameBuffer(static_cast<bgfx::ViewId>(pass), frameBuffer);
	}
	_redirected = true;
	bgfx::blit(k_CopyView, readBack, 0, 0, textures[0]);

	Capture capture {
	    .path = path,
	    .size = size,
	    .frameBuffer = fromBgfx(frameBuffer),
	    .readBack = fromBgfx(readBack),
	    .pixels = std::vector<uint8_t>(static_cast<size_t>(size.x) * size.y * k_BytesPerPixel),
	    .readyAt = 0,
	};
	// The vector's buffer stays where it is while the capture moves about, so bgfx may write into it later
	capture.readyAt = bgfx::readTexture(readBack, capture.pixels.data());
	_captures.push_back(std::move(capture));
}

void OffscreenScreenshots::FrameEnded(uint32_t frame, ScreenshotFailures& failures)
{
	if (_redirected)
	{
		for (const auto pass : k_ScreenPasses)
		{
			bgfx::setViewFrameBuffer(static_cast<bgfx::ViewId>(pass), BGFX_INVALID_HANDLE);
		}
		_redirected = false;
	}
	std::erase_if(_captures, [frame, &failures](const Capture& capture) {
		if (frame < capture.readyAt)
		{
			return false;
		}
		const bool yflip = bgfx::getCaps()->originBottomLeft;
		if (auto why = WriteScreenshotPng(capture.path, capture.size.x, capture.size.y, capture.size.x * k_BytesPerPixel,
		                                  capture.pixels, yflip);
		    !why.empty())
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("graphics"), "Failed to save the screenshot drawn aside at {}: {}",
			                    capture.path.string(), why);
			failures.Record(capture.path.string(), "the picture drawn aside couldn't be written: " + why);
		}
		else
		{
			SPDLOG_LOGGER_INFO(spdlog::get("graphics"), "Screenshot drawn aside ({}x{}) saved at {}", capture.size.x,
			                   capture.size.y, capture.path.string());
		}
		Release(capture);
		return true;
	});
}

void OffscreenScreenshots::Release(const Capture& capture)
{
	bgfx::destroy(toBgfx(capture.frameBuffer));
	bgfx::destroy(toBgfx(capture.readBack));
}

} // namespace openblack::graphics
