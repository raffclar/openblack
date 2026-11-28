/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "BinkFile.h"

#include <condition_variable>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <fstream>
#include <mutex>
#include <ranges>
#include <thread>
#include <utility>

#include <fmt/format.h>

using namespace openblack::bink;

/// The bytes, the thread and the progress of a file that streams
struct BinkFile::StreamState
{
	ReadBytes read;
	uint32_t chunkSize {0};
	uint32_t tracks {0};
	/// All the bytes of the file. The thread writes only after `filled`, and the readers read only before it
	std::vector<uint8_t> bytes;
	/// Frame count + 1 offsets, without the key frame flag
	std::vector<uint32_t> offsets;
	/// Per frame, where its video data starts. The thread writes it before `filled` covers the frame
	std::vector<uint32_t> videoStart;
	/// The number of frames that the thread checked
	uint32_t checkedFrames {0};
	std::atomic<uint64_t> filled {0};
	std::atomic<bool> failed {false};
	std::atomic<bool> stop {false};
	/// When the thread started, and the number of bytes that the file had then
	std::chrono::steady_clock::time_point started;
	uint64_t startBytes {0};
	std::mutex mutex;
	std::condition_variable changed;
};

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

/// Checks the audio sizes of the packet of `frame` from `at` to `end`, and gives where its video data starts
std::optional<uint32_t> PacketVideoStart(std::span<const uint8_t> bytes, uint64_t at, uint64_t end, uint32_t tracks,
                                         uint32_t frame, std::string* error)
{
	// Each packet: per audio track a 32-bit size and that many bytes, then the video
	for (uint32_t t = 0; t < tracks; ++t)
	{
		if (at + 4 > end)
		{
			return Fail(error, fmt::format("frame {}: no room for the size of audio track {}", frame, t));
		}
		const uint32_t size = ReadU32(bytes, static_cast<size_t>(at));
		if (at + 4 + size > end)
		{
			return Fail(error, fmt::format("frame {}: audio track {} has {} bytes, past the packet", frame, t, size));
		}
		at += 4 + static_cast<uint64_t>(size);
	}
	return static_cast<uint32_t>(at);
}

/// The end of the track and frame tables, from the header at the start of `bytes`. The counts are untrusted, so the
/// sums are 64-bit
[[nodiscard]] uint64_t TableEnd(std::span<const uint8_t> bytes) noexcept
{
	const uint64_t tracks = ReadU32(bytes, 40);
	const uint64_t frames = ReadU32(bytes, 8);
	return BinkFile::k_HeaderSize + tracks * BinkFile::k_AudioTrackSize + (frames + 1) * 4;
}
} // namespace

std::optional<BinkFile> BinkFile::ParseTables(std::span<const uint8_t> bytes, uint64_t fileSize, std::string* error)
{
	if (fileSize < k_HeaderSize || bytes.size() < k_HeaderSize)
	{
		return Fail(error, fmt::format("{} bytes: shorter than the {}-byte header", fileSize, k_HeaderSize));
	}
	if (bytes[0] != 'B' || bytes[1] != 'I' || bytes[2] != 'K')
	{
		return Fail(error, "not a Bink 1 file (no \"BIK\" signature)");
	}

	BinkFile file;
	file._size = static_cast<size_t>(fileSize);
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

	// The track and frame tables must lie inside the file
	const uint64_t trackTable = k_HeaderSize;
	const uint64_t frameTable = trackTable + static_cast<uint64_t>(tracks) * k_AudioTrackSize;
	const uint64_t tableEnd = TableEnd(bytes);
	if (tableEnd > fileSize || tableEnd > bytes.size())
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
	return file;
}

std::optional<BinkFile> BinkFile::Parse(std::vector<uint8_t> data, std::string* error)
{
	auto file = ParseTables(data, data.size(), error);
	if (!file)
	{
		return std::nullopt;
	}
	const auto tracks = static_cast<uint32_t>(file->_audioTracks.size());
	file->_videoStart.resize(file->FrameCount());
	for (uint32_t i = 0; i < file->FrameCount(); ++i)
	{
		const auto start = PacketVideoStart(data, file->FrameOffset(i), file->FrameOffset(i + 1), tracks, i, error);
		if (!start)
		{
			return std::nullopt;
		}
		file->_videoStart[i] = *start;
	}
	file->_data = std::move(data);
	return file;
}

