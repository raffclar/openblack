/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

/// The Bink 1 video container (.bik), as the game's five videos use it:
///
/// - a 44-byte header: "BIK" and the revision letter, the file size less 8, the frame count, the largest packet, the
///   frame count again, width, height, the frame rate as a fraction, the video flags and the number of audio tracks;
/// - 12 bytes per audio track (the largest decoded buffer, the sample rate and flags, the track id);
/// - frame count + 1 offsets: bit 0 marks a key frame, the rest is where the frame's packet starts; the last offset is
///   the file size;
/// - per frame a packet: for each audio track a 32-bit size and that many bytes, then the video data.
///
/// The game's videos are all revision 'i' with no audio track. INTRO, pre_intro and fall have a single key frame (the
/// first), so they can only be decoded from the start; logo and tips are all key frames.
///
/// A file can also stream: it has its header and its tables at once, and a thread of its own reads the packets in
/// order while the video plays.
namespace openblack::bink
{

/// A read of a file that streams. It fills `out` with the bytes at `offset`, and waits until it has them. It gives
/// false when it cannot read them.
using ReadBytes = std::function<bool(uint64_t offset, std::span<uint8_t> out)>;

/// The 44-byte header
struct Header
{
	/// The 4th byte of the signature: 'i' for every video of the game
	char revision {0};
	/// The file size less 8; not checked, as other readers ignore it too
	uint32_t fileSizeLess8 {0};
	uint32_t frameCount {0};
	/// The largest packet, in bytes
	uint32_t largestPacket {0};
	uint32_t width {0};
	uint32_t height {0};
	/// The frame rate is fpsNumerator / fpsDenominator frames a second
	uint32_t fpsNumerator {0};
	uint32_t fpsDenominator {1};
	uint32_t videoFlags {0};
};

struct AudioTrack
{
	/// The largest decoded buffer of the track
	uint32_t maxBuffer {0};
	uint16_t sampleRate {0};
	uint16_t flags {0};
	uint32_t id {0};
};

class BinkFile
{
public:
	static constexpr size_t k_HeaderSize = 44;
	static constexpr size_t k_AudioTrackSize = 12;
	static constexpr uint32_t k_KeyFrameFlag = 0x1;
	static constexpr uint32_t k_VideoFlagAlpha = 0x00100000;

	/// Reads a whole .bik held in memory and checks it: the "BIK" signature (Bink 1), a picture and a frame rate, the
	/// tables inside the file, the frame offsets strictly increasing from the end of the tables with the last one at the
	/// file size, and every packet's audio sizes inside the packet. None when it isn't valid; `error`, when given, says
	/// why
	[[nodiscard]] static std::optional<BinkFile> Parse(std::vector<uint8_t> data, std::string* error = nullptr);
	/// Reads a file from disk, then parses it
	[[nodiscard]] static std::optional<BinkFile> Open(const std::filesystem::path& path, std::string* error = nullptr);
	/// Reads the header and the tables of a file of `fileSize` bytes, and does the checks of Parse on them. A thread then
	/// reads the packets in order, `chunkSize` bytes at a time, on chunk bounds, and checks each packet when it arrives.
	/// None when the start of the file is not valid; `error`, when given, says why
	[[nodiscard]] static std::optional<BinkFile> Stream(ReadBytes read, uint64_t fileSize, uint32_t chunkSize,
	                                                    std::string* error = nullptr);

	~BinkFile();
	BinkFile(const BinkFile&) = delete;
	BinkFile& operator=(const BinkFile&) = delete;
	BinkFile(BinkFile&& other) noexcept;
	BinkFile& operator=(BinkFile&& other) noexcept;

	[[nodiscard]] const Header& GetHeader() const noexcept { return _header; }
	[[nodiscard]] uint32_t FrameCount() const noexcept { return _header.frameCount; }
	[[nodiscard]] uint32_t Width() const noexcept { return _header.width; }
	[[nodiscard]] uint32_t Height() const noexcept { return _header.height; }
	/// The whole frames a second the original plays at: the header's fraction rounded down (24 for the intro)
	[[nodiscard]] uint32_t IntegerFps() const noexcept { return _header.fpsNumerator / _header.fpsDenominator; }
	[[nodiscard]] std::span<const AudioTrack> AudioTracks() const noexcept { return _audioTracks; }

	[[nodiscard]] bool IsKeyFrame(uint32_t frame) const noexcept;
	[[nodiscard]] uint32_t KeyFrameCount() const noexcept;
	/// The last key frame at or before `frame` (frame 0 when none is marked)
	[[nodiscard]] uint32_t KeyFrameAtOrBefore(uint32_t frame) const noexcept;
	/// Where a frame's packet starts in the file; FrameOffset(FrameCount()) is the file size
	[[nodiscard]] uint32_t FrameOffset(uint32_t frame) const noexcept;
	/// A frame's whole packet (audio and video); empty past the last frame
	[[nodiscard]] std::span<const uint8_t> FramePacket(uint32_t frame) const noexcept;
	/// A frame's video data: its packet after the audio packets
	[[nodiscard]] std::span<const uint8_t> VideoPacket(uint32_t frame) const noexcept;
	/// A track's audio packet in a frame, without its size; empty when the track has nothing in that frame
	[[nodiscard]] std::span<const uint8_t> AudioPacket(uint32_t frame, uint32_t track) const noexcept;
	[[nodiscard]] size_t Size() const noexcept { return _size; }

	/// The bytes from the start of the file that the file has now. A file that does not stream has all its bytes
	[[nodiscard]] uint64_t BytesRead() const noexcept;
	/// True when the file has all its bytes, which is always so for a file that does not stream
	[[nodiscard]] bool IsComplete() const noexcept { return BytesRead() == _size; }
	/// True when a stream stopped before the end: a read failed or a packet was not valid
	[[nodiscard]] bool HasFailed() const noexcept;
	/// True when the file has the whole packet of `frame`, and for each frame past the last
	[[nodiscard]] bool HasFrame(uint32_t frame) const noexcept;
	/// Waits until the file has the whole packet of `frame`. False when the stream stopped before it
	[[nodiscard]] bool WaitForFrame(uint32_t frame) const;
	/// The mean speed of the stream since its thread started, in bytes a second. None for a file that does not stream,
	/// and before the thread read its first chunk
	[[nodiscard]] std::optional<double> ReadSpeed() const;

private:
	struct StreamState;

	BinkFile() = default;
	/// Reads the header and the tables from the start of a file of `fileSize` bytes, and checks them
	[[nodiscard]] static std::optional<BinkFile> ParseTables(std::span<const uint8_t> bytes, uint64_t fileSize,
	                                                         std::string* error);
	/// Checks each packet that ends at or before `end` and that is not checked yet. False at the first bad packet
	static bool CheckPackets(StreamState& state, uint64_t end, std::string* error);
	/// The thread of a stream: reads the chunks in order until the end, a stop or a failure
	static void Fill(StreamState& state);
	[[nodiscard]] std::span<const uint8_t> Bytes() const noexcept;
	[[nodiscard]] uint32_t VideoStart(uint32_t frame) const noexcept;

	std::vector<uint8_t> _data;
	size_t _size {0};
	/// The bytes and the thread of a file that streams. The thread keeps the state while it reads, also after the file
	/// is gone
	std::shared_ptr<StreamState> _stream;
	Header _header;
	std::vector<AudioTrack> _audioTracks;
	/// Frame count + 1 entries as in the file, bit 0 the key frame flag
	std::vector<uint32_t> _offsets;
	/// Per frame, where its video data starts in the file
	std::vector<uint32_t> _videoStart;
};

} // namespace openblack::bink
