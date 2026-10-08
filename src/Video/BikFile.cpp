/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "BikFile.h"

#include <algorithm>
#include <array>
#include <exception>
#include <memory>
#include <utility>

#include <fmt/format.h>

#include "FileSystem/FileSystemInterface.h"
#include "FileSystem/Stream.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::video;

namespace
{
uint32_t ReadU32(std::span<const uint8_t> data, size_t offset)
{
	return static_cast<uint32_t>(data[offset]) | static_cast<uint32_t>(data[offset + 1]) << 8 |
	       static_cast<uint32_t>(data[offset + 2]) << 16 | static_cast<uint32_t>(data[offset + 3]) << 24;
}

uint16_t ReadU16(std::span<const uint8_t> data, size_t offset)
{
	return static_cast<uint16_t>(data[offset] | data[offset + 1] << 8);
}

/// Reads `length` bytes at `offset` of the stream; false when they cannot be read
bool ReadStream(filesystem::Stream& stream, uint64_t offset, uint8_t* out, size_t length)
{
	try
	{
		stream.Seek(static_cast<size_t>(offset), filesystem::Stream::SeekMode::Begin);
		stream.Read(out, length);
	}
	catch (const std::exception&)
	{
		return false;
	}
	return true;
}
} // namespace

BikFile::BikFile() = default;
BikFile::~BikFile() = default;
BikFile::BikFile(BikFile&&) noexcept = default;
BikFile& BikFile::operator=(BikFile&&) noexcept = default;

bool BikFile::Open(const std::filesystem::path& path)
{
	Close();
	if (!Locator::filesystem::has_value())
	{
		return Fail(fmt::format("cannot open {}: no file system", path.string()));
	}
	std::unique_ptr<filesystem::Stream> stream;
	try
	{
		stream = Locator::filesystem::value().Open(path, filesystem::Stream::Mode::Read);
	}
	catch (const std::exception&)
	{
		return Fail(fmt::format("cannot open {}", path.string()));
	}
	// only the header and the tables are read now: each packet is read when it is decoded
	std::vector<uint8_t> head;
	uint64_t fileSize = 0;
	try
	{
		fileSize = stream->Size();
		head.resize(static_cast<size_t>(std::min<uint64_t>(fileSize, k_HeaderSize)));
		if (!head.empty())
		{
			stream->Read(head.data(), head.size());
		}
		if (head.size() == k_HeaderSize)
		{
			const uint64_t tracks = ReadU32(head, 40);
			const uint64_t frames = ReadU32(head, 8);
			const uint64_t tableEnd = k_HeaderSize + tracks * k_AudioTrackSize + (frames + 1) * 4;
			if (tableEnd <= fileSize)
			{
				head.resize(static_cast<size_t>(tableEnd));
				stream->Read(head.data() + k_HeaderSize, head.size() - k_HeaderSize);
			}
		}
	}
	catch (const std::exception&)
	{
		return Fail(fmt::format("cannot read {}", path.string()));
	}
	auto* const opened = stream.get();
	const auto read = [opened](uint64_t offset, uint32_t& value) {
		std::array<uint8_t, 4> bytes {};
		if (!ReadStream(*opened, offset, bytes.data(), bytes.size()))
		{
			return false;
		}
		value = ReadU32(bytes, 0);
		return true;
	};
	if (!ParseTables(head, fileSize, read))
	{
		return false;
	}
	_stream = std::move(stream);
	_size = static_cast<size_t>(fileSize);
	return true;
}

bool BikFile::Parse(std::vector<uint8_t> data)
{
	Close();
	const auto read = [&data](uint64_t offset, uint32_t& value) {
		value = ReadU32(data, static_cast<size_t>(offset));
		return true;
	};
	if (!ParseTables(data, data.size(), read))
	{
		return false;
	}
	_size = data.size();
	_data = std::move(data);
	return true;
}

