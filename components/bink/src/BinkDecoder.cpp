/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "BinkDecoder.h"

#include <algorithm>
#include <utility>

#include "BinkTables.h"

using namespace openblack::bink;

namespace
{
/// Revision 'i' starts each packet with a 32-bit field the picture doesn't use
constexpr size_t k_PacketHeaderBits = 32;
/// The planes in the order a packet holds them: Y, then V, then U
constexpr std::array<size_t, 3> k_PlaneOrder = {0, 2, 1};
constexpr char k_Revision = 'i';

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

/// The two colours of a pattern block, then one byte per row whose bits (lowest first) choose between them
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

std::optional<Decoder> Decoder::Create(const Header& header, std::string* error)
{
	const auto fail = [error](const char* message) {
		if (error != nullptr)
		{
			*error = message;
		}
		return std::nullopt;
	};
	if (header.width == 0 || header.height == 0)
	{
		return fail("no picture");
	}
	if (header.revision != k_Revision)
	{
		return fail("only Bink revision 'i' is decoded");
	}
	if ((header.videoFlags & BinkFile::k_VideoFlagAlpha) != 0)
	{
		return fail("videos with an alpha plane are not decoded");
	}
	return Decoder(header);
}

Decoder::Decoder(const Header& header)
    : _bundles(static_cast<size_t>((header.width + 7) >> 3) * ((header.height + 7) >> 3))
{
	for (size_t plane = 0; plane < 3; ++plane)
	{
		// Chroma planes are half the picture, counted in blocks of the luma's 16x16 grid
		const bool chroma = plane != 0;
		auto& size = _sizes[plane];
		size.width = chroma ? (header.width + 1) >> 1 : header.width;
		size.height = chroma ? (header.height + 1) >> 1 : header.height;
		size.blocksWide = chroma ? (header.width + 15) >> 4 : (header.width + 7) >> 3;
		size.blocksHigh = chroma ? (header.height + 15) >> 4 : (header.height + 7) >> 3;
		// An even number of blocks each way, so a 16x16 block on the last row or column stays inside
		size.stride = static_cast<size_t>((size.blocksWide + 1) & ~1u) * 8;
		size.rows = static_cast<size_t>((size.blocksHigh + 1) & ~1u) * 8;
		_picture[plane].assign(size.stride * size.rows, 0);
		_work[plane].assign(size.stride * size.rows, 0);
	}
}

Picture Decoder::GetPicture() const noexcept
{
	const auto view = [this](size_t plane) {
		const auto& size = _sizes[plane];
		return PlaneView {
		    .pixels = _picture[plane],
		    .stride = static_cast<uint32_t>(size.stride),
		    .width = size.width,
		    .height = size.height,
		};
	};
	return {.y = view(0), .u = view(1), .v = view(2)};
}

bool Decoder::Decode(std::span<const uint8_t> packet)
{
	if (packet.empty())
	{
		return false;
	}
	BitReader reader(packet);
	reader.Skip(k_PacketHeaderBits);
	for (size_t i = 0; i < k_PlaneOrder.size(); ++i)
	{
		const size_t plane = k_PlaneOrder[i];
		if (!DecodePlane(reader, plane))
		{
			return false;
		}
		if (reader.Position() >= reader.Size())
		{
			// A packet may end before its last planes: those keep the last picture's
			for (size_t rest = i + 1; rest < k_PlaneOrder.size(); ++rest)
			{
				_work[k_PlaneOrder[rest]] = _picture[k_PlaneOrder[rest]];
			}
			break;
		}
	}
	std::swap(_picture, _work);
	_hasPicture = true;
	return true;
}

bool Decoder::DecodePlane(BitReader& reader, size_t plane)
{
	const auto& size = _sizes[plane];
	const Plane out {_work[plane], size.stride};
	// Before the first picture there is nothing to refer to: the picture being decoded is its own reference
	const Plane previous {_hasPicture ? std::span<uint8_t>(_picture[plane]) : std::span<uint8_t>(_work[plane]), size.stride};
	if (!_bundles.StartPlane(reader, std::max(size.width, 8u), size.blocksWide))
	{
		return false;
	}
	for (uint32_t y = 0; y < size.blocksHigh; ++y)
	{
		if (!_bundles.ReadRow(reader))
		{
			return false;
		}
		for (uint32_t x = 0; x < size.blocksWide; ++x)
		{
			const size_t offset = static_cast<size_t>(y) * 8 * out.stride + static_cast<size_t>(x) * 8;
			const auto type = static_cast<BlockType>(_bundles.Next(Source::BlockTypes));
			if (type == BlockType::Scaled)
			{
				// A 16x16 block starts on an even row and column; on the others it is already decoded
				if ((y & 1) == 0 && (x & 1) == 0 && !DecodeScaledBlock(reader, out, offset))
				{
					return false;
				}
				++x;
				continue;
			}
			if (!DecodeBlock(reader, type, out, previous, offset, size))
			{
				return false;
			}
		}
	}
	reader.Align32();
	return true;
}

bool Decoder::DecodeScaledBlock(BitReader& reader, const Plane& out, size_t offset)
{
	std::array<uint8_t, 64> block {};
	const auto put = [&block](size_t pixel, uint8_t colour) { block[pixel] = colour; };
	switch (static_cast<BlockType>(_bundles.Next(Source::SubBlockTypes)))
	{
	case BlockType::Run:
		if (!DecodeRuns(reader, _bundles, put))
		{
			return false;
		}
		break;
	case BlockType::Intra:
	{
		const auto dct = DecodeDct(reader, _bundles, true);
		if (!dct)
		{
			return false;
		}
		std::ranges::transform(*dct, block.begin(), [](int32_t v) { return static_cast<uint8_t>(v); });
		break;
	}
	case BlockType::Fill:
		FillBlock(out, offset, static_cast<uint8_t>(_bundles.Next(Source::Colours)), 16);
		return true;
	case BlockType::Pattern:
		DecodePattern(_bundles, put);
		break;
	case BlockType::Raw:
		std::ranges::generate(block, [this]() { return static_cast<uint8_t>(_bundles.Next(Source::Colours)); });
		break;
	default:
		return false;
	}
	ScaleBlock(out, offset, block);
	return true;
}

bool Decoder::DecodeBlock(BitReader& reader, BlockType type, const Plane& out, const Plane& previous, size_t offset,
                          const PlaneSize& size)
{
	const auto put = [&out, offset](size_t pixel, uint8_t colour) {
		out.pixels[offset + (pixel >> 3) * out.stride + (pixel & 7)] = colour;
	};
	// A moved block: an x and y offset into the previous picture, which only has to start inside the plane's blocks
	const auto move = [&]() {
		const auto dx = static_cast<ptrdiff_t>(_bundles.Next(Source::XOffsets));
		const auto dy = static_cast<ptrdiff_t>(_bundles.Next(Source::YOffsets));
		const ptrdiff_t from = static_cast<ptrdiff_t>(offset) + dx + dy * static_cast<ptrdiff_t>(out.stride);
		const auto last =
		    static_cast<ptrdiff_t>((size.blocksWide - 1) * 8 + static_cast<size_t>(size.blocksHigh - 1) * 8 * out.stride);
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
		return DecodeRuns(reader, _bundles, put);
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
		const auto dct = DecodeDct(reader, _bundles, true);
		if (!dct)
		{
			return false;
		}
		PutBlock(out, offset, *dct);
		return true;
	}
	case BlockType::Fill:
		FillBlock(out, offset, static_cast<uint8_t>(_bundles.Next(Source::Colours)), 8);
		return true;
	case BlockType::Inter:
	{
		if (!move())
		{
			return false;
		}
		const auto dct = DecodeDct(reader, _bundles, false);
		if (!dct)
		{
			return false;
		}
		AddBlock(out, offset, *dct);
		return true;
	}
	case BlockType::Pattern:
		DecodePattern(_bundles, put);
		return true;
	case BlockType::Raw:
		for (size_t pixel = 0; pixel < 64; ++pixel)
		{
			put(pixel, static_cast<uint8_t>(_bundles.Next(Source::Colours)));
		}
		return true;
	default:
		return false;
	}
}

