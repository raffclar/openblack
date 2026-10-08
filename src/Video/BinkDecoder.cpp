/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "BinkDecoder.h"

#include <algorithm>
#include <string>
#include <utility>

#include "BinkTables.h"
#include "BinkYuv.h"

using namespace openblack::video;
using namespace openblack::video::bink;

namespace
{
/// Revision 'i' starts each packet with a 32-bit field the picture does not use
constexpr size_t k_PacketHeaderBits = 32;
/// The planes in the order the packet holds them: Y, then V, then U
constexpr std::array<size_t, 3> k_PlaneOrder = {0, 2, 1};

/// A plane's size in 8x8 blocks: chroma planes are half the picture, rounded up to whole blocks of the luma's 16x16
struct PlaneSize
{
	uint32_t width;  ///< pixels
	uint32_t blocks; ///< 8x8 blocks across
	uint32_t rows;   ///< 8x8 blocks down
};

PlaneSize SizeOf(const BikFile& file, size_t plane) noexcept
{
	const bool chroma = plane != 0;
	const uint32_t width = file.Width() >> (chroma ? 1 : 0);
	const uint32_t round = chroma ? 15 : 7;
	const uint32_t shift = chroma ? 4 : 3;
	return {width, (file.Width() + round) >> shift, (file.Height() + round) >> shift};
}

/// Planes are kept with an even number of blocks each way, so a 16x16 block on the last row or column stays inside
size_t Stride(const PlaneSize& size) noexcept
{
	return static_cast<size_t>((size.blocks + 1) & ~1u) * 8;
}

size_t Rows(const PlaneSize& size) noexcept
{
	return static_cast<size_t>((size.rows + 1) & ~1u) * 8;
}

/// The run block: the 64 pixels in one of 16 orders, as runs of one colour or of colours one by one; `put` stores a
/// pixel (0 to 63, row * 8 + x)
template <typename Put>
bool DecodeRuns(BitReader& reader, Bundles& bundles, Put put) noexcept
{
	const auto& order = k_RunPatterns[reader.Read(4)];
	size_t done = 0;
	do
	{
		const size_t run = static_cast<size_t>(bundles.Next(Source::Runs)) + 1;
		if (done + run > 64)
		{
			return false;
		}
		if (reader.ReadBit())
		{
			const auto colour = static_cast<uint8_t>(bundles.Next(Source::Colours));
			for (size_t i = 0; i < run; ++i)
			{
				put(order[done + i], colour);
			}
		}
		else
		{
			for (size_t i = 0; i < run; ++i)
			{
				put(order[done + i], static_cast<uint8_t>(bundles.Next(Source::Colours)));
			}
		}
		done += run;
	} while (done < 63);
	if (done == 63)
	{
		put(order[63], static_cast<uint8_t>(bundles.Next(Source::Colours)));
	}
	return true;
}

/// The two colours of a pattern block, then one byte per row whose bits (lowest first) choose them
template <typename Put>
void DecodePattern(Bundles& bundles, Put put) noexcept
{
	const std::array<uint8_t, 2> colours = {static_cast<uint8_t>(bundles.Next(Source::Colours)),
	                                        static_cast<uint8_t>(bundles.Next(Source::Colours))};
	for (size_t row = 0; row < 8; ++row)
	{
		auto bits = static_cast<uint32_t>(bundles.Next(Source::Patterns));
		for (size_t x = 0; x < 8; ++x, bits >>= 1)
		{
			put(row * 8 + x, colours[bits & 1]);
		}
	}
}

/// An intra or inter DCT block: the first coefficient from its bundle, the others and the quantiser from the packet
std::optional<Coefficients> DecodeDct(BitReader& reader, Bundles& bundles, bool intra) noexcept
{
	Coefficients block {};
	block[0] = bundles.Next(intra ? Source::IntraDc : Source::InterDc);
	CodedCoefficients coded;
	const auto quantiser = ReadDctCoefficients(reader, block, coded);
	if (!quantiser)
	{
		return std::nullopt;
	}
	Dequantise(block, intra ? k_IntraQuant[*quantiser] : k_InterQuant[*quantiser], coded);
	InverseDct(block);
	return block;
}
} // namespace

