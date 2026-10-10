/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "BinkFile.h"

#include <algorithm>
#include <fstream>
#include <ranges>
#include <utility>

#include <fmt/format.h>

using namespace openblack::bink;

namespace
{
[[nodiscard]] uint32_t ReadU32(std::span<const uint8_t> data, size_t offset) noexcept
{
	return static_cast<uint32_t>(data[offset]) | static_cast<uint32_t>(data[offset + 1]) << 8 |
	       static_cast<uint32_t>(data[offset + 2]) << 16 | static_cast<uint32_t>(data[offset + 3]) << 24;
}

[[nodiscard]] uint16_t ReadU16(std::span<const uint8_t> data, size_t offset) noexcept
{
	return static_cast<uint16_t>(data[offset] | data[offset + 1] << 8);
}

std::nullopt_t Fail(std::string* error, std::string message)
{
	if (error != nullptr)
	{
		*error = std::move(message);
	}
	return std::nullopt;
}
} // namespace

std::optional<BinkFile> BinkFile::Parse(std::vector<uint8_t> data, std::string* error)
{
	const std::span<const uint8_t> bytes = data;
	const uint64_t fileSize = bytes.size();
	if (fileSize < k_HeaderSize)
	{
		return Fail(error, fmt::format("{} bytes: shorter than the {}-byte header", fileSize, k_HeaderSize));
	}
	if (bytes[0] != 'B' || bytes[1] != 'I' || bytes[2] != 'K')
	{
		return Fail(error, "not a Bink 1 file (no \"BIK\" signature)");
	}

	BinkFile file;
	auto& header = file._header;
	header = {
	    .revision = static_cast<char>(bytes[3]),
	    .fileSizeLess8 = ReadU32(bytes, 4),
	    .frameCount = ReadU32(bytes, 8),
	    .largestPacket = ReadU32(bytes, 12),
	    .width = ReadU32(bytes, 20),
	    .height = ReadU32(bytes, 24),
	    .fpsNumerator = ReadU32(bytes, 28),
	    .fpsDenominator = ReadU32(bytes, 32),
	    .videoFlags = ReadU32(bytes, 36),
	};
	const uint32_t tracks = ReadU32(bytes, 40);
	if (header.frameCount == 0)
	{
		return Fail(error, "no frames");
	}
	if (header.width == 0 || header.height == 0)
	{
		return Fail(error, fmt::format("no picture ({} x {})", header.width, header.height));
	}
	if (header.fpsNumerator == 0 || header.fpsDenominator == 0)
	{
		return Fail(error, fmt::format("no frame rate ({} / {})", header.fpsNumerator, header.fpsDenominator));
	}

	// The track and frame tables must lie inside the file; the counts are untrusted, so the sums are 64-bit
	const uint64_t trackTable = k_HeaderSize;
	const uint64_t frameTable = trackTable + static_cast<uint64_t>(tracks) * k_AudioTrackSize;
	const uint64_t tableEnd = frameTable + (static_cast<uint64_t>(header.frameCount) + 1) * 4;
	if (tableEnd > fileSize)
	{
		return Fail(error, fmt::format("the tables of {} tracks and {} frames end at {}, past the {} bytes of the file", tracks,
		                               header.frameCount, tableEnd, fileSize));
	}
	file._audioTracks.resize(tracks);
	for (uint32_t i = 0; i < tracks; ++i)
	{
		// The table holds all the buffer sizes, then all the rates and flags, then all the ids
		const auto at = static_cast<size_t>(trackTable);
		const auto count = static_cast<size_t>(tracks);
		file._audioTracks[i] = {
		    .maxBuffer = ReadU32(bytes, at + 4 * i),
		    .sampleRate = ReadU16(bytes, at + 4 * (count + i)),
		    .flags = ReadU16(bytes, at + 4 * (count + i) + 2),
		    .id = ReadU32(bytes, at + 4 * (2 * count + i)),
		};
	}

	file._offsets.resize(static_cast<size_t>(header.frameCount) + 1);
	for (size_t i = 0; i < file._offsets.size(); ++i)
	{
		file._offsets[i] = ReadU32(bytes, static_cast<size_t>(frameTable) + 4 * i);
	}
	const auto offset = [&file](size_t frame) { return file._offsets[frame] & ~k_KeyFrameFlag; };
	if (offset(0) < tableEnd)
	{
		return Fail(error, fmt::format("frame 0 at {}, inside the tables (they end at {})", offset(0), tableEnd));
	}
	for (uint32_t i = 0; i < header.frameCount; ++i)
	{
		if (offset(i) >= offset(i + 1))
		{
			return Fail(error, fmt::format("frame {} at {} is not before frame {} at {}", i, offset(i), i + 1, offset(i + 1)));
		}
	}
	if (offset(header.frameCount) != fileSize)
	{
		return Fail(error,
		            fmt::format("the frame table ends at {}, the file has {} bytes", offset(header.frameCount), fileSize));
	}

	// Each packet: per audio track a 32-bit size and that many bytes, then the video
	file._videoStart.resize(header.frameCount);
	for (uint32_t i = 0; i < header.frameCount; ++i)
	{
		uint64_t at = offset(i);
		const uint64_t end = offset(i + 1);
		for (uint32_t t = 0; t < tracks; ++t)
		{
			if (at + 4 > end)
			{
				return Fail(error, fmt::format("frame {}: no room for the size of audio track {}", i, t));
			}
			const uint32_t size = ReadU32(bytes, static_cast<size_t>(at));
			if (at + 4 + size > end)
			{
				return Fail(error, fmt::format("frame {}: audio track {} has {} bytes, past the packet", i, t, size));
			}
			at += 4 + static_cast<uint64_t>(size);
		}
		file._videoStart[i] = static_cast<uint32_t>(at);
	}
	file._data = std::move(data);
	return file;
}

