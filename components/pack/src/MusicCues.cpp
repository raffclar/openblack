/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "MusicCues.h"

#include <charconv>

#include <algorithm>
#include <ranges>

#include "PackFile.h"

using namespace openblack::pack;

namespace
{
constexpr char k_CueMark = '!';

[[nodiscard]] bool IsDigit(char c)
{
	return c >= '0' && c <= '9';
}
} // namespace

std::vector<ChunkCue> openblack::pack::ParseChunkCues(std::string_view text)
{
	std::vector<ChunkCue> cues;
	if (text.empty() || text.front() != k_CueMark)
	{
		return cues;
	}
	size_t at = 0;
	while (at < text.size() && text[at] == k_CueMark)
	{
		const auto digitsBegin = at + 1;
		auto digitsEnd = digitsBegin;
		while (digitsEnd < text.size() && IsDigit(text[digitsEnd]))
		{
			++digitsEnd;
		}
		auto frame = uint32_t {0};
		std::from_chars(text.data() + digitsBegin, text.data() + digitsEnd, frame);
		// The character after the digits separates the frame from the cue
		const auto textBegin = std::min(digitsEnd + 1, text.size());
		const auto textEnd = std::min(text.find(k_CueMark, textBegin), text.size());
		cues.push_back({.frame = frame, .text = std::string(text.substr(textBegin, textEnd - textBegin))});
		at = textEnd;
	}
	return cues;
}

std::vector<MusicCue> openblack::pack::ReadMusicCues(std::span<const AudioBankSampleHeader> headers)
{
	std::vector<MusicCue> cues;
	for (size_t i = 0; i < headers.size(); ++i)
	{
		const auto& description = headers[i].description;
		const auto length = std::ranges::find(description, '\0') - description.begin();
		const auto chunkCues = ParseChunkCues(std::string_view(description.data(), static_cast<size_t>(length)));
		for (const auto& cue : std::views::reverse(chunkCues))
		{
			cues.push_back({.chunk = static_cast<uint32_t>(i + 1), .frame = cue.frame, .text = cue.text});
		}
	}
	return cues;
}