bool BinkDecoder::Open(const BikFile& file)
{
	_file = nullptr;
	_bundles.reset();
	_error.clear();
	if (!file.IsOpen() || file.Width() == 0 || file.Height() == 0)
	{
		_error = "no film";
		return false;
	}
	if (file.Revision() != k_Revision)
	{
		_error = std::string("unsupported Bink revision '") + file.Revision() + "'";
		return false;
	}
	if ((file.VideoFlags() & BikFile::k_VideoFlagAlpha) != 0)
	{
		_error = "films with an alpha plane are not supported";
		return false;
	}
	for (size_t plane = 0; plane < 3; ++plane)
	{
		const auto size = SizeOf(file, plane);
		_strides[plane] = Stride(size);
		_picture[plane].assign(_strides[plane] * Rows(size), 0);
		_work[plane].assign(_strides[plane] * Rows(size), 0);
	}
	_bundles.emplace(static_cast<size_t>((file.Width() + 7) >> 3) * ((file.Height() + 7) >> 3));
	_rgba.assign(static_cast<size_t>(file.Width()) * file.Height() * 4, 0);
	_file = &file;
	_next = 0;
	_hasPicture = false;
	return true;
}

std::span<const uint8_t> BinkDecoder::DecodeNext(uint32_t index)
{
	if (DecodeOnly(index) == 0)
	{
		return {};
	}
	bink_yuv::CopyToRgba8(PicturePlanes(), _rgba);
	return _rgba;
}

void BinkDecoder::CopyPicture(graphics::rgb16::Format format, std::span<uint16_t> texels, std::span<uint8_t> rgba)
{
	if (_file == nullptr)
	{
		return;
	}
	bink_yuv::CopyToRgb16AndRgba8(PicturePlanes(), format, texels, rgba);
}

bink_yuv::Planes BinkDecoder::PicturePlanes() const
{
	return {_picture[0].data(),
	        _picture[1].data(),
	        _picture[2].data(),
	        static_cast<ptrdiff_t>(_strides[0]),
	        static_cast<ptrdiff_t>(_strides[1]),
	        static_cast<ptrdiff_t>(_strides[2]),
	        _file->Width(),
	        _file->Height()};
}

size_t BinkDecoder::DecodeOnly(uint32_t index)
{
	if (_file == nullptr || index >= _file->FrameCount())
	{
		return 0;
	}
	if (index != _next)
	{
		// From the last key frame <= index, unless it is forward with no key frame in between
		uint32_t key = index;
		while (key > 0 && !_file->IsKeyFrame(key))
		{
			--key;
		}
		if (!(index > _next && key <= _next))
		{
			_next = key;
		}
		for (; _next < index; ++_next)
		{
			DecodeFrame(_next); // a failed frame keeps the picture before it, as in order
		}
	}
	const bool decoded = DecodeFrame(index);
	_next = index + 1;
	return decoded ? _rgba.size() : 0;
}

bool BinkDecoder::DecodeFrame(uint32_t index)
{
	const auto data = _file->VideoData(index);
	if (data.empty())
	{
		_error = "empty video packet";
		return false;
	}
	BitReader reader(data);
	reader.Skip(k_PacketHeaderBits);
	for (const size_t plane : k_PlaneOrder)
	{
		if (!DecodePlane(reader, plane))
		{
			_error = "damaged video data in frame " + std::to_string(index);
			return false;
		}
		// A packet may end before its last planes
		if (reader.Position() >= reader.Size())
		{
			break;
		}
	}
	std::swap(_picture, _work);
	_hasPicture = true;
	return true;
}

bool BinkDecoder::DecodePlane(BitReader& reader, size_t plane)
{
	const auto size = SizeOf(*_file, plane);
	const Plane out {_work[plane], _strides[plane]};
	// Before the first picture there is nothing to refer to: the picture being decoded is its own reference
	const Plane previous {_hasPicture ? std::span<uint8_t>(_picture[plane]) : std::span<uint8_t>(_work[plane]),
	                      _strides[plane]};
	auto& bundles = *_bundles;
	if (!bundles.StartPlane(reader, std::max(size.width, 8u), size.blocks))
	{
		return false;
	}
	for (uint32_t y = 0; y < size.rows; ++y)
	{
		if (!bundles.ReadRow(reader))
		{
			return false;
		}
		for (uint32_t x = 0; x < size.blocks; ++x)
		{
			const size_t offset = static_cast<size_t>(y) * 8 * out.stride + static_cast<size_t>(x) * 8;
			const auto type = static_cast<BlockType>(bundles.Next(Source::BlockTypes));
			if (type == BlockType::Scaled)
			{
				// A 16x16 block starts on an even row and column; on the other three it is already decoded
				if ((y & 1) == 0 && (x & 1) == 0 && !DecodeScaledBlock(reader, out, offset))
				{
					return false;
				}
				++x;
				continue;
			}
			if (!DecodeBlock(reader, type, out, previous, offset, size.blocks, size.rows))
			{
				return false;
			}
		}
	}
	reader.Align32();
	return true;
}

