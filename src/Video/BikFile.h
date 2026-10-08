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
#include <span>
#include <string>
#include <vector>

/// The Bink 1 container (.bik) the original plays through binkw32.dll 1.0w (BinkOpen, BinkGetSummary).
/// See docs/bw1-notes/video.md. The original never parses the file itself: this is the public layout of the container
/// (RAD's Bink 1 documentation, FFmpeg libavformat/bink.c), checked against the five files of the game:
///
/// - a 44-byte header: "BIK" + the revision, the file size - 8, the frames, the largest frame, the frames again, width,
///   height, the frame rate as a fraction, the video flags and the audio tracks;
/// - per audio track 12 bytes (the largest decoded buffer, the sample rate and flags, the track id);
/// - frames + 1 offsets: bit 0 is the key frame flag, the rest the packet's offset; the last one is the file size;
/// - per frame a packet: for each audio track a 32-bit size and that many bytes, then the video data.
///
/// INTRO.bik, pre_intro.bik and Spells\fall\fall.bik have one key frame (frame 0): they can only be decoded from the
/// start, which is why the original never seeks them (BinkGoto only for tips.bik and logo.bik, all
/// key frames). None of the five has an audio track.
namespace openblack::filesystem
{
class Stream;
}

namespace openblack::video
{

class BikFile
{
public:
	/// The header before the audio track table (fields at offsets 0..40)
	static constexpr size_t k_HeaderSize = 44;
	/// Per audio track: the largest buffer (4), the sample rate and flags (2 + 2), the id (4)
	static constexpr size_t k_AudioTrackSize = 12;
	/// Bit 0 of a frame offset: the frame is a key frame
	static constexpr uint32_t k_KeyFrameFlag = 0x1;
	/// The video flags of the header: an alpha plane, a grey scale picture
	static constexpr uint32_t k_VideoFlagAlpha = 0x00100000;
	static constexpr uint32_t k_VideoFlagGrey = 0x00020000;
	/// The audio track flags: 16 bits, stereo, DCT (else RDFT)
	static constexpr uint16_t k_AudioFlag16Bits = 0x4000;
	static constexpr uint16_t k_AudioFlagStereo = 0x2000;
	static constexpr uint16_t k_AudioFlagDct = 0x1000;

	struct AudioTrack
	{
		uint32_t maxBuffer;  ///< the largest decoded buffer of the track
		uint16_t sampleRate; ///< Hz
		uint16_t flags;      ///< k_AudioFlag*
		uint32_t id;
	};

	BikFile();
	~BikFile();
	BikFile(BikFile&&) noexcept;
	BikFile& operator=(BikFile&&) noexcept;
	BikFile(const BikFile&) = delete;
	BikFile& operator=(const BikFile&) = delete;

	/// Open the file through the file system (Locator::filesystem), read its header and tables and check them as Parse
	/// does; the stream is kept, and each frame's packet is read from it when asked for. False (GetError says why) if it
	/// cannot be read or is not a valid Bink 1 file
	bool Open(const std::filesystem::path& path);
	/// Take a whole .bik in memory and check it: "BIK" (Bink 1; "KB2" is Bink 2), a picture and a frame rate, the
	/// tables inside the file, the frame offsets strictly increasing from the end of the table with the last one at
	/// the file size, and each packet's audio sizes inside the packet
	bool Parse(std::vector<uint8_t> data);
	void Close();

	[[nodiscard]] bool IsOpen() const { return _size != 0; }
	[[nodiscard]] const std::string& GetError() const { return _error; }

