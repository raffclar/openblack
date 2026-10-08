/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "AudioBankSample.h"

#include <algorithm>
#include <optional>

#include "Audio/Codec/WaveFile.h"

using namespace openblack::debug::gui;

namespace
{
[[nodiscard]] uint32_t ReadU32(std::span<const uint8_t> bytes, size_t at) noexcept
{
	return static_cast<uint32_t>(bytes[at]) | static_cast<uint32_t>(bytes[at + 1]) << 8 |
	       static_cast<uint32_t>(bytes[at + 2]) << 16 | static_cast<uint32_t>(bytes[at + 3]) << 24;
}

[[nodiscard]] bool StartsWith(std::span<const uint8_t> bytes, size_t at, std::string_view tag) noexcept
{
	return bytes.size() >= at + tag.size() && std::equal(tag.begin(), tag.end(), bytes.begin() + static_cast<ptrdiff_t>(at));
}

/// The tag of a RIFF wave's "fmt " chunk, for the waves the wave decoder does not open (the MPEG ones)
[[nodiscard]] std::optional<uint16_t> RiffFormatTag(std::span<const uint8_t> bytes) noexcept
{
	for (size_t at = 12; at + 8 <= bytes.size();)
	{
		const uint32_t size = ReadU32(bytes, at + 4);
		if (StartsWith(bytes, at, "fmt ") && size >= 2 && at + 10 <= bytes.size())
		{
			return static_cast<uint16_t>(bytes[at + 8] | bytes[at + 9] << 8);
		}
		if (size > bytes.size() - at - 8)
		{
			break;
		}
		at += 8 + size + (size & 1);
	}
	return std::nullopt;
}

[[nodiscard]] std::string_view TagName(uint16_t tag) noexcept
{
	switch (tag)
	{
	case 0x0001:
		return "PCM";
	case 0x0002:
		return "MS-ADPCM";
	case 0x0003:
		return "IEEE float";
	case 0x0006:
		return "A-law";
	case 0x0007:
		return "mu-law";
	case 0x0011:
		return "IMA ADPCM";
	case 0x0050:
		return "MPEG";
	case 0x0055:
		return "MPEG layer III";
	default:
		return "unknown";
	}
}
} // namespace

std::string_view openblack::debug::gui::SampleFormatName(std::span<const uint8_t> sample) noexcept
{
	if (const auto wave = audio::codec::ParseWaveFile(sample))
	{
		return TagName(wave->format);
	}
	if (StartsWith(sample, 0, "RIFF") && StartsWith(sample, 8, "WAVE"))
	{
		const auto tag = RiffFormatTag(sample);
		return tag ? TagName(*tag) : "broken wave";
	}
	return "raw MPEG";
}
