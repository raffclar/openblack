/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ScriptSound.h"

#include <spdlog/spdlog.h>

#include "Audio/Services/Voices.h"
#include "Common/HelpText.h"

using namespace openblack;
using namespace openblack::audio;

SfxBank script_sound::BankType(int bank)
{
	if (bank <= static_cast<int>(SfxBank::None) || bank >= static_cast<int>(SfxBank::_COUNT))
	{
		return SfxBank::None;
	}
	return static_cast<SfxBank>(bank);
}

Channel script_sound::PlaySoundEffect(int sample, int bank, glm::vec3 position, bool withPosition)
{
	// the default play options (those of sample_play::Options)
	PlayOptions options;
	options.sample = {Bank(BankType(bank)), sample};
	options.owner = Owner::Key(static_cast<uint32_t>(sample)); // the sample number
	options.is3D = withPosition;
	options.track = false;
	options.position = position;
	options.keepPcm = true;
	if (SfxTrace())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("audio"), "SFX: PLAY_SOUND_EFFECT({}, {}, ({:.1f}, {:.1f}, {:.1f}), {})", sample, bank,
		                   position.x, position.y, position.z, withPosition ? 1 : 0);
	}
	if (options.sample.bank == k_NoBank)
	{
		return k_NoChannel;
	}
	return audio::PlaySoundEffect(options);
}

void script_sound::StopSoundEffect(bool isSay, uint32_t id, int bank)
{
	if (!isSay)
	{
		// stop the sample id of owner id in that bank
		audio::StopSoundEffect(static_cast<int>(id), Owner::Key(id), BankType(bank));
		return;
	}
	// the narrator of the help text [0 < id < count ? id : 0] (helptext::GetEntry has that rule)
	const int32_t narrator = helptext::GetEntry(id).narrator;
	// the say table's bank and sample, not bound-checked: (approximate) an id past the table has no voice here
	const auto voice = voices::Table().Get(id);
	const int sample = static_cast<int>(voice.sample);
	if (narrator == helptext::k_NarratorGoodSpirit)
	{
		audio::StopSoundEffect(sample, Owner::Key(k_OwnerAdvisor), voice.bank);
		return;
	}
	// the voice's stop owner, then the alternative one
	audio::StopSoundEffect(sample, Owner::Key(k_OwnerVoiceStop), voice.bank);
	audio::StopSoundEffect(sample, Owner::Key(k_OwnerVoiceAlt), voice.bank);
}

bool script_sound::GameSoundPlaying(int sample, int bank)
{
	// the sample as the owner
	return audio::IsPlaying(Owner::Key(static_cast<uint32_t>(sample)), sample, BankType(bank));
}

tags::TagId script_sound::AttachSoundTag(bool threeD, int sample, int bank, entt::entity thing)
{
	// nothing without a thing
	if (thing == entt::null)
	{
		return tags::k_NoTag;
	}
	// track = threeD != 0, mode 2, loops 0, flag 0, is3D threeD, bank, delay 0
	return tags::Create(thing, sample, threeD, 2, 0, false, threeD, BankType(bank), 0);
}

void script_sound::DetachSoundTag(int sample, int bank, entt::entity thing)
{
	// nothing without a thing
	if (thing == entt::null)
	{
		return;
	}
	tags::Remove(thing, sample, BankType(bank));
}
