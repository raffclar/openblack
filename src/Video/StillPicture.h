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
#include <vector>

#include "Graphics/VideoOverlay.h"

namespace openblack::bink
{
class BinkFile;
class FrameReader;
} // namespace openblack::bink

namespace openblack::video
{

/// A video read through the resource cache for the screens that show its frames themselves: the logos, the pre-intro
/// and the tips. The cache lets the file go when it closes
class VideoFile
{
public:
	/// None when the file isn't there, can't be read, or the game is built without the decoder
	[[nodiscard]] static std::optional<VideoFile> Open(const std::filesystem::path& path);

	VideoFile(VideoFile&&) noexcept;
	VideoFile& operator=(VideoFile&&) noexcept;
	VideoFile(const VideoFile&) = delete;
	VideoFile& operator=(const VideoFile&) = delete;
	~VideoFile();

	/// Decodes a frame, from 0. False when it is damaged or past the end: the picture stays the last good one
	bool Decode(uint32_t frame);
	/// The picture decoded last, for the video overlay
	[[nodiscard]] graphics::VideoOverlay::Planes GetPlanes() const;
	/// The picture decoded last as blue, green, red and an unused byte a pixel, with the game's video colours
	[[nodiscard]] std::vector<uint8_t> GetBgrx() const;

	[[nodiscard]] uint32_t Width() const noexcept;
	[[nodiscard]] uint32_t Height() const noexcept;
	[[nodiscard]] uint32_t FrameCount() const noexcept;
	[[nodiscard]] uint32_t FpsNumerator() const noexcept;
	[[nodiscard]] uint32_t FpsDenominator() const noexcept;

private:
	VideoFile(std::filesystem::path path, std::shared_ptr<const bink::BinkFile> file,
	          std::unique_ptr<bink::FrameReader> reader);

	std::filesystem::path _path;
	std::shared_ptr<const bink::BinkFile> _file;
	std::unique_ptr<bink::FrameReader> _reader;
	uint32_t _serial {0};
};

/// One frame of a video kept on its own, after the file is closed
struct StillPicture
{
	std::vector<uint8_t> y;
	std::vector<uint8_t> u;
	std::vector<uint8_t> v;
	uint32_t yStride {0};
	uint32_t chromaStride {0};
	uint32_t width {0};
	uint32_t height {0};
	/// Blue, green, red and an unused byte a pixel
	std::vector<uint8_t> bgrx;

	[[nodiscard]] graphics::VideoOverlay::Planes GetPlanes() const
	{
		return {.y = y,
		        .u = u,
		        .v = v,
		        .yStride = yStride,
		        .chromaStride = chromaStride,
		        .width = width,
		        .height = height,
		        .serial = 1};
	}
};

/// A frame of a video file, none when it can't be had
[[nodiscard]] std::optional<StillPicture> ReadStill(const std::filesystem::path& path, uint32_t frame);

} // namespace openblack::video
