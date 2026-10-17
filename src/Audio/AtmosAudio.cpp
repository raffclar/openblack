/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "AtmosAudio.h"

#include <algorithm>
#include <exception>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include <PackFile.h>
#include <fmt/format.h>
#include <spdlog/spdlog.h>

#include "3D/LandIslandInterface.h"
#include "3D/Seabed.h"
#include "AudioManagerInterface.h"
#include "ECS/Systems/SkySystemInterface.h"
#include "ECS/Systems/UnderwaterSystemInterface.h"
#include "FileSystem/FileSystemInterface.h"
#include "Graphics/Underwater.h"
#include "InfoConstants.h"
#include "ListenerFrame.h"
#include "Locator.h"
#include "Resources/Loaders.h"
#include "Resources/ResourcesInterface.h"

namespace openblack::audio
{

namespace
{
// Ambience stays silent for the first turns of a level
constexpr uint32_t k_FirstAudibleTurn = 6;

// Banks switch to their evil samples at or below this alignment
constexpr double k_EvilAlignment = -0.6;

constexpr float k_FastFadeStep = 0.04f;
constexpr float k_SlowFadeStep = 0.02f;
constexpr float k_FastFadeMin = 0.1f;
constexpr float k_FastFadeMax = 0.8f;
constexpr float k_BankVolumeScale = 127.0f;

constexpr uint32_t k_GoodGroup = 1;
constexpr uint32_t k_EvilGroup = 2;
/// EXPERIMENT (not for merge): the sea's own loop, a 16-bit copy of the experiment's sound
constexpr std::string_view k_UnderwaterLoopPath = "E:/openblack/experiments/underwater/underwater_atmos_looped_s16.wav";
constexpr std::string_view k_UnderwaterBankName = "UnderwaterAtmos.sad";
constexpr int32_t k_UnderwaterSample = 1;
/// How loud the loop is just under the surface, of the bank's full volume
constexpr float k_UnderwaterVolume = 0.8f;
/// The crashes of the waves at the cliffs: how many feet of the cliffs are heard at once, how far apart they are at
/// least, and how far away a crash is heard
constexpr size_t k_CrashVoices = 3;
constexpr float k_CrashSpacing = 60.0f;
constexpr float k_CrashRange = 400.0f;
/// The spray's power of a crash at full volume, of the smallest crash heard, and of a crash that breaks
constexpr float k_CrashFull = 0.5f;
constexpr float k_CrashLeast = 0.05f;
constexpr float k_CrashBig = 0.22f;
/// The distance at which a crash is half as loud as at the cliff
constexpr float k_CrashNear = 60.0f;
/// How loud a crash is under the sea, of its volume above it, and the lowest volume played (0 to 127)
constexpr float k_CrashUnderwater = 0.25f;
constexpr uint32_t k_CrashQuietest = 3;
/// The first sample and the number of samples of a crash: the coast's waves that break, the sea's waves that lap
constexpr std::pair<int32_t, int32_t> k_ShoreCrashSamples {2, 18};
constexpr std::pair<int32_t, int32_t> k_SeaCrashSamples {2, 14};

int32_t Ftol(double value)
{
	if (value <= -2147483649.0 || value >= 2147483648.0)
	{
		return INT32_MIN;
	}
	return static_cast<int32_t>(value);
}
} // namespace

void AtmosAudio::Init()
{
	if (_registered)
	{
		Release();
	}

	auto& audio = Locator::audio::value();
	auto& fileSystem = Locator::filesystem::value();
	for (size_t i = 0; i < k_AtmosTypeCount; ++i)
	{
		_banks.at(i) = 0;
		const auto& bank = k_AtmosTypeInfos.at(i).bank;
		if (bank.empty())
		{
			continue;
		}

		const auto relativePath = fileSystem.GetPath<filesystem::Path::Audio>() / bank;
		if (!fileSystem.Exists(relativePath))
		{
			SPDLOG_LOGGER_WARN(spdlog::get("audio"), "Atmosphere bank {} not found", bank);
			continue;
		}
		const auto path = fileSystem.FindPath(relativePath);

		pack::PackFile soundPack;
		const auto result = soundPack.ReadFile(*fileSystem.GetData(path));
		if (result != pack::PackResult::Success)
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("audio"), "Unable to load atmosphere bank {}: {}", bank, pack::ResultToStr(result));
			continue;
		}
		_banks.at(i) = audio.AtmosRegisterBank(path.filename().string(), soundPack.GetAudioSampleHeaders(),
		                                       soundPack.GetAudioBankAtmosCount());
	}

	_targets.fill(0.0f);
	_current.fill(0.0f);
	RegisterUnderwaterBank();
	_registered = true;
}

