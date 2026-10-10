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
#include <cstdlib>

#include <algorithm>
#include <filesystem>
#include <initializer_list>
#include <numeric>
#include <optional>
#include <span>
#include <vector>

#include <BinkBundles.h>
#include <BinkFile.h>

#include "BinkBitWriter.h"

/// Synthetic Bink 1 files for the component's tests, and where the game's own videos are when they're installed
namespace openblack::bink::test
{

inline void PutU32(std::vector<uint8_t>& data, size_t at, uint32_t value)
{
	for (size_t i = 0; i < 4; ++i)
	{
		data[at + i] = static_cast<uint8_t>(value >> (8 * i));
	}
}

inline void PutU16(std::vector<uint8_t>& data, size_t at, uint16_t value)
{
	data[at] = static_cast<uint8_t>(value);
	data[at + 1] = static_cast<uint8_t>(value >> 8);
}

struct BikOptions
{
	uint32_t width {16};
	uint32_t height {16};
	uint32_t fpsNumerator {24};
	uint32_t fpsDenominator {1};
	uint32_t audioTracks {0};
	/// Which frames are key frames; frame 0 only when empty
	std::vector<uint32_t> keyFrames;
};

/// A Bink 1 revision 'i' file of `packets`. Packets should have even sizes, as in a real file, since bit 0 of an
/// offset is the key frame flag
inline std::vector<uint8_t> MakeBik(const std::vector<std::vector<uint8_t>>& packets, const BikOptions& options = {})
{
	const auto frames = static_cast<uint32_t>(packets.size());
	const size_t trackTable = BinkFile::k_HeaderSize;
	const size_t frameTable = trackTable + BinkFile::k_AudioTrackSize * options.audioTracks;
	std::vector<uint8_t> data(frameTable + 4 * (static_cast<size_t>(frames) + 1), 0);
	data[0] = 'B';
	data[1] = 'I';
	data[2] = 'K';
	data[3] = 'i';
	size_t largest = 0;
	for (uint32_t i = 0; i < frames; ++i)
	{
		const bool key =
		    options.keyFrames.empty() ? i == 0 : std::ranges::find(options.keyFrames, i) != options.keyFrames.end();
		PutU32(data, frameTable + 4 * i, static_cast<uint32_t>(data.size()) | (key ? 1u : 0u));
		data.insert(data.end(), packets[i].begin(), packets[i].end());
		largest = std::max(largest, packets[i].size());
	}
	PutU32(data, frameTable + 4 * static_cast<size_t>(frames), static_cast<uint32_t>(data.size()));
	PutU32(data, 4, static_cast<uint32_t>(data.size() - 8));
	PutU32(data, 8, frames);
	PutU32(data, 12, static_cast<uint32_t>(largest));
	PutU32(data, 16, frames);
	PutU32(data, 20, options.width);
	PutU32(data, 24, options.height);
	PutU32(data, 28, options.fpsNumerator);
	PutU32(data, 32, options.fpsDenominator);
	PutU32(data, 36, 0);
	PutU32(data, 40, options.audioTracks);
	for (uint32_t t = 0; t < options.audioTracks; ++t)
	{
		PutU32(data, trackTable + 4 * t, 0x1000 + t);
		PutU16(data, trackTable + 4 * (options.audioTracks + t), static_cast<uint16_t>(22050 + t));
		PutU16(data, trackTable + 4 * (options.audioTracks + t) + 2, 0x2000);
		PutU32(data, trackTable + 4 * (2 * options.audioTracks + t), 7 + t);
	}
	return data;
}

/// Writes the video packet of a 16x16 frame: luma 2x2 blocks, each chroma plane 1 block. All trees are tree 0 (plain
/// 4-bit symbols); every chunk count is 10 bits except the chroma's sub-block types (9). The packet's leading field,
/// where its chroma planes start, is filled in when the first chroma plane begins
class FrameWriter
{
public:
	FrameWriter() { _w.Put(0, 32); }

