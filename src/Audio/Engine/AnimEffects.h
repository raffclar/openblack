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

#include <array>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

#include <entt/core/fwd.hpp>

#include "Audio/Engine/SamplePlay.h"
#include "Audio/Game/Banks.h"

namespace openblack::pack
{
class PackFile;
}

// The audio library's anim effects: the "anim effect" tables of each bank, read once when the bank is registered (the
// attribute rows with their width and count, and the wave number lists), and the two ways to play an anim effect (by
// sample, by key). The game's filters in front of them are audio::PlayAnimationEffect (Audio.h).

namespace openblack::audio
{

/// The 5 attributes a caller passes: {voice, 2, group, surface, soundId} for the animations, {size, alignment, 1,
/// surface, action} for ParticleSound
using AnimKey = std::array<int32_t, 5>;
/// The 4th argument of PlayAnimationEffect: 0 plays one sample of the row, any other value goes to the key variant:
/// 1 stops the row's samples, the others release their loops
enum class AnimAction : int32_t
{
	Play = 0,
	Stop = 1,
	Release = 2
};

/// The anim effect tables of a .sad bank: LHAudioAnimArrayTable rows of 5 attribute columns plus an index into
/// LHAudioWaveNumTable, whose lists are {count, sample numbers...} (the miracles' AnimEffectBank, moved here unchanged:
/// AnimEffectBank.h keeps that name as an alias)
struct AnimEffectTable
{
	static constexpr int32_t k_Wildcard = 0x0FFF0000;

	/// The per-sample playback fields that matter to the callers, from the bank's sample header (only counted when
	/// their override bit is set; the defaults are those of the sample play options)
	struct Sample
	{
		int32_t loops {0};    ///< Override bit 0x40: -1 = forever
		int32_t playMode {3}; ///< Override bit 0x400: 1 new channel, 2 nothing if already playing for that object, 3 restart
		float maxDistance {0};
	};

	std::string name;                         ///< the sound ids are "<name>/<sample number>"
	std::vector<std::array<int32_t, 6>> rows; ///< LHAudioAnimArrayTable
	std::vector<int32_t> waves;               ///< LHAudioWaveNumTable
	std::unordered_map<int32_t, Sample> samples;

	/// Reads the tables of a bank file
	void Load(const pack::PackFile& file);
	/// The list of the matching row with the most exact columns (the later one on a tie): its sample numbers, empty if
	/// no row matches
	[[nodiscard]] std::vector<int32_t> FindList(const std::array<int32_t, 5>& key) const;
	/// The resource id of one of its samples ("<name>/<sample>")
	[[nodiscard]] entt::id_type SoundId(int32_t sample) const;
	[[nodiscard]] const Sample* FindSample(int32_t sample) const;
};

namespace anim_effects
{

/// The farthest an anim effect plays, stops or releases (a distance, compared with the caller's)
inline constexpr float k_MaxDistance = 800.0f;

/// The bank's tables, read from its file once as the bank is registered (Game's bank loop). A bank without the two
/// blocks has none.
void RegisterTables(BankId bank, const pack::PackFile& file);
/// The tables of a registered bank (nullptr when it has none)
[[nodiscard]] const AnimEffectTable* Tables(BankId bank);
/// Every bank's tables forgotten (the audio closing, the tests)
void Clear();

/// The row of the key (FindList), then its list: 0 for none or an empty one, the only sample of a list of 1, else
/// list[Random(count)]
[[nodiscard]] int Number(const AnimKey& key, BankId bank);

/// Plays one anim effect sample: nothing while the audio is off, for no bank, farther than k_MaxDistance or than the
/// sample's max distance (the raw .sad value); options 3D, extra3DFlag off, track, owner, the sample, min / max with their
/// caller bits 0x80 / 0x100 only when > 0; the channel allocated and the point of the game's 3D function for the owner
/// (the camera for none; nothing for the atmos owner or an unavailable thing); then the sample plays. `distance` is the
/// caller's (the camera's distance to the object), not measured here.
Channel Play(Owner owner, float distance, int sample, bool track, BankId bank, float minDistance, float maxDistance);

/// The key variant: the same gates (audio on, bank, k_MaxDistance), the key's row (none: nothing); action 0 plays one
/// of its samples as Play (the game never calls it with 0); 1 stops, for each sample of the list, the first channel of
/// (bank, owner, sample); any other value releases its loop
Channel PlayKey(Owner owner, float distance, const AnimKey& key, AnimAction action, bool track, BankId bank, float minDistance,
                float maxDistance);

} // namespace anim_effects
} // namespace openblack::audio