void AtmosAudio::RegisterUnderwaterBank()
{
	_underwaterBank = 0;
	_underwaterTarget = 0.0f;
	_underwaterCurrent = 0.0f;
	const std::filesystem::path path {k_UnderwaterLoopPath};
	if (!std::filesystem::exists(path))
	{
		return;
	}
	// One looping bed in its own bank, of every group
	pack::AudioBankSampleHeader header {};
	const std::string_view name = "underwater_atmos_looped";
	std::copy(name.begin(), name.end(), header.name.begin());
	header.id = k_UnderwaterSample;
	header.sampleRate = 48000;
	header.atmosInterval = 0;
	header.atmosGroup = 0;
	header.loop = -1;
	auto& sounds = Locator::resources::value().GetSounds();
	const auto id = entt::hashed_string(fmt::format("{}/{}", k_UnderwaterBankName, k_UnderwaterSample).c_str()).value();
	if (!sounds.Contains(id))
	{
		try
		{
			auto bytes = Locator::filesystem::value().ReadAll(path);
			sounds.Load(id, resources::SoundLoader::FromBufferTag {}, header,
			            std::vector<std::vector<uint8_t>> {std::move(bytes)});
		}
		catch (const std::exception& error)
		{
			SPDLOG_LOGGER_ERROR(spdlog::get("audio"), "Unable to load the underwater loop: {}", error.what());
			return;
		}
	}
	auto& audio = Locator::audio::value();
	// The loop is also a sound bank of its own, so that the debug audio player lists it with the game's banks
	const std::string bankName {k_UnderwaterBankName};
	const auto& groups = audio.GetSoundGroups();
	if (!groups.contains(bankName) || std::ranges::find(groups.at(bankName).sounds, id) == groups.at(bankName).sounds.end())
	{
		audio.CreateSoundGroup(bankName);
		audio.AddToSoundGroup(bankName, id);
	}
	_underwaterBank = audio.AtmosRegisterBank(bankName, {header}, 1);
}

void AtmosAudio::Release()
{
	if (!_registered)
	{
		return;
	}

	auto& audio = Locator::audio::value();
	for (auto& bank : _banks)
	{
		if (bank != 0)
		{
			audio.AtmosReleaseBank(bank);
			bank = 0;
		}
	}
	if (_underwaterBank != 0)
	{
		audio.AtmosReleaseBank(_underwaterBank);
		_underwaterBank = 0;
	}
	_registered = false;
}

// From the camera's alignment, mapped to 0-1
void AtmosAudio::SetAlignment(float alignment)
{
	_alignment = CalculateAlignmentValue(alignment);
}

float AtmosAudio::CalculateAlignmentValue(float alignment)
{
	const auto goodness = static_cast<float>((static_cast<double>(alignment) + 1.0) * 0.5);
	auto clamped = 0.0f;
	if (goodness >= 0.0f)
	{
		clamped = goodness > 1.0f ? 1.0f : goodness;
	}
	const auto evilness = (1.0 - clamped) + (1.0 - clamped);
	return static_cast<float>((2.0 - evilness) - 1.0);
}

