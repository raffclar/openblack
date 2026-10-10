/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "HelpSpeech.h"

#include <cctype>

#include <algorithm>
#include <unordered_map>

using namespace openblack::audio;

namespace
{
std::string Upper(std::string_view text)
{
	std::string result(text);
	std::ranges::transform(result, result.begin(),
	                       [](char c) { return static_cast<char>(std::toupper(static_cast<unsigned char>(c))); });
	return result;
}

/// The order the banks are looked in for a text's sample
constexpr std::array k_SearchOrder {SpeechBank::Villagers, SpeechBank::HelpSprites, SpeechBank::Guidance};
} // namespace

std::string_view HelpSpeechTable::SampleName(std::string_view file)
{
	const auto slash = file.find_last_of("\\/");
	if (slash != std::string_view::npos)
	{
		file.remove_prefix(slash + 1);
	}
	const auto dot = file.find_last_of('.');
	if (dot != std::string_view::npos)
	{
		file.remove_suffix(file.size() - dot);
	}
	return file;
}

HelpSpeechTable::HelpSpeechTable(std::span<const std::string> textNames,
                                 std::span<const std::vector<SpeechBankSample>, 3> banks)
{
	// Every sample by its name, the first bank searched winning
	std::unordered_map<std::string, SpeechSample> byName;
	for (const auto bank : k_SearchOrder)
	{
		for (const auto& sample : banks[static_cast<size_t>(bank)])
		{
			byName.try_emplace(Upper(SampleName(sample.file)),
			                   SpeechSample {.bank = bank, .sample = sample.sample, .sound = sample.sound});
		}
	}

	_samples.reserve(textNames.size());
	for (const auto& name : textNames)
	{
		const auto found = byName.find(Upper(name));
		_samples.emplace_back(found != byName.end() ? std::optional(found->second) : std::nullopt);
	}
}

std::optional<SpeechSample> HelpSpeechTable::Find(uint32_t text) const
{
	if (text >= _samples.size())
	{
		text = 0;
	}
	return text < _samples.size() ? _samples[text] : std::nullopt;
}

size_t HelpSpeechTable::GetSpokenCount() const noexcept
{
	return static_cast<size_t>(std::ranges::count_if(_samples, [](const auto& sample) { return sample.has_value(); }));
}

void SpeechVoices::Add(SpeechVoice voice, SpeechSample sample, entt::entity emitter)
{
	if (emitter != entt::null)
	{
		_lines.push_back({.voice = voice, .sample = sample, .emitter = emitter});
	}
}

bool SpeechVoices::IsSaying(SpeechVoice voice, SpeechSample sample, const std::function<bool(entt::entity)>& isPlaying)
{
	std::erase_if(_lines, [&isPlaying](const Line& line) { return !isPlaying(line.emitter); });
	return std::ranges::any_of(_lines,
	                           [voice, sample](const Line& line) { return line.voice == voice && line.sample == sample; });
}
