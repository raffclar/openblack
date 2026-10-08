/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <entt/entity/entity.hpp>
#include <glm/vec3.hpp>

#include "Audio/Audio.h"

// The script's sound effects: what the script functions do between their POPs and the audio.
// CHLApi pops the arguments in the original's order and calls these. The bank argument is an AUDIO_SFX_BANK_TYPE that
// the original indexes its bank table with unchecked: (approximate) a value outside 0..10 plays / stops nothing here.

namespace openblack::audio::script_sound
{

/// PLAY_SOUND_EFFECT (043): default play options with the bank of that type, the sample, the owner = the sample number
/// itself (Owner::Key), is3D = withPos, no track, the point (copied even when withPos is 0) and keep the PCM; then
/// PlaySoundEffect with no field of the caller's mask set (the .sad decides). Returns the channel (the original
/// returns nothing).
Channel PlaySoundEffect(int sample, int bank, glm::vec3 position, bool withPosition);

/// STOP_SOUND_EFFECT (424): not isSay -> stop the sample id of owner id in that bank (the channel of
/// PLAY_SOUND_EFFECT). isSay: id is a HELP_TEXT and `bank` is not used; the narrator of the help text
/// [0 < id < count ? id : 0] and the voice {bank, sample} of the say table (id unchecked); the good spirit (narrator 2)
/// -> stop(sample, k_OwnerAdvisor, bank), else stop(sample, k_OwnerVoiceStop, bank) and stop(sample, k_OwnerVoiceAlt,
/// bank). The 2D voice of RUN_TEXT / SAY without alt (k_OwnerVoice) is never stopped.
void StopSoundEffect(bool isSay, uint32_t id, int bank);

/// GAME_SOUND_PLAYING (450): whether the sample plays in that bank for the owner = the sample number
[[nodiscard]] bool GameSoundPlaying(int sample, int bank);

/// ATTACH_SOUND_TAG (447): a sound tag on the thing (sample, track = threeD != 0, mode 2, loops 0, flag 0,
/// is3D = threeD, bank, delay 0; no offset). Nothing for no thing.
tags::TagId AttachSoundTag(bool threeD, int sample, int bank, entt::entity thing);

/// DETACH_SOUND_TAG (448): the thing's tags of that sample and bank are removed (every matching tag goes, a playing
/// loop finishes its pass). Nothing for no thing.
void DetachSoundTag(int sample, int bank, entt::entity thing);

/// The AUDIO_SFX_BANK_TYPE of a script value, SfxBank::None outside 1..10 (approximate, see above)
[[nodiscard]] SfxBank BankType(int bank);

} // namespace openblack::audio::script_sound
