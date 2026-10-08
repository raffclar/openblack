/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The frame search, the layer II and III tables (the Huffman tables as lookup trees among them), the Xing and LAME
// header reading and the order of the floating point operations of the dequantisation, the stereo processing, the
// inverse MDCT and the synthesis follow minimp3 (Copyright (c) lieff, CC0 1.0) as dr_mp3 0.7 (public domain or MIT-0)
// carries it, with its SSE path on x64, so that the samples are bit for bit those openblack played with dr_mp3.

#include "MpegAudio.h"

#include <cmath>
#include <cstring>

#include <algorithm>
#include <vector>

using namespace openblack::audio::codec;

namespace
{
constexpr int k_HeaderSize = 4;
constexpr int k_MaxFreeFormatFrameSize = 2304;
constexpr int k_MaxFrameSyncMatches = 10;
constexpr int k_ModeMono = 3;
constexpr int k_ModeJointStereo = 1;

// ---- Headers -------------------------------------------------------------------------------------------------------

using Header = const uint8_t*;

bool IsMono(Header h) noexcept
{
	return (h[3] & 0xC0) == 0xC0;
}
bool IsFreeFormat(Header h) noexcept
{
	return (h[2] & 0xF0) == 0;
}
bool HasCrc(Header h) noexcept
{
	return (h[1] & 1) == 0;
}
bool HasPadding(Header h) noexcept
{
	return (h[2] & 0x2) != 0;
}
bool IsMpeg1(Header h) noexcept
{
	return (h[1] & 0x8) != 0;
}
bool IsNotMpeg25(Header h) noexcept
{
	return (h[1] & 0x10) != 0;
}
int StereoMode(Header h) noexcept
{
	return (h[3] >> 6) & 3;
}
int StereoModeExtension(Header h) noexcept
{
	return (h[3] >> 4) & 3;
}
int LayerBits(Header h) noexcept
{
	return (h[1] >> 1) & 3;
}
int BitrateIndex(Header h) noexcept
{
	return h[2] >> 4;
}
int SampleRateIndex(Header h) noexcept
{
	return (h[2] >> 2) & 3;
}
bool IsFrame576(Header h) noexcept
{
	return (h[1] & 14) == 2;
}
bool IsLayer1(Header h) noexcept
{
	return (h[1] & 6) == 6;
}

bool IsValid(Header h) noexcept
{
	return h[0] == 0xFF && ((h[1] & 0xF0) == 0xF0 || (h[1] & 0xFE) == 0xE2) && LayerBits(h) != 0 && BitrateIndex(h) != 15 &&
	       SampleRateIndex(h) != 3;
}

/// The same stream: version, layer, sample rate and free format alike
bool Matches(Header h1, Header h2) noexcept
{
	return IsValid(h2) && ((h1[1] ^ h2[1]) & 0xFE) == 0 && ((h1[2] ^ h2[2]) & 0x0C) == 0 &&
	       IsFreeFormat(h1) == IsFreeFormat(h2);
}

unsigned BitrateKbps(Header h) noexcept
{
	static constexpr uint8_t k_HalfRate[2][3][15] = {
	    {{0, 4, 8, 12, 16, 20, 24, 28, 32, 40, 48, 56, 64, 72, 80},
	     {0, 4, 8, 12, 16, 20, 24, 28, 32, 40, 48, 56, 64, 72, 80},
	     {0, 16, 24, 28, 32, 40, 48, 56, 64, 72, 80, 88, 96, 112, 128}},
	    {{0, 16, 20, 24, 28, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160},
	     {0, 16, 24, 28, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192},
	     {0, 16, 32, 48, 64, 80, 96, 112, 128, 144, 160, 176, 192, 208, 224}},
	};
	return 2u * k_HalfRate[IsMpeg1(h) ? 1 : 0][LayerBits(h) - 1][BitrateIndex(h)];
}

unsigned SampleRateHz(Header h) noexcept
{
	static constexpr unsigned k_Hz[3] = {44100, 48000, 32000};
	return k_Hz[SampleRateIndex(h)] >> (IsMpeg1(h) ? 0 : 1) >> (IsNotMpeg25(h) ? 0 : 1);
}

unsigned FrameSamples(Header h) noexcept
{
	return IsLayer1(h) ? 384 : (1152 >> (IsFrame576(h) ? 1 : 0));
}

int FrameBytes(Header h, int freeFormatSize) noexcept
{
	int bytes = static_cast<int>(FrameSamples(h) * BitrateKbps(h) * 125 / SampleRateHz(h));
	if (IsLayer1(h))
	{
		bytes &= ~3;
	}
	return bytes != 0 ? bytes : freeFormatSize;
}

int Padding(Header h) noexcept
{
	return HasPadding(h) ? (IsLayer1(h) ? 4 : 1) : 0;
}

/// Whether the frames that follow the one at `h` carry on the same stream (up to 10 of them, as far as the data goes)
bool FollowedByFrames(Header h, int bytes, int frameBytes) noexcept
{
	int at = 0;
	for (int matched = 0; matched < k_MaxFrameSyncMatches; ++matched)
	{
		at += FrameBytes(h + at, frameBytes) + Padding(h + at);
		if (at + k_HeaderSize > bytes)
		{
			return matched > 0;
		}
		if (!Matches(h, h + at))
		{
			return false;
		}
	}
	return true;
}

/// The offset of the first frame that is followed by more of the same stream (or that fills the data exactly); sets
/// its size with its padding, 0 when there is none
int FindFrame(const uint8_t* data, int bytes, int& freeFormatBytes, int& frameBytesOut) noexcept
{
	for (int i = 0; i < bytes - k_HeaderSize; ++i, ++data)
	{
		if (!IsValid(data))
		{
			continue;
		}
		int frameBytes = FrameBytes(data, freeFormatBytes);
		int frameAndPadding = frameBytes + Padding(data);
		for (int k = k_HeaderSize; frameBytes == 0 && k < k_MaxFreeFormatFrameSize && i + 2 * k < bytes - k_HeaderSize; ++k)
		{
			if (Matches(data, data + k))
			{
				const int candidate = k - Padding(data);
				const int next = candidate + Padding(data + k);
				if (i + k + next + k_HeaderSize > bytes || !Matches(data, data + k + next))
				{
					continue;
				}
				frameAndPadding = k;
				frameBytes = candidate;
				freeFormatBytes = candidate;
			}
		}
		if ((frameBytes != 0 && i + frameAndPadding <= bytes && FollowedByFrames(data, bytes - i, frameBytes)) ||
		    (i == 0 && frameAndPadding == bytes))
		{
			frameBytesOut = frameAndPadding;
			return i;
		}
		freeFormatBytes = 0;
	}
	frameBytesOut = 0;
	return bytes;
}

// ---- Bits ----------------------------------------------------------------------------------------------------------

/// Most significant bit first. Past the end it gives 0 (and still moves on, so the caller can tell)
struct BitStream
{
	const uint8_t* data;
	int position {0};
	int limit;

	BitStream(const uint8_t* bytes, int size) noexcept
	    : data(bytes)
	    , limit(size * 8)
	{
	}

	uint32_t Read(int n) noexcept
	{
		const uint32_t shift = static_cast<uint32_t>(position & 7);
		int left = n + static_cast<int>(shift);
		const uint8_t* p = data + (position >> 3);
		if ((position += n) > limit)
		{
			return 0;
		}
		uint32_t cache = 0;
		uint32_t next = *p++ & (255u >> shift);
		while ((left -= 8) > 0)
		{
			cache |= next << left;
			next = *p++;
		}
		return cache | (next >> -left);
	}
};

// ---- Layer II scale information and samples ------------------------------------------------------------------------

struct SubbandAllocation
{
	uint8_t tableOffset;
	uint8_t codeWidth;
	uint8_t bandCount;
};

struct ScaleInfo
{
	std::array<float, 3 * 64> scalefactors {};
	uint8_t totalBands {0};
	uint8_t stereoBands {0};
	std::array<uint8_t, 64> allocation {};
	std::array<uint8_t, 64> scfsi {};
};

const SubbandAllocation* AllocationTable(Header h, ScaleInfo& info) noexcept
{
	const int mode = StereoMode(h);
	const int stereoBands = mode == k_ModeMono ? 0 : mode == k_ModeJointStereo ? (StereoModeExtension(h) << 2) + 4 : 32;
	const SubbandAllocation* table = nullptr;
	int bands = 0;
	if (IsLayer1(h))
	{
		static constexpr SubbandAllocation k_Layer1[] = {{76, 4, 32}};
		table = k_Layer1;
		bands = 32;
	}
	else if (!IsMpeg1(h))
	{
		static constexpr SubbandAllocation k_Mpeg2[] = {{60, 4, 4}, {44, 3, 7}, {44, 2, 19}};
		table = k_Mpeg2;
		bands = 30;
	}
	else
	{
		static constexpr SubbandAllocation k_Mpeg1[] = {{0, 4, 3}, {16, 4, 8}, {32, 3, 12}, {40, 2, 7}};
		const int rateIndex = SampleRateIndex(h);
		unsigned kbps = BitrateKbps(h) >> (mode != k_ModeMono ? 1 : 0);
		if (kbps == 0)
		{
			kbps = 192; // free format
		}
		table = k_Mpeg1;
		bands = 27;
		if (kbps < 56)
		{
			static constexpr SubbandAllocation k_Mpeg1LowRate[] = {{44, 4, 2}, {44, 3, 10}};
			table = k_Mpeg1LowRate;
			bands = rateIndex == 2 ? 12 : 8;
		}
		else if (kbps >= 96 && rateIndex != 1)
		{
			bands = 30;
		}
	}
	info.totalBands = static_cast<uint8_t>(bands);
	info.stereoBands = static_cast<uint8_t>(std::min(stereoBands, bands));
	return table;
}

/// The 1, 2 or 3 scale factors of each band and channel, by their selection code
void ReadScalefactors(BitStream& bits, const uint8_t* allocation, const uint8_t* scfsi, int bands, float* out) noexcept
{
	static constexpr auto k_Dequantise = []() {
		// 2^-20, 2^-20.33, 2^-20.67 over each quantiser's levels
		std::array<float, 18 * 3> t {};
		constexpr int k_Levels[18] = {3, 7, 15, 31, 63, 127, 255, 511, 1023, 2047, 4095, 8191, 16383, 32767, 65535, 3, 5, 9};
		for (int i = 0; i < 18; ++i)
		{
			t[i * 3 + 0] = 9.53674316e-07f / static_cast<float>(k_Levels[i]);
			t[i * 3 + 1] = 7.56931807e-07f / static_cast<float>(k_Levels[i]);
			t[i * 3 + 2] = 6.00777173e-07f / static_cast<float>(k_Levels[i]);
		}
		return t;
	}();
	for (int i = 0; i < bands; ++i)
	{
		float s = 0;
		const int ba = *allocation++;
		const int mask = ba != 0 ? 4 + ((19 >> scfsi[i]) & 3) : 0;
		for (int m = 4; m != 0; m >>= 1)
		{
			if ((mask & m) != 0)
			{
				const int b = static_cast<int>(bits.Read(6));
				s = k_Dequantise[static_cast<size_t>(ba * 3 - 6 + b % 3)] * static_cast<float>(1 << 21 >> b / 3);
			}
			*out++ = s;
		}
	}
}

void ReadScaleInfo(Header h, BitStream& bits, ScaleInfo& info) noexcept
{
	static constexpr uint8_t k_AllocationCodes[] = {
	    0, 17, 3,  4,  5,  6, 7, 8,  9, 10, 11, 12, 13, 14, 15, 16, //
	    0, 17, 18, 3,  19, 4, 5, 6,  7, 8,  9,  10, 11, 12, 13, 16, //
	    0, 17, 18, 3,  19, 4, 5, 16,                                //
	    0, 17, 18, 16,                                              //
	    0, 17, 18, 19, 4,  5, 6, 7,  8, 9,  10, 11, 12, 13, 14, 15, //
	    0, 17, 18, 3,  19, 4, 5, 6,  7, 8,  9,  10, 11, 12, 13, 14, //
	    0, 2,  3,  4,  5,  6, 7, 8,  9, 10, 11, 12, 13, 14, 15, 16,
	};
	const SubbandAllocation* subband = AllocationTable(h, info);
	int next = 0;
	int codeWidth = 0;
	const uint8_t* codes = k_AllocationCodes;
	for (int i = 0; i < info.totalBands; ++i)
	{
		if (i == next)
		{
			next += subband->bandCount;
			codeWidth = subband->codeWidth;
			codes = k_AllocationCodes + subband->tableOffset;
			++subband;
		}
		uint8_t ba = codes[bits.Read(codeWidth)];
		info.allocation[2 * i] = ba;
		if (i < info.stereoBands)
		{
			ba = codes[bits.Read(codeWidth)];
		}
		info.allocation[2 * i + 1] = info.stereoBands != 0 ? ba : 0;
	}
	for (int i = 0; i < 2 * info.totalBands; ++i)
	{
		info.scfsi[i] = static_cast<uint8_t>(info.allocation[i] != 0 ? (IsLayer1(h) ? 2 : bits.Read(2)) : 6);
	}
	ReadScalefactors(bits, info.allocation.data(), info.scfsi.data(), info.totalBands * 2, info.scalefactors.data());
	for (int i = info.stereoBands; i < info.totalBands; ++i)
	{
		info.allocation[2 * i + 1] = 0;
	}
}

/// One granule's samples of every band (3 a band for layer II, 1 for layer I): left at 18 * band, right 576 after
int DequantiseGranule(float* granule, BitStream& bits, const ScaleInfo& info, int groupSize) noexcept
{
	int channelOffset = 576;
	for (int j = 0; j < 4; ++j)
	{
		float* out = granule + groupSize * j;
		for (int i = 0; i < 2 * info.totalBands; ++i)
		{
			const int ba = info.allocation[i];
			if (ba != 0)
			{
				if (ba < 17)
				{
					const int half = (1 << (ba - 1)) - 1;
					for (int k = 0; k < groupSize; ++k)
					{
						out[k] = static_cast<float>(static_cast<int>(bits.Read(ba)) - half);
					}
				}
				else
				{
					// 3, 5 or 9 levels: a group of three samples in one code of 5, 7 or 10 bits
					const unsigned levels = (2u << (ba - 17)) + 1;
					unsigned code = bits.Read(static_cast<int>(levels + 2 - (levels >> 3)));
					for (int k = 0; k < groupSize; ++k, code /= levels)
					{
						out[k] = static_cast<float>(static_cast<int>(code % levels - levels / 2));
					}
				}
			}
			out += channelOffset;
			channelOffset = 18 - channelOffset;
		}
	}
	return groupSize * 4;
}

/// Scales 12 samples of each band by its scale factor; the bands above the stereo ones get the left channel's
void ApplyScalefactors(const ScaleInfo& info, const float* scalefactors, float* granule) noexcept
{
	std::memcpy(granule + 576 + info.stereoBands * 18, granule + info.stereoBands * 18,
	            static_cast<size_t>(info.totalBands - info.stereoBands) * 18 * sizeof(float));
	for (int i = 0; i < info.totalBands; ++i, granule += 18, scalefactors += 6)
	{
		for (int k = 0; k < 12; ++k)
		{
			granule[k + 0] *= scalefactors[0];
			granule[k + 576] *= scalefactors[3];
		}
	}
}

// ---- Synthesis -----------------------------------------------------------------------------------------------------

/// Four lanes of single precision floats with the operations the vector code uses: each lane rounded on its own
struct F4
{
	std::array<float, 4> v {};