std::optional<BinkFile> BinkFile::Stream(ReadBytes read, uint64_t fileSize, uint32_t chunkSize, std::string* error)
{
	if (!read || chunkSize == 0)
	{
		return Fail(error, "no read or no chunk size");
	}
	if (fileSize < k_HeaderSize)
	{
		return Fail(error, fmt::format("{} bytes: shorter than the {}-byte header", fileSize, k_HeaderSize));
	}
	auto state = std::make_shared<StreamState>();
	state->read = std::move(read);
	state->chunkSize = chunkSize;
	state->bytes.resize(static_cast<size_t>(fileSize));
	// Read whole chunks from the start, until they hold the header and the tables
	uint64_t filled = 0;
	const auto readTo = [&state, &filled, fileSize, chunkSize](uint64_t end) {
		while (filled < end)
		{
			const uint64_t next = std::min(fileSize, (filled / chunkSize + 1) * chunkSize);
			if (!state->read(filled, std::span<uint8_t>(state->bytes).subspan(filled, next - filled)))
			{
				return false;
			}
			filled = next;
		}
		return true;
	};
	if (!readTo(k_HeaderSize))
	{
		return Fail(error, "cannot read the header");
	}
	if (const uint64_t tableEnd = TableEnd(state->bytes); tableEnd <= fileSize && !readTo(tableEnd))
	{
		return Fail(error, "cannot read the tables");
	}
	auto file = ParseTables(std::span<const uint8_t>(state->bytes).first(filled), fileSize, error);
	if (!file)
	{
		return std::nullopt;
	}
	state->tracks = static_cast<uint32_t>(file->_audioTracks.size());
	state->offsets.resize(file->_offsets.size());
	std::ranges::transform(file->_offsets, state->offsets.begin(), [](uint32_t value) { return value & ~k_KeyFrameFlag; });
	state->videoStart.resize(file->FrameCount());
	if (!CheckPackets(*state, filled, error))
	{
		return std::nullopt;
	}
	state->filled.store(filled, std::memory_order_release);
	state->startBytes = filled;
	state->started = std::chrono::steady_clock::now();
	file->_stream = state;
	if (filled < fileSize)
	{
		// The thread keeps the state, so that the file can go while a read waits
		std::thread([state] { Fill(*state); }).detach();
	}
	else
	{
		state->read = nullptr;
	}
	return file;
}

bool BinkFile::CheckPackets(StreamState& state, uint64_t end, std::string* error)
{
	const auto frames = static_cast<uint32_t>(state.videoStart.size());
	while (state.checkedFrames < frames && state.offsets[state.checkedFrames + 1] <= end)
	{
		const uint32_t frame = state.checkedFrames;
		const auto start =
		    PacketVideoStart(state.bytes, state.offsets[frame], state.offsets[frame + 1], state.tracks, frame, error);
		if (!start)
		{
			return false;
		}
		state.videoStart[frame] = *start;
		++state.checkedFrames;
	}
	return true;
}

void BinkFile::Fill(StreamState& state)
{
	const uint64_t fileSize = state.bytes.size();
	uint64_t filled = state.filled.load(std::memory_order_relaxed);
	bool good = true;
	while (filled < fileSize && !state.stop.load(std::memory_order_relaxed))
	{
		const uint64_t next = std::min(fileSize, (filled / state.chunkSize + 1) * state.chunkSize);
		// The packets that the file now holds whole get their check before the readers can see them
		good = state.read(filled, std::span<uint8_t>(state.bytes).subspan(filled, next - filled)) &&
		       CheckPackets(state, next, nullptr);
		if (!good)
		{
			break;
		}
		filled = next;
		{
			const std::scoped_lock lock(state.mutex);
			state.filled.store(filled, std::memory_order_release);
		}
		state.changed.notify_all();
	}
	if (!good)
	{
		{
			const std::scoped_lock lock(state.mutex);
			state.failed.store(true, std::memory_order_release);
		}
		state.changed.notify_all();
	}
	// The read can keep a file open: release it when the stream ends
	state.read = nullptr;
}

BinkFile::~BinkFile()
{
	if (_stream)
	{
		_stream->stop.store(true, std::memory_order_relaxed);
	}
}

BinkFile::BinkFile(BinkFile&& other) noexcept = default;

BinkFile& BinkFile::operator=(BinkFile&& other) noexcept
{
	if (this != &other)
	{
		if (_stream)
		{
			_stream->stop.store(true, std::memory_order_relaxed);
		}
		_data = std::move(other._data);
		_size = other._size;
		_stream = std::move(other._stream);
		_header = other._header;
		_audioTracks = std::move(other._audioTracks);
		_offsets = std::move(other._offsets);
		_videoStart = std::move(other._videoStart);
	}
	return *this;
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

std::span<const uint8_t> BinkFile::Bytes() const noexcept
{
	return _stream ? std::span<const uint8_t>(_stream->bytes) : std::span<const uint8_t>(_data);
}

uint32_t BinkFile::VideoStart(uint32_t frame) const noexcept
{
	return _stream ? _stream->videoStart[frame] : _videoStart[frame];
}

uint64_t BinkFile::BytesRead() const noexcept
{
	return _stream ? _stream->filled.load(std::memory_order_acquire) : _size;
}

bool BinkFile::HasFailed() const noexcept
{
	return _stream && _stream->failed.load(std::memory_order_acquire);
}

bool BinkFile::HasFrame(uint32_t frame) const noexcept
{
	return frame >= _header.frameCount || FrameOffset(frame + 1) <= BytesRead();
}

bool BinkFile::WaitForFrame(uint32_t frame) const
{
	if (HasFrame(frame))
	{
		return true;
	}
	std::unique_lock lock(_stream->mutex);
	_stream->changed.wait(lock, [this, frame] { return HasFrame(frame) || HasFailed(); });
	return HasFrame(frame);
}

std::optional<double> BinkFile::ReadSpeed() const
{
	if (!_stream)
	{
		return std::nullopt;
	}
	const uint64_t read = BytesRead() - _stream->startBytes;
	const auto seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - _stream->started).count();
	if (read == 0 || seconds <= 0.0)
	{
		return std::nullopt;
	}
	return static_cast<double>(read) / seconds;
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
	return Bytes().subspan(FrameOffset(frame), FrameOffset(frame + 1) - FrameOffset(frame));
}

std::span<const uint8_t> BinkFile::VideoPacket(uint32_t frame) const noexcept
{
	if (frame >= _header.frameCount)
	{
		return {};
	}
	return Bytes().subspan(VideoStart(frame), FrameOffset(frame + 1) - VideoStart(frame));
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