uint32_t AtmosAudio::GetGroup() const
{
	return static_cast<double>(_alignment) <= k_EvilAlignment ? k_EvilGroup : k_GoodGroup;
}

float AtmosAudio::CalculateSkyType(float time, const DayNightTimes& times)
{
	// Mirrored around midday
	if (time > 12.0f)
	{
		time = static_cast<float>(24.0 - time);
	}

	if (time < times.nightFull)
	{
		return 2.0f;
	}
	if (time < times.duskStart)
	{
		return static_cast<float>(
		    2.0 - ((static_cast<double>(time) - times.nightFull) / (static_cast<double>(times.duskStart) - times.nightFull)));
	}
	if (time < times.duskEnd)
	{
		return 1.0f;
	}
	if (time < times.dayFull)
	{
		return static_cast<float>(
		    1.0 - ((static_cast<double>(time) - times.duskEnd) / (static_cast<double>(times.dayFull) - times.duskEnd)));
	}
	return 0.0f;
}

void AtmosAudio::EndTurn(const TurnInputs& inputs)
{
	const SoundMap::Inputs mapInputs {
	    .receiver = inputs.camera,
	    .weather = inputs.weather,
	    .skyType = CalculateSkyType(Locator::skySystem::value().GetTime(), Locator::skySystem::value().GetDayNightTimes()),
	};
	_soundMap.Update(Locator::terrainSystem::value(), Locator::infoConstants::value().sound, mapInputs);
	if (inputs.paused || inputs.turn < k_FirstAudibleTurn)
	{
		Locator::audio::value().AtmosProcess(false);
		return;
	}
	ContinueTurn(inputs);
}

void AtmosAudio::ContinueTurn(const TurnInputs& inputs)
{
	// The game's audio turn, which runs for every turn of the temple, however early
	auto& audio = Locator::audio::value();
	CopyTargets(inputs.inCitadel);
	ProcessAtmosBanks();
	audio.StopOutOfRangeEmitters();
	if (!inputs.videoPlaying)
	{
		audio.AtmosProcess(true);
	}
}

// The sound map's volumes, or none inside the temple
void AtmosAudio::CopyTargets(bool inCitadel)
{
	// EXPERIMENT (not for merge): under the sea the sea's own loop takes the place of the land's banks, a little
	// louder deeper down
	const bool underwater =
	    !inCitadel && Locator::underwaterSystem::has_value() && Locator::underwaterSystem::value().IsEyeUnder();
	_underwaterTarget =
	    underwater ? std::min(1.0f, k_UnderwaterVolume + Locator::underwaterSystem::value().GetEyeDepth() * 0.006f) : 0.0f;
	if (inCitadel || underwater)
	{
		_targets.fill(0.0f);
		return;
	}
	_targets = _soundMap.GetVolumes();
}

void AtmosAudio::ProcessAtmosBanks()
{
	auto& audio = Locator::audio::value();
	const auto group = GetGroup();
	for (size_t i = 0; i < k_AtmosTypeCount; ++i)
	{
		audio.AtmosSetGroup(_banks.at(i), group);

		const auto [current, bankVolume] = StepBankVolume(_current.at(i), _targets.at(i));
		_current.at(i) = current;
		audio.AtmosSetBankVolume(_banks.at(i), bankVolume);
	}
	if (_underwaterBank != 0)
	{
		const auto [current, bankVolume] = StepBankVolume(_underwaterCurrent, _underwaterTarget);
		_underwaterCurrent = current;
		audio.AtmosSetBankVolume(_underwaterBank, bankVolume);
	}
}

