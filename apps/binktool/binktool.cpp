/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// Inspects Bink videos and prints a checksum of every decoded frame, to compare the decoder with other players.
//
//   binktool info <file.bik>
//   binktool crc <file.bik> [order]   frame,crc_rgb32,crc_555,crc_565,crc_y,crc_u,crc_v (zlib CRC-32s)
//   binktool goto <file.bik> [visits] frame,crc_rgb32 visiting the frames in the order (k * 11 + 3) mod frames
//   binktool dump <file.bik> <frame> <out.raw>   the frame's 32-bit picture (B, G, R, 0)
//   binktool bench <file.bik>         decode times

#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include <algorithm>
#include <chrono>
#include <fstream>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <BinkDecoder.h>
#include <BinkFile.h>
#include <BinkYuv.h>

using namespace openblack::bink;

namespace
{
constexpr std::array<uint32_t, 256> MakeCrcTable() noexcept
{
	std::array<uint32_t, 256> table {};
	for (uint32_t i = 0; i < 256; ++i)
	{
		uint32_t c = i;
		for (int k = 0; k < 8; ++k)
		{
			c = (c & 1) != 0 ? 0xEDB88320u ^ (c >> 1) : c >> 1;
		}
		table[i] = c;
	}
	return table;
}

constexpr auto k_CrcTable = MakeCrcTable();

class Crc32
{
public:
	void Add(std::span<const uint8_t> bytes) noexcept
	{
		for (const uint8_t byte : bytes)
		{
			_crc = k_CrcTable[(_crc ^ byte) & 0xFF] ^ (_crc >> 8);
		}
	}
	[[nodiscard]] uint32_t Value() const noexcept { return ~_crc; }

private:
	uint32_t _crc {0xFFFFFFFFu};
};

uint32_t PlaneCrc(const PlaneView& plane)
{
	Crc32 crc;
	for (uint32_t row = 0; row < plane.height; ++row)
	{
		crc.Add(plane.pixels.subspan(static_cast<size_t>(row) * plane.stride, plane.width));
	}
	return crc.Value();
}

/// The 16-bit copies of a 32-bit picture: each channel's top bits, 5-5-5 or 5-6-5, little-endian
uint32_t Crc16(std::span<const uint8_t> bgrx, bool green6)
{
	Crc32 crc;
	for (size_t i = 0; i < bgrx.size(); i += 4)
	{
		const uint32_t b = bgrx[i];
		const uint32_t g = bgrx[i + 1];
		const uint32_t r = bgrx[i + 2];
		const auto texel =
		    static_cast<uint16_t>(green6 ? (r >> 3) << 11 | (g >> 2) << 5 | b >> 3 : (r >> 3) << 10 | (g >> 3) << 5 | b >> 3);
		const std::array<uint8_t, 2> bytes = {static_cast<uint8_t>(texel), static_cast<uint8_t>(texel >> 8)};
		crc.Add(bytes);
	}
	return crc.Value();
}

std::shared_ptr<const BinkFile> Load(const char* path)
{
	std::string error;
	auto file = BinkFile::Open(path, &error);
	if (!file)
	{
		std::fprintf(stderr, "%s: %s\n", path, error.c_str());
		return nullptr;
	}
	return std::make_shared<const BinkFile>(std::move(*file));
}

int Info(const BinkFile& file)
{
	const auto& h = file.GetHeader();
	std::printf("revision %c, %u x %u, %u/%u fps (%u), %u frames, %u key frames, %zu audio tracks, flags 0x%08X\n", h.revision,
	            h.width, h.height, h.fpsNumerator, h.fpsDenominator, file.IntegerFps(), h.frameCount, file.KeyFrameCount(),
	            file.AudioTracks().size(), h.videoFlags);
	return 0;
}

int Crc(std::shared_ptr<const BinkFile> file, bool gotoOrder, uint32_t visits)
{
	std::string error;
	auto reader = FrameReader::Create(file, &error);
	if (!reader)
	{
		std::fprintf(stderr, "%s\n", error.c_str());
		return 1;
	}
	std::vector<uint8_t> bgrx(static_cast<size_t>(file->Width()) * file->Height() * 4);
	std::printf(gotoOrder ? "frame,crc_rgb32\n" : "frame,crc_rgb32,crc_555,crc_565,crc_y,crc_u,crc_v\n");
	int failures = 0;
	for (uint32_t k = 0; k < visits; ++k)
	{
		const uint32_t frame = gotoOrder ? static_cast<uint32_t>((static_cast<uint64_t>(k) * 11 + 3) % file->FrameCount()) : k;
		if (!reader->DecodeFrame(frame))
		{
			std::fprintf(stderr, "frame %u failed to decode\n", frame);
			++failures;
		}
		const auto picture = reader->GetPicture();
		ConvertPicture(picture, PixelLayout::Bgrx8, bgrx);
		Crc32 crc;
		crc.Add(bgrx);
		if (gotoOrder)
		{
			std::printf("%u,%08x\n", frame, crc.Value());
			continue;
		}
		std::printf("%u,%08x,%08x,%08x,%08x,%08x,%08x\n", frame, crc.Value(), Crc16(bgrx, false), Crc16(bgrx, true),
		            PlaneCrc(picture.y), PlaneCrc(picture.u), PlaneCrc(picture.v));
	}
	return failures == 0 ? 0 : 1;
}

int Dump(std::shared_ptr<const BinkFile> file, uint32_t frame, const char* out)
{
	auto reader = FrameReader::Create(file);
	if (!reader || frame >= file->FrameCount())
	{
		return 1;
	}
	for (uint32_t i = 0; i <= frame; ++i)
	{
		[[maybe_unused]] const bool decoded = reader->DecodeFrame(i);
	}
	std::vector<uint8_t> bgrx(static_cast<size_t>(file->Width()) * file->Height() * 4);
	ConvertPicture(reader->GetPicture(), PixelLayout::Bgrx8, bgrx);
	std::ofstream stream(out, std::ios::binary);
	stream.write(reinterpret_cast<const char*>(bgrx.data()), static_cast<std::streamsize>(bgrx.size()));
	return stream ? 0 : 1;
}

int Bench(std::shared_ptr<const BinkFile> file)
{
	auto reader = FrameReader::Create(file);
	if (!reader)
	{
		return 1;
	}
	using Clock = std::chrono::steady_clock;
	std::vector<double> times;
	times.reserve(file->FrameCount());
	for (uint32_t frame = 0; frame < file->FrameCount(); ++frame)
	{
		const auto start = Clock::now();
		[[maybe_unused]] const bool decoded = reader->DecodeFrame(frame);
		times.push_back(std::chrono::duration<double, std::milli>(Clock::now() - start).count());
	}
	double total = 0.0;
	for (const double t : times)
	{
		total += t;
	}
	std::ranges::sort(times);
	std::printf("%u frames: mean %.3f ms, median %.3f ms, 99th %.3f ms, max %.3f ms\n", file->FrameCount(),
	            total / static_cast<double>(times.size()), times[times.size() / 2], times[times.size() * 99 / 100],
	            times.back());
	return 0;
}
} // namespace

int main(int argc, char** argv)
{
	if (argc < 3)
	{
		std::fprintf(stderr, "usage: binktool info|crc|goto|bench <file.bik>, binktool dump <file.bik> <frame> <out.raw>\n");
		return 2;
	}
	const std::string_view command = argv[1];
	const auto file = Load(argv[2]);
	if (!file)
	{
		return 1;
	}
	if (command == "info")
	{
		return Info(*file);
	}
	if (command == "crc")
	{
		return Crc(file, false, file->FrameCount());
	}
	if (command == "goto")
	{
		const auto visits = argc > 3 ? static_cast<uint32_t>(std::strtoul(argv[3], nullptr, 10)) : file->FrameCount();
		return Crc(file, true, visits);
	}
	if (command == "bench")
	{
		return Bench(file);
	}
	if (command == "dump" && argc == 5)
	{
		return Dump(file, static_cast<uint32_t>(std::strtoul(argv[3], nullptr, 10)), argv[4]);
	}
	std::fprintf(stderr, "unknown command\n");
	return 2;
}