std::optional<BinkFile> BinkFile::Open(const std::filesystem::path& path, std::string* error)
{
	std::ifstream stream(path, std::ios::binary);
	if (!stream)
	{
		return Fail(error, fmt::format("cannot open {}", path.string()));
	}
	std::error_code code;
	const auto size = std::filesystem::file_size(path, code);
	if (code)
	{
		return Fail(error, fmt::format("cannot read the size of {}", path.string()));
	}
	std::vector<uint8_t> data(static_cast<size_t>(size));
	if (!stream.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(data.size())))
	{
		return Fail(error, fmt::format("cannot read {}", path.string()));
	}
	return Parse(std::move(data), error);
}

bool BinkFile::IsKeyFrame(uint32_t frame) const noexcept
{
	return frame < _header.frameCount && (_offsets[frame] & k_KeyFrameFlag) != 0;
}

uint32_t BinkFile::KeyFrameCount() const noexcept
{
	return static_cast<uint32_t>(std::ranges::count_if(std::views::iota(uint32_t {0}, _header.frameCount),
	                                                   [this](uint32_t frame) { return IsKeyFrame(frame); }));
}

uint32_t BinkFile::KeyFrameAtOrBefore(uint32_t frame) const noexcept
{
	uint32_t key = std::min(frame, _header.frameCount - 1);
	while (key > 0 && !IsKeyFrame(key))
	{
		--key;
	}
	return key;
}

uint32_t BinkFile::FrameOffset(uint32_t frame) const noexcept
{
	return frame <= _header.frameCount ? _offsets[frame] & ~k_KeyFrameFlag : 0;
}

std::span<const uint8_t> BinkFile::FramePacket(uint32_t frame) const noexcept
{
	if (frame >= _header.frameCount)
	{
		return {};
	}
	return std::span<const uint8_t>(_data).subspan(FrameOffset(frame), FrameOffset(frame + 1) - FrameOffset(frame));
}

std::span<const uint8_t> BinkFile::VideoPacket(uint32_t frame) const noexcept
{
	if (frame >= _header.frameCount)
	{
		return {};
	}
	return std::span<const uint8_t>(_data).subspan(_videoStart[frame], FrameOffset(frame + 1) - _videoStart[frame]);
}

std::span<const uint8_t> BinkFile::AudioPacket(uint32_t frame, uint32_t track) const noexcept
{
	if (frame >= _header.frameCount || track >= _audioTracks.size())
	{
		return {};
	}
	const auto packet = FramePacket(frame);
	size_t at = 0;
	for (uint32_t t = 0; t < track; ++t)
	{
		at += 4 + ReadU32(packet, at);
	}
	return packet.subspan(at + 4, ReadU32(packet, at));
}
