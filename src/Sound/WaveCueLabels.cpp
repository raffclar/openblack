/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "WaveCueLabels.h"

#include <cstring>

#include <algorithm>
#include <optional>
#include <string_view>

namespace openblack::audio
{

namespace
{
constexpr size_t k_ChunkHeader = 8;
constexpr size_t k_CuePointSize = 24;

uint32_t ReadU32(std::span<const uint8_t> data, size_t offset)
{
	uint32_t value = 0;
	std::memcpy(&value, data.data() + offset, sizeof(value));
	return value;
}

bool IdIs(std::span<const uint8_t> data, size_t offset, std::string_view id)
{
	return offset + 4 <= data.size() && std::memcmp(data.data() + offset, id.data(), 4) == 0;
}

/// A chunk found: where its body starts and how long it is
struct Chunk
{
	size_t body;
	size_t size;
	/// Where the next chunk starts (the size rounded up to even)
	[[nodiscard]] size_t End() const { return body + size + (size & 1); }
};

/// The first chunk with this id from `from` up to `end`, walking chunk by chunk; for a list, `listType` is the list's
/// type, which must match too
std::optional<Chunk> FindChunk(std::span<const uint8_t> data, size_t from, size_t end, std::string_view id,
                               std::string_view listType = {})
{
	end = std::min(end, data.size());
	size_t at = from;
	while (at + k_ChunkHeader <= end)
	{
		const Chunk chunk {.body = at + k_ChunkHeader, .size = ReadU32(data, at + 4)};
		if (IdIs(data, at, id) && (listType.empty() || IdIs(data, chunk.body, listType)))
		{
			return chunk;
		}
		at = chunk.End();
	}
	return std::nullopt;
}
} // namespace

std::vector<WaveCueLabel> ReadWaveCueLabels(std::span<const uint8_t> riff)
{
	std::vector<WaveCueLabel> labels;
	const auto wave = FindChunk(riff, 0, riff.size(), "RIFF", "WAVE");
	if (!wave)
	{
		return labels;
	}
	const size_t waveEnd = wave->body + wave->size;
	const auto format = FindChunk(riff, wave->body + 4, waveEnd, "fmt ");
	if (!format || format->body + 8 > riff.size())
	{
		return labels;
	}
	const uint32_t sampleRate = ReadU32(riff, format->body + 4);
	// The cue points are looked for after the format, and their labels after the cue points
	const auto cue = FindChunk(riff, format->End(), waveEnd, "cue ");
	if (!cue || cue->body + 4 > riff.size())
	{
		return labels;
	}
	const uint32_t count = ReadU32(riff, cue->body);
	const size_t points = cue->body + 4;
	if (points + static_cast<size_t>(count) * k_CuePointSize > riff.size())
	{
		return labels;
	}
	const auto list = FindChunk(riff, cue->End(), riff.size(), "LIST", "adtl");
	if (!list)
	{
		return labels;
	}
	const size_t listEnd = list->body + list->size;
	size_t at = list->body + 4;
	for (uint32_t n = 0; n < count; ++n)
	{
		const auto label = FindChunk(riff, at, listEnd, "labl");
		if (!label || label->body + 4 > riff.size())
		{
			break;
		}
		at = label->End();
		const uint32_t id = ReadU32(riff, label->body);
		// The text runs to its terminating zero
		const auto* text = riff.data() + label->body + 4;
		const auto* textEnd = std::find(text, riff.data() + riff.size(), uint8_t {0});
		for (uint32_t p = 0; p < count; ++p)
		{
			const size_t point = points + static_cast<size_t>(p) * k_CuePointSize;
			if (ReadU32(riff, point) == id)
			{
				const uint32_t position = ReadU32(riff, point + 4);
				labels.push_back({
				    .text = std::string(text, textEnd),
				    .time = static_cast<float>(position) / static_cast<float>(sampleRate),
				});
				break;
			}
		}
	}
	return labels;
}

} // namespace openblack::audio
