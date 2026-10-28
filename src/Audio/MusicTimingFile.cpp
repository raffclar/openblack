/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "MusicTimingFile.h"

#include <utility>

#include <MusicCues.h>
#include <PackFile.h>

#include "SoundDecoder.h"

using namespace openblack::audio;

std::optional<MusicTiming> openblack::audio::MakeMusicTiming(std::span<const pack::AudioBankSampleHeader> headers,
                                                             std::span<const std::vector<uint8_t>> chunks)
{
	if (headers.empty() || headers.size() != chunks.size())
	{
		return std::nullopt;
	}
	MusicTiming timing;
	const auto& first = headers.front();
	timing.sampleRate = first.sampleRate;
	if ((first.overrideFlags & static_cast<uint32_t>(pack::AudioBankOverride::Loop)) != 0)
	{
		timing.loopOverride = first.loop;
	}
	for (auto& cue : pack::ReadMusicCues(headers))
	{
		timing.cues.push_back({.chunk = cue.chunk, .frame = cue.frame, .text = std::move(cue.text)});
	}
	timing.chunkFrames.reserve(chunks.size());
	for (const auto& chunk : chunks)
	{
		const auto frames = CountMpegFrames(chunk);
		if (!frames.has_value())
		{
			return std::nullopt;
		}
		timing.chunkFrames.push_back(static_cast<uint32_t>(*frames));
	}
	return timing;
}

std::optional<MusicTiming> openblack::audio::ReadMusicTiming(const std::vector<uint8_t>& bankFile)
{
	pack::PackFile pack;
	if (pack.Open(bankFile) != pack::PackResult::Success)
	{
		return std::nullopt;
	}
	return MakeMusicTiming(pack.GetAudioSampleHeaders(), pack.GetAudioSamplesData());
}