	static F4 Load(const float* p) noexcept { return {{p[0], p[1], p[2], p[3]}}; }
	static F4 Set(float x) noexcept { return {{x, x, x, x}}; }
	void Store(float* p) const noexcept { std::copy(v.begin(), v.end(), p); }
	friend F4 operator+(const F4& a, const F4& b) noexcept
	{
		return {{a.v[0] + b.v[0], a.v[1] + b.v[1], a.v[2] + b.v[2], a.v[3] + b.v[3]}};
	}
	friend F4 operator-(const F4& a, const F4& b) noexcept
	{
		return {{a.v[0] - b.v[0], a.v[1] - b.v[1], a.v[2] - b.v[2], a.v[3] - b.v[3]}};
	}
	friend F4 operator*(const F4& a, const F4& b) noexcept
	{
		return {{a.v[0] * b.v[0], a.v[1] * b.v[1], a.v[2] * b.v[2], a.v[3] * b.v[3]}};
	}
	friend F4 operator*(const F4& a, float s) noexcept { return a * Set(s); }
};

/// The 32-point DCT-II of each of `n` columns of samples (18 floats apart), four columns at a time
void DctII(float* granule, int n) noexcept
{
	static constexpr float k_Sec[24] = {10.19000816f, 0.50060302f, 0.50241929f, 3.40760851f, 0.50547093f, 0.52249861f,
	                                    2.05778098f,  0.51544732f, 0.56694406f, 1.48416460f, 0.53104258f, 0.64682180f,
	                                    1.16943991f,  0.55310392f, 0.78815460f, 0.97256821f, 0.58293498f, 1.06067765f,
	                                    0.83934963f,  0.62250412f, 1.72244716f, 0.74453628f, 0.67480832f, 5.10114861f};
	for (int k = 0; k < n; k += 4)
	{
		std::array<std::array<F4, 8>, 4> t;
		float* y = granule + k;
		for (int i = 0; i < 8; ++i)
		{
			const F4 x0 = F4::Load(&y[i * 18]);
			const F4 x1 = F4::Load(&y[(15 - i) * 18]);
			const F4 x2 = F4::Load(&y[(16 + i) * 18]);
			const F4 x3 = F4::Load(&y[(31 - i) * 18]);
			const F4 t0 = x0 + x3;
			const F4 t1 = x1 + x2;
			const F4 t2 = (x1 - x2) * k_Sec[3 * i + 0];
			const F4 t3 = (x0 - x3) * k_Sec[3 * i + 1];
			t[0][i] = t0 + t1;
			t[1][i] = (t0 - t1) * k_Sec[3 * i + 2];
			t[2][i] = t3 + t2;
			t[3][i] = (t3 - t2) * k_Sec[3 * i + 2];
		}
		for (auto& x : t)
		{
			F4 x0 = x[0], x1 = x[1], x2 = x[2], x3 = x[3], x4 = x[4], x5 = x[5], x6 = x[6], x7 = x[7];
			F4 xt = x0 - x7;
			x0 = x0 + x7;
			x7 = x1 - x6;
			x1 = x1 + x6;
			x6 = x2 - x5;
			x2 = x2 + x5;
			x5 = x3 - x4;
			x3 = x3 + x4;
			x4 = x0 - x3;
			x0 = x0 + x3;
			x3 = x1 - x2;
			x1 = x1 + x2;
			x[0] = x0 + x1;
			x[4] = (x0 - x1) * 0.70710677f;
			x5 = x5 + x6;
			x6 = (x6 + x7) * 0.70710677f;
			x7 = x7 + xt;
			x3 = (x3 + x4) * 0.70710677f;
			x5 = x5 - x7 * 0.198912367f; // rotate by pi / 8
			x7 = x7 + x5 * 0.382683432f;
			x5 = x5 - x7 * 0.198912367f;
			x0 = xt - x6;
			xt = xt + x6;
			x[1] = (xt + x7) * 0.50979561f;
			x[2] = (x4 + x3) * 0.54119611f;
			x[3] = (x0 - x5) * 0.60134488f;
			x[5] = (x0 + x5) * 0.89997619f;
			x[6] = (x4 - x3) * 1.30656302f;
			x[7] = (xt - x7) * 2.56291556f;
		}
		// Only the lanes of the columns that exist are stored
		const int lanes = std::min(4, n - k);
		const auto store = [&](float* p, const F4& value) { std::copy_n(value.v.begin(), lanes, p); };
		for (int i = 0; i < 7; ++i, y += 4 * 18)
		{
			const F4 s = t[3][i] + t[3][i + 1];
			store(&y[0 * 18], t[0][i]);
			store(&y[1 * 18], t[2][i] + s);
			store(&y[2 * 18], t[1][i] + t[1][i + 1]);
			store(&y[3 * 18], t[2][1 + i] + s);
		}
		store(&y[0 * 18], t[0][7]);
		store(&y[1 * 18], t[2][7] + t[3][7]);
		store(&y[2 * 18], t[1][7]);
		store(&y[3 * 18], t[3][7]);
	}
}

/// Rounds half away from zero (the two edge samples of each block of 64)
int16_t ScalePcm(float sample) noexcept
{
	if (sample >= 32766.5f)
	{
		return 32767;
	}
	if (sample <= -32767.5f)
	{
		return -32768;
	}
	auto s = static_cast<int16_t>(static_cast<int32_t>(sample + .5f));
	s = static_cast<int16_t>(s - (s < 0 ? 1 : 0));
	return s;
}

/// Clamps then rounds half to even (the vector conversion of the other samples)
int16_t RoundPcm(float sample) noexcept
{
	return static_cast<int16_t>(std::nearbyint(std::max(std::min(sample, 32767.0f), -32768.0f)));
}

void SynthPair(int16_t* pcm, int channels, const float* z) noexcept
{
	float a = (z[14 * 64] - z[0]) * 29;
	a += (z[1 * 64] + z[13 * 64]) * 213;
	a += (z[12 * 64] - z[2 * 64]) * 459;
	a += (z[3 * 64] + z[11 * 64]) * 2037;
	a += (z[10 * 64] - z[4 * 64]) * 5153;
	a += (z[5 * 64] + z[9 * 64]) * 6574;
	a += (z[8 * 64] - z[6 * 64]) * 37489;
	a += z[7 * 64] * 75038;
	pcm[0] = ScalePcm(a);

	z += 2;
	a = z[14 * 64] * 104;
	a += z[12 * 64] * 1567;
	a += z[10 * 64] * 9727;
	a += z[8 * 64] * 64019;
	a += z[6 * 64] * -9975;
	a += z[4 * 64] * -45;
	a += z[2 * 64] * 146;
	a += z[0 * 64] * -5;
	pcm[16 * channels] = ScalePcm(a);
}

/// 64 samples of each channel from two columns of the DCT output; `lins` is the filter history slid by 64
void Synth(const float* xl, int16_t* dstl, int channels, float* lins) noexcept
{
	static constexpr float k_Window[] = {
	    -1, 26, -31, 208, 218, 401,  -519,  2063, 2000, 4788, -5517, 7134, 5959,  35640, -39336, 74992,
	    -1, 24, -35, 202, 222, 347,  -581,  2080, 1952, 4425, -5879, 7640, 5288,  33791, -41176, 74856,
	    -1, 21, -38, 196, 225, 294,  -645,  2087, 1893, 4063, -6237, 8092, 4561,  31947, -43006, 74630,
	    -1, 19, -41, 190, 227, 244,  -711,  2085, 1822, 3705, -6589, 8492, 3776,  30112, -44821, 74313,
	    -1, 17, -45, 183, 228, 197,  -779,  2075, 1739, 3351, -6935, 8840, 2935,  28289, -46617, 73908,
	    -1, 16, -49, 176, 228, 153,  -848,  2057, 1644, 3004, -7271, 9139, 2037,  26482, -48390, 73415,
	    -2, 14, -53, 169, 227, 111,  -919,  2032, 1535, 2663, -7597, 9389, 1082,  24694, -50137, 72835,
	    -2, 13, -58, 161, 224, 72,   -991,  2001, 1414, 2330, -7910, 9592, 70,    22929, -51853, 72169,
	    -2, 11, -63, 154, 221, 36,   -1064, 1962, 1280, 2006, -8209, 9750, -998,  21189, -53534, 71420,
	    -2, 10, -68, 147, 215, 2,    -1137, 1919, 1131, 1692, -8491, 9863, -2122, 19478, -55178, 70590,
	    -3, 9,  -73, 139, 208, -29,  -1210, 1870, 970,  1388, -8755, 9935, -3300, 17799, -56778, 69679,
	    -3, 8,  -79, 132, 200, -57,  -1283, 1817, 794,  1095, -8998, 9966, -4533, 16155, -58333, 68692,
	    -4, 7,  -85, 125, 189, -83,  -1356, 1759, 605,  814,  -9219, 9959, -5818, 14548, -59838, 67629,
	    -4, 7,  -91, 117, 177, -106, -1428, 1698, 402,  545,  -9416, 9916, -7154, 12980, -61289, 66494,
	    -5, 6,  -97, 111, 163, -127, -1498, 1634, 185,  288,  -9585, 9838, -8540, 11455, -62684, 65290,
	};
	const float* xr = xl + 576 * (channels - 1);
	int16_t* dstr = dstl + (channels - 1);
	float* zlin = lins + 15 * 64;
	const float* w = k_Window;

	zlin[4 * 15] = xl[18 * 16];
	zlin[4 * 15 + 1] = xr[18 * 16];
	zlin[4 * 15 + 2] = xl[0];
	zlin[4 * 15 + 3] = xr[0];

	zlin[4 * 31] = xl[1 + 18 * 16];
	zlin[4 * 31 + 1] = xr[1 + 18 * 16];
	zlin[4 * 31 + 2] = xl[1];
	zlin[4 * 31 + 3] = xr[1];

	SynthPair(dstr, channels, lins + 4 * 15 + 1);
	SynthPair(dstr + 32 * channels, channels, lins + 4 * 15 + 64 + 1);
	SynthPair(dstl, channels, lins + 4 * 15);
	SynthPair(dstl + 32 * channels, channels, lins + 4 * 15 + 64);

	for (int i = 14; i >= 0; --i)
	{
		zlin[4 * i] = xl[18 * (31 - i)];
		zlin[4 * i + 1] = xr[18 * (31 - i)];
		zlin[4 * i + 2] = xl[1 + 18 * (31 - i)];
		zlin[4 * i + 3] = xr[1 + 18 * (31 - i)];
		zlin[4 * i + 64] = xl[1 + 18 * (1 + i)];
		zlin[4 * i + 64 + 1] = xr[1 + 18 * (1 + i)];
		zlin[4 * i - 64 + 2] = xl[18 * (1 + i)];
		zlin[4 * i - 64 + 3] = xr[18 * (1 + i)];

		F4 a;
		F4 b;
		// k even: a += vz * w0 - vy * w1; k odd: a += vy * w1 - vz * w0; b += vz * w1 + vy * w0 always
		for (int k = 0; k < 8; ++k)
		{
			const F4 w0 = F4::Set(*w++);
			const F4 w1 = F4::Set(*w++);
			const F4 vz = F4::Load(&zlin[4 * i - 64 * k]);
			const F4 vy = F4::Load(&zlin[4 * i - 64 * (15 - k)]);
			const F4 bk = vz * w1 + vy * w0;
			const F4 ak = (k & 1) == 0 ? vz * w0 - vy * w1 : vy * w1 - vz * w0;
			b = k == 0 ? bk : b + bk;
			a = k == 0 ? ak : a + ak;
		}
		dstr[(15 - i) * channels] = RoundPcm(a.v[1]);
		dstr[(17 + i) * channels] = RoundPcm(b.v[1]);
		dstl[(15 - i) * channels] = RoundPcm(a.v[0]);
		dstl[(17 + i) * channels] = RoundPcm(b.v[0]);
		dstr[(47 - i) * channels] = RoundPcm(a.v[3]);
		dstr[(49 + i) * channels] = RoundPcm(b.v[3]);
		dstl[(47 - i) * channels] = RoundPcm(a.v[2]);
		dstl[(49 + i) * channels] = RoundPcm(b.v[2]);
	}
}

void SynthGranule(std::array<float, 15 * 64>& filter, float* granule, int bands, int channels, int16_t* pcm,
                  float* lins) noexcept
{
	for (int i = 0; i < channels; ++i)
	{
		DctII(granule + 576 * i, bands);
	}
	std::copy(filter.begin(), filter.end(), lins);
	for (int i = 0; i < bands; i += 2)
	{
		Synth(granule + i, pcm + 32 * channels * i, channels, lins + i * 64);
	}
	if (channels == 1)
	{
		// One channel keeps only every other value of the history (the reference decoder's standard behaviour)
		for (size_t i = 0; i < filter.size(); i += 2)
		{
			filter[i] = lins[bands * 64 + i];
		}
	}
	else
	{
		std::copy_n(lins + bands * 64, filter.size(), filter.begin());
	}
}

// ---- Layer III side information ------------------------------------------------------------------------------------

constexpr int k_ShortBlock = 2;
constexpr int k_StopBlock = 3;

bool IsMsStereo(Header h) noexcept
{
	return (h[3] & 0xE0) == 0x60;
}
bool HasIntensityStereo(Header h) noexcept
{
	return (h[3] & 0x10) != 0;
}
bool HasMsStereo(Header h) noexcept
{
	return (h[3] & 0x20) != 0;
}
/// The sample rate as one index over the versions: 0-2 MPEG-2.5, 3-5 MPEG-2, 6-8 MPEG-1
int RateOverVersions(Header h) noexcept
{
	return SampleRateIndex(h) + (((h[1] >> 3) & 1) + ((h[1] >> 4) & 1)) * 3;
}

/// The widths of the scale factor bands for each sample rate (MPEG-1, MPEG-2, MPEG-2.5 with the 8 kHz rate apart),
/// in samples, 0 after the last. Short blocks list each band three times, once a window
constexpr uint8_t k_LongBands[8][23] = {
    {6, 6, 6, 6, 6, 6, 8, 10, 12, 14, 16, 20, 24, 28, 32, 38, 46, 52, 60, 68, 58, 54, 0},
    {12, 12, 12, 12, 12, 12, 16, 20, 24, 28, 32, 40, 48, 56, 64, 76, 90, 2, 2, 2, 2, 2, 0},
    {6, 6, 6, 6, 6, 6, 8, 10, 12, 14, 16, 20, 24, 28, 32, 38, 46, 52, 60, 68, 58, 54, 0},
    {6, 6, 6, 6, 6, 6, 8, 10, 12, 14, 16, 18, 22, 26, 32, 38, 46, 54, 62, 70, 76, 36, 0},
    {6, 6, 6, 6, 6, 6, 8, 10, 12, 14, 16, 20, 24, 28, 32, 38, 46, 52, 60, 68, 58, 54, 0},
    {4, 4, 4, 4, 4, 4, 6, 6, 8, 8, 10, 12, 16, 20, 24, 28, 34, 42, 50, 54, 76, 158, 0},
    {4, 4, 4, 4, 4, 4, 6, 6, 6, 8, 10, 12, 16, 18, 22, 28, 34, 40, 46, 54, 54, 192, 0},
    {4, 4, 4, 4, 4, 4, 6, 6, 8, 10, 12, 16, 20, 24, 30, 38, 46, 56, 68, 84, 102, 26, 0},
};
constexpr uint8_t k_ShortBands[8][40] = {
    {4,  4,  4,  4,  4,  4,  4,  4,  4,  6,  6,  6,  8,  8,  8,  10, 10, 10, 12, 12,
     12, 14, 14, 14, 18, 18, 18, 24, 24, 24, 30, 30, 30, 40, 40, 40, 18, 18, 18, 0},
    {8,  8,  8,  8,  8,  8,  8,  8, 8, 12, 12, 12, 16, 16, 16, 20, 20, 20, 24, 24,
     24, 28, 28, 28, 36, 36, 36, 2, 2, 2,  2,  2,  2,  2,  2,  2,  26, 26, 26, 0},
    {4,  4,  4,  4,  4,  4,  4,  4,  4,  6,  6,  6,  6,  6,  6,  8,  8,  8,  10, 10,
     10, 14, 14, 14, 18, 18, 18, 26, 26, 26, 32, 32, 32, 42, 42, 42, 18, 18, 18, 0},
    {4,  4,  4,  4,  4,  4,  4,  4,  4,  6,  6,  6,  8,  8,  8,  10, 10, 10, 12, 12,
     12, 14, 14, 14, 18, 18, 18, 24, 24, 24, 32, 32, 32, 44, 44, 44, 12, 12, 12, 0},
    {4,  4,  4,  4,  4,  4,  4,  4,  4,  6,  6,  6,  8,  8,  8,  10, 10, 10, 12, 12,
     12, 14, 14, 14, 18, 18, 18, 24, 24, 24, 30, 30, 30, 40, 40, 40, 18, 18, 18, 0},
    {4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  6,  6,  6,  8,  8,  8,  10, 10,
     10, 12, 12, 12, 14, 14, 14, 18, 18, 18, 22, 22, 22, 30, 30, 30, 56, 56, 56, 0},
    {4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  6,  6,  6,  6,  6,  6,  10, 10,
     10, 12, 12, 12, 14, 14, 14, 16, 16, 16, 20, 20, 20, 26, 26, 26, 66, 66, 66, 0},
    {4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  4,  6,  6,  6,  8,  8,  8,  12, 12,
     12, 16, 16, 16, 20, 20, 20, 26, 26, 26, 34, 34, 34, 42, 42, 42, 12, 12, 12, 0},
};
constexpr uint8_t k_MixedBands[8][40] = {
    {6,  6,  6,  6,  6,  6,  6,  6,  6,  8,  8,  8,  10, 10, 10, 12, 12, 12, 14,
     14, 14, 18, 18, 18, 24, 24, 24, 30, 30, 30, 40, 40, 40, 18, 18, 18, 0},
    {12, 12, 12, 4,  4,  4,  8,  8, 8, 12, 12, 12, 16, 16, 16, 20, 20, 20, 24, 24,
     24, 28, 28, 28, 36, 36, 36, 2, 2, 2,  2,  2,  2,  2,  2,  2,  26, 26, 26, 0},
    {6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  6,  8,  8,  8,  10, 10, 10, 14,
     14, 14, 18, 18, 18, 26, 26, 26, 32, 32, 32, 42, 42, 42, 18, 18, 18, 0},
    {6,  6,  6,  6,  6,  6,  6,  6,  6,  8,  8,  8,  10, 10, 10, 12, 12, 12, 14,
     14, 14, 18, 18, 18, 24, 24, 24, 32, 32, 32, 44, 44, 44, 12, 12, 12, 0},
    {6,  6,  6,  6,  6,  6,  6,  6,  6,  8,  8,  8,  10, 10, 10, 12, 12, 12, 14,
     14, 14, 18, 18, 18, 24, 24, 24, 30, 30, 30, 40, 40, 40, 18, 18, 18, 0},
    {4,  4,  4,  4,  4,  4,  6,  6,  4,  4,  4,  6,  6,  6,  8,  8,  8,  10, 10, 10,
     12, 12, 12, 14, 14, 14, 18, 18, 18, 22, 22, 22, 30, 30, 30, 56, 56, 56, 0},
    {4,  4,  4,  4,  4,  4,  6,  6,  4,  4,  4,  6,  6,  6,  6,  6,  6,  10, 10, 10,
     12, 12, 12, 14, 14, 14, 16, 16, 16, 20, 20, 20, 26, 26, 26, 66, 66, 66, 0},
    {4,  4,  4,  4,  4,  4,  6,  6,  4,  4,  4,  6,  6,  6,  8,  8,  8,  12, 12, 12,
     16, 16, 16, 20, 20, 20, 26, 26, 26, 34, 34, 34, 42, 42, 42, 12, 12, 12, 0},
};

/// What one granule of one channel says about its samples
struct GranuleInfo
{
	const uint8_t* bandWidths {nullptr};
	uint16_t part23Length {0}; ///< the bits of its scale factors and samples
	uint16_t bigValues {0};    ///< the pairs of samples coded by the big tables
	uint16_t scalefacCompress {0};
	uint8_t globalGain {0};
	uint8_t blockType {0};
	uint8_t mixedBlock {0};
	uint8_t longBands {0};
	uint8_t shortBands {0};
	std::array<uint8_t, 3> tableSelect {};
	std::array<uint8_t, 3> regionCount {};
	std::array<uint8_t, 3> subblockGain {};
	uint8_t preflag {0};
	uint8_t scalefacScale {0};
	uint8_t count1Table {0};
	uint8_t scfsi {0};
};

/// The granules' side information (2 granules of each channel for MPEG-1, 1 for MPEG-2); returns where the main data
/// starts, in bytes back from this frame, or -1 when the frame cannot be decoded
int ReadSideInfo(BitStream& bits, GranuleInfo* granule, Header h) noexcept
{
	unsigned scfsi = 0;
	int mainDataBegin = 0;
	int part23Sum = 0;
	int count = IsMono(h) ? 1 : 2;
	int rate = RateOverVersions(h);
	rate -= rate != 0 ? 1 : 0;

	if (IsMpeg1(h))
	{
		count *= 2;
		mainDataBegin = static_cast<int>(bits.Read(9));
		scfsi = bits.Read(7 + count);
	}
	else
	{
		mainDataBegin = static_cast<int>(bits.Read(8 + count) >> count);
	}

	do
	{
		if (IsMono(h))
		{
			scfsi <<= 4;
		}
		GranuleInfo& gr = *granule;
		gr.part23Length = static_cast<uint16_t>(bits.Read(12));
		part23Sum += gr.part23Length;
		gr.bigValues = static_cast<uint16_t>(bits.Read(9));
		if (gr.bigValues > 288)
		{
			return -1;
		}
		gr.globalGain = static_cast<uint8_t>(bits.Read(8));
		gr.scalefacCompress = static_cast<uint16_t>(bits.Read(IsMpeg1(h) ? 4 : 9));
		gr.bandWidths = k_LongBands[rate];
		gr.longBands = 22;
		gr.shortBands = 0;
		unsigned tables = 0;
		if (bits.Read(1) != 0)
		{
			gr.blockType = static_cast<uint8_t>(bits.Read(2));
			if (gr.blockType == 0)
			{
				return -1;
			}
			gr.mixedBlock = static_cast<uint8_t>(bits.Read(1));
			gr.regionCount[0] = 7;
			gr.regionCount[1] = 255;
			if (gr.blockType == k_ShortBlock)
			{
				scfsi &= 0x0F0F;
				if (gr.mixedBlock == 0)
				{
					gr.regionCount[0] = 8;
					gr.bandWidths = k_ShortBands[rate];
					gr.longBands = 0;
					gr.shortBands = 39;
				}
				else
				{
					gr.bandWidths = k_MixedBands[rate];
					gr.longBands = IsMpeg1(h) ? 8 : 6;
					gr.shortBands = 30;
				}
			}
			tables = bits.Read(10);
			tables <<= 5;
			gr.subblockGain[0] = static_cast<uint8_t>(bits.Read(3));
			gr.subblockGain[1] = static_cast<uint8_t>(bits.Read(3));
			gr.subblockGain[2] = static_cast<uint8_t>(bits.Read(3));
		}
		else
		{
			gr.blockType = 0;
			gr.mixedBlock = 0;
			tables = bits.Read(15);
			gr.regionCount[0] = static_cast<uint8_t>(bits.Read(4));
			gr.regionCount[1] = static_cast<uint8_t>(bits.Read(3));
			gr.regionCount[2] = 255;
		}
		gr.tableSelect[0] = static_cast<uint8_t>(tables >> 10);
		gr.tableSelect[1] = static_cast<uint8_t>((tables >> 5) & 31);
		gr.tableSelect[2] = static_cast<uint8_t>(tables & 31);
		gr.preflag = static_cast<uint8_t>(IsMpeg1(h) ? bits.Read(1) : (gr.scalefacCompress >= 500 ? 1 : 0));
		gr.scalefacScale = static_cast<uint8_t>(bits.Read(1));
		gr.count1Table = static_cast<uint8_t>(bits.Read(1));
		gr.scfsi = static_cast<uint8_t>((scfsi >> 12) & 15);
		scfsi <<= 4;
		++granule;
	} while (--count != 0);

	if (part23Sum + bits.position > bits.limit + mainDataBegin * 8)
	{
		return -1;
	}
	return mainDataBegin;
}

// ---- Layer III scale factors ---------------------------------------------------------------------------------------

/// The scale factors of up to four partitions of bands, of `widths` bits each, or the previous granule's where the
/// selection information says they are shared. For MPEG-2 (scfsi < 0) the largest value of a partition also marks its
/// bands as having no intensity position
void ReadScalefactorValues(uint8_t* scf, uint8_t* istPos, const uint8_t* widths, const uint8_t* counts, BitStream& bits,
                           int scfsi) noexcept
{
	for (int i = 0; i < 4 && counts[i] != 0; ++i, scfsi *= 2)
	{
		const int count = counts[i];
		if ((scfsi & 8) != 0)
		{
			std::memcpy(scf, istPos, static_cast<size_t>(count));
		}
		else
		{
			const int width = widths[i];
			if (width == 0)
			{
				std::memset(scf, 0, static_cast<size_t>(count));
				std::memset(istPos, 0, static_cast<size_t>(count));
			}
			else
			{
				const int largest = scfsi < 0 ? (1 << width) - 1 : -1;
				for (int k = 0; k < count; ++k)
				{
					const int s = static_cast<int>(bits.Read(width));
					istPos[k] = static_cast<uint8_t>(s == largest ? -1 : s);
					scf[k] = static_cast<uint8_t>(s);
				}
			}
		}
		istPos += count;
		scf += count;
	}
	scf[0] = scf[1] = scf[2] = 0;
}

/// y * 2^(-exponent / 4), in steps of at most 2^-30
float ScaleByQuarterPowers(float y, int exponent) noexcept
{
	static constexpr float k_Fractions[4] = {9.31322575e-10f, 7.83145814e-10f, 6.58544508e-10f, 5.53767716e-10f};
	int e = 0;
	do
	{
		e = std::min(30 * 4, exponent);
		y *= k_Fractions[e & 3] * static_cast<float>(1 << 30 >> (e >> 2));
	} while ((exponent -= e) > 0);
	return y;
}

/// Each band's gain: the global gain, the band's scale factor and the boosts of short windows and high bands
void DecodeScalefactors(Header h, uint8_t* istPos, BitStream& bits, const GranuleInfo& gr, float* scf, int channel) noexcept
{
	static constexpr uint8_t k_Partitions[3][28] = {
	    {6, 5, 5, 5, 6, 5, 5, 5, 6, 5, 7, 3, 11, 10, 0, 0, 7, 7, 7, 0, 6, 6, 6, 3, 8, 8, 5, 0},
	    {8, 9, 6, 12, 6, 9, 9, 9, 6, 9, 12, 6, 15, 18, 0, 0, 6, 15, 12, 0, 6, 12, 9, 6, 6, 18, 9, 0},
	    {9, 9, 6, 12, 9, 9, 9, 9, 9, 9, 12, 6, 18, 18, 0, 0, 12, 12, 12, 0, 12, 9, 9, 6, 15, 12, 9, 0},
	};
	const uint8_t* partition = k_Partitions[(gr.shortBands != 0 ? 1 : 0) + (gr.longBands == 0 ? 1 : 0)];
	std::array<uint8_t, 4> widths {};
	std::array<uint8_t, 40> values {};
	const int shift = gr.scalefacScale + 1;
	int scfsi = gr.scfsi;

	if (IsMpeg1(h))
	{
		static constexpr uint8_t k_Widths[16] = {0, 1, 2, 3, 12, 5, 6, 7, 9, 10, 11, 13, 14, 15, 18, 19};
		const int part = k_Widths[gr.scalefacCompress];
		widths[1] = widths[0] = static_cast<uint8_t>(part >> 2);
		widths[3] = widths[2] = static_cast<uint8_t>(part & 3);
	}
	else
	{
		static constexpr uint8_t k_Moduli[6 * 4] = {5, 5, 4, 4, 5, 5, 4, 1, 4, 3, 1, 1, 5, 6, 6, 1, 4, 4, 4, 1, 4, 3, 1, 1};
		const int intensity = HasIntensityStereo(h) && channel != 0 ? 1 : 0;
		int sfc = gr.scalefacCompress >> intensity;
		int k = intensity * 3 * 4;
		for (int product = 1; sfc >= 0; sfc -= product, k += 4)
		{
			product = 1;
			for (int i = 3; i >= 0; --i)
			{
				widths[static_cast<size_t>(i)] = static_cast<uint8_t>(sfc / product % k_Moduli[k + i]);
				product *= k_Moduli[k + i];
			}
		}
		partition += k;
		scfsi = -16;
	}
	ReadScalefactorValues(values.data(), istPos, widths.data(), partition, bits, scfsi);

	if (gr.shortBands != 0)
	{
		const int subblockShift = 3 - shift;
		for (int i = 0; i < gr.shortBands; i += 3)
		{
			for (int w = 0; w < 3; ++w)
			{
				auto& v = values[static_cast<size_t>(gr.longBands + i + w)];
				v = static_cast<uint8_t>(v + (gr.subblockGain[static_cast<size_t>(w)] << subblockShift));
			}
		}
	}
	else if (gr.preflag != 0)
	{
		static constexpr uint8_t k_Preamp[10] = {1, 1, 1, 1, 2, 2, 3, 3, 3, 2};
		for (int i = 0; i < 10; ++i)
		{
			values[static_cast<size_t>(11 + i)] = static_cast<uint8_t>(values[static_cast<size_t>(11 + i)] + k_Preamp[i]);
		}
	}

	// The samples come out at 2^-1 of full scale; the largest gain exponent is 44 quarter steps
	constexpr int k_DequantiserOut = -1;
	constexpr int k_MaxGain = ((255 + k_DequantiserOut * 4 - 210) + 3) & ~3;
	const int gainExponent = gr.globalGain + k_DequantiserOut * 4 - 210 - (IsMsStereo(h) ? 2 : 0);
	const float gain = ScaleByQuarterPowers(static_cast<float>(1 << (k_MaxGain / 4)), k_MaxGain - gainExponent);
	for (int i = 0; i < gr.longBands + gr.shortBands; ++i)
	{
		scf[i] = ScaleByQuarterPowers(gain, values[static_cast<size_t>(i)] << shift);
	}
}

// ---- Layer III samples ---------------------------------------------------------------------------------------------

/// x^(4/3) for -15..128 (negative at 0..15), then by interpolation
constexpr float k_Pow43[129 + 16] = {
    0,           -1,          -2.519842f,  -4.326749f,  -6.349604f,  -8.549880f,  -10.902724f, -13.390518f, -16.000000f,
    -18.720754f, -21.544347f, -24.463781f, -27.473142f, -30.567351f, -33.741992f, -36.993181f, 0,           1,
    2.519842f,   4.326749f,   6.349604f,   8.549880f,   10.902724f,  13.390518f,  16.000000f,  18.720754f,  21.544347f,
    24.463781f,  27.473142f,  30.567351f,  33.741992f,  36.993181f,  40.317474f,  43.711787f,  47.173345f,  50.699631f,
    54.288352f,  57.937408f,  61.644865f,  65.408941f,  69.227979f,  73.100443f,  77.024898f,  81.000000f,  85.024491f,
    89.097188f,  93.216975f,  97.382800f,  101.593667f, 105.848633f, 110.146801f, 114.487321f, 118.869381f, 123.292209f,
    127.755065f, 132.257246f, 136.798076f, 141.376907f, 145.993119f, 150.646117f, 155.335327f, 160.060199f, 164.820202f,
    169.614826f, 174.443577f, 179.305980f, 184.201575f, 189.129918f, 194.090580f, 199.083145f, 204.107210f, 209.162385f,
    214.248292f, 219.364564f, 224.510845f, 229.686789f, 234.892058f, 240.126328f, 245.389280f, 250.680604f, 256.000000f,
    261.347174f, 266.721841f, 272.123723f, 277.552547f, 283.008049f, 288.489971f, 293.998060f, 299.532071f, 305.091761f,
    310.676898f, 316.287249f, 321.922592f, 327.582707f, 333.267377f, 338.976394f, 344.709550f, 350.466646f, 356.247482f,
    362.051866f, 367.879608f, 373.730522f, 379.604427f, 385.501143f, 391.420496f, 397.362314f, 403.326427f, 409.312672f,
    415.320884f, 421.350905f, 427.402579f, 433.475750f, 439.570269f, 445.685987f, 451.822757f, 457.980436f, 464.158883f,
    470.357960f, 476.577530f, 482.817459f, 489.077615f, 495.357868f, 501.658090f, 507.978156f, 514.317941f, 520.677324f,
    527.056184f, 533.454404f, 539.871867f, 546.308458f, 552.764065f, 559.238575f, 565.731879f, 572.243870f, 578.774440f,
    585.323483f, 591.890898f, 598.476581f, 605.080431f, 611.702349f, 618.342238f, 625.000000f, 631.675540f, 638.368763f,
    645.079578f,
};

float Pow43(int x) noexcept
{
	if (x < 129)
	{
		return k_Pow43[16 + x];
	}
	int mult = 256;
	if (x < 1024)
	{
		mult = 16;
		x <<= 3;
	}
	const int sign = 2 * x & 64;
	const float frac = static_cast<float>((x & 63) - sign) / static_cast<float>((x & ~63) + sign);
	return k_Pow43[16 + ((x + sign) >> 6)] * (1.f + frac * ((4.f / 3) + frac * (2.f / 9))) * static_cast<float>(mult);
}

// The format's 32 Huffman tables as lookup trees: a leaf holds its code's length (bits 8 up) and its two values (4 bits
// each); a node (negative) holds the bits that index its subtree (low 3 bits) and the subtree's offset (the rest)
constexpr int16_t k_HuffmanTrees[] = {
    0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
    0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,     785,   785,
    785,   785,   784,   784,   784,   784,   513,   513,   513,   513,   513,   513,   513,   513,   256,   256,   256,
    256,   256,   256,   256,   256,   256,   256,   256,   256,   256,   256,   256,   256,   -255,  1313,  1298,  1282,
    785,   785,   785,   785,   784,   784,   784,   784,   769,   769,   769,   769,   256,   256,   256,   256,   256,
    256,   256,   256,   256,   256,   256,   256,   256,   256,   256,   256,   290,   288,   -255,  1313,  1298,  1282,
    769,   769,   769,   769,   529,   529,   529,   529,   529,   529,   529,   529,   528,   528,   528,   528,   528,
    528,   528,   528,   512,   512,   512,   512,   512,   512,   512,   512,   290,   288,   -253,  -318,  -351,  -367,
    785,   785,   785,   785,   784,   784,   784,   784,   769,   769,   769,   769,   256,   256,   256,   256,   256,
    256,   256,   256,   256,   256,   256,   256,   256,   256,   256,   256,   819,   818,   547,   547,   275,   275,
    275,   275,   561,   560,   515,   546,   289,   274,   288,   258,   -254,  -287,  1329,  1299,  1314,  1312,  1057,
    1057,  1042,  1042,  1026,  1026,  784,   784,   784,   784,   529,   529,   529,   529,   529,   529,   529,   529,
    769,   769,   769,   769,   768,   768,   768,   768,   563,   560,   306,   306,   291,   259,   -252,  -413,  -477,
    -542,  1298,  -575,  1041,  1041,  784,   784,   784,   784,   769,   769,   769,   769,   256,   256,   256,   256,
    256,   256,   256,   256,   256,   256,   256,   256,   256,   256,   256,   256,   -383,  -399,  1107,  1092,  1106,
    1061,  849,   849,   789,   789,   1104,  1091,  773,   773,   1076,  1075,  341,   340,   325,   309,   834,   804,
    577,   577,   532,   532,   516,   516,   832,   818,   803,   816,   561,   561,   531,   531,   515,   546,   289,
    289,   288,   258,   -252,  -429,  -493,  -559,  1057,  1057,  1042,  1042,  529,   529,   529,   529,   529,   529,
    529,   529,   784,   784,   784,   784,   769,   769,   769,   769,   512,   512,   512,   512,   512,   512,   512,
    512,   -382,  1077,  -415,  1106,  1061,  1104,  849,   849,   789,   789,   1091,  1076,  1029,  1075,  834,   834,
    597,   581,   340,   340,   339,   324,   804,   833,   532,   532,   832,   772,   818,   803,   817,   787,   816,
    771,   290,   290,   290,   290,   288,   258,   -253,  -349,  -414,  -447,  -463,  1329,  1299,  -479,  1314,  1312,
    1057,  1057,  1042,  1042,  1026,  1026,  785,   785,   785,   785,   784,   784,   784,   784,   769,   769,   769,
    769,   768,   768,   768,   768,   -319,  851,   821,   -335,  836,   850,   805,   849,   341,   340,   325,   336,
    533,   533,   579,   579,   564,   564,   773,   832,   578,   548,   563,   516,   321,   276,   306,   291,   304,
    259,   -251,  -572,  -733,  -830,  -863,  -879,  1041,  1041,  784,   784,   784,   784,   769,   769,   769,   769,
    256,   256,   256,   256,   256,   256,   256,   256,   256,   256,   256,   256,   256,   256,   256,   256,   -511,
    -527,  -543,  1396,  1351,  1381,  1366,  1395,  1335,  1380,  -559,  1334,  1138,  1138,  1063,  1063,  1350,  1392,
    1031,  1031,  1062,  1062,  1364,  1363,  1120,  1120,  1333,  1348,  881,   881,   881,   881,   375,   374,   359,
    373,   343,   358,   341,   325,   791,   791,   1123,  1122,  -703,  1105,  1045,  -719,  865,   865,   790,   790,
    774,   774,   1104,  1029,  338,   293,   323,   308,   -799,  -815,  833,   788,   772,   818,   803,   816,   322,
    292,   307,   320,   561,   531,   515,   546,   289,   274,   288,   258,   -251,  -525,  -605,  -685,  -765,  -831,
    -846,  1298,  1057,  1057,  1312,  1282,  785,   785,   785,   785,   784,   784,   784,   784,   769,   769,   769,
    769,   512,   512,   512,   512,   512,   512,   512,   512,   1399,  1398,  1383,  1367,  1382,  1396,  1351,  -511,
    1381,  1366,  1139,  1139,  1079,  1079,  1124,  1124,  1364,  1349,  1363,  1333,  882,   882,   882,   882,   807,
    807,   807,   807,   1094,  1094,  1136,  1136,  373,   341,   535,   535,   881,   775,   867,   822,   774,   -591,
    324,   338,   -671,  849,   550,   550,   866,   864,   609,   609,   293,   336,   534,   534,   789,   835,   773,
    -751,  834,   804,   308,   307,   833,   788,   832,   772,   562,   562,   547,   547,   305,   275,   560,   515,
    290,   290,   -252,  -397,  -477,  -557,  -622,  -653,  -719,  -735,  -750,  1329,  1299,  1314,  1057,  1057,  1042,
    1042,  1312,  1282,  1024,  1024,  785,   785,   785,   785,   784,   784,   784,   784,   769,   769,   769,   769,
    -383,  1127,  1141,  1111,  1126,  1140,  1095,  1110,  869,   869,   883,   883,   1079,  1109,  882,   882,   375,
    374,   807,   868,   838,   881,   791,   -463,  867,   822,   368,   263,   852,   837,   836,   -543,  610,   610,
    550,   550,   352,   336,   534,   534,   865,   774,   851,   821,   850,   805,   593,   533,   579,   564,   773,
    832,   578,   578,   548,   548,   577,   577,   307,   276,   306,   291,   516,   560,   259,   259,   -250,  -2107,
    -2507, -2764, -2909, -2974, -3007, -3023, 1041,  1041,  1040,  1040,  769,   769,   769,   769,   256,   256,   256,
    256,   256,   256,   256,   256,   256,   256,   256,   256,   256,   256,   256,   256,   -767,  -1052, -1213, -1277,
    -1358, -1405, -1469, -1535, -1550, -1582, -1614, -1647, -1662, -1694, -1726, -1759, -1774, -1807, -1822, -1854, -1886,
    1565,  -1919, -1935, -1951, -1967, 1731,  1730,  1580,  1717,  -1983, 1729,  1564,  -1999, 1548,  -2015, -2031, 1715,
    1595,  -2047, 1714,  -2063, 1610,  -2079, 1609,  -2095, 1323,  1323,  1457,  1457,  1307,  1307,  1712,  1547,  1641,
    1700,  1699,  1594,  1685,  1625,  1442,  1442,  1322,  1322,  -780,  -973,  -910,  1279,  1278,  1277,  1262,  1276,
    1261,  1275,  1215,  1260,  1229,  -959,  974,   974,   989,   989,   -943,  735,   478,   478,   495,   463,   506,
    414,   -1039, 1003,  958,   1017,  927,   942,   987,   957,   431,   476,   1272,  1167,  1228,  -1183, 1256,  -1199,
    895,   895,   941,   941,   1242,  1227,  1212,  1135,  1014,  1014,  490,   489,   503,   487,   910,   1013,  985,
    925,   863,   894,   970,   955,   1012,  847,   -1343, 831,   755,   755,   984,   909,   428,   366,   754,   559,
    -1391, 752,   486,   457,   924,   997,   698,   698,   983,   893,   740,   740,   908,   877,   739,   739,   667,
    667,   953,   938,   497,   287,   271,   271,   683,   606,   590,   712,   726,   574,   302,   302,   738,   736,
    481,   286,   526,   725,   605,   711,   636,   724,   696,   651,   589,   681,   666,   710,   364,   467,   573,
    695,   466,   466,   301,   465,   379,   379,   709,   604,   665,   679,   316,   316,   634,   633,   436,   436,
    464,   269,   424,   394,   452,   332,   438,   363,   347,   408,   393,   448,   331,   422,   362,   407,   392,
    421,   346,   406,   391,   376,   375,   359,   1441,  1306,  -2367, 1290,  -2383, 1337,  -2399, -2415, 1426,  1321,
    -2431, 1411,  1336,  -2447, -2463, -2479, 1169,  1169,  1049,  1049,  1424,  1289,  1412,  1352,  1319,  -2495, 1154,
    1154,  1064,  1064,  1153,  1153,  416,   390,   360,   404,   403,   389,   344,   374,   373,   343,   358,   372,
    327,   357,   342,   311,   356,   326,   1395,  1394,  1137,  1137,  1047,  1047,  1365,  1392,  1287,  1379,  1334,
    1364,  1349,  1378,  1318,  1363,  792,   792,   792,   792,   1152,  1152,  1032,  1032,  1121,  1121,  1046,  1046,
    1120,  1120,  1030,  1030,  -2895, 1106,  1061,  1104,  849,   849,   789,   789,   1091,  1076,  1029,  1090,  1060,
    1075,  833,   833,   309,   324,   532,   532,   832,   772,   818,   803,   561,   561,   531,   560,   515,   546,
    289,   274,   288,   258,   -250,  -1179, -1579, -1836, -1996, -2124, -2253, -2333, -2413, -2477, -2542, -2574, -2607,
    -2622, -2655, 1314,  1313,  1298,  1312,  1282,  785,   785,   785,   785,   1040,  1040,  1025,  1025,  768,   768,
    768,   768,   -766,  -798,  -830,  -862,  -895,  -911,  -927,  -943,  -959,  -975,  -991,  -1007, -1023, -1039, -1055,
    -1070, 1724,  1647,  -1103, -1119, 1631,  1767,  1662,  1738,  1708,  1723,  -1135, 1780,  1615,  1779,  1599,  1677,
    1646,  1778,  1583,  -1151, 1777,  1567,  1737,  1692,  1765,  1722,  1707,  1630,  1751,  1661,  1764,  1614,  1736,
    1676,  1763,  1750,  1645,  1598,  1721,  1691,  1762,  1706,  1582,  1761,  1566,  -1167, 1749,  1629,  767,   766,
    751,   765,   494,   494,   735,   764,   719,   749,   734,   763,   447,   447,   748,   718,   477,   506,   431,
    491,   446,   476,   461,   505,   415,   430,   475,   445,   504,   399,   460,   489,   414,   503,   383,   474,
    429,   459,   502,   502,   746,   752,   488,   398,   501,   473,   413,   472,   486,   271,   480,   270,   -1439,
    -1455, 1357,  -1471, -1487, -1503, 1341,  1325,  -1519, 1489,  1463,  1403,  1309,  -1535, 1372,  1448,  1418,  1476,
    1356,  1462,  1387,  -1551, 1475,  1340,  1447,  1402,  1386,  -1567, 1068,  1068,  1474,  1461,  455,   380,   468,
    440,   395,   425,   410,   454,   364,   467,   466,   464,   453,   269,   409,   448,   268,   432,   1371,  1473,
    1432,  1417,  1308,  1460,  1355,  1446,  1459,  1431,  1083,  1083,  1401,  1416,  1458,  1445,  1067,  1067,  1370,
    1457,  1051,  1051,  1291,  1430,  1385,  1444,  1354,  1415,  1400,  1443,  1082,  1082,  1173,  1113,  1186,  1066,
    1185,  1050,  -1967, 1158,  1128,  1172,  1097,  1171,  1081,  -1983, 1157,  1112,  416,   266,   375,   400,   1170,
    1142,  1127,  1065,  793,   793,   1169,  1033,  1156,  1096,  1141,  1111,  1155,  1080,  1126,  1140,  898,   898,
    808,   808,   897,   897,   792,   792,   1095,  1152,  1032,  1125,  1110,  1139,  1079,  1124,  882,   807,   838,
    881,   853,   791,   -2319, 867,   368,   263,   822,   852,   837,   866,   806,   865,   -2399, 851,   352,   262,
    534,   534,   821,   836,   594,   594,   549,   549,   593,   593,   533,   533,   848,   773,   579,   579,   564,
    578,   548,   563,   276,   276,   577,   576,   306,   291,   516,   560,   305,   305,   275,   259,   -251,  -892,
    -2058, -2620, -2828, -2957, -3023, -3039, 1041,  1041,  1040,  1040,  769,   769,   769,   769,   256,   256,   256,
    256,   256,   256,   256,   256,   256,   256,   256,   256,   256,   256,   256,   256,   -511,  -527,  -543,  -559,
    1530,  -575,  -591,  1528,  1527,  1407,  1526,  1391,  1023,  1023,  1023,  1023,  1525,  1375,  1268,  1268,  1103,
    1103,  1087,  1087,  1039,  1039,  1523,  -604,  815,   815,   815,   815,   510,   495,   509,   479,   508,   463,
    507,   447,   431,   505,   415,   399,   -734,  -782,  1262,  -815,  1259,  1244,  -831,  1258,  1228,  -847,  -863,
    1196,  -879,  1253,  987,   987,   748,   -767,  493,   493,   462,   477,   414,   414,   686,   669,   478,   446,
    461,   445,   474,   429,   487,   458,   412,   471,   1266,  1264,  1009,  1009,  799,   799,   -1019, -1276, -1452,
    -1581, -1677, -1757, -1821, -1886, -1933, -1997, 1257,  1257,  1483,  1468,  1512,  1422,  1497,  1406,  1467,  1496,
    1421,  1510,  1134,  1134,  1225,  1225,  1466,  1451,  1374,  1405,  1252,  1252,  1358,  1480,  1164,  1164,  1251,
    1251,  1238,  1238,  1389,  1465,  -1407, 1054,  1101,  -1423, 1207,  -1439, 830,   830,   1248,  1038,  1237,  1117,
    1223,  1148,  1236,  1208,  411,   426,   395,   410,   379,   269,   1193,  1222,  1132,  1235,  1221,  1116,  976,
    976,   1192,  1162,  1177,  1220,  1131,  1191,  963,   963,   -1647, 961,   780,   -1663, 558,   558,   994,   993,
    437,   408,   393,   407,   829,   978,   813,   797,   947,   -1743, 721,   721,   377,   392,   844,   950,   828,
    890,   706,   706,   812,   859,   796,   960,   948,   843,   934,   874,   571,   571,   -1919, 690,   555,   689,
    421,   346,   539,   539,   944,   779,   918,   873,   932,   842,   903,   888,   570,   570,   931,   917,   674,
    674,   -2575, 1562,  -2591, 1609,  -2607, 1654,  1322,  1322,  1441,  1441,  1696,  1546,  1683,  1593,  1669,  1624,
    1426,  1426,  1321,  1321,  1639,  1680,  1425,  1425,  1305,  1305,  1545,  1668,  1608,  1623,  1667,  1592,  1638,
    1666,  1320,  1320,  1652,  1607,  1409,  1409,  1304,  1304,  1288,  1288,  1664,  1637,  1395,  1395,  1335,  1335,
    1622,  1636,  1394,  1394,  1319,  1319,  1606,  1621,  1392,  1392,  1137,  1137,  1137,  1137,  345,   390,   360,
    375,   404,   373,   1047,  -2751, -2767, -2783, 1062,  1121,  1046,  -2799, 1077,  -2815, 1106,  1061,  789,   789,
    1105,  1104,  263,   355,   310,   340,   325,   354,   352,   262,   339,   324,   1091,  1076,  1029,  1090,  1060,
    1075,  833,   833,   788,   788,   1088,  1028,  818,   818,   803,   803,   561,   561,   531,   531,   816,   771,
    546,   546,   289,   274,   288,   258,   -253,  -317,  -381,  -446,  -478,  -509,  1279,  1279,  -811,  -1179, -1451,
    -1756, -1900, -2028, -2189, -2253, -2333, -2414, -2445, -2511, -2526, 1313,  1298,  -2559, 1041,  1041,  1040,  1040,
    1025,  1025,  1024,  1024,  1022,  1007,  1021,  991,   1020,  975,   1019,  959,   687,   687,   1018,  1017,  671,
    671,   655,   655,   1016,  1015,  639,   639,   758,   758,   623,   623,   757,   607,   756,   591,   755,   575,
    754,   559,   543,   543,   1009,  783,   -575,  -621,  -685,  -749,  496,   -590,  750,   749,   734,   748,   974,
    989,   1003,  958,   988,   973,   1002,  942,   987,   957,   972,   1001,  926,   986,   941,   971,   956,   1000,
    910,   985,   925,   999,   894,   970,   -1071, -1087, -1102, 1390,  -1135, 1436,  1509,  1451,  1374,  -1151, 1405,
    1358,  1480,  1420,  -1167, 1507,  1494,  1389,  1342,  1465,  1435,  1450,  1326,  1505,  1310,  1493,  1373,  1479,
    1404,  1492,  1464,  1419,  428,   443,   472,   397,   736,   526,   464,   464,   486,   457,   442,   471,   484,
    482,   1357,  1449,  1434,  1478,  1388,  1491,  1341,  1490,  1325,  1489,  1463,  1403,  1309,  1477,  1372,  1448,
    1418,  1433,  1476,  1356,  1462,  1387,  -1439, 1475,  1340,  1447,  1402,  1474,  1324,  1461,  1371,  1473,  269,
    448,   1432,  1417,  1308,  1460,  -1711, 1459,  -1727, 1441,  1099,  1099,  1446,  1386,  1431,  1401,  -1743, 1289,
    1083,  1083,  1160,  1160,  1458,  1445,  1067,  1067,  1370,  1457,  1307,  1430,  1129,  1129,  1098,  1098,  268,
    432,   267,   416,   266,   400,   -1887, 1144,  1187,  1082,  1173,  1113,  1186,  1066,  1050,  1158,  1128,  1143,
    1172,  1097,  1171,  1081,  420,   391,   1157,  1112,  1170,  1142,  1127,  1065,  1169,  1049,  1156,  1096,  1141,
    1111,  1155,  1080,  1126,  1154,  1064,  1153,  1140,  1095,  1048,  -2159, 1125,  1110,  1137,  -2175, 823,   823,
    1139,  1138,  807,   807,   384,   264,   368,   263,   868,   838,   853,   791,   867,   822,   852,   837,   866,
    806,   865,   790,   -2319, 851,   821,   836,   352,   262,   850,   805,   849,   -2399, 533,   533,   835,   820,
    336,   261,   578,   548,   563,   577,   532,   532,   832,   772,   562,   562,   547,   547,   305,   275,   560,
    515,   290,   290,   288,   258,
};
constexpr uint8_t k_Count1TreeA[] = {130, 162, 193, 209, 44,  28,  76,  140, 9,   9,   9,   9,  9,   9,
                                     9,   9,   190, 254, 222, 238, 126, 94,  157, 157, 109, 61, 173, 205};
constexpr uint8_t k_Count1TreeB[] = {252, 236, 220, 204, 188, 172, 156, 140, 124, 108, 92, 76, 60, 44, 28, 12};
constexpr int16_t k_TreeStart[2 * 16] = {0,    32,   64,   98,   0,    132,  180,  218,  292,  364,  426,
                                         538,  648,  746,  0,    1126, 1460, 1460, 1460, 1460, 1460, 1460,
                                         1460, 1460, 1842, 1842, 1842, 1842, 1842, 1842, 1842, 1842};
constexpr uint8_t k_LinBits[32] = {0, 0, 0, 0, 0, 0, 0,  0,  0, 0, 0, 0, 0, 0, 0,  0,
                                   1, 2, 3, 4, 6, 8, 10, 13, 4, 5, 6, 7, 8, 9, 11, 13};

/// A 32-bit window over the bits, refilled a byte at a time (0 past the buffer)
class HuffmanBits
{
public:
	HuffmanBits(const BitStream& bits, int capacity) noexcept
	    : _data(bits.data)
	    , _capacity(capacity)
	    , _next(bits.position / 8)
	    , _shift((bits.position & 7) - 8)
	{
		_cache = (((Byte(_next) * 256u + Byte(_next + 1)) * 256u + Byte(_next + 2)) * 256u + Byte(_next + 3))
		         << (bits.position & 7);
		_next += 4;
	}