bool BinkDecoder::DecodeScaledBlock(BitReader& reader, const Plane& out, size_t offset)
{
	auto& bundles = *_bundles;
	std::array<uint8_t, 64> block {};
	const auto put = [&block](size_t pixel, uint8_t colour) { block[pixel] = colour; };
	switch (static_cast<BlockType>(bundles.Next(Source::SubBlockTypes)))
	{
	case BlockType::Run:
		if (reader.BitsLeft() < 4 || !DecodeRuns(reader, bundles, put))
		{
			return false;
		}
		break;
	case BlockType::Intra:
	{
		const auto dct = DecodeDct(reader, bundles, true);
		if (!dct)
		{
			return false;
		}
		std::transform(dct->begin(), dct->end(), block.begin(), [](int32_t v) { return static_cast<uint8_t>(v); });
		break;
	}
	case BlockType::Fill:
		FillBlock(out, offset, static_cast<uint8_t>(bundles.Next(Source::Colours)), 16);
		return true;
	case BlockType::Pattern:
		DecodePattern(bundles, put);
		break;
	case BlockType::Raw:
		for (auto& pixel : block)
		{
			pixel = static_cast<uint8_t>(bundles.Next(Source::Colours));
		}
		break;
	default:
		return false;
	}
	ScaleBlock(out, offset, block);
	return true;
}

bool BinkDecoder::DecodeBlock(BitReader& reader, BlockType type, const Plane& out, const Plane& previous, size_t offset,
                              uint32_t blocks, uint32_t rows)
{
	auto& bundles = *_bundles;
	const auto put = [&out, offset](size_t pixel, uint8_t colour) {
		out.pixels[offset + (pixel >> 3) * out.stride + (pixel & 7)] = colour;
	};
	// A moved block: an x and y offset into the previous picture, which must stay inside its blocks
	const auto move = [&]() {
		const auto dx = static_cast<ptrdiff_t>(bundles.Next(Source::XOffsets));
		const auto dy = static_cast<ptrdiff_t>(bundles.Next(Source::YOffsets));
		const ptrdiff_t from = static_cast<ptrdiff_t>(offset) + dx + dy * static_cast<ptrdiff_t>(out.stride);
		const auto last = static_cast<ptrdiff_t>((blocks - 1) * 8 + static_cast<size_t>(rows - 1) * 8 * out.stride);
		if (from < 0 || from > last)
		{
			return false;
		}
		CopyBlock(out, offset, previous, static_cast<size_t>(from));
		return true;
	};
	switch (type)
	{
	case BlockType::Skip:
		CopyBlock(out, offset, previous, offset);
		return true;
	case BlockType::Motion:
		return move();
	case BlockType::Run:
		return DecodeRuns(reader, bundles, put);
	case BlockType::Residue:
	{
		if (!move())
		{
			return false;
		}
		const auto masks = static_cast<int32_t>(reader.Read(7));
		AddBlock(out, offset, ReadResidue(reader, masks));
		return true;
	}
	case BlockType::Intra:
	{
		const auto dct = DecodeDct(reader, bundles, true);
		if (!dct)
		{
			return false;
		}
		PutBlock(out, offset, *dct);
		return true;
	}
	case BlockType::Fill:
		FillBlock(out, offset, static_cast<uint8_t>(bundles.Next(Source::Colours)), 8);
		return true;
	case BlockType::Inter:
	{
		if (!move())
		{
			return false;
		}
		const auto dct = DecodeDct(reader, bundles, false);
		if (!dct)
		{
			return false;
		}
		AddBlock(out, offset, *dct);
		return true;
	}
	case BlockType::Pattern:
		DecodePattern(bundles, put);
		return true;
	case BlockType::Raw:
		for (size_t pixel = 0; pixel < 64; ++pixel)
		{
			put(pixel, static_cast<uint8_t>(bundles.Next(Source::Colours)));
		}
		return true;
	default:
		return false;
	}
}