	/// Starts a plane: its trees
	FrameWriter& Plane(bool chroma)
	{
		if (chroma && !_chromaPlaced)
		{
			_w.Patch32(0, static_cast<uint32_t>(_w.Bits() / 8));
			_chromaPlaced = true;
		}
		_chroma = chroma;
		for (int i = 0; i < 7 + 16; ++i)
		{
			_w.Put(0, 4);
		}
		return *this;
	}
	/// `count` copies of `value` (block types, runs)
	FrameWriter& Repeat(uint32_t count, uint32_t value)
	{
		_w.Put(count, 10).Bit(true).Put(value, 4);
		return *this;
	}
	/// Block types coded one by one
	FrameWriter& Types(std::initializer_list<BlockType> types)
	{
		_w.Put(static_cast<uint32_t>(types.size()), 10).Bit(false);
		for (const auto type : types)
		{
			_w.Put(static_cast<uint32_t>(type), 4);
		}
		return *this;
	}
	/// `count` copies of a colour
	FrameWriter& Colour(uint32_t count, uint8_t colour)
	{
		_w.Put(count, 10).Bit(true).Put(colour >> 4, 4).Put(colour & 15, 4);
		return *this;
	}
	/// Colours one by one
	FrameWriter& Colours(std::span<const uint8_t> colours)
	{
		_w.Put(static_cast<uint32_t>(colours.size()), 10).Bit(false);
		for (const uint8_t c : colours)
		{
			_w.Put(c >> 4, 4).Put(c & 15, 4);
		}
		return *this;
	}
	/// `count` copies of a motion offset
	FrameWriter& Offset(uint32_t count, int32_t offset)
	{
		_w.Put(count, 10).Bit(true).Put(static_cast<uint32_t>(std::abs(offset)), 4);
		if (offset != 0)
		{
			_w.Bit(offset < 0);
		}
		return *this;
	}
	/// A bundle with nothing more in this plane
	FrameWriter& End() { return EndSub(false); }
	/// The sub-block types' end, 9 bits in a chroma plane
	FrameWriter& EndSub(bool sub = true)
	{
		_w.Put(0, sub && _chroma ? 9 : 10);
		return *this;
	}
	FrameWriter& Ends(int count)
	{
		for (int i = 0; i < count; ++i)
		{
			End();
		}
		return *this;
	}
	FrameWriter& EndPlane()
	{
		_w.Align32();
		return *this;
	}
	/// A whole plane of one fill colour: luma two rows of two blocks, chroma one block
	FrameWriter& FillPlane(bool chroma, uint8_t colour)
	{
		Plane(chroma);
		const uint32_t blocks = chroma ? 1 : 2;
		Repeat(blocks, static_cast<uint32_t>(BlockType::Fill)).EndSub().Colour(blocks, colour).Ends(6);
		if (!chroma)
		{
			Repeat(2, static_cast<uint32_t>(BlockType::Fill)).Colour(2, colour);
		}
		return EndPlane();
	}
	/// A whole plane of skip blocks
	FrameWriter& SkipPlane(bool chroma)
	{
		Plane(chroma);
		Repeat(chroma ? 1 : 2, static_cast<uint32_t>(BlockType::Skip)).EndSub().Ends(7);
		if (!chroma)
		{
			Repeat(2, static_cast<uint32_t>(BlockType::Skip));
		}
		return EndPlane();
	}

	[[nodiscard]] std::vector<uint8_t> Bytes() const { return _w.Bytes(); }

private:
	BinkBitWriter _w;
	bool _chroma {false};
	bool _chromaPlaced {false};
};

/// A frame of one colour: Y, then V, then U
inline std::vector<uint8_t> FillFrame(uint8_t y, uint8_t u, uint8_t v)
{
	return FrameWriter().FillPlane(false, y).FillPlane(true, v).FillPlane(true, u).Bytes();
}

inline std::vector<uint8_t> SkipFrame()
{
	return FrameWriter().SkipPlane(false).SkipPlane(true).SkipPlane(true).Bytes();
}

/// Block (0, 0) raw with the values 0..63, the other three filled with 50; grey chroma
inline std::vector<uint8_t> RawFrame()
{
	std::vector<uint8_t> colours(64);
	std::iota(colours.begin(), colours.end(), uint8_t {0});
	colours.push_back(50);
	FrameWriter f;
	f.Plane(false).Types({BlockType::Raw, BlockType::Fill}).EndSub().Colours(colours).Ends(6);
	f.Repeat(2, static_cast<uint32_t>(BlockType::Fill)).Colour(2, 50).EndPlane();
	return f.FillPlane(true, 128).FillPlane(true, 128).Bytes();
}

/// Block (1, 0) moved from `dx` pixels across in the previous frame, the rest skipped
inline std::vector<uint8_t> MotionFrame(int32_t dx)
{
	FrameWriter f;
	f.Plane(false).Types({BlockType::Skip, BlockType::Motion}).EndSub().Ends(2).Offset(1, dx).Offset(1, 0).Ends(3);
	// The offsets have been taken: they read their next chunk, empty
	f.Repeat(2, static_cast<uint32_t>(BlockType::Skip)).Ends(2).EndPlane();
	return f.SkipPlane(true).SkipPlane(true).Bytes();
}

/// The game's folder when OPENBLACK_GAME_PATH names one
inline std::optional<std::filesystem::path> GamePath()
{
	const char* path = std::getenv("OPENBLACK_GAME_PATH");
	if (path == nullptr || *path == '\0' || !std::filesystem::is_directory(path))
	{
		return std::nullopt;
	}
	return std::filesystem::path(path);
}

} // namespace openblack::bink::test