	[[nodiscard]] uint32_t Peek(int n) const noexcept { return _cache >> (32 - n); }
	[[nodiscard]] bool SignBit() const noexcept { return static_cast<int32_t>(_cache) < 0; }
	[[nodiscard]] uint32_t Cache() const noexcept { return _cache; }
	void Flush(int n) noexcept
	{
		_cache <<= n;
		_shift += n;
	}
	void Refill() noexcept
	{
		while (_shift >= 0)
		{
			_cache |= Byte(_next++) << _shift;
			_shift -= 8;
		}
	}
	/// The position of the next unread bit
	[[nodiscard]] int Position() const noexcept { return _next * 8 - 24 + _shift; }

private:
	[[nodiscard]] uint32_t Byte(int i) const noexcept { return i < _capacity ? _data[i] : 0u; }

	const uint8_t* _data;
	int _capacity;
	int _next;
	int _shift;
	uint32_t _cache {0};
};

/// The granule's 576 samples of one channel: pairs from the big tables region by region, then quadruples of -1, 0, 1
/// up to `limit`; each scaled by its band's gain
void ReadSamples(float* dst, BitStream& bits, int capacity, const GranuleInfo& gr, const float* scf, int limit) noexcept
{
	HuffmanBits in(bits, capacity);
	float one = 0.0f;
	int region = 0;
	int bigValues = gr.bigValues;
	int pairs = 0;
	const uint8_t* band = gr.bandWidths;

	while (bigValues > 0)
	{
		const int table = gr.tableSelect[static_cast<size_t>(region)];
		int bandsLeft = gr.regionCount[static_cast<size_t>(region++)];
		const int16_t* tree = k_HuffmanTrees + k_TreeStart[table];
		const int linbits = k_LinBits[table];
		do
		{
			pairs = *band++ / 2;
			int left = std::min(bigValues, pairs);
			one = *scf++;
			do
			{
				int width = 5;
				int leaf = tree[in.Peek(width)];
				while (leaf < 0)
				{
					in.Flush(width);
					width = leaf & 7;
					leaf = tree[in.Peek(width) - static_cast<uint32_t>(leaf >> 3)];
				}
				in.Flush(leaf >> 8);
				for (int j = 0; j < 2; ++j, ++dst, leaf >>= 4)
				{
					int value = leaf & 0x0F;
					if (linbits != 0 && value == 15)
					{
						// The largest value carries on in `linbits` more bits
						value += static_cast<int>(in.Peek(linbits));
						in.Flush(linbits);
						in.Refill();
						*dst = one * Pow43(value) * static_cast<float>(in.SignBit() ? -1 : 1);
					}
					else
					{
						*dst = k_Pow43[16 + value - 16 * static_cast<int>(in.Cache() >> 31)] * one;
					}
					in.Flush(value != 0 ? 1 : 0);
				}
				in.Refill();
			} while (--left != 0);
		} while ((bigValues -= pairs) > 0 && --bandsLeft >= 0);
	}

	// Counts the pairs left in the band; past its last band the granule ends
	pairs = 1 - bigValues;
	const auto nextPair = [&]() {
		if (--pairs == 0)
		{
			pairs = *band++ / 2;
			if (pairs == 0)
			{
				return false;
			}
			one = *scf++;
		}
		return true;
	};
	const auto quad = [&](int leaf, int s) {
		if ((leaf & (128 >> s)) != 0)
		{
			dst[s] = in.SignBit() ? -one : one;
			in.Flush(1);
		}
	};
	const uint8_t* tree = gr.count1Table != 0 ? k_Count1TreeB : k_Count1TreeA;
	for (;; dst += 4)
	{
		int leaf = tree[in.Peek(4)];
		if ((leaf & 8) == 0)
		{
			leaf = tree[(leaf >> 3) + static_cast<int>(in.Cache() << 4 >> (32 - (leaf & 3)))];
		}
		in.Flush(leaf & 7);
		if (in.Position() > limit)
		{
			break;
		}
		if (!nextPair())
		{
			break;
		}
		quad(leaf, 0);
		quad(leaf, 1);
		if (!nextPair())
		{
			break;
		}
		quad(leaf, 2);
		quad(leaf, 3);
		in.Refill();
	}
	bits.position = limit;
}

// ---- Layer III stereo ----------------------------------------------------------------------------------------------

/// Middle and side to left and right (the right channel 576 after the left)
void MidSideStereo(float* left, int n) noexcept
{
	float* right = left + 576;
	for (int i = 0; i < n; ++i)
	{
		const float a = left[i];
		const float b = right[i];
		left[i] = a + b;
		right[i] = a - b;
	}
}

void IntensityStereoBand(float* left, int n, float kl, float kr) noexcept
{
	for (int i = 0; i < n; ++i)
	{
		left[i + 576] = left[i] * kr;
		left[i] = left[i] * kl;
	}
}

/// The last band of each window (or of all three) where the right channel has a sample
void StereoTopBand(const float* right, const uint8_t* widths, int bands, std::array<int, 3>& top) noexcept
{
	top = {-1, -1, -1};
	for (int i = 0; i < bands; ++i)
	{
		for (int k = 0; k < widths[i]; k += 2)
		{
			if (right[k] != 0 || right[k + 1] != 0)
			{
				top[static_cast<size_t>(i % 3)] = i;
				break;
			}
		}
		right += widths[i];
	}
}

/// Above the right channel's top band, the bands with an intensity position pan the left channel into both
void StereoProcess(float* left, const uint8_t* istPos, const uint8_t* widths, Header h, const std::array<int, 3>& top,
                   int mpeg2Shift) noexcept
{
	static constexpr float k_Pan[7 * 2] = {0,    1,           0.21132487f, 0.78867513f, 0.36602540f, 0.63397460f, 0.5f,
	                                       0.5f, 0.63397460f, 0.36602540f, 0.78867513f, 0.21132487f, 1,           0};
	const unsigned maxPosition = IsMpeg1(h) ? 7 : 64;
	for (unsigned i = 0; widths[i] != 0; ++i)
	{
		const unsigned position = istPos[i];
		if (static_cast<int>(i) > top[i % 3] && position < maxPosition)
		{
			float kl = 0;
			float kr = 0;
			const float s = HasMsStereo(h) ? 1.41421356f : 1;
			if (IsMpeg1(h))
			{
				kl = k_Pan[2 * position];
				kr = k_Pan[2 * position + 1];
			}
			else
			{
				kl = 1;
				kr = ScaleByQuarterPowers(1, static_cast<int>((position + 1) >> 1 << mpeg2Shift));
				if ((position & 1) != 0)
				{
					kl = kr;
					kr = 1;
				}
			}
			IntensityStereoBand(left, widths[i], kl * s, kr * s);
		}
		else if (HasMsStereo(h))
		{
			MidSideStereo(left, widths[i]);
		}
		left += widths[i];
	}
}

void IntensityStereo(float* left, uint8_t* istPos, const GranuleInfo* gr, Header h) noexcept
{
	std::array<int, 3> top {};
	const int bands = gr->longBands + gr->shortBands;
	const int windows = gr->shortBands != 0 ? 3 : 1;
	StereoTopBand(left + 576, gr->bandWidths, bands, top);
	if (gr->longBands != 0)
	{
		top[0] = top[1] = top[2] = std::max(std::max(top[0], top[1]), top[2]);
	}
	for (int i = 0; i < windows; ++i)
	{
		const int defaultPosition = IsMpeg1(h) ? 3 : 0;
		const int last = bands - windows + i;
		const int previous = last - windows;
		istPos[last] = static_cast<uint8_t>(top[static_cast<size_t>(i)] >= previous ? defaultPosition : istPos[previous]);
	}
	StereoProcess(left, istPos, gr->bandWidths, h, top, gr[1].scalefacCompress & 1);
}

// ---- Layer III synthesis -------------------------------------------------------------------------------------------

/// Short blocks come window by window within each band: interleave them sample by sample, through `scratch`
void Reorder(float* granule, float* scratch, const uint8_t* widths) noexcept
{
	float* src = granule;
	float* dst = scratch;
	for (int len = 0; (len = *widths) != 0; widths += 3, src += 2 * len)
	{
		for (int i = 0; i < len; ++i, ++src)
		{
			*dst++ = src[0 * len];
			*dst++ = src[1 * len];
			*dst++ = src[2 * len];
		}
	}
	std::copy(scratch, dst, granule);
}

/// The butterflies between each pair of neighbouring subbands
void Antialias(float* granule, int bands) noexcept
{
	static constexpr float k_Cs[8] = {0.85749293f, 0.88174200f, 0.94962865f, 0.98331459f,
	                                  0.99551782f, 0.99916056f, 0.99989920f, 0.99999316f};
	static constexpr float k_Ca[8] = {0.51449576f, 0.47173197f, 0.31337745f, 0.18191320f,
	                                  0.09457419f, 0.04096558f, 0.01419856f, 0.00369997f};
	for (; bands > 0; --bands, granule += 18)
	{
		for (int i = 0; i < 8; ++i)
		{
			const float u = granule[18 + i];
			const float d = granule[17 - i];
			granule[18 + i] = u * k_Cs[i] - d * k_Ca[i];
			granule[17 - i] = u * k_Ca[i] + d * k_Cs[i];
		}
	}
}

/// The 9-point DCT-III of the even or odd half of a long block
void Dct3x9(float* y) noexcept
{
	float s0 = y[0];
	float s2 = y[2];
	float s4 = y[4];
	float s6 = y[6];
	float s8 = y[8];
	float t0 = s0 + s6 * 0.5f;
	s0 -= s6;
	float t4 = (s4 + s2) * 0.93969262f;
	float t2 = (s8 + s2) * 0.76604444f;
	s6 = (s4 - s8) * 0.17364818f;
	s4 += s8 - s2;

	s2 = s0 - s4 * 0.5f;
	y[4] = s4 + s0;
	s8 = t0 - t2 + s6;
	s0 = t0 - t4 + t2;
	s4 = t0 + t4 - s6;

	float s1 = y[1];
	float s3 = y[3];
	float s5 = y[5];
	float s7 = y[7];

	s3 *= 0.86602540f;
	t0 = (s5 + s1) * 0.98480775f;
	t4 = (s5 - s7) * 0.34202014f;
	t2 = (s1 + s7) * 0.64278761f;
	s1 = (s1 - s5 - s7) * 0.86602540f;

	s5 = t0 - s3 - t2;
	s7 = t4 - s3 - t0;
	s3 = t4 + s3 - t2;

	y[0] = s4 - s7;
	y[1] = s2 + s1;
	y[2] = s0 - s3;
	y[3] = s8 + s5;
	y[5] = s8 - s5;
	y[6] = s0 + s3;
	y[7] = s2 - s1;
	y[8] = s4 + s7;
}

/// The 36-point inverse MDCT of each long block subband, windowed and overlapped with the previous granule's
void Imdct36(float* granule, float* overlap, const float* window, int bands) noexcept
{
	static constexpr float k_Twiddle[18] = {0.73727734f, 0.79335334f, 0.84339145f, 0.88701083f, 0.92387953f, 0.95371695f,
	                                        0.97629601f, 0.99144486f, 0.99904822f, 0.67559021f, 0.60876143f, 0.53729961f,
	                                        0.46174861f, 0.38268343f, 0.30070580f, 0.21643961f, 0.13052619f, 0.04361938f};
	for (int j = 0; j < bands; ++j, granule += 18, overlap += 9)
	{
		std::array<float, 9> co {};
		std::array<float, 9> si {};
		co[0] = -granule[0];
		si[0] = granule[17];
		for (int i = 0; i < 4; ++i)
		{
			si[static_cast<size_t>(8 - 2 * i)] = granule[4 * i + 1] - granule[4 * i + 2];
			co[static_cast<size_t>(1 + 2 * i)] = granule[4 * i + 1] + granule[4 * i + 2];
			si[static_cast<size_t>(7 - 2 * i)] = granule[4 * i + 4] - granule[4 * i + 3];
			co[static_cast<size_t>(2 + 2 * i)] = -(granule[4 * i + 3] + granule[4 * i + 4]);
		}
		Dct3x9(co.data());
		Dct3x9(si.data());

		si[1] = -si[1];
		si[3] = -si[3];
		si[5] = -si[5];
		si[7] = -si[7];

		for (size_t i = 0; i < 9; ++i)
		{
			const float previous = overlap[i];
			const float sum = co[i] * k_Twiddle[9 + i] + si[i] * k_Twiddle[0 + i];
			overlap[i] = co[i] * k_Twiddle[0 + i] - si[i] * k_Twiddle[9 + i];
			granule[i] = previous * window[0 + i] - sum * window[9 + i];
			granule[17 - i] = previous * window[9 + i] + sum * window[0 + i];
		}
	}
}

void Idct3(float x0, float x1, float x2, float* dst) noexcept
{
	const float m1 = x1 * 0.86602540f;
	const float a1 = x0 - x2 * 0.5f;
	dst[1] = x0 + x2;
	dst[0] = a1 + m1;
	dst[2] = a1 - m1;
}

/// The 12-point inverse MDCT of one short window, windowed and overlapped
void Imdct12(const float* x, float* dst, float* overlap) noexcept
{
	static constexpr float k_Twiddle[6] = {0.79335334f, 0.92387953f, 0.99144486f, 0.60876143f, 0.38268343f, 0.13052619f};
	std::array<float, 3> co {};
	std::array<float, 3> si {};
	Idct3(-x[0], x[6] + x[3], x[12] + x[9], co.data());
	Idct3(x[15], x[12] - x[9], x[6] - x[3], si.data());
	si[1] = -si[1];
	for (size_t i = 0; i < 3; ++i)
	{
		const float previous = overlap[i];
		const float sum = co[i] * k_Twiddle[3 + i] + si[i] * k_Twiddle[0 + i];
		overlap[i] = co[i] * k_Twiddle[0 + i] - si[i] * k_Twiddle[3 + i];
		dst[i] = previous * k_Twiddle[2 - i] - sum * k_Twiddle[5 - i];
		dst[5 - i] = previous * k_Twiddle[5 - i] + sum * k_Twiddle[2 - i];
	}
}

void ImdctShort(float* granule, float* overlap, int bands) noexcept
{
	for (; bands > 0; --bands, overlap += 9, granule += 18)
	{
		std::array<float, 18> tmp {};
		std::copy_n(granule, tmp.size(), tmp.begin());
		std::copy_n(overlap, 6, granule);
		Imdct12(tmp.data(), granule + 6, overlap + 6);
		Imdct12(tmp.data() + 1, granule + 12, overlap + 6);
		Imdct12(tmp.data() + 2, overlap, overlap + 6);
	}
}

/// Every odd sample of every odd subband changes sign (the frequency inversion of the polyphase filter bank)
void ChangeSign(float* granule) noexcept
{
	granule += 18;
	for (int b = 0; b < 32; b += 2, granule += 36)
	{
		for (int i = 1; i < 18; i += 2)
		{
			granule[i] = -granule[i];
		}
	}
}

void ImdctGranule(float* granule, float* overlap, unsigned blockType, unsigned longBands) noexcept
{
	static constexpr float k_Window[2][18] = {
	    {0.99904822f, 0.99144486f, 0.97629601f, 0.95371695f, 0.92387953f, 0.88701083f, 0.84339145f, 0.79335334f, 0.73727734f,
	     0.04361938f, 0.13052619f, 0.21643961f, 0.30070580f, 0.38268343f, 0.46174861f, 0.53729961f, 0.60876143f, 0.67559021f},
	    {1, 1, 1, 1, 1, 1, 0.99144486f, 0.92387953f, 0.79335334f, 0, 0, 0, 0, 0, 0, 0.13052619f, 0.38268343f, 0.60876143f},
	};
	if (longBands != 0)
	{
		Imdct36(granule, overlap, k_Window[0], static_cast<int>(longBands));
		granule += 18 * longBands;
		overlap += 9 * longBands;
	}
	if (blockType == k_ShortBlock)
	{
		ImdctShort(granule, overlap, static_cast<int>(32 - longBands));
	}
	else
	{
		Imdct36(granule, overlap, k_Window[blockType == k_StopBlock ? 1 : 0], static_cast<int>(32 - longBands));
	}
}

/// One granule of every channel from the main data, up to the subband samples the synthesis takes
void DecodeGranule(Header h, BitStream& bits, int capacity, const GranuleInfo* gr, int channels, float* granule, float* scf,
                   float* scratch, std::array<std::array<uint8_t, 39>, 2>& istPos, float* overlap) noexcept
{
	for (int ch = 0; ch < channels; ++ch)
	{
		const int limit = bits.position + gr[ch].part23Length;
		DecodeScalefactors(h, istPos[static_cast<size_t>(ch)].data(), bits, gr[ch], scf, ch);
		ReadSamples(granule + 576 * ch, bits, capacity, gr[ch], scf, limit);
	}
	if (HasIntensityStereo(h))
	{
		IntensityStereo(granule, istPos[1].data(), gr, h);
	}
	else if (IsMsStereo(h))
	{
		MidSideStereo(granule, 576);
	}
	for (int ch = 0; ch < channels; ++ch)
	{
		float* samples = granule + 576 * ch;
		int antialiasBands = 31;
		// Mixed blocks: the first 2 subbands (4 at 8 kHz) are long
		const int longBands = (gr[ch].mixedBlock != 0 ? 2 : 0) << (RateOverVersions(h) == 2 ? 1 : 0);
		if (gr[ch].shortBands != 0)
		{
			antialiasBands = longBands - 1;
			Reorder(samples + longBands * 18, scratch, gr[ch].bandWidths + gr[ch].longBands);
		}
		Antialias(samples, antialiasBands);
		ImdctGranule(samples, overlap + 9 * 32 * ch, gr[ch].blockType, static_cast<unsigned>(longBands));
		ChangeSign(samples);
	}
}

/// What a Xing or Info header in the first frame says
struct XingTag
{
	std::optional<uint32_t> frames; ///< the frames of audio that follow
	uint32_t delay {0};             ///< the encoder's samples to cut at the start (with the decoder's own delay)
	uint32_t padding {0};           ///< and at the end
};

/// The Xing or Info header of the frame found by the call that started at `from` (`frameBytes` from there, any bytes
/// skipped before the frame included, as the reference reads it), with its LAME tag. Bytes past the data read as 0
std::optional<XingTag> ReadXingTag(std::span<const uint8_t> data, size_t from, size_t frameBytes) noexcept
{
	const auto byteAt = [&data](size_t i) -> uint32_t { return i < data.size() ? data[i] : 0u; };
	std::array<uint8_t, k_HeaderSize> header {};
	for (size_t i = 0; i < header.size(); ++i)
	{
		header[i] = static_cast<uint8_t>(byteAt(from + i));
	}
	if (from + frameBytes > data.size() || frameBytes < k_HeaderSize)
	{
		return std::nullopt;
	}
	BitStream bits(data.data() + from + k_HeaderSize, static_cast<int>(frameBytes) - k_HeaderSize);
	if (HasCrc(header.data()))
	{
		bits.Read(16);
	}
	std::array<GranuleInfo, 4> granules;
	if (ReadSideInfo(bits, granules.data(), header.data()) < 0)
	{
		return std::nullopt;
	}
	const size_t begin = from + k_HeaderSize + static_cast<size_t>(bits.position / 8);
	const auto is = [&](const char* id) {
		return byteAt(begin) == static_cast<uint8_t>(id[0]) && byteAt(begin + 1) == static_cast<uint8_t>(id[1]) &&
		       byteAt(begin + 2) == static_cast<uint8_t>(id[2]) && byteAt(begin + 3) == static_cast<uint8_t>(id[3]);
	};
	if (!is("Xing") && !is("Info"))
	{
		return std::nullopt;
	}
	const auto bigEndian = [&byteAt](size_t i) {
		return byteAt(i) << 24 | byteAt(i + 1) << 16 | byteAt(i + 2) << 8 | byteAt(i + 3);
	};
	XingTag tag;
	const uint32_t flags = byteAt(begin + 7);
	size_t p = begin + 8;
	if ((flags & 0x01) != 0)
	{
		tag.frames = bigEndian(p);
		p += 4;
	}
	if ((flags & 0x02) != 0)
	{
		p += 4; // the stream's bytes
	}
	if ((flags & 0x04) != 0)
	{
		p += 100; // the seek table
	}
	if ((flags & 0x08) != 0)
	{
		p += 4; // the quality
	}
	// A LAME tag (an encoder name first): 12 bits of delay and 12 of padding, 21 bytes in; the decoder's own delay
	// of 528 + 1 samples moves the cut
	if (byteAt(p) != 0)
	{
		p += 21;
		if (p - from + 14 < frameBytes)
		{
			constexpr int k_DecoderDelay = 528 + 1;
			const int delay = static_cast<int>(byteAt(p) << 4 | byteAt(p + 1) >> 4) + k_DecoderDelay;
			const int padding = static_cast<int>((byteAt(p + 1) & 0xF) << 8 | byteAt(p + 2)) - k_DecoderDelay;
			tag.delay = static_cast<uint32_t>(delay);
			tag.padding = static_cast<uint32_t>(std::max(padding, 0));
		}
	}
	return tag;
}
} // namespace

