/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The header rules, the conversions to 16 bits and the ADPCM decoders follow dr_wav 0.14 (public domain or MIT-0, by
// David Reid), which openblack used before, so that every wave decodes to the same samples.

#include "WaveFile.h"

#include <cstring>

#include <algorithm>
#include <array>
#include <limits>

using namespace openblack::audio::codec;

namespace
{
constexpr uint32_t k_MaxSampleRate = 384000;
constexpr uint32_t k_MaxChannels = 256;
constexpr uint32_t k_MaxBitsPerSample = 64;
/// Longer than this is taken as a broken header rather than allocated
constexpr uint64_t k_MaxSamples = uint64_t {1} << 28;
/// The uncompressed formats are converted in pieces of this many bytes: a frame larger than it gives silence
constexpr size_t k_ConversionBytes = 4096;

constexpr std::array<uint8_t, 16> k_GuidW64Riff = {0x72, 0x69, 0x66, 0x66, 0x2E, 0x91, 0xCF, 0x11,
                                                   0xA5, 0xD6, 0x28, 0xDB, 0x04, 0xC1, 0x00, 0x00};
constexpr std::array<uint8_t, 16> k_GuidW64Wave = {0x77, 0x61, 0x76, 0x65, 0xF3, 0xAC, 0xD3, 0x11,
                                                   0x8C, 0xD1, 0x00, 0xC0, 0x4F, 0x8E, 0xDB, 0x8A};
constexpr std::array<uint8_t, 16> k_GuidW64Fmt = {0x66, 0x6D, 0x74, 0x20, 0xF3, 0xAC, 0xD3, 0x11,
                                                  0x8C, 0xD1, 0x00, 0xC0, 0x4F, 0x8E, 0xDB, 0x8A};
constexpr std::array<uint8_t, 16> k_GuidW64Fact = {0x66, 0x61, 0x63, 0x74, 0xF3, 0xAC, 0xD3, 0x11,
                                                   0x8C, 0xD1, 0x00, 0xC0, 0x4F, 0x8E, 0xDB, 0x8A};
constexpr std::array<uint8_t, 16> k_GuidW64Data = {0x64, 0x61, 0x74, 0x61, 0xF3, 0xAC, 0xD3, 0x11,
                                                   0x8C, 0xD1, 0x00, 0xC0, 0x4F, 0x8E, 0xDB, 0x8A};

/// A cursor over the file's bytes: reads stop at the end, seeks outside it fail and do not move
class Stream
{
public:
	explicit Stream(std::span<const uint8_t> data) noexcept
	    : _data(data)
	{
	}

	size_t Read(void* out, size_t count) noexcept
	{
		count = std::min(count, _data.size() - _position);
		if (count > 0)
		{
			std::memcpy(out, _data.data() + _position, count);
		}
		_position += count;
		return count;
	}
	template <size_t N>
	bool ReadExactly(std::array<uint8_t, N>& out) noexcept
	{
		return Read(out.data(), N) == N;
	}
	bool Seek(int64_t offset) noexcept { return SeekTo(static_cast<int64_t>(_position) + offset); }
	bool SeekTo(int64_t position) noexcept
	{
		if (position < 0 || static_cast<uint64_t>(position) > _data.size())
		{
			return false;
		}
		_position = static_cast<size_t>(position);
		return true;
	}
	/// Seeks forward by any amount, in steps the way the reference does
	bool Forward(uint64_t offset) noexcept
	{
		while (offset > 0)
		{
			const uint64_t step = std::min<uint64_t>(offset, 0x7FFFFFFF);
			if (!Seek(static_cast<int64_t>(step)))
			{
				return false;
			}
			offset -= step;
		}
		return true;
	}
	[[nodiscard]] size_t Position() const noexcept { return _position; }

private:
	std::span<const uint8_t> _data;
	size_t _position {0};
};

bool IsBigEndian(WaveContainer container) noexcept
{
	return container == WaveContainer::Rifx || container == WaveContainer::Aiff;
}

uint16_t U16(const uint8_t* p, bool bigEndian) noexcept
{
	return bigEndian ? static_cast<uint16_t>(p[0] << 8 | p[1]) : static_cast<uint16_t>(p[1] << 8 | p[0]);
}

uint32_t U32(const uint8_t* p, bool bigEndian) noexcept
{
	return bigEndian ? static_cast<uint32_t>(U16(p, true)) << 16 | U16(p + 2, true)
	                 : static_cast<uint32_t>(U16(p + 2, false)) << 16 | U16(p, false);
}

uint64_t U64(const uint8_t* p) noexcept
{
	return static_cast<uint64_t>(U32(p + 4, false)) << 32 | U32(p, false);
}

bool FourCc(const uint8_t* p, const char (&id)[5]) noexcept
{
	return std::memcmp(p, id, 4) == 0;
}

/// AIFF's 80-bit extended float sample rate, as a whole number
int64_t AiffExtendedToInt(const uint8_t* data) noexcept
{
	uint32_t exponent = static_cast<uint32_t>(data[0]) << 8 | data[1];
	const uint64_t hi = U32(data + 2, true);
	const uint64_t lo = U32(data + 6, true);
	uint64_t significand = hi << 32 | lo;
	const bool negative = (exponent >> 15) != 0;
	exponent &= 0x7FFF;
	if (exponent == 0 && significand == 0)
	{
		return 0;
	}
	if (exponent == 0x7FFF)
	{
		return negative ? std::numeric_limits<int64_t>::min() : std::numeric_limits<int64_t>::max();
	}
	exponent -= 16383;
	if (static_cast<int32_t>(exponent) > 63)
	{
		return negative ? std::numeric_limits<int64_t>::min() : std::numeric_limits<int64_t>::max();
	}
	if (static_cast<int32_t>(exponent) < 1)
	{
		return 0;
	}
	significand >>= (63 - exponent);
	return negative ? -static_cast<int64_t>(significand) : static_cast<int64_t>(significand);
}

bool IsCompressed(uint16_t format) noexcept
{
	return format == k_FormatMsAdpcm || format == k_FormatImaAdpcm;
}

/// The bytes of one frame: from the bits per sample when they are whole bytes, else the block size
uint32_t BytesPerFrame(const WaveFile& wave) noexcept
{
	uint32_t bytes =
	    (wave.bitsPerSample & 7) == 0 ? static_cast<uint32_t>(wave.bitsPerSample * wave.channels) >> 3 : wave.blockAlign;
	if ((wave.format == k_FormatALaw || wave.format == k_FormatMuLaw) && bytes != wave.channels)
	{
		bytes = 0;
	}
	return bytes;
}

struct ChunkHeader
{
	std::array<uint8_t, 16> id {};
	uint64_t size {0};
	uint64_t padding {0};
};

std::optional<ChunkHeader> ReadChunkHeader(Stream& s, WaveContainer container) noexcept
{
	ChunkHeader header;
	if (container == WaveContainer::Wave64)
	{
		std::array<uint8_t, 8> size {};
		if (s.Read(header.id.data(), 16) != 16 || !s.ReadExactly(size))
		{
			return std::nullopt;
		}
		header.size = U64(size.data()) - 24; // a Wave64 size counts its header
		header.padding = header.size % 8;
		return header;
	}
	std::array<uint8_t, 4> size {};
	if (s.Read(header.id.data(), 4) != 4 || !s.ReadExactly(size))
	{
		return std::nullopt;
	}
	header.size = U32(size.data(), IsBigEndian(container));
	header.padding = header.size % 2;
	return header;
}

// ---- Reading the samples -------------------------------------------------------------------------------------------

/// The reading state: frames given so far and data bytes left
struct Reader
{
	const WaveFile& wave;
	Stream stream;
	uint64_t cursor {0};
	uint64_t bytesRemaining;

	/// Up to `frames` frames of raw bytes into `out`, byte-swapped to little-endian for the big-endian containers, and
	/// signed 8-bit AIFF made unsigned
	uint64_t ReadFrames(uint64_t frames, uint8_t* out) noexcept
	{
		if (IsCompressed(wave.format) || frames == 0)
		{
			return 0;
		}
		frames = std::min(frames, wave.frames - cursor);
		const uint32_t bytesPerFrame = BytesPerFrame(wave);
		if (bytesPerFrame == 0)
		{
			return 0;
		}
		const uint64_t bytes = std::min(frames * bytesPerFrame, bytesRemaining);
		if (bytes == 0)
		{
			return 0;
		}
		const size_t read = stream.Read(out, static_cast<size_t>(bytes));
		cursor += read / bytesPerFrame;
		bytesRemaining -= read;
		const uint64_t got = read / bytesPerFrame;
		const uint64_t samples = got * wave.channels;
		if (IsBigEndian(wave.container) && !(wave.container == WaveContainer::Aiff && wave.aiffLittleEndian))
		{
			const uint32_t size = bytesPerFrame / wave.channels;
			if (size == 2 || size == 3 || size == 4 || size == 8)
			{
				for (uint64_t i = 0; i < samples; ++i)
				{
					std::reverse(out + i * size, out + (i + 1) * size);
				}
			}
		}
		if (wave.container == WaveContainer::Aiff && wave.bitsPerSample == 8 && !wave.aiffUnsigned)
		{
			for (uint64_t i = 0; i < samples; ++i)
			{
				out[i] = static_cast<uint8_t>(out[i] + 128);
			}
		}
		return got;
	}
};

int16_t IntegerToS16(const uint8_t* in, uint32_t bytesPerSample) noexcept
{
	switch (bytesPerSample)
	{
	case 1:
		return static_cast<int16_t>((static_cast<int32_t>(in[0]) << 8) - 32768);
	case 2:
		return static_cast<int16_t>(U16(in, false));
	case 3:
		return static_cast<int16_t>(static_cast<int32_t>(static_cast<uint32_t>(in[0]) << 8 |
		                                                 static_cast<uint32_t>(in[1]) << 16 |
		                                                 static_cast<uint32_t>(in[2]) << 24) >>
		                            16);
	case 4:
		return static_cast<int16_t>(static_cast<int32_t>(U32(in, false)) >> 16);
	default:
		break;
	}
	if (bytesPerSample > 8)
	{
		return 0;
	}
	// The sample's bytes at the top of 64 bits, then its top 16
	uint64_t sample = 0;
	uint32_t shift = (8 - bytesPerSample) * 8;
	for (uint32_t j = 0; j < bytesPerSample; ++j, shift += 8)
	{
		sample |= static_cast<uint64_t>(in[j]) << shift;
	}
	return static_cast<int16_t>(static_cast<int64_t>(sample) >> 48);
}

int16_t FloatToS16(const uint8_t* in, uint32_t bytesPerSample) noexcept
{
	if (bytesPerSample == 4)
	{
		float x = 0;
		std::memcpy(&x, in, 4);
		float c = x < -1 ? -1 : (x > 1 ? 1 : x);
		c = c + 1;
		return static_cast<int16_t>(static_cast<int32_t>(c * 32767.5f) - 32768);
	}
	if (bytesPerSample == 8)
	{
		double x = 0;
		std::memcpy(&x, in, 8);
		double c = x < -1 ? -1 : (x > 1 ? 1 : x);
		c = c + 1;
		return static_cast<int16_t>(static_cast<int32_t>(c * 32767.5) - 32768);
	}
	return 0;
}

/// ITU-T G.711 A-law
int16_t ALawToS16(uint8_t v) noexcept
{
	const uint8_t a = v ^ 0x55;
	const int32_t exponent = (a & 0x70) >> 4;
	int32_t magnitude = ((a & 0x0F) << 4) + (exponent == 0 ? 8 : 0x108);
	if (exponent > 1)
	{
		magnitude <<= exponent - 1;
	}
	return static_cast<int16_t>((a & 0x80) != 0 ? magnitude : -magnitude);
}

/// ITU-T G.711 mu-law
int16_t MuLawToS16(uint8_t v) noexcept
{
	const auto u = static_cast<uint8_t>(~v);
	const int32_t exponent = (u & 0x70) >> 4;
	const int32_t magnitude = (((u & 0x0F) << 3) + 0x84) << exponent;
	return static_cast<int16_t>((u & 0x80) != 0 ? 0x84 - magnitude : magnitude - 0x84);
}

/// The uncompressed formats: pieces of up to 4096 bytes read, then converted a sample at a time
template <typename Convert>
void ReadConverted(Reader& reader, std::span<int16_t> out, Convert convert) noexcept
{
	const uint32_t bytesPerFrame = BytesPerFrame(reader.wave);
	if (bytesPerFrame == 0)
	{
		return;
	}
	const uint32_t bytesPerSample = bytesPerFrame / reader.wave.channels;
	if (bytesPerSample == 0 || bytesPerFrame % reader.wave.channels != 0)
	{
		return;
	}
	std::array<uint8_t, k_ConversionBytes> piece {};
	uint64_t frames = reader.wave.frames;
	size_t written = 0;
	while (frames > 0)
	{
		const uint64_t got = reader.ReadFrames(std::min<uint64_t>(frames, k_ConversionBytes / bytesPerFrame), piece.data());
		if (got == 0)
		{
			break;
		}
		const uint64_t samples = got * reader.wave.channels;
		for (uint64_t i = 0; i < samples; ++i)
		{
			out[written++] = convert(piece.data() + i * bytesPerSample, bytesPerSample);
		}
		frames -= got;
	}
}

/// MS-ADPCM: blocks of a header per channel (predictor, delta, the last two samples) and nibbles
void DecodeMsAdpcm(Reader& reader, std::span<int16_t> out) noexcept
{
	static constexpr std::array<int32_t, 16> k_Adaptation = {230, 230, 230, 230, 307, 409, 512, 614,
	                                                         768, 614, 512, 409, 307, 230, 230, 230};
	// The 7 standard predictor pairs (the file's own table is not read)
	static constexpr std::array<int32_t, 7> k_Coefficient1 = {256, 512, 0, 192, 240, 460, 392};
	static constexpr std::array<int32_t, 7> k_Coefficient2 = {0, -256, 0, 64, 0, -208, -232};
	struct Channel
	{
		uint8_t predictor {0};
		int32_t delta {0};
		int32_t older {0};
		int32_t last {0};
	};
	const auto step = [](Channel& c, uint32_t nibble) {
		const int32_t signedNibble = (nibble & 8) != 0 ? static_cast<int32_t>(nibble) - 16 : static_cast<int32_t>(nibble);
		int32_t sample = (c.last * k_Coefficient1[c.predictor] + c.older * k_Coefficient2[c.predictor]) >> 8;
		sample = std::clamp(sample + signedNibble * c.delta, -32768, 32767);
		c.delta = std::max((k_Adaptation[nibble] * c.delta) >> 8, 16);
		c.older = c.last;
		c.last = sample;
		return sample;
	};
	const auto& wave = reader.wave;
	const size_t channels = wave.channels;
	std::array<Channel, 2> state {};
	uint64_t frame = 0;
	uint32_t remaining = 0;
	const auto put = [&](size_t c, int32_t v) { out[static_cast<size_t>(frame) * channels + c] = static_cast<int16_t>(v); };
	while (frame < wave.frames)
	{
		if (remaining == 0)
		{
			std::array<uint8_t, 14> header {};
			const size_t size = 7 * channels;
			if (reader.stream.Read(header.data(), size) != size)
			{
				return;
			}
			remaining = static_cast<uint32_t>(wave.blockAlign - size);
			// Mono: predictor, delta, sample 1, sample 0; stereo: both predictors, both deltas, both samples 1, both 0
			for (size_t c = 0; c < channels; ++c)
			{
				auto& s = state[c];
				s.predictor = header[c];
				s.delta = static_cast<int16_t>(U16(&header[channels + 2 * c], false));
				s.last = static_cast<int16_t>(U16(&header[3 * channels + 2 * c], false));
				s.older = static_cast<int16_t>(U16(&header[5 * channels + 2 * c], false));
			}
			if (state[0].predictor >= k_Coefficient1.size() || (channels == 2 && state[1].predictor >= k_Coefficient1.size()))
			{
				return;
			}
			for (const bool older : {true, false})
			{
				if (frame >= wave.frames)
				{
					return;
				}
				for (size_t c = 0; c < channels; ++c)
				{
					put(c, older ? state[c].older : state[c].last);
				}
				++frame;
			}
			continue;
		}
		uint8_t nibbles = 0;
		if (reader.stream.Read(&nibbles, 1) != 1)
		{
			return;
		}
		--remaining;
		if (channels == 1)
		{
			const int32_t first = step(state[0], nibbles >> 4u);
			const int32_t second = step(state[0], nibbles & 15u);
			put(0, first);
			if (++frame >= wave.frames)
			{
				return;
			}
			put(0, second);
			++frame;
		}
		else
		{
			put(0, step(state[0], nibbles >> 4u));
			put(1, step(state[1], nibbles & 15u));
			++frame;
		}
	}
}

/// IMA ADPCM: blocks of a header per channel (the predictor and the step index) and, per channel, 4 bytes of 8 nibbles
void DecodeImaAdpcm(Reader& reader, std::span<int16_t> out) noexcept
{
	static constexpr std::array<int32_t, 16> k_IndexStep = {-1, -1, -1, -1, 2, 4, 6, 8, -1, -1, -1, -1, 2, 4, 6, 8};
	static constexpr std::array<int32_t, 89> k_Steps = {
	    7,    8,    9,    10,   11,    12,    13,    14,    16,    17,    19,    21,    23,    25,    28,    31,    34,   37,
	    41,   45,   50,   55,   60,    66,    73,    80,    88,    97,    107,   118,   130,   143,   157,   173,   190,  209,
	    230,  253,  279,  307,  337,   371,   408,   449,   494,   544,   598,   658,   724,   796,   876,   963,   1060, 1166,
	    1282, 1411, 1552, 1707, 1878,  2066,  2272,  2499,  2749,  3024,  3327,  3660,  4026,  4428,  4871,  5358,  5894, 6484,
	    7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899, 15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767};
	const auto& wave = reader.wave;
	const size_t channels = wave.channels;
	std::array<int32_t, 2> predictor {};
	std::array<int32_t, 2> index {};
	std::array<int32_t, 16> cache {};
	size_t cached = 0;
	uint32_t remaining = 0;
	uint64_t frame = 0;
	const auto decode = [&](size_t c, uint32_t nibble) {
		const int32_t s = k_Steps[static_cast<size_t>(index[c])];
		int32_t diff = s >> 3;
		if ((nibble & 1) != 0)
		{
			diff += s >> 2;
		}
		if ((nibble & 2) != 0)
		{
			diff += s >> 1;
		}
		if ((nibble & 4) != 0)
		{
			diff += s;
		}
		if ((nibble & 8) != 0)
		{
			diff = -diff;
		}
		predictor[c] = std::clamp(predictor[c] + diff, -32768, 32767);
		index[c] = std::clamp(index[c] + k_IndexStep[nibble], 0, static_cast<int32_t>(k_Steps.size()) - 1);
		return predictor[c];
	};
	while (frame < wave.frames)
	{
		if (cached == 0 && remaining == 0)
		{
			std::array<uint8_t, 8> header {};
			const size_t size = 4 * channels;
			if (reader.stream.Read(header.data(), size) != size)
			{
				return;
			}
			remaining = static_cast<uint32_t>(wave.blockAlign - size);
			if (header[2] >= k_Steps.size() || (channels == 2 && header[6] >= k_Steps.size()))
			{
				return;
			}
			for (size_t c = 0; c < channels; ++c)
			{
				predictor[c] = static_cast<int16_t>(U16(&header[4 * c], false));
				index[c] = header[4 * c + 2];
				cache[16 - channels + c] = predictor[c];
			}
			cached = 1;
		}
		while (cached > 0 && frame < wave.frames)
		{
			for (size_t c = 0; c < channels; ++c)
			{
				out[static_cast<size_t>(frame) * channels + c] = static_cast<int16_t>(cache[16 - cached * channels + c]);
			}
			++frame;
			--cached;
		}
		if (frame >= wave.frames || remaining == 0)
		{
			continue;
		}
		cached = 8;
		for (size_t c = 0; c < channels; ++c)
		{
			std::array<uint8_t, 4> nibbles {};
			if (!reader.stream.ReadExactly(nibbles))
			{
				return;
			}
			remaining -= 4;
			for (size_t b = 0; b < 4; ++b)
			{
				cache[16 - 8 * channels + (b * 2 + 0) * channels + c] = decode(c, nibbles[b] & 15u);
				cache[16 - 8 * channels + (b * 2 + 1) * channels + c] = decode(c, nibbles[b] >> 4u);
			}
		}
	}
}
} // namespace

std::optional<WaveFile> openblack::audio::codec::ParseWaveFile(std::span<const uint8_t> file) noexcept
{
	if (file.empty())
	{
		return std::nullopt;
	}
	Stream s(file);
	WaveFile wave;
	std::array<uint8_t, 4> id {};
	if (!s.ReadExactly(id))
	{
		return std::nullopt;
	}
	bool aifc = false;
	if (FourCc(id.data(), "RIFF"))
	{
		wave.container = WaveContainer::Riff;
	}
	else if (FourCc(id.data(), "RIFX"))
	{
		wave.container = WaveContainer::Rifx;
	}
	else if (FourCc(id.data(), "riff"))
	{
		wave.container = WaveContainer::Wave64;
		std::array<uint8_t, 12> rest {};
		if (!s.ReadExactly(rest) || !std::equal(rest.begin(), rest.end(), k_GuidW64Riff.begin() + 4))
		{
			return std::nullopt;
		}
	}
	else if (FourCc(id.data(), "RF64"))
	{
		wave.container = WaveContainer::Rf64;
	}
	else if (FourCc(id.data(), "FORM"))
	{
		wave.container = WaveContainer::Aiff;
	}
	else
	{
		return std::nullopt;
	}
	const bool bigEndian = IsBigEndian(wave.container);
	if (wave.container == WaveContainer::Riff || wave.container == WaveContainer::Rifx || wave.container == WaveContainer::Rf64)
	{
		std::array<uint8_t, 4> size {};
		std::array<uint8_t, 4> type {};
		if (!s.ReadExactly(size) || (wave.container == WaveContainer::Rf64 && U32(size.data(), false) != 0xFFFFFFFF) ||
		    !s.ReadExactly(type) || !FourCc(type.data(), "WAVE"))
		{
			return std::nullopt;
		}
	}
	else if (wave.container == WaveContainer::Wave64)
	{
		std::array<uint8_t, 8> size {};
		std::array<uint8_t, 16> type {};
		if (!s.ReadExactly(size) || U64(size.data()) < 80 || !s.ReadExactly(type) || type != k_GuidW64Wave)
		{
			return std::nullopt;
		}
	}
	else
	{
		std::array<uint8_t, 4> size {};
		std::array<uint8_t, 4> type {};
		if (!s.ReadExactly(size) || U32(size.data(), true) < 18 || !s.ReadExactly(type))
		{
			return std::nullopt;
		}
		if (FourCc(type.data(), "AIFC"))
		{
			aifc = true;
		}
		else if (!FourCc(type.data(), "AIFF"))
		{
			return std::nullopt;
		}
	}

	uint64_t dataSize = 0;
	uint64_t factFrames = 0;
	uint64_t aiffFrames = 0;
	uint32_t riffFactFrames = 0; ///< a RIFF "fact" chunk's length, only used for MS-ADPCM
	// Where the reference believes it is: it counts the chunk sizes it skips, so a chunk whose size overflows a seek
	// puts it elsewhere than the bytes it read, and the data is looked for there
	uint64_t cursor = s.Position();
	const size_t headerBytes = wave.container == WaveContainer::Wave64 ? 24 : 8;
	if (wave.container == WaveContainer::Rf64)
	{
		// "ds64" first: the data size and the sample count
		const auto header = ReadChunkHeader(s, wave.container);
		if (!header || !FourCc(header->id.data(), "ds64"))
		{
			return std::nullopt;
		}
		cursor += headerBytes;
		uint64_t left = header->size + header->padding;
		std::array<uint8_t, 8> value {};
		if (!s.Forward(8))
		{
			return std::nullopt;
		}
		left -= 8;
		cursor += 8;
		if (!s.ReadExactly(value))
		{
			return std::nullopt;
		}
		left -= 8;
		cursor += 8;
		dataSize = U64(value.data());
		if (!s.ReadExactly(value))
		{
			return std::nullopt;
		}
		left -= 8;
		cursor += 8;
		factFrames = U64(value.data());
		if (!s.Forward(left))
		{
			return std::nullopt;
		}
		cursor += left;
	}

	uint16_t tag = 0;
	uint16_t subFormat = 0;
	bool haveFormat = false;
	bool haveData = false;
	const bool riffLike =
	    wave.container == WaveContainer::Riff || wave.container == WaveContainer::Rifx || wave.container == WaveContainer::Rf64;
	for (;;)
	{
		const auto header = ReadChunkHeader(s, wave.container);
		if (!header)
		{
			break;
		}
		cursor += headerBytes;
		uint64_t chunkSize = header->size;
		const auto* chunkId = header->id.data();
		const auto is = [&](const char(&fourCc)[5], const std::array<uint8_t, 16>& guid) {
			return (riffLike && FourCc(chunkId, fourCc)) || (wave.container == WaveContainer::Wave64 && header->id == guid);
		};
		if (is("fmt ", k_GuidW64Fmt))
		{
			haveFormat = true;
			std::array<uint8_t, 16> f {};
			if (!s.ReadExactly(f))
			{
				return std::nullopt;
			}
			cursor += 16;
			tag = U16(&f[0], bigEndian);
			wave.channels = U16(&f[2], bigEndian);
			wave.sampleRate = U32(&f[4], bigEndian);
			wave.blockAlign = U16(&f[12], bigEndian);
			wave.bitsPerSample = U16(&f[14], bigEndian);
			subFormat = 0;
			if (header->size > 16)
			{
				std::array<uint8_t, 2> cbSize {};
				if (!s.ReadExactly(cbSize))
				{
					return std::nullopt;
				}
				cursor += 2;
				int64_t readSoFar = 18;
				const uint16_t extended = U16(cbSize.data(), bigEndian);
				if (extended > 0)
				{
					if (tag == k_FormatExtensible)
					{
						std::array<uint8_t, 22> extension {};
						if (extended != 22 || !s.ReadExactly(extension))
						{
							return std::nullopt;
						}
						subFormat = U16(&extension[6], bigEndian);
					}
					else if (!s.Seek(extended))
					{
						return std::nullopt;
					}
					cursor += extended;
					readSoFar += extended;
				}
				if (!s.Seek(static_cast<int32_t>(header->size - static_cast<uint64_t>(readSoFar))))
				{
					return std::nullopt;
				}
				cursor += header->size - static_cast<uint64_t>(readSoFar);
			}
			if (header->padding > 0)
			{
				if (!s.Forward(header->padding))
				{
					break;
				}
				cursor += header->padding;
			}
			continue;
		}
		if (is("data", k_GuidW64Data))
		{
			haveData = true;
			wave.dataOffset = static_cast<size_t>(cursor);
			if (wave.container != WaveContainer::Rf64)
			{
				dataSize = chunkSize;
			}
			break;
		}
		if (is("fact", k_GuidW64Fact))
		{
			if (wave.container == WaveContainer::Riff || wave.container == WaveContainer::Rifx)
			{
				// The real length of an MS-ADPCM wave, whose last block is padded (the old wave library missed it)
				std::array<uint8_t, 4> count {};
				if (!s.ReadExactly(count))
				{
					return std::nullopt;
				}
				chunkSize -= 4;
				cursor += 4;
				factFrames = 0;
				riffFactFrames = U32(count.data(), bigEndian);
			}
			else if (wave.container == WaveContainer::Wave64)
			{
				std::array<uint8_t, 8> count {};
				if (!s.ReadExactly(count))
				{
					return std::nullopt;
				}
				chunkSize -= 8;
				cursor += 8;
				factFrames = U64(count.data());
			}
			if (!s.Forward(chunkSize + header->padding))
			{
				break;
			}
			cursor += chunkSize + header->padding;
			continue;
		}
		if (wave.container == WaveContainer::Aiff && FourCc(chunkId, "COMM"))
		{
			haveFormat = true;
			std::array<uint8_t, 24> comm {};
			const size_t commSize = aifc ? 24 : 18;
			if ((aifc && header->size < commSize) || (!aifc && header->size != commSize) ||
			    s.Read(comm.data(), commSize) != commSize)
			{
				return std::nullopt;
			}
			cursor += commSize;
			const uint16_t channels = U16(&comm[0], true);
			const uint32_t frames = U32(&comm[2], true);
			const uint16_t bits = U16(&comm[6], true);
			const int64_t rate = AiffExtendedToInt(&comm[8]);
			if (rate < 0 || rate > 0xFFFFFFFF)
			{
				return std::nullopt;
			}
			uint16_t compression = k_FormatPcm;
			if (aifc)
			{
				const auto* type = &comm[18];
				if (FourCc(type, "NONE"))
				{
					compression = k_FormatPcm;
				}
				else if (FourCc(type, "raw "))
				{
					wave.aiffUnsigned = bits == 8;
				}
				else if (FourCc(type, "sowt"))
				{
					wave.aiffLittleEndian = true;
				}
				else if (FourCc(type, "fl32") || FourCc(type, "fl64") || FourCc(type, "FL32") || FourCc(type, "FL64"))
				{
					compression = k_FormatIeeeFloat;
				}
				else if (FourCc(type, "alaw") || FourCc(type, "ALAW"))
				{
					compression = k_FormatALaw;
				}
				else if (FourCc(type, "ulaw") || FourCc(type, "ULAW"))
				{
					compression = k_FormatMuLaw;
				}
				else
				{
					return std::nullopt; // "ima4" and the rest
				}
			}
			aiffFrames = frames;
			tag = compression;
			wave.channels = channels;
			wave.sampleRate = static_cast<uint32_t>(rate);
			wave.bitsPerSample = bits;
			wave.blockAlign = static_cast<uint16_t>(channels * bits / 8);
			if ((compression == k_FormatALaw || compression == k_FormatMuLaw) && bits > 8)
			{
				// Some A-law and mu-law files say 16 bits
				wave.bitsPerSample = 8;
				wave.blockAlign = channels;
			}
			wave.bitsPerSample = static_cast<uint16_t>(wave.bitsPerSample + (wave.bitsPerSample & 7));
			if (aifc)
			{
				if (!s.Forward(chunkSize - commSize))
				{
					return std::nullopt;
				}
				cursor += chunkSize - commSize;
			}
			continue;
		}
		if (wave.container == WaveContainer::Aiff && FourCc(chunkId, "SSND"))
		{
			haveData = true;
			std::array<uint8_t, 8> offsetAndBlock {};
			if (!s.ReadExactly(offsetAndBlock))
			{
				return std::nullopt;
			}
			cursor += 8;
			const uint32_t offset = U32(offsetAndBlock.data(), true);
			wave.dataOffset = static_cast<size_t>(cursor + offset);
			dataSize = chunkSize > offset ? chunkSize - offset : 0;
			if (!s.Forward(chunkSize + header->padding - 8))
			{
				break;
			}
			cursor += chunkSize + header->padding - 8;
			continue;
		}
		if (!s.Forward(chunkSize + header->padding))
		{
			break;
		}
		cursor += chunkSize + header->padding;
	}
	if (!haveFormat || !haveData)
	{
		return std::nullopt;
	}
	if (wave.sampleRate == 0 || wave.sampleRate > k_MaxSampleRate || wave.channels == 0 || wave.channels > k_MaxChannels ||
	    wave.bitsPerSample == 0 || wave.bitsPerSample > k_MaxBitsPerSample || wave.blockAlign == 0)
	{
		return std::nullopt;
	}
	wave.format = tag == k_FormatExtensible ? subFormat : tag;
	if (!s.SeekTo(static_cast<int64_t>(wave.dataOffset)))
	{
		return std::nullopt;
	}
	// The data cut to the file (a size of 0xFFFFFFFF, "to the end", is that too)
	if (dataSize + wave.dataOffset > file.size())
	{
		dataSize = file.size() - wave.dataOffset;
	}
	if (dataSize == 0xFFFFFFFF && (wave.container == WaveContainer::Riff || wave.container == WaveContainer::Rifx))
	{
		dataSize = 0;
	}
	if (!IsCompressed(wave.format))
	{
		const uint32_t bytesPerFrame = BytesPerFrame(wave);
		if (bytesPerFrame > 0)
		{
			dataSize -= dataSize % bytesPerFrame;
		}
	}
	wave.dataSize = dataSize;
	if (factFrames != 0)
	{
		wave.frames = factFrames;
	}
	else if (aiffFrames != 0)
	{
		wave.frames = aiffFrames;
	}
	else
	{
		const uint32_t bytesPerFrame = BytesPerFrame(wave);
		if (bytesPerFrame == 0)
		{
			return std::nullopt;
		}
		wave.frames = dataSize / bytesPerFrame;
		if (IsCompressed(wave.format))
		{
			// Two samples a byte once the block headers (6 bytes a channel for MS-ADPCM, 4 for IMA) are taken off; a last
			// partial block counts as a block; IMA's headers each carry a sample
			uint64_t blocks = dataSize / wave.blockAlign;
			if (blocks * wave.blockAlign < dataSize)
			{
				++blocks;
			}
			const bool ima = wave.format == k_FormatImaAdpcm;
			const uint64_t headers = blocks * (ima ? 4 : 6) * wave.channels;
			wave.frames = (dataSize - headers) * 2 / wave.channels + (ima ? blocks : 0);
			// An MS-ADPCM wave ends at its "fact" length, before the padding of its last block
			if (wave.format == k_FormatMsAdpcm && riffFactFrames != 0 && riffFactFrames < wave.frames)
			{
				wave.frames = riffFactFrames;
			}
		}
	}
	if (IsCompressed(wave.format) && wave.channels > 2)
	{
		return std::nullopt;
	}
	if (BytesPerFrame(wave) == 0)
	{
		return std::nullopt;
	}
	return wave;
}

std::optional<DecodedAudio> openblack::audio::codec::DecodeWaveFile(std::span<const uint8_t> file)
{
	const auto wave = ParseWaveFile(file);
	if (!wave || wave->frames > k_MaxSamples || wave->frames * wave->channels > k_MaxSamples)
	{
		return std::nullopt;
	}
	DecodedAudio audio;
	audio.channels = wave->channels;
	audio.sampleRate = wave->sampleRate;
	audio.samples.assign(static_cast<size_t>(wave->frames * wave->channels), 0);
	Reader reader {*wave, Stream(file), 0, wave->dataSize};
	reader.stream.SeekTo(static_cast<int64_t>(wave->dataOffset));
	switch (wave->format)
	{
	case k_FormatPcm:
		if (wave->bitsPerSample == 16)
		{
			// Straight into the samples (byte-swapped for the big-endian containers), little-endian
			reader.ReadFrames(wave->frames, reinterpret_cast<uint8_t*>(audio.samples.data()));
			for (auto& sample : audio.samples)
			{
				std::array<uint8_t, 2> bytes {};
				std::memcpy(bytes.data(), &sample, 2);
				sample = static_cast<int16_t>(U16(bytes.data(), false));
			}
		}
		else
		{
			ReadConverted(reader, audio.samples, IntegerToS16);
		}
		break;
	case k_FormatIeeeFloat:
		ReadConverted(reader, audio.samples, FloatToS16);
		break;
	case k_FormatALaw:
		ReadConverted(reader, audio.samples, [](const uint8_t* in, uint32_t) { return ALawToS16(*in); });
		break;
	case k_FormatMuLaw:
		ReadConverted(reader, audio.samples, [](const uint8_t* in, uint32_t) { return MuLawToS16(*in); });
		break;
	case k_FormatMsAdpcm:
		DecodeMsAdpcm(reader, audio.samples);
		break;
	case k_FormatImaAdpcm:
		DecodeImaAdpcm(reader, audio.samples);
		break;
	default:
		// Any other format opens as silence of its length (as before)
		break;
	}
	return audio;
}