std::optional<FrameReader> FrameReader::Create(std::shared_ptr<const BinkFile> file, std::string* error)
{
	if (!file)
	{
		if (error != nullptr)
		{
			*error = "no file";
		}
		return std::nullopt;
	}
	auto decoder = Decoder::Create(file->GetHeader(), error);
	if (!decoder)
	{
		return std::nullopt;
	}
	return FrameReader(std::move(file), std::move(*decoder));
}

FrameReader::FrameReader(std::shared_ptr<const BinkFile> file, Decoder decoder)
    : _file(std::move(file))
    , _decoder(std::move(decoder))
{
}

bool FrameReader::DecodeFrame(uint32_t frame)
{
	if (frame >= _file->FrameCount())
	{
		return false;
	}
	if (frame != _next)
	{
		// From the key frame at or before it, unless that lies behind where decoding already is
		const uint32_t key = _file->KeyFrameAtOrBefore(frame);
		if (frame < _next || key > _next)
		{
			_next = key;
		}
		// A frame that fails on the way keeps the picture before it, as when playing in order
		for (; _next < frame; ++_next)
		{
			[[maybe_unused]] const bool decoded = _decoder.Decode(_file->VideoPacket(_next));
		}
	}
	_next = frame + 1;
	return _decoder.Decode(_file->VideoPacket(frame));
}
