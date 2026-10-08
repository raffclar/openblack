/*******************************************************************************
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
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "BinkBitReader.h"
#include "BinkBlocks.h"
#include "BinkBundles.h"
#include "BinkYuv.h"
#include "VideoDecoder.h"

/// openblack's own Bink 1 video decoder, for the game's films (all of them revision 'i', no alpha, no audio): the
/// packets of a BikFile decoded to YUV 4:2:0 planes, then turned to RGBA8 with the original library's colours
/// (BinkYuv.h). See docs/bw1-notes/video.md.
///
/// - Every frame after a key frame changes the one before, so the decoder keeps two pictures and swaps them.
/// - DecodeNext(i) in order decodes one packet. Any other `i` starts again from the last key frame <= i and decodes up
///   to `i`, unless it goes forward with no key frame in between.
/// - A frame that fails keeps the picture before it as the reference, and DecodeNext returns nothing for it.
namespace openblack::video
{

class BinkDecoder final: public IVideoDecoder
{
public:
	/// Only revision 'i' is decoded (the game's), without an alpha plane
	static constexpr char k_Revision = 'i';

	bool Open(const BikFile& file) override;
	[[nodiscard]] std::span<const uint8_t> DecodeNext(uint32_t index) override;
	size_t DecodeOnly(uint32_t index) override;
	void CopyPicture(graphics::rgb16::Format format, std::span<uint16_t> texels, std::span<uint8_t> rgba) override;

	/// Why Open or the last DecodeNext failed
	[[nodiscard]] const std::string& GetError() const { return _error; }

	/// The planes of the last picture decoded: 0 Y, 1 U, 2 V, padded to whole blocks
	[[nodiscard]] std::span<const uint8_t> PlanePixels(size_t plane) const { return _picture[plane]; }
	[[nodiscard]] size_t PlaneStride(size_t plane) const { return _strides[plane]; }

private:
	/// The last picture decoded, as bink_yuv reads it
	[[nodiscard]] bink_yuv::Planes PicturePlanes() const;
	/// Decodes the video data of frame `index` into the next picture; true and swapped when it worked
	bool DecodeFrame(uint32_t index);
	/// One plane: false when the data is damaged
	bool DecodePlane(bink::BitReader& reader, size_t plane);
	/// A 16x16 block at `offset`: an 8x8 block of another type, scaled up. False when the data is damaged
	bool DecodeScaledBlock(bink::BitReader& reader, const bink::Plane& out, size_t offset);
	/// An 8x8 block of `type` at `offset` of a plane `blocks` x `rows` blocks big, `previous` the picture before. False
	/// when the data is damaged
	bool DecodeBlock(bink::BitReader& reader, bink::BlockType type, const bink::Plane& out, const bink::Plane& previous,
	                 size_t offset, uint32_t blocks, uint32_t rows);

	const BikFile* _file {nullptr};
	uint32_t _next {0};       ///< the frame decoded next in order
	bool _hasPicture {false}; ///< a frame has been decoded since Open: the reference is the previous picture
	std::array<std::vector<uint8_t>, 3> _picture; ///< the last picture decoded, the reference of the next
	std::array<std::vector<uint8_t>, 3> _work;    ///< the picture being decoded
	std::array<size_t, 3> _strides {};
	std::optional<bink::Bundles> _bundles;
	std::vector<uint8_t> _rgba;
	std::string _error;
};

} // namespace openblack::video
