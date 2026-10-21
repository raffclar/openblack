/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "AtmosAudio.h"

#include <PackFile.h>
#include <spdlog/spdlog.h>

#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "AudioManagerInterface.h"
#include "Common/X87.h"
#include "ECS/Systems/SkySystemInterface.h"
#include "FileSystem/FileSystemInterface.h"
#include "InfoConstants.h"
#include "Locator.h"

namespace openblack::audio
{

namespace
{
// Ambience stays silent for the first turns of a level
constexpr uint32_t k_FirstAudibleTurn = 6;

// Banks switch to their evil samples at or below this alignment. The game keeps it as a double
constexpr double k_EvilAlignment = -0.6;

constexpr float k_FastFadeStep = 0.04f;
constexpr float k_SlowFadeStep = 0.02f;
constexpr float k_FastFadeMin = 0.1f;
constexpr float k_FastFadeMax = 0.8f;
constexpr float k_BankVolumeScale = 127.0f;

constexpr uint32_t k_GoodGroup = 1;
constexpr uint32_t k_EvilGroup = 2;
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

		// Only the headers of the samples are read now: each sample is read from the file when it is wanted
		pack::PackFile soundPack;
		const auto result = soundPack.ReadAudioHeaders(*fileSystem.GetData(path));
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
	_registered = true;
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
	_registered = false;
}

// From the camera's alignment, mapped to 0-1
void AtmosAudio::SetAlignment(float alignment)
{
	_alignment = CalculateAlignmentValue(alignment);
}

float AtmosAudio::CalculateAlignmentValue(float alignment)
{
	const auto goodness = (alignment + 1.0f) * 0.5f;
	auto clamped = 0.0f;
	if (goodness >= 0.0f)
	{
		clamped = goodness > 1.0f ? 1.0f : goodness;
	}
	const auto evilness = (1.0f - clamped) + (1.0f - clamped);
	return (2.0f - evilness) - 1.0f;
}

uint32_t AtmosAudio::GetGroup() const
{
	return x87::LessEqual(_alignment, k_EvilAlignment) ? k_EvilGroup : k_GoodGroup;
}

float AtmosAudio::CalculateSkyType(float time, const DayNightTimes& times)
{
	// Mirrored around midday
	if (time > 12.0f)
	{
		time = 24.0f - time;
	}

	if (time < times.nightFull)
	{
		return 2.0f;
	}
	if (time < times.duskStart)
	{
		return 2.0f - ((time - times.nightFull) / (times.duskStart - times.nightFull));
	}
	if (time < times.duskEnd)
	{
		return 1.0f;
	}
	if (time < times.dayFull)
	{
		return 1.0f - ((time - times.duskEnd) / (times.dayFull - times.duskEnd));
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
	// Then the listener moves to the camera, and every placed sound is heard from where it is now
	audio.ProcessTurn();
	if (!inputs.videoPlaying)
	{
		audio.AtmosProcess(true);
	}
}

// The sound map's volumes, or none inside the temple
void AtmosAudio::CopyTargets(bool inCitadel)
{
	if (inCitadel)
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
}

std::pair<float, int32_t> AtmosAudio::StepBankVolume(float current, float target)
{
	const auto step = current > k_FastFadeMax || current <= k_FastFadeMin ? k_SlowFadeStep : k_FastFadeStep;

	float volume;
	if (target <= current)
	{
		volume = current - step;
		if (!(target <= volume))
		{
			volume = target;
		}
	}
	else
	{
		volume = current + step;
		if (target < volume)
		{
			volume = target;
		}
	}
	return {volume, map_coords::FtoL(volume * k_BankVolumeScale)};
}

} // namespace openblack::audio
