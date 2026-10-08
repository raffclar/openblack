/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Voices.h"

#include <cctype>

#include <array>
#include <unordered_map>
#include <utility>

#include <spdlog/spdlog.h>

#include "Audio/Audio.h"
#include "Audio/Services/Advisor.h"
#include "Common/HelpText.h"
#include "ECS/Systems/AudioStateInterface.h"
#include "Locator.h"

namespace openblack::audio
{

namespace
{
std::string Upper(std::string_view s)
{
	std::string out(s);
	for (auto& c : out)
	{
		c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
	}
	return out;
}

/// What this module keeps between calls (Locator::audioState)
struct VoicesState
{
	std::array<VoiceTable::SampleNames, static_cast<size_t>(SfxBank::_COUNT)> bankNames {};
	VoiceTable table {};
};

VoicesState& VoicesData()
{
	return openblack::Locator::audioState::value().Get<VoicesState>();
}
} // namespace

std::string VoiceSampleKey(std::string_view waveName)
{
	const auto slash = waveName.find_last_of("\\/");
	if (slash != std::string_view::npos)
	{
		waveName.remove_prefix(slash + 1);
	}
	auto key = Upper(waveName);
	if (key.size() >= 4 && key.compare(key.size() - 4, 4, ".WAV") == 0)
	{
		key.resize(key.size() - 4);
	}
	return key;
}

VoiceTable VoiceTable::Build(const std::vector<std::string>& textNames, const SampleNames& villagers,
                             const SampleNames& helpSprites, const SampleNames& guidance)
{
	// the first sample of each name, in the search order: villagers, HelpSprites, Guidance
	std::unordered_map<std::string, TextVoice> byName;
	const std::array<std::pair<SfxBank, const SampleNames*>, 3> order = {{
	    {SfxBank::Villagers, &villagers},
	    {SfxBank::HelpSprites, &helpSprites},
	    {SfxBank::Guidance, &guidance},
	}};
	for (const auto& [bank, names] : order)
	{
		for (size_t i = 0; i < names->size(); ++i)
		{
			byName.try_emplace(VoiceSampleKey((*names)[i]), TextVoice {bank, static_cast<uint32_t>(i + 1)});
		}
	}

	VoiceTable table;
	table._voices.reserve(textNames.size());
	for (const auto& name : textNames)
	{
		const auto found = byName.find(Upper(name));
		table._voices.push_back(found != byName.end() ? found->second : TextVoice {});
	}
	return table;
}

TextVoice VoiceTable::Get(uint32_t textId) const
{
	return textId < _voices.size() ? _voices[textId] : TextVoice {};
}

bool voices::IsTableBank(SfxBank bank)
{
	return bank == SfxBank::Villagers || bank == SfxBank::HelpSprites || bank == SfxBank::Guidance;
}

void voices::SetBankSampleNames(SfxBank bank, VoiceTable::SampleNames names)
{
	VoicesData().bankNames[static_cast<size_t>(bank)] = std::move(names);
}

void voices::BuildTable()
{
	auto& state = VoicesData();
	std::vector<std::string> textNames;
	textNames.reserve(helptext::Count());
	for (uint32_t i = 0; i < helptext::Count(); ++i)
	{
		textNames.push_back(helptext::GetEntry(i).name);
	}
	state.table = VoiceTable::Build(textNames, state.bankNames[static_cast<size_t>(SfxBank::Villagers)],
	                                state.bankNames[static_cast<size_t>(SfxBank::HelpSprites)],
	                                state.bankNames[static_cast<size_t>(SfxBank::Guidance)]);
	std::array<size_t, static_cast<size_t>(SfxBank::_COUNT)> counts {};
	for (uint32_t i = 0; i < state.table.Size(); ++i)
	{
		if (const auto voice = state.table.Get(i); voice.HasVoice())
		{
			++counts[static_cast<size_t>(voice.bank)];
		}
	}
	if (const auto logger = spdlog::get("audio"); logger != nullptr)
	{
		SPDLOG_LOGGER_INFO(logger, "Voice table: {} texts, HelpSprites {}, villagers {}, Guidance {}", state.table.Size(),
		                   counts[static_cast<size_t>(SfxBank::HelpSprites)], counts[static_cast<size_t>(SfxBank::Villagers)],
		                   counts[static_cast<size_t>(SfxBank::Guidance)]);
	}
}

const VoiceTable& voices::Table()
{
	return VoicesData().table;
}

void voices::SetTable(VoiceTable table)
{
	VoicesData().table = std::move(table);
}

bool voices::BankRegistered(SfxBank bank)
{
	return Bank(bank) != k_NoBank;
}

Channel voices::RunTextVoice(int32_t narrator, TextVoice voice)
{
	if (voice.bank == SfxBank::HelpSprites)
	{
		const int dude = narrator == helptext::k_NarratorGoodSpirit   ? advisor::k_GoodSpirit
		                 : narrator == helptext::k_NarratorEvilSpirit ? advisor::k_EvilSpirit
		                                                              : -1;
		if (dude >= 0)
		{
			advisor::Stop(advisor::k_EvilSpirit);
			advisor::Stop(advisor::k_GoodSpirit);
			advisor::Say(dude, static_cast<int>(voice.sample), false);
			return k_NoChannel;
		}
	}
	if (voice.sample == 0 || voice.bank == SfxBank::None)
	{
		return k_NoChannel;
	}
	PlayOptions options; // 2D, the .sad decides the rest
	options.sample = {Bank(voice.bank), static_cast<int>(voice.sample)};
	options.owner = Owner::Key(k_OwnerVoice); // the narration
	options.keepPcm = true;
	if (options.sample.bank == k_NoBank)
	{
		return k_NoChannel;
	}
	return PlaySoundEffect(options);
}

Channel voices::Say(uint32_t textId, bool withPosition, bool alt, glm::vec3 position)
{
	if (textId >= helptext::k_TextCount)
	{
		textId = 0;
	}
	// the say table; the original's entries all have id == index, so its id test never zeroes the bank (VoiceTable
	// keeps no id)
	const auto voice = Table().Get(textId);
	if (voice.sample == 0)
	{
		return k_NoChannel;
	}
	PlayOptions options;
	options.sample = {Bank(voice.bank), static_cast<int>(voice.sample)};
	options.owner = Owner::Key(alt ? k_OwnerVoiceAlt : k_OwnerVoice);
	options.is3D = withPosition;
	options.keepPcm = true;
	if (withPosition)
	{
		options.position = position;
	}
	options.track = false;
	if (SfxTrace())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("audio"), "SFX: SAY({}, {}, ({:.1f}, {:.1f}, {:.1f}), alt {}) -> bank {} sample {}",
		                   textId, withPosition ? 1 : 0, position.x, position.y, position.z, alt ? 1 : 0,
		                   static_cast<int>(voice.bank), voice.sample);
	}
	if (options.sample.bank == k_NoBank)
	{
		return k_NoChannel;
	}
	return PlaySoundEffect(options);
}

bool voices::IsSaying(bool alt, uint32_t textId)
{
	if (textId >= helptext::k_TextCount)
	{
		textId = 0;
	}
	const auto voice = Table().Get(textId);
	if (voice.sample == 0)
	{
		return false;
	}
	// the say owner's channel plays that sample of that bank
	return IsPlaying(Owner::Key(alt ? k_OwnerVoiceAlt : k_OwnerVoice), static_cast<int>(voice.sample), voice.bank);
}

void voices::CutByClick()
{
	// every narration sample of villagers.sad stops
	StopSoundEffect(0, Owner::Key(k_OwnerVoice), SfxBank::Villagers);
}

} // namespace openblack::audio