bool BikFile::ParseTables(std::span<const uint8_t> head, uint64_t fileSize, const ReadU32At& read)
{
	if (fileSize < k_HeaderSize)
	{
		return Fail(fmt::format("{} bytes: shorter than the {}-byte header", fileSize, k_HeaderSize));
	}
	if (head[0] != 'B' || head[1] != 'I' || head[2] != 'K')
	{
		return Fail("not a Bink 1 file (no \"BIK\" signature)");
	}
	_revision = static_cast<char>(head[3]);
	_headerFileSize = ReadU32(head, 4);
	_frameCount = ReadU32(head, 8);
	_largestFrameSize = ReadU32(head, 12);
	_width = ReadU32(head, 20);
	_height = ReadU32(head, 24);
	_fpsNumerator = ReadU32(head, 28);
	_fpsDenominator = ReadU32(head, 32);
	_videoFlags = ReadU32(head, 36);
	const uint32_t tracks = ReadU32(head, 40);
	if (_frameCount == 0)
	{
		return Fail("no frames");
	}
	if (_width == 0 || _height == 0)
	{
		return Fail(fmt::format("no picture ({} x {})", _width, _height));
	}
	if (_fpsNumerator == 0 || _fpsDenominator == 0)
	{
		return Fail(fmt::format("no frame rate ({} / {})", _fpsNumerator, _fpsDenominator));
	}

	// the audio track table and the frame table must be inside the file (64-bit sums: the counts are untrusted)
	const uint64_t trackTable = k_HeaderSize;
	const uint64_t frameTable = trackTable + static_cast<uint64_t>(tracks) * k_AudioTrackSize;
	const uint64_t tableEnd = frameTable + (static_cast<uint64_t>(_frameCount) + 1) * 4;
	if (tableEnd > fileSize)
	{
		return Fail(fmt::format("the tables of {} tracks and {} frames end at {}, past the {} bytes of the file", tracks,
		                        _frameCount, tableEnd, fileSize));
	}
	_audioTracks.resize(tracks);
	for (uint32_t i = 0; i < tracks; ++i)
	{
		auto& track = _audioTracks[i];
		track.maxBuffer = ReadU32(head, static_cast<size_t>(trackTable) + 4 * i);
		track.sampleRate = ReadU16(head, static_cast<size_t>(trackTable) + 4 * (tracks + i));
		track.flags = ReadU16(head, static_cast<size_t>(trackTable) + 4 * (tracks + i) + 2);
		track.id = ReadU32(head, static_cast<size_t>(trackTable) + 4 * (2 * tracks + i));
	}

	// bik_frames.py: strictly increasing from the end of the table, the last one at the file size
	_offsets.resize(static_cast<size_t>(_frameCount) + 1);
	for (uint32_t i = 0; i <= _frameCount; ++i)
	{
		_offsets[i] = ReadU32(head, static_cast<size_t>(frameTable) + 4 * static_cast<size_t>(i));
	}
	if (Offset(0) < tableEnd)
	{
		return Fail(fmt::format("frame 0 at {}, inside the tables (they end at {})", Offset(0), tableEnd));
	}
	for (uint32_t i = 0; i < _frameCount; ++i)
	{
		if (Offset(i) >= Offset(i + 1))
		{
			return Fail(fmt::format("frame {} at {} is not before frame {} at {}", i, Offset(i), i + 1, Offset(i + 1)));
		}
	}
	if (Offset(_frameCount) != fileSize)
	{
		return Fail(fmt::format("the frame table ends at {}, the file has {} bytes", Offset(_frameCount), fileSize));
	}

	// each packet: per audio track a 32-bit size and the bytes, then the video
	_videoStart.resize(_frameCount);
	for (uint32_t i = 0; i < _frameCount; ++i)
	{
		uint64_t at = Offset(i);
		const uint64_t end = Offset(i + 1);
		for (uint32_t t = 0; t < tracks; ++t)
		{
			if (at + 4 > end)
			{
				return Fail(fmt::format("frame {}: no room for the size of audio track {}", i, t));
			}
			uint32_t size = 0;
			if (!read(at, size))
			{
				return Fail(fmt::format("frame {}: cannot read the size of audio track {}", i, t));
			}
			if (at + 4 + size > end)
			{
				return Fail(fmt::format("frame {}: audio track {} has {} bytes, past the packet", i, t, size));
			}
			at += 4 + static_cast<uint64_t>(size);
		}
		_videoStart[i] = static_cast<uint32_t>(at);
	}
	return true;
}

void BikFile::Close()
{
	_data.clear();
	_data.shrink_to_fit();
	_stream.reset();
	_packet.clear();
	_packet.shrink_to_fit();
	_packetFrame = UINT32_MAX;
	_size = 0;
	_offsets.clear();
	_videoStart.clear();
	_audioTracks.clear();
	_error.clear();
	_revision = 0;
	_headerFileSize = 0;
	_frameCount = 0;
	_largestFrameSize = 0;
	_width = 0;
	_height = 0;
	_fpsNumerator = 0;
	_fpsDenominator = 1;
	_videoFlags = 0;
}

bool BikFile::Fail(std::string error)
{
	Close();
	_error = std::move(error);
	return false;
}

bool BikFile::IsKeyFrame(uint32_t frame) const
{
	return frame < _frameCount && (_offsets[frame] & k_KeyFrameFlag) != 0;
}

uint32_t BikFile::KeyFrameCount() const
{
	uint32_t count = 0;
	for (uint32_t i = 0; i < _frameCount; ++i)
	{
		count += IsKeyFrame(i) ? 1 : 0;
	}
	return count;
}

std::span<const uint8_t> BikFile::Packet(uint32_t frame) const
{
	const size_t length = Offset(frame + 1) - Offset(frame);
	if (!_stream)
	{
		return std::span<const uint8_t>(_data).subspan(Offset(frame), length);
	}
	if (_packetFrame != frame)
	{
		_packet.resize(length);
		if (!ReadStream(*_stream, Offset(frame), _packet.data(), length))
		{
			_packetFrame = UINT32_MAX;
			return {};
		}
		_packetFrame = frame;
	}
	return _packet;
}

std::span<const uint8_t> BikFile::FrameData(uint32_t frame) const
{
	if (frame >= _frameCount)
	{
		return {};
	}
	return Packet(frame);
}

std::span<const uint8_t> BikFile::VideoData(uint32_t frame) const
{
	if (frame >= _frameCount)
	{
		return {};
	}
	const auto packet = Packet(frame);
	if (packet.empty())
	{
		return {};
	}
	return packet.subspan(_videoStart[frame] - Offset(frame));
}

std::span<const uint8_t> BikFile::AudioData(uint32_t frame, uint32_t track) const
{
	if (frame >= _frameCount || track >= _audioTracks.size())
	{
		return {};
	}
	const auto packet = Packet(frame);
	if (packet.empty())
	{
		return {};
	}
	size_t at = 0;
	for (uint32_t t = 0; t < track; ++t)
	{
		at += 4 + ReadU32(packet, at);
	}
	return packet.subspan(at + 4, ReadU32(packet, at));
}
