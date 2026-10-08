/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Banks.h"

#include <cctype>
#include <cstdlib>

#include <algorithm>
#include <array>
#include <fstream>
#include <memory>
#include <utility>
#include <vector>

#include <PackFile.h>
#include <entt/core/hashed_string.hpp>
#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include "Audio/Device/Sound.h"
#include "Audio/Engine/AnimEffects.h"
#include "Audio/Engine/MusicBank.h"
#include "Audio/Services/Voices.h"
#include "ECS/Systems/AudioStateInterface.h"
#include "FileSystem/FileSystemInterface.h"
#include "Locator.h"
#include "Resources/Loaders.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::audio;

namespace
{
struct BankEntry
{
	std::string path; ///< lower case, '/' separators
	std::string group;
	int samples {0};                   ///< The .sad's sample table size
	std::vector<entt::id_type> sounds; ///< the samples LoadAll loaded (the debug panel, the atmos banks)
};

struct BanksState
{
	std::vector<BankEntry> banks; ///< BankId - 1
	/// The bank of each type
	std::array<BankId, static_cast<size_t>(SfxBank::_COUNT)> types {};
	/// The bank of each music type, and whether a type was tried already
	std::array<std::unique_ptr<MusicBank>, static_cast<size_t>(MusicType::_COUNT)> music;
	std::array<bool, static_cast<size_t>(MusicType::_COUNT)> musicTried {};
};

/// The Banks state (Locator::audioState)
BanksState& BanksData()
{
	return openblack::Locator::audioState::value().Get<BanksState>();
}

bool Trace()
{
	static const bool k_Trace = std::getenv("OPENBLACK_AUDIO_TRACE") != nullptr;
	return k_Trace;
}

std::string Normalised(std::string_view path)
{
	std::string text(path);
	std::replace(text.begin(), text.end(), '\\', '/');
	std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	return text;
}

bool EndsWith(const std::string& text, const std::string& tail)
{
	return text.size() >= tail.size() && text.compare(text.size() - tail.size(), tail.size(), tail) == 0;
}

/// The path is the one of a type of k_SfxBankPaths (any case, any separator)
bool IsBankOfType(const std::filesystem::path& path, SfxBank type)
{
	return EndsWith(Normalised(path.generic_string()), Normalised(SfxBankPath(type)));
}

/// The .sad's headers read, and its first wave is an ".mpg" (a music bank); its waves are not read
bool IsMusicBank(std::istream& stream)
{
	pack::PackFile headers;
	if (headers.ReadAudioHeaders(stream) != pack::PackResult::Success || headers.GetAudioSampleHeaders().empty())
	{
		return false;
	}
	return std::filesystem::path(headers.GetAudioSampleHeaders()[0].name.data()).extension() == ".mpg";
}

/// Registers one sample bank, as Game read the .sad: its headers (and, but for the dialogue banks, its waves), its anim
/// effect tables, its samples as resources
void LoadBank(const std::filesystem::path& f)
{
	auto& fileSystem = Locator::filesystem::value();
	auto& soundManager = Locator::resources::value().GetSounds();

	pack::PackFile soundPack;
	SPDLOG_LOGGER_DEBUG(spdlog::get("audio"), "Opening sound pack {}", f.filename().string());
	// The dialogue banks of k_SfxBankPaths (types 6..10, Audio\Dialogue) are registered as the original does: only the
	// headers are read, and each wave is read from the file at its first play (Sound::waveFile). The other banks keep
	// their bytes in memory (approximate: the original reads every bank that way).
	bool onDemand = false;
	for (const auto bank :
	     {SfxBank::HelpSprites, SfxBank::Villagers, SfxBank::VillagersBanter, SfxBank::SpellDialogue, SfxBank::Guidance})
	{
		onDemand = onDemand || IsBankOfType(f, bank);
	}
	// A music bank (its first wave is ".mpg") is registered by the music (MusicBankOf, k_MusicBanks), not here: its
	// headers are enough to know it, so its waves are never read. A bank whose headers do not read goes on, and
	// reports its error as before.
	if (!onDemand && IsMusicBank(*fileSystem.GetData(f)))
	{
		return;
	}
	const auto result =
	    onDemand ? soundPack.ReadAudioHeaders(*fileSystem.GetData(f)) : soundPack.ReadFile(*fileSystem.GetData(f));
	if (result != pack::PackResult::Success)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("game"), "Unable to load sound pack {}: {}", f.filename().string(),
		                    pack::ResultToStr(result));
		return;
	}
	const auto& audioHeaders = soundPack.GetAudioSampleHeaders();
	if (audioHeaders.empty())
	{
		SPDLOG_LOGGER_WARN(spdlog::get("audio"), "Empty sound pack found for {}. Skipping", f.filename().string());
		return;
	}
	auto soundName = std::filesystem::path(audioHeaders[0].name.data());

	auto groupName = f.filename().string();

	// The wave names of the dialogue banks of the voice table (k_SfxBankPaths 6, 7, 10)
	for (const auto bank : {SfxBank::Villagers, SfxBank::HelpSprites, SfxBank::Guidance})
	{
		if (IsBankOfType(f, bank))
		{
			std::vector<std::string> names;
			names.reserve(audioHeaders.size());
			for (const auto& header : audioHeaders)
			{
				names.emplace_back(header.name.begin(), std::find(header.name.begin(), header.name.end(), '\0'));
			}
			voices::SetBankSampleNames(bank, std::move(names));
		}
	}

	// A music bank (its waves are ".mpg"): the music registers it by MUSIC_TYPE (MusicBankOf, k_MusicBanks)
	if (soundName.extension() == ".mpg")
	{
		return;
	}
	// The bank of its samples (the 11 types of k_SfxBankPaths by path, any case)
	const auto bankId = RegisterBank(f, groupName);
	SetBankSampleCount(bankId, static_cast<int>(audioHeaders.size()));
	// Its anim effect tables, read once here (audio::anim_effects)
	anim_effects::RegisterTables(bankId, soundPack);
	// the samples' bytes, moved into their sounds (the pack is not used for them afterwards)
	auto audioData = soundPack.TakeAudioSamplesData();
	auto& sounds = BanksData().banks[bankId - 1].sounds;
	sounds.clear();
	for (size_t i = 0; i < audioHeaders.size(); i++)
	{
		soundName = std::filesystem::path(audioHeaders[i].name.data());
		if (onDemand ? audioHeaders[i].size == 0 : audioData[i].empty())
		{
			SPDLOG_LOGGER_WARN(spdlog::get("audio"), "Empty sound buffer found for {}. Skipping", soundName.string());
			continue; // the next ones still load (spells.sad has an empty entry 31 before 32..88)
		}

		const auto stringId = fmt::format("{}/{}", groupName, audioHeaders[i].id);
		const entt::id_type id = entt::hashed_string(stringId.c_str());
		std::vector<std::vector<uint8_t>> buffer;
		if (!onDemand)
		{
			buffer.emplace_back(std::move(audioData[i]));
		}
		SPDLOG_LOGGER_DEBUG(spdlog::get("audio"), "Loading sound {}: {}", stringId, audioHeaders[i].name.data());
		soundManager.Load(id, resources::SoundLoader::FromBufferTag {}, audioHeaders[i], std::move(buffer));
		soundManager.Handle(id)->bank = bankId;
		if (onDemand)
		{
			soundManager.Handle(id)->waveFile = f;
			soundManager.Handle(id)->waveOffset = soundPack.GetAudioWaveDataOffset() + audioHeaders[i].offset;
			soundManager.Handle(id)->waveSize = audioHeaders[i].size;
		}
		sounds.emplace_back(id);
	}
}
} // namespace

