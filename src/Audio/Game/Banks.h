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

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include <entt/core/fwd.hpp>

#include "Audio/Game/BankTables.h"

// The banks of openblack's audio, registered as the game's audio system does (src/Audio/Game, layer 2 of the audio
// engine, docs/bw1-notes/audio.md). The only place that reads a .sad: the sample banks (k_SfxBankPaths and the rest of
// Audio\, the atmos ones among them), their anim effect tables, the waves of the dialogue banks read at their first
// play and the music banks of MUSIC_TYPE. audio::Init loads the sample banks (banks::LoadAll); the music banks are
// registered at their first use (music::GetBank).

namespace openblack::audio
{
class MusicBank;
class Sound;

/// Any registered bank (the 11 of k_SfxBankPaths, the 14 atmos ones, a creature's). 0 = none.
using BankId = uint16_t;
inline constexpr BankId k_NoBank = 0;

/// Every bank Game reads gets an id, by its path (the original file system ignores case, so
/// "audio/dialogue/Villagers.sad" of k_SfxBankPaths is villagers.sad on disk). `group` is the sound group of its
/// samples ("<file>.sad", their ids "<file>.sad/<n>"). The 11 types of k_SfxBankPaths are recognised by path.
BankId RegisterBank(const std::filesystem::path& path, std::string_view group);
/// The size of a registered bank's sample table, as Game read it (the samples are 1..count)
void SetBankSampleCount(BankId bank, int samples);
/// The bank's number of samples (advisor::SaySentence): 0 for no bank
[[nodiscard]] int BankSampleCount(BankId bank);
/// The bank of a type (k_NoBank for type 0 or a bank not loaded)
[[nodiscard]] BankId Bank(SfxBank type);
/// The bank registered for a path (case-insensitive, any separator; matched on its end), k_NoBank if none
[[nodiscard]] BankId FindBank(std::string_view path);
/// The sound group of a bank ("InGame.sad"), empty for k_NoBank
[[nodiscard]] std::string BankGroup(BankId bank);
/// The sound id of sample `number` (1-based, the .sad's sample number) of a bank: "<group>/<number>" (0 when the bank
/// is unknown)
[[nodiscard]] entt::id_type SampleId(BankId bank, int number);

namespace banks
{

/// The 11 types of k_SfxBankPaths, the 14 atmos banks and the creature banks: every sample bank (.sad) under Audio\,
/// all from audio::Init (approximate: the original registers the atmos banks later, when the game finishes its
/// initialisation; nothing plays in between). The dialogue banks (types 6..10, Audio\Dialogue) keep only their headers,
/// each wave read from the file at its first play (ReadWave); the others keep their bytes in memory (approximate: the
/// original reads every bank that way).
/// A music bank (its waves are ".mpg") is left to MusicBankOf. Nothing without the file system and the resources (the
/// tests).
void LoadAll();

/// The banks registered, 1..Count()
[[nodiscard]] size_t Count();
/// A registered bank's path as RegisterBank got it (lower case, '/' separators), empty for k_NoBank
[[nodiscard]] std::string Path(BankId bank);
/// The sound ids of a bank's samples LoadAll loaded (its empty records left out), in the .sad's order
[[nodiscard]] const std::vector<entt::id_type>& Samples(BankId bank);

/// The bytes of a wave left in its .sad (Sound::waveFile, the dialogue banks: only the headers are read at the
/// registration, and a wave at its first play). False when the sound has no such wave or the file cannot be read.
[[nodiscard]] bool ReadWave(const Sound& sound, std::vector<uint8_t>& out);

/// The bank of a MUSIC_TYPE (k_MusicBanks), registered on its first use (through MusicBank::Register; the original
/// registers the 85 at once when the audio system is created). nullptr if it is not installed (WELCOME_DANCE).
/// `registeredNow` is set when this call registered it.
MusicBank* MusicBankOf(MusicType type, bool& registeredNow);
/// The music banks released (as the music's close, after the music thread)
void ReleaseMusicBanks();

} // namespace banks
} // namespace openblack::audio