	/// The 4th byte of the signature: 'i' for every file of the game (BIKi)
	[[nodiscard]] char Revision() const { return _revision; }
	/// Header +4: the file size - 8 (not checked: FFmpeg ignores it too; it matches in the five files)
	[[nodiscard]] uint32_t HeaderFileSize() const { return _headerFileSize; }
	/// Header +8: the frames (BINKSUMMARY.Frames)
	[[nodiscard]] uint32_t FrameCount() const { return _frameCount; }
	/// Header +12: the largest packet in bytes
	[[nodiscard]] uint32_t LargestFrameSize() const { return _largestFrameSize; }
	/// Header +20 / +24
	[[nodiscard]] uint32_t Width() const { return _width; }
	[[nodiscard]] uint32_t Height() const { return _height; }
	/// Header +28 / +32: the frame rate, a fraction
	[[nodiscard]] uint32_t FpsNumerator() const { return _fpsNumerator; }
	[[nodiscard]] uint32_t FpsDenominator() const { return _fpsDenominator; }
	/// The frame rate the original plays at: FileFrameRate / FileFrameRateDiv, unsigned
	/// and truncated (24 for INTRO.bik)
	[[nodiscard]] uint32_t Fps() const { return _fpsNumerator / _fpsDenominator; }
	/// Header +36
	[[nodiscard]] uint32_t VideoFlags() const { return _videoFlags; }
	[[nodiscard]] std::span<const AudioTrack> AudioTracks() const { return _audioTracks; }

	[[nodiscard]] bool IsKeyFrame(uint32_t frame) const;
	[[nodiscard]] uint32_t KeyFrameCount() const;
	/// Where the packet of a frame starts in the file (the key frame bit cleared); FrameOffset(FrameCount()) is the
	/// file size; 0 past it
	[[nodiscard]] uint32_t FrameOffset(uint32_t frame) const
	{
		return frame <= _frameCount && !_offsets.empty() ? Offset(frame) : 0;
	}
	/// The whole packet of a frame (audio and video); empty past the last frame, or when an opened file cannot be read
	/// there. An opened file's packet is valid up to the next FrameData, VideoData or AudioData of another frame
	[[nodiscard]] std::span<const uint8_t> FrameData(uint32_t frame) const;
	/// The video data of a frame: the packet after its audio packets
	[[nodiscard]] std::span<const uint8_t> VideoData(uint32_t frame) const;
	/// The audio packet of a track in a frame (without its size); empty if the track has nothing in that frame
	[[nodiscard]] std::span<const uint8_t> AudioData(uint32_t frame, uint32_t track) const;
	/// The file size
	[[nodiscard]] size_t Size() const { return _size; }

private:
	/// Reads the 32-bit little-endian value at an offset of the file; false when it cannot be read
	using ReadU32At = std::function<bool(uint64_t offset, uint32_t& value)>;

	bool Fail(std::string error);
	/// Parse's checks on the file's first bytes (`head`: at least the header and the tables, when the file is that big)
	/// and its size; `read` gives the audio sizes inside the packets
	bool ParseTables(std::span<const uint8_t> head, uint64_t fileSize, const ReadU32At& read);
	[[nodiscard]] uint32_t Offset(uint32_t frame) const { return _offsets[frame] & ~k_KeyFrameFlag; }
	/// The packet of a frame: the bytes in memory, or read from the stream into _packet
	[[nodiscard]] std::span<const uint8_t> Packet(uint32_t frame) const;

	std::vector<uint8_t> _data;                  ///< a whole file given to Parse
	std::unique_ptr<filesystem::Stream> _stream; ///< a file opened with Open: the packets are read from it
	mutable std::vector<uint8_t> _packet;        ///< the last packet read from the stream
	mutable uint32_t _packetFrame {UINT32_MAX};  ///< its frame (UINT32_MAX: none)
	size_t _size {0};                            ///< the file size, 0 when nothing is open
	std::vector<uint32_t> _offsets;              ///< frames + 1 entries as in the file (bit 0: key frame)
	std::vector<uint32_t> _videoStart;           ///< per frame: the offset of the video data in the file
	std::vector<AudioTrack> _audioTracks;
	std::string _error;
	char _revision {0};
	uint32_t _headerFileSize {0};
	uint32_t _frameCount {0};
	uint32_t _largestFrameSize {0};
	uint32_t _width {0};
	uint32_t _height {0};
	uint32_t _fpsNumerator {0};
	uint32_t _fpsDenominator {1};
	uint32_t _videoFlags {0};
};

} // namespace openblack::video