// ---- the registry ---------------------------------------------------------------------------------------------------

BankId audio::RegisterBank(const std::filesystem::path& path, std::string_view group)
{
	auto& state = BanksData();
	const auto normalised = Normalised(path.generic_string());
	for (size_t i = 0; i < state.banks.size(); ++i)
	{
		if (state.banks[i].path == normalised)
		{
			return static_cast<BankId>(i + 1);
		}
	}
	state.banks.push_back({normalised, std::string(group)});
	const auto id = static_cast<BankId>(state.banks.size());
	// The slot of a type is filled once, with the bank of its path
	for (size_t type = 1; type < k_SfxBankPaths.size(); ++type)
	{
		if (state.types[type] == k_NoBank && EndsWith(normalised, Normalised(k_SfxBankPaths[type])))
		{
			state.types[type] = id;
		}
	}
	return id;
}

void audio::SetBankSampleCount(BankId bank, int samples)
{
	auto& state = BanksData();
	if (bank != k_NoBank && bank <= state.banks.size())
	{
		state.banks[bank - 1].samples = samples;
	}
}

int audio::BankSampleCount(BankId bank)
{
	auto& state = BanksData();
	return bank != k_NoBank && bank <= state.banks.size() ? state.banks[bank - 1].samples : 0;
}

