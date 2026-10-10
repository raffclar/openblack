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
namespace openblack::bink
{

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
	static constexpr uint32_t k_VideoFlagGrey = 0x00020000;

	/// Reads a whole .bik held in memory and checks it: the "BIK" signature (Bink 1), a picture and a frame rate, the
	/// tables inside the file, the frame offsets strictly increasing from the end of the tables with the last one at the
	/// file size, and every packet's audio sizes inside the packet. None when it isn't valid; `error`, when given, says
	/// why
	[[nodiscard]] static std::optional<BinkFile> Parse(std::vector<uint8_t> data, std::string* error = nullptr);
	/// Reads a file from disk, then parses it
	[[nodiscard]] static std::optional<BinkFile> Open(const std::filesystem::path& path, std::string* error = nullptr);

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
	[[nodiscard]] size_t Size() const noexcept { return _data.size(); }

private:
	BinkFile() = default;

	std::vector<uint8_t> _data;
	Header _header;
	std::vector<AudioTrack> _audioTracks;
	/// Frame count + 1 entries as in the file, bit 0 the key frame flag
	std::vector<uint32_t> _offsets;
	/// Per frame, where its video data starts in the file
	std::vector<uint32_t> _videoStart;
};

} // namespace openblack::bink
