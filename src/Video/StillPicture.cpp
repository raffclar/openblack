/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "StillPicture.h"

#include <utility>

#include <BinkDecoder.h>
#include <BinkFile.h>
#include <BinkYuv.h>
#include <entt/core/hashed_string.hpp>
#include <spdlog/spdlog.h>

#include "FileSystem/FileSystemInterface.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::video;

namespace
{
entt::id_type VideoId(const std::filesystem::path& path)
{
	return entt::hashed_string(path.generic_string().c_str()).value();
}
} // namespace

VideoFile::VideoFile(std::filesystem::path path, std::shared_ptr<const bink::BinkFile> file,
                     std::unique_ptr<bink::FrameReader> reader)
    : _path(std::move(path))
    , _file(std::move(file))
    , _reader(std::move(reader))
{
}

VideoFile::VideoFile(VideoFile&&) noexcept = default;
VideoFile& VideoFile::operator=(VideoFile&&) noexcept = default;

VideoFile::~VideoFile()
{
	if (_file && Locator::resources::has_value())
	{
		// The full-screen player may hold the same file: the cache only lets its own reference go
		_file.reset();
		Locator::resources::value().GetVideos().Erase(VideoId(_path));
	}
}

std::optional<VideoFile> VideoFile::Open(const std::filesystem::path& path)
{
#if defined(OPENBLACK_BINK_DECODER)
	if (!Locator::filesystem::value().Exists(path))
	{
		SPDLOG_LOGGER_WARN(spdlog::get("game"), "The video {} isn't there", path.generic_string());
		return std::nullopt;
	}
	try
	{
		const auto [it, loaded] =
		    Locator::resources::value().GetVideos().Load(VideoId(path), resources::VideoLoader::FromDiskTag {}, path);
		if (!it->second)
		{
			return std::nullopt;
		}
		std::shared_ptr<const bink::BinkFile> file = it->second.handle();
		std::string error;
		auto reader = bink::FrameReader::Create(file, &error);
		if (!reader)
		{
			SPDLOG_LOGGER_WARN(spdlog::get("game"), "The video {} can't be decoded: {}", path.generic_string(), error);
			return std::nullopt;
		}
		return VideoFile(path, std::move(file), std::make_unique<bink::FrameReader>(std::move(*reader)));
	}
	catch (const std::exception& error)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "Can't read the video {}: {}", path.generic_string(), error.what());
		return std::nullopt;
	}
#else
	static_cast<void>(path);
	return std::nullopt;
#endif
}

bool VideoFile::Decode(uint32_t frame)
{
	if (frame >= _file->FrameCount())
	{
		return false;
	}
	const bool decoded = _reader->DecodeFrame(frame);
	++_serial;
	return decoded;
}

graphics::VideoOverlay::Planes VideoFile::GetPlanes() const
{
	const auto picture = _reader->GetPicture();
	return {
	    .y = picture.y.pixels,
	    .u = picture.u.pixels,
	    .v = picture.v.pixels,
	    .yStride = picture.y.stride,
	    .chromaStride = picture.u.stride,
	    .width = _file->Width(),
	    .height = _file->Height(),
	    .serial = _serial,
	};
}

std::vector<uint8_t> VideoFile::GetBgrx() const
{
	std::vector<uint8_t> bgrx(static_cast<size_t>(Width()) * Height() * 4);
	bink::ConvertPicture(_reader->GetPicture(), bink::PixelLayout::Bgrx8, bgrx);
	return bgrx;
}

uint32_t VideoFile::Width() const noexcept
{
	return _file->Width();
}

uint32_t VideoFile::Height() const noexcept
{
	return _file->Height();
}

uint32_t VideoFile::FrameCount() const noexcept
{
	return _file->FrameCount();
}

uint32_t VideoFile::FpsNumerator() const noexcept
{
	return _file->GetHeader().fpsNumerator;
}

uint32_t VideoFile::FpsDenominator() const noexcept
{
	return _file->GetHeader().fpsDenominator;
}

std::optional<StillPicture> openblack::video::ReadStill(const std::filesystem::path& path, uint32_t frame)
{
	auto file = VideoFile::Open(path);
	if (!file || !file->Decode(frame))
	{
		return std::nullopt;
	}
	const auto planes = file->GetPlanes();
	return StillPicture {
	    .y = {planes.y.begin(), planes.y.end()},
	    .u = {planes.u.begin(), planes.u.end()},
	    .v = {planes.v.begin(), planes.v.end()},
	    .yStride = planes.yStride,
	    .chromaStride = planes.chromaStride,
	    .width = planes.width,
	    .height = planes.height,
	    .bgrx = file->GetBgrx(),
	};
}