MpegFrame MpegFrameDecoder::DecodeLayer3(const uint8_t* h, int frameSize, std::span<int16_t> pcm, MpegFrame result) noexcept
{
	BitStream bits(h + k_HeaderSize, frameSize - k_HeaderSize);
	if (HasCrc(h))
	{
		bits.Read(16);
	}
	std::array<GranuleInfo, 4> granules;
	const int mainDataBegin = ReadSideInfo(bits, granules.data(), h);
	if (mainDataBegin < 0 || bits.position > bits.limit)
	{
		Reset();
		return result;
	}

	// The main data: the last `mainDataBegin` bytes of the reservoir, then the rest of this frame
	const int reservoirBytes = _reservoirBytes;
	const int have = std::min(reservoirBytes, mainDataBegin);
	const int frameBytes = std::min((bits.limit - bits.position) / 8, static_cast<int>(k_MaxPayloadBytes));
	std::copy_n(_reservoir.data() + std::max(0, reservoirBytes - mainDataBegin), have, _mainData.data());
	std::copy_n(bits.data + bits.position / 8, frameBytes, _mainData.data() + have);
	BitStream main(_mainData.data(), have + frameBytes);
	const bool complete = reservoirBytes >= mainDataBegin;
	if (complete && !pcm.empty())
	{
		float* granule = _scratch.data();
		float* scf = granule + k_GranuleFloats;
		float* lins = scf + k_ScalefactorFloats;
		int16_t* out = pcm.data();
		for (int i = 0; i < (IsMpeg1(h) ? 2 : 1); ++i, out += 576 * result.channels)
		{
			std::fill_n(granule, k_GranuleFloats, 0.0f);
			DecodeGranule(h, main, static_cast<int>(_mainData.size()), granules.data() + i * result.channels, result.channels,
			              granule, scf, lins, _intensityPositions, _overlap.data());
			SynthGranule(_filter, granule, 18, result.channels, out, lins);
		}
	}

	// What follows the main data this frame used is the start of the next frames'
	int keep = (main.position + 7) / 8;
	int remains = std::max(0, main.limit / 8 - keep);
	if (remains > static_cast<int>(k_MaxReservoirBytes))
	{
		keep += remains - static_cast<int>(k_MaxReservoirBytes);
		remains = static_cast<int>(k_MaxReservoirBytes);
	}
	std::memmove(_reservoir.data(), _mainData.data() + keep, static_cast<size_t>(remains));
	_reservoirBytes = remains;
	result.samples = complete ? FrameSamples(_header.data()) : 0;
	return result;
}

