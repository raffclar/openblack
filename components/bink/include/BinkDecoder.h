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

#include <array>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "BinkBitReader.h"
#include "BinkBlocks.h"
#include "BinkBundles.h"
#include "BinkFile.h"

/// The Bink 1 video decoder for the game's videos (revision 'i', no alpha plane): each frame's video packet decoded to
/// YUV 4:2:0 planes. It gives the same planes as the game's own Bink library on every frame of the five videos.
///
/// What makes it exact: the planes are coded Y, then V, then U, each starting on a 32-bit boundary; pixel stores wrap
/// modulo 256 and are never clamped; the inverse DCT's multiplies wrap at 32 bits and its rows round with
/// (x + 127) >> 8; a motion vector is only checked against the plane's block area, so a negative x in the first column
/// reads the end of the row above; before the first picture the picture being decoded is its own reference.
namespace openblack::bink
{

/// One decoded plane: `width` x `height` visible pixels, rows `stride` bytes apart. The buffer is padded to whole
/// 16x16 blocks of the picture
struct PlaneView
{
	std::span<const uint8_t> pixels;
	uint32_t stride {0};
	uint32_t width {0};
	uint32_t height {0};
};

/// A decoded picture: full-size luma and half-size chroma (rounded up)
struct Picture
{
	PlaneView y;
	PlaneView u;
	PlaneView v;
};

/// Decodes video packets one after the other, each over the picture before it
class Decoder
{
public:
	/// Only revision 'i' without an alpha plane is decoded. None when the header asks for anything else
	[[nodiscard]] static std::optional<Decoder> Create(const Header& header, std::string* error = nullptr);

	/// Decodes a frame's video packet over the last picture. False when the data is damaged: the last good picture
	/// stays, both to show and as the reference of the next packet
	[[nodiscard]] bool Decode(std::span<const uint8_t> packet);

	/// The last good picture; black (all zero) before any
	[[nodiscard]] Picture GetPicture() const noexcept;
	/// A packet has been decoded since it was created
	[[nodiscard]] bool HasPicture() const noexcept { return _hasPicture; }

private:
	/// A plane's size: visible pixels and 8x8 blocks
	struct PlaneSize
	{
		uint32_t width {0};
		uint32_t height {0};
		uint32_t blocksWide {0};
		uint32_t blocksHigh {0};
		size_t stride {0};
		size_t rows {0};
	};

	explicit Decoder(const Header& header);

	/// One plane; false when the data is damaged
	bool DecodePlane(BitReader& reader, size_t plane);
	/// A 16x16 block: an 8x8 block of another type, each pixel doubled. False when the data is damaged
	bool DecodeScaledBlock(BitReader& reader, const Plane& out, size_t offset);
	/// An 8x8 block of `type`. False when the data is damaged
	bool DecodeBlock(BitReader& reader, BlockType type, const Plane& out, const Plane& previous, size_t offset,
	                 const PlaneSize& size);

	std::array<PlaneSize, 3> _sizes;
	/// The last good picture, the reference of the next packet
	std::array<std::vector<uint8_t>, 3> _picture;
	/// The picture being decoded
	std::array<std::vector<uint8_t>, 3> _work;
	Bundles _bundles;
	bool _hasPicture {false};
};

/// Decodes a file's frames in order, or any frame by going back to the key frame at or before it
class FrameReader
{
public:
	/// None when the file's video can't be decoded (another revision, an alpha plane)
	[[nodiscard]] static std::optional<FrameReader> Create(std::shared_ptr<const BinkFile> file, std::string* error = nullptr);

	/// Decodes `frame`. The next frame in order decodes one packet; any other goes back to the key frame at or before
	/// it (or carries on forwards when no key frame lies in between) and decodes up to it. False when the frame failed
	/// or is past the end: the picture is then the last good one
	[[nodiscard]] bool DecodeFrame(uint32_t frame);

	/// The frame DecodeFrame decodes with one packet
	[[nodiscard]] uint32_t NextFrame() const noexcept { return _next; }
	[[nodiscard]] Picture GetPicture() const noexcept { return _decoder.GetPicture(); }
	[[nodiscard]] const BinkFile& GetFile() const noexcept { return *_file; }

private:
	FrameReader(std::shared_ptr<const BinkFile> file, Decoder decoder);

	std::shared_ptr<const BinkFile> _file;
	Decoder _decoder;
	uint32_t _next {0};
};

} // namespace openblack::bink
