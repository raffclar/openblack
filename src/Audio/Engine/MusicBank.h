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
#include <memory>
#include <string>
#include <vector>

#include "FileSystem/Stream.h"

// A .sad bank as the audio library registers it for the music engine (not in memory, which is what the game always
// passes). Only the headers are read; the wave data chunk stays in the file, which is kept open, and each segment
// is read when it is needed (the music thread streams them).
// In a music bank every sample is a segment "c:\windows\temp\sectNNNN.mpg" of MPEG-2 Layer II, 22050 Hz, 21 frames =
// 24192 samples (the last one shorter), contiguous and frame-aligned.

namespace openblack::audio
{

/// Samples per segment, as the marker clock counts them: 21 MPEG Layer II frames of 1152
inline constexpr int k_MusicSamplesPerSegment = 0x5E80;

/// Flags of the first segment's record (at 0x244) that a music play applies over the play options
enum class MusicBankFlag : uint32_t
{
	Volume = 0x20,       ///< the channel's volume = u16 at 0x25C (else 127)
	Loops = 0x40,        ///< the channel's loops = i32 at 0x248 (else the requested ones); -1 = forever
	MinDistance = 0x80,  ///< distance mapping min = f32 at 0x268 (else the requested one)
	MaxDistance = 0x100, ///< distance mapping max = f32 at 0x26C (else the requested one)
	Scale = 0x200,       ///< distance mapping scale = f32 at 0x270 (else the requested one)
};

/// One sample of the bank's sample table: its bytes inside the wave data chunk
struct MusicSegment
{
	uint32_t offset; ///< from the start of the wave data chunk
	uint32_t size;
};

/// A marker parsed from the "!<sample>=<label>!..." texts of the segment descriptions
struct MusicMarker
{
	int chunk;  ///< segment index + 1 (1-based, like the channel's chunk counters)
	int sample; ///< atoi of the digits, sample inside that chunk
	std::string label;
};

/// Distance mapping of a 3D music channel, as passed to QMixer
struct MusicDistanceMapping
{
	float minDistance;
	float maxDistance;
	float scale;
};

class MusicBank
{
public:
	/// Size of one sample table record
	static constexpr size_t k_RecordSize = 0x280;
	using Record = std::array<uint8_t, k_RecordSize>;

	/// nullptr when the bank cannot be used (file missing, no LiOnHeAd header, or no bank info, wave data or
	/// sample table chunk). The early returns of the system itself (not created, no path) belong to the caller.
	[[nodiscard]] static std::unique_ptr<MusicBank> Register(const std::filesystem::path& path);

	MusicBank(const MusicBank&) = delete;
	MusicBank& operator=(const MusicBank&) = delete;
	~MusicBank();

	[[nodiscard]] const std::filesystem::path& GetPath() const { return _path; }

	/// The 3rd u32 of the bank info chunk; non-zero = music
	[[nodiscard]] bool IsMusic() const { return _musicFlag != 0; }
	[[nodiscard]] uint32_t GetMusicFlag() const { return _musicFlag; }

	/// The low u16 of the table's first u32 (the high one counts the atmos samples and is 0 in music banks)
	[[nodiscard]] size_t GetSegmentCount() const { return _segments.size(); }
	[[nodiscard]] const std::vector<MusicSegment>& GetSegments() const { return _segments; }
	[[nodiscard]] const Record& GetRecord(size_t index) const { return _records[index]; }
	/// Bytes of the wave data chunk (the segments must fit in it)
	[[nodiscard]] uint32_t GetWaveDataSize() const { return _waveDataSize; }

	/// u16 at 0x118 of the first segment, -1 if not a music bank
	[[nodiscard]] int GetGroupId() const;
	/// f32 at 0x268 of the first segment, -1 if not a music bank
	[[nodiscard]] float GetMinDistance() const;
	/// f32 at 0x26C of the first segment, -1 if not a music bank
	[[nodiscard]] float GetMaxDistance() const;

	/// u32 at 0x244 of the first segment, see MusicBankFlag
	[[nodiscard]] uint32_t GetFlags() const;
	[[nodiscard]] bool HasFlag(MusicBankFlag flag) const { return (GetFlags() & static_cast<uint32_t>(flag)) != 0; }
	/// u32 at 0x128 of the first segment: a music play gives it to the channel, the Hz of every chunk
	[[nodiscard]] uint32_t GetSampleRate() const;
	/// The volume of a music play: 127, or u32 at 0x25C & 0xFFFF with flag 0x20
	[[nodiscard]] int GetVolume() const;
	/// The loops of a music play: the requested loops, or i32 at 0x248 with flag 0x40
	[[nodiscard]] int GetLoops(int requestedLoops) const;
	/// Distance mapping of a music play for a 3D channel: the requested one with 0x26C if 0x100, 0x268 if 0x80 and
	/// 0x270 if 0x200
	[[nodiscard]] MusicDistanceMapping GetDistanceMapping(const MusicDistanceMapping& requested) const;

	/// The markers (a music play reads them when a marker callback is set), in the order of the original's
	/// linked list. Segments are walked from the last to the first and each node is pushed at the head, so the list
	/// goes by segment and, inside a segment, in the reverse order of the text.
	[[nodiscard]] std::vector<MusicMarker> ParseMarkers() const;

	/// Read the bytes of one segment from the open file: the wave data chunk + offset, size bytes (the music thread). Not
	/// thread-safe: the caller serialises the reads, like the original's single music thread.
	[[nodiscard]] bool ReadSegment(size_t index, std::vector<uint8_t>& out);

private:
	MusicBank() = default;

	[[nodiscard]] uint32_t FirstRecordUint(size_t offset) const;
	[[nodiscard]] float FirstRecordFloat(size_t offset) const;

	std::filesystem::path _path;
	std::unique_ptr<filesystem::Stream> _file; ///< kept open: the bank is not loaded in memory
	uint32_t _musicFlag {0};
	uint64_t _waveDataOffset {0}; ///< file offset of the wave data chunk's data
	uint32_t _waveDataSize {0};
	std::vector<Record> _records;
	std::vector<MusicSegment> _segments;
};

} // namespace openblack::audio