MpegFrame MpegFrameDecoder::Decode(std::span<const uint8_t> data, std::span<int16_t> pcm) noexcept
{
	const auto* mp3 = data.data();
	const int bytes = static_cast<int>(std::min<size_t>(data.size(), INT32_MAX));
	MpegFrame result;
	int offset = 0;
	int frameSize = 0;
	// The next frame of the stream already followed: straight to it when the one after it starts where it should
	if (bytes > 4 && _header[0] == 0xFF && Matches(_header.data(), mp3))
	{
		frameSize = FrameBytes(mp3, _freeFormatBytes) + Padding(mp3);
		if (frameSize != bytes && (frameSize + k_HeaderSize > bytes || !Matches(mp3, mp3 + frameSize)))
		{
			frameSize = 0;
		}
	}
	if (frameSize == 0)
	{
		_header = {};
		_freeFormatBytes = 0;
		_filter = {};
		_overlap = {};
		_reservoir = {};
		_reservoirBytes = 0;
		offset = FindFrame(mp3, bytes, _freeFormatBytes, frameSize);
		if (frameSize == 0 || offset + frameSize > bytes)
		{
			result.bytes = static_cast<size_t>(offset);
			return result;
		}
	}
	Header h = mp3 + offset;
	std::copy_n(h, 4, _header.begin());
	result.bytes = static_cast<size_t>(offset + frameSize);
	result.channels = IsMono(h) ? 1 : 2;
	result.sampleRate = SampleRateHz(h);
	result.layer = static_cast<uint8_t>(4 - LayerBits(h));
	if (result.layer == 3)
	{
		return DecodeLayer3(h, frameSize, pcm, result);
	}
	if (pcm.empty())
	{
		result.samples = FrameSamples(h);
		return result;
	}
	BitStream bits(h + k_HeaderSize, frameSize - k_HeaderSize);
	if (HasCrc(h))
	{
		bits.Read(16);
	}
	ScaleInfo info;
	ReadScaleInfo(h, bits, info);
	std::vector<float> granule(576 * 2, 0.0f);
	std::vector<float> lins((18 + 15) * 64, 0.0f);
	int16_t* out = pcm.data();
	const int groupSize = result.layer | 1;
	for (int i = 0, part = 0; part < 3; ++part)
	{
		if (12 == (i += DequantiseGranule(granule.data() + i, bits, info, groupSize)))
		{
			i = 0;
			ApplyScalefactors(info, info.scalefactors.data() + part, granule.data());
			SynthGranule(_filter, granule.data(), 12, result.channels, out, lins.data());
			std::fill(granule.begin(), granule.end(), 0.0f);
			out += 384 * result.channels;
		}
		if (bits.position > bits.limit)
		{
			Reset();
			return result;
		}
	}
	result.samples = FrameSamples(_header.data());
	return result;
}

