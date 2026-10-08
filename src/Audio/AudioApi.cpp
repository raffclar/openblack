/*******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The functions of Audio.h: each one hands its call to the audio service (Locator::audio)

#include <stdexcept>
#include <utility>

#include "Audio/Audio.h"
#include "Audio/AudioManagerInterface.h"
#include "Audio/GameQueries.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::audio;

namespace
{
audio::AudioManagerInterface& Manager()
{
	if (!Locator::audio::has_value())
	{
		throw std::logic_error("audio: no audio service (Locator::audio is empty)");
	}
	return Locator::audio::value();
}
} // namespace

BankId audio::CreatureBank(std::string_view species)
{
	return Manager().CreatureBank(species);
}

float audio::MaxDistance(Sample sample)
{
	return Manager().MaxDistance(sample);
}

std::optional<Sample> audio::FindSample(BankId bank, std::string_view wavName)
{
	return Manager().FindSample(bank, wavName);
}

void audio::RegisterObject(uint32_t id, ObjectPositionFn position)
{
	Manager().RegisterObject(id, std::move(position));
}

void audio::UnregisterObject(uint32_t id)
{
	Manager().UnregisterObject(id);
}

uint32_t audio::NewObjectId()
{
	return Manager().NewObjectId();
}

Channel audio::PlaySoundEffect(const PlayOptions& options)
{
	return Manager().PlaySoundEffect(options);
}

Channel audio::PlaySoundEffect(Owner owner, int sample, int mode, int loops, bool extra3DFlag, bool is3D, SfxBank bank)
{
	return Manager().PlaySoundEffect(owner, sample, mode, loops, extra3DFlag, is3D, bank);
}

Channel audio::PlaySoundEffect(Owner owner, int sample, int mode, int loops, bool extra3DFlag, bool is3D, BankId bank)
{
	return Manager().PlaySoundEffect(owner, sample, mode, loops, extra3DFlag, is3D, bank);
}

Channel audio::PlaySoundEffectAt(Owner owner, glm::vec3 position, int sample, int mode, int loops, bool extra3DFlag, bool is3D,
                                 SfxBank bank)
{
	return Manager().PlaySoundEffectAt(owner, position, sample, mode, loops, extra3DFlag, is3D, bank);
}

Channel audio::PlaySoundEffectAt(Owner owner, glm::vec3 position, int sample, int mode, int loops, bool extra3DFlag, bool is3D,
                                 BankId bank)
{
	return Manager().PlaySoundEffectAt(owner, position, sample, mode, loops, extra3DFlag, is3D, bank);
}

Channel audio::PlaySoundEffectAt(Owner owner, glm::vec3 position, glm::vec3 offset, int sample, bool track, int mode, int loops,
                                 bool extra3DFlag, bool is3D, BankId bank)
{
	return Manager().PlaySoundEffectAt(owner, position, offset, sample, track, mode, loops, extra3DFlag, is3D, bank);
}

void audio::StopSoundEffect(int sample, Owner owner, SfxBank bank)
{
	Manager().StopSoundEffect(sample, owner, bank);
}

void audio::StopSoundEffect(int sample, Owner owner, BankId bank)
{
	Manager().StopSoundEffect(sample, owner, bank);
}

void audio::StopAllSoundEffects()
{
	Manager().StopAllSoundEffects();
}

void audio::ReleaseLoop(Owner owner, int sample, SfxBank bank)
{
	Manager().ReleaseLoop(owner, sample, bank);
}

void audio::ReleaseLoop(Owner owner, int sample, BankId bank)
{
	Manager().ReleaseLoop(owner, sample, bank);
}

bool audio::IsPlaying(Owner owner, int sample, SfxBank bank)
{
	return Manager().IsPlaying(owner, sample, bank);
}

bool audio::IsPlaying(Owner owner, int sample, BankId bank)
{
	return Manager().IsPlaying(owner, sample, bank);
}

bool audio::IsPlaying(Owner owner, SfxBank bank)
{
	return Manager().IsPlaying(owner, bank);
}

void audio::SetPitch(BankId bank, Owner owner, int sample, int percent)
{
	Manager().SetPitch(bank, owner, sample, percent);
}

void audio::SetVolume(Channel channel, int volume)
{
	Manager().SetVolume(channel, volume);
}

bool audio::IsPlaying(Channel channel)
{
	return Manager().IsPlaying(channel);
}

Channel audio::PlayingChannel(Owner owner, BankId bank)
{
	return Manager().PlayingChannel(owner, bank);
}

int audio::Volume(Channel channel)
{
	return Manager().Volume(channel);
}

int audio::NextCounter(Counter counter)
{
	return Manager().NextCounter(counter);
}

uint32_t audio::TickCount()
{
	return Manager().TickCount();
}

bool audio::SfxTrace()
{
	return Manager().SfxTrace();
}

void audio::MusicStop(int fade)
{
	Manager().MusicStop(fade);
}

void audio::LeaveCitadel()
{
	Manager().LeaveCitadel();
}

void audio::Init(GameQueries queries)
{
	Manager().Init(std::move(queries));
}

void audio::Shutdown()
{
	Manager().Shutdown();
}

void audio::ProcessTurn()
{
	Manager().ProcessTurn();
}

void audio::ProcessCitadelTurn()
{
	Manager().ProcessCitadelTurn();
}

void audio::Paused()
{
	Manager().Paused();
}

void audio::UpdateFrame()
{
	Manager().UpdateFrame();
}

void audio::OnThingDeleted(entt::entity thing)
{
	Manager().OnThingDeleted(thing);
}

void audio::ClearMap()
{
	Manager().ClearMap();
}

void audio::OnFocus(bool active)
{
	Manager().OnFocus(active);
}

void audio::SetSampleMainVolume(int volume)
{
	Manager().SetSampleMainVolume(volume);
}

int audio::SampleMainVolume()
{
	return Manager().SampleMainVolume();
}

Channel audio::PlayAnimationEffect(Owner owner, float distance, const AnimKey& key, AnimAction action, BankId bank, bool track,
                                   float minDistance, float maxDistance)
{
	return Manager().PlayAnimationEffect(owner, distance, key, action, bank, track, minDistance, maxDistance);
}

bool audio::SoundExists()
{
	return Manager().SoundExists();
}