BankId audio::Bank(SfxBank type)
{
	auto& state = BanksData();
	const auto index = static_cast<size_t>(type);
	return index < state.types.size() ? state.types[index] : k_NoBank;
}

BankId audio::FindBank(std::string_view path)
{
	auto& state = BanksData();
	const auto wanted = Normalised(path);
	for (size_t i = 0; i < state.banks.size(); ++i)
	{
		if (EndsWith(state.banks[i].path, wanted))
		{
			return static_cast<BankId>(i + 1);
		}
	}
	return k_NoBank;
}

std::string audio::BankGroup(BankId bank)
{
	auto& state = BanksData();
	return bank != k_NoBank && bank <= state.banks.size() ? state.banks[bank - 1].group : std::string {};
}

entt::id_type audio::SampleId(BankId bank, int number)
{
	const auto group = BankGroup(bank);
	if (group.empty())
	{
		return 0;
	}
	const auto key = fmt::format("{}/{}", group, number);
	return entt::hashed_string(key.c_str()).value();
}

// ---- loading --------------------------------------------------------------------------------------------------------

void banks::LoadAll()
{
	if (!Locator::filesystem::has_value() || !Locator::resources::has_value())
	{
		return;
	}
	auto& fileSystem = Locator::filesystem::value();
	// every sound pack in the Audio directory, in the file system's order
	fileSystem.Iterate(fileSystem.GetPath<filesystem::Path::Audio>(), true, [](const std::filesystem::path& f) {
		if (f.extension() == ".sad")
		{
			LoadBank(f);
		}
	});
}

size_t banks::Count()
{
	return BanksData().banks.size();
}

std::string banks::Path(BankId bank)
{
	auto& state = BanksData();
	return bank != k_NoBank && bank <= state.banks.size() ? state.banks[bank - 1].path : std::string {};
}

const std::vector<entt::id_type>& banks::Samples(BankId bank)
{
	auto& state = BanksData();
	static const std::vector<entt::id_type> k_None;
	return bank != k_NoBank && bank <= state.banks.size() ? state.banks[bank - 1].sounds : k_None;
}

bool banks::ReadWave(const Sound& sound, std::vector<uint8_t>& out)
{
	out.clear();
	if (sound.waveFile.empty() || sound.waveSize == 0)
	{
		return false;
	}
	// The wave is read from the bank's file when the sample first plays
	std::unique_ptr<std::istream> stream;
	if (Locator::filesystem::has_value())
	{
		stream = Locator::filesystem::value().GetData(sound.waveFile);
	}
	else
	{
		stream = std::make_unique<std::ifstream>(sound.waveFile, std::ios::binary);
	}
	if (!stream || !*stream)
	{
		return false;
	}
	out.resize(sound.waveSize);
	stream->seekg(static_cast<std::streamoff>(sound.waveOffset));
	stream->read(reinterpret_cast<char*>(out.data()), static_cast<std::streamsize>(out.size()));
	if (stream->gcount() != static_cast<std::streamsize>(out.size()))
	{
		out.clear();
		return false;
	}
	if (Trace())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("audio"), "Wave of {} read from {} ({} bytes at {})", sound.name,
		                   sound.waveFile.filename().string(), sound.waveSize, sound.waveOffset);
	}
	return true;
}

// ---- the music banks ------------------------------------------------------------------------------------------------

MusicBank* banks::MusicBankOf(MusicType type, bool& registeredNow)
{
	auto& state = BanksData();
	registeredNow = false;
	const auto index = static_cast<size_t>(type);
	if (index >= state.music.size())
	{
		return nullptr;
	}
	if (!state.musicTried[index])
	{
		state.musicTried[index] = true;
		const auto& entry = k_MusicBanks[index];
		if (!entry.path.empty() && Locator::filesystem::has_value())
		{
			try
			{
				const auto path = Locator::filesystem::value().FindPath(std::filesystem::path(entry.path));
				state.music[index] = MusicBank::Register(path);
			}
			catch (const std::exception& e)
			{
				if (auto logger = spdlog::get("audio"))
				{
					SPDLOG_LOGGER_WARN(logger, "music: {} ({}): {}", entry.name, entry.path, e.what());
				}
			}
		}
		registeredNow = state.music[index] != nullptr;
	}
	return state.music[index].get();
}

void banks::ReleaseMusicBanks()
{
	auto& state = BanksData();
	for (auto& bank : state.music)
	{
		bank.reset();
	}
	state.musicTried.fill(false);
}