std::optional<DecodedAudio> openblack::audio::codec::DecodeMpegStream(std::span<const uint8_t> stream)
{
	// Tags at the end: ID3v1 (128 bytes from "TAG"), then an APE footer ("APETAGEX", its size at +24)
	size_t size = stream.size();
	if (size > 128 && std::memcmp(stream.data() + size - 128, "TAG", 3) == 0)
	{
		size -= 128;
	}
	if (size > 32 && std::memcmp(stream.data() + size - 32, "APETAGEX", 8) == 0)
	{
		const auto* footer = stream.data() + size - 32;
		const uint32_t tag = footer[24] | footer[25] << 8 | footer[26] << 16 | static_cast<uint32_t>(footer[27]) << 24;
		size -= std::min<size_t>(size, 32 + static_cast<size_t>(tag));
	}
	// An ID3v2 tag at the start: 10 bytes, its size in 7-bit bytes, a footer when flagged
	size_t start = 0;
	if (size < 10)
	{
		return std::nullopt;
	}
	if (stream[0] == 'I' && stream[1] == 'D' && stream[2] == '3')
	{
		uint32_t tag = (stream[6] & 0x7Fu) << 21 | (stream[7] & 0x7Fu) << 14 | (stream[8] & 0x7Fu) << 7 | (stream[9] & 0x7Fu);
		if ((stream[5] & 0x10) != 0)
		{
			tag += 10;
		}
		start = std::min<size_t>(size, 10 + static_cast<size_t>(tag));
	}
	const auto body = stream.subspan(start, size - start);

	// The next frame with samples from `at`, skipping those without; `from` is where the call that found it started
	const auto next = [&body](MpegFrameDecoder& decoder, size_t& at, std::span<int16_t> pcm, size_t* from = nullptr) {
		for (;;)
		{
			const auto frame = decoder.Decode(body.subspan(at), pcm);
			if (frame.samples > 0 || frame.bytes == 0)
			{
				if (from != nullptr)
				{
					*from = at;
				}
				at += frame.bytes;
				return frame;
			}
			at += frame.bytes;
		}
	};

	// The first frame tells the channels and the rate, and may be a Xing or Info header instead of audio
	MpegFrameDecoder decoder;
	std::array<int16_t, MpegFrameDecoder::k_MaxSamples> frame {};
	size_t at = 0;
	size_t firstFrom = 0;
	const auto first = next(decoder, at, frame, &firstFrom);
	if (first.samples == 0)
	{
		return std::nullopt;
	}
	size_t streamStart = 0;
	const auto tag = ReadXingTag(stream, static_cast<size_t>(body.data() - stream.data()) + firstFrom, first.bytes);
	if (tag)
	{
		streamStart = at;
		decoder.Reset();
	}

	// The length: the Xing header's frames, or every frame counted
	const bool lengthKnown = tag && tag->frames;
	uint64_t total = lengthKnown ? uint64_t {*tag->frames} * first.samples : 0;
	const uint32_t delay = tag ? tag->delay : 0;
	const uint32_t padding = tag ? tag->padding : 0;
	uint64_t length = total;
	if (lengthKnown)
	{
		length -= length >= delay ? delay : 0;
		length -= length >= padding ? padding : 0;
	}
	else
	{
		decoder.Reset();
		at = streamStart;
		for (;;)
		{
			const auto counted = next(decoder, at, {});
			if (counted.samples == 0)
			{
				break;
			}
			length += counted.samples;
		}
	}

	DecodedAudio audio;
	audio.channels = first.channels;
	audio.sampleRate = first.sampleRate;
	audio.samples.assign(static_cast<size_t>(length * first.channels), 0);

	// From the start: the delay skipped, then up to the padding (when the length is known)
	const bool trimEnd = lengthKnown && total > padding;
	const uint64_t end = trimEnd ? total - padding : 0;
	decoder.Reset();
	at = streamStart;
	uint64_t position = 0; // in the stream, the delay included
	uint64_t written = 0;
	uint32_t remaining = 0;
	uint32_t consumed = 0;
	uint16_t frameChannels = first.channels;
	while (written < length)
	{
		if (position < delay)
		{
			const auto skip = static_cast<uint32_t>(std::min<uint64_t>(remaining, delay - position));
			position += skip;
			consumed += skip;
			remaining -= skip;
		}
		auto take = static_cast<uint32_t>(std::min<uint64_t>(remaining, length - written));
		if (trimEnd)
		{
			if (position >= end)
			{
				break;
			}
			take = static_cast<uint32_t>(std::min<uint64_t>(take, end - position));
		}
		// The stream's channel count is the first frame's
		std::copy_n(frame.begin() + static_cast<ptrdiff_t>(consumed * frameChannels), take * first.channels,
		            audio.samples.begin() + static_cast<ptrdiff_t>(written * first.channels));
		position += take;
		consumed += take;
		remaining -= take;
		written += take;
		if (written == length || (trimEnd && position >= end))
		{
			break;
		}
		const auto decoded = next(decoder, at, frame);
		if (decoded.samples == 0)
		{
			break;
		}
		remaining = decoded.samples;
		consumed = 0;
		frameChannels = decoded.channels;
	}
	return audio;
}