void AtmosAudio::UpdateCliffCrashes()
{
	if (!_registered || !Locator::underwaterSystem::has_value())
	{
		return;
	}
	const auto& system = Locator::underwaterSystem::value();
	const float seconds = system.GetSeconds();
	const bool moved = seconds != _crashSeconds;
	_crashSeconds = seconds;
	const auto& seabed = system.GetSeabed();
	auto& audio = Locator::audio::value();
	if (!system.IsEnabled() || seabed.cliffFoot.empty())
	{
		_crashFeet.clear();
		return;
	}
	const auto listener = audio.GetListener();
	const auto heard = graphics::underwater::HeardCliffFeet(seabed.cliffFoot, {listener.origin.x, listener.origin.z},
	                                                        k_CrashVoices, k_CrashSpacing, k_CrashRange);
	const auto& waves = system.GetWaves();
	const float seaLevel = system.GetSeaLevel();
	const float under = system.IsEyeUnder() ? k_CrashUnderwater : 1.0f;
	std::vector<std::pair<size_t, float>> next;
	next.reserve(heard.size());
	for (const auto index : heard)
	{
		const auto& foot = seabed.cliffFoot.at(index);
		const glm::vec2 xz {foot.x, foot.y};
		const float cliff = graphics::underwater::ShoreCliff(
		    graphics::underwater::ShoreAt(seabed.shore, seabed.cliff, seabed.size, seabed.spacing, xz));
		const auto crash = graphics::underwater::CliffCrashAt(xz, seconds, waves, seaLevel, cliff);
		next.emplace_back(index, crash.number);
		// A foot newly heard waits for its next crash; nothing starts while the clock stands still
		const auto before = std::ranges::find(_crashFeet, index, &std::pair<size_t, float>::first);
		if (!moved || before == _crashFeet.end() || before->second == crash.number || crash.power < k_CrashLeast)
		{
			continue;
		}
		const glm::vec3 place {foot.x, seaLevel, foot.y};
		const float distance = glm::distance(place, listener.origin);
		const float loudness = std::min(crash.power / k_CrashFull, 1.0f) * k_CrashNear / (k_CrashNear + distance) * under;
		const auto volume = static_cast<uint32_t>(std::clamp(loudness * k_BankVolumeScale, 0.0f, k_BankVolumeScale));
		if (volume < k_CrashQuietest)
		{
			continue;
		}
		// A big crash is a wave breaking on the shore, a small one a wave that slaps the rock
		const bool big = crash.power >= k_CrashBig;
		const auto bank = _banks.at(static_cast<size_t>(big ? AtmosType::Coastal : AtmosType::Sea));
		if (bank == 0)
		{
			continue;
		}
		const auto [first, count] = big ? k_ShoreCrashSamples : k_SeaCrashSamples;
		const auto pick = static_cast<int32_t>(graphics::underwater::Hash({crash.seed, 29.0f}) * static_cast<float>(count));
		// Placed by its direction only: the volume above already has the distance in it
		auto towards = ToListenerFrame(place, listener.origin, listener.forward, listener.up);
		const float length = glm::length(towards);
		towards = length > 0.0f ? towards / length : glm::vec3(0.0f, 1.0f, 0.0f);
		const auto sample = first + std::min(pick, count - 1);
		SPDLOG_LOGGER_DEBUG(spdlog::get("audio"),
		                    "Cliff crash at ({:.0f}, {:.0f}): power {:.2f}, {} away, {} sample {} volume {}", foot.x, foot.y,
		                    crash.power, distance, big ? "shore" : "ocean", sample, volume);
		audio.AtmosPlayOnce(bank, sample, volume, towards);
	}
	_crashFeet = std::move(next);
}

std::pair<float, int32_t> AtmosAudio::StepBankVolume(float current, float target)
{
	const auto step = current > k_FastFadeMax || current <= k_FastFadeMin ? k_SlowFadeStep : k_FastFadeStep;

	double volume;
	if (target <= current)
	{
		volume = static_cast<double>(current) - step;
		if (!(static_cast<double>(target) <= volume))
		{
			volume = target;
		}
	}
	else
	{
		volume = static_cast<double>(current) + step;
		if (static_cast<double>(target) < volume)
		{
			volume = target;
		}
	}
	return {static_cast<float>(volume), Ftol(volume * k_BankVolumeScale)};
}

} // namespace openblack::audio
