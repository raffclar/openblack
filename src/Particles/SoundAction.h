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

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "PSysFile.h"

// The SOUND_ACTION property of the spell files and its value (docs/bw1-notes/particles.md, "Sound of the particles").

namespace openblack::psys
{

/// A particle sound action: the SoundAction.h value and the attribute slots handed to the .sad anim effect table
struct SoundAction
{
	// The bits of `flags`
	static constexpr uint8_t k_Looping = 0x01; ///< LOOPING: the update keeps re-issuing it while the atom lives
	static constexpr uint8_t k_Delayed = 0x02; ///< set by code (thunder): starts distance / 347 s later
	/// SOFTRELEASE: when the atom goes, the loop ends its pass instead of being cut
	static constexpr uint8_t k_SoftRelease = 0x04;
	/// USESURFACE: the surface slot is the sound map's surface type under the atom
	static constexpr uint8_t k_UseSurface = 0x08;
	static constexpr uint8_t k_SnapToGround = 0x20; ///< set by code (thunder): the position's height is the land's

	int32_t action {-1}; ///< the sound action, -1 = NO_SOUND
	int32_t surface {1};
	int32_t size {2};      ///< size class: 1 large, 2 medium, 3 small (set by the rule that plays it)
	int32_t alignment {2}; ///< replaced by the owner player's discrete alignment when there is one
	int32_t fadeStep {0};  ///< volume taken off per turn once the atom is gone (0..127 units)
	uint8_t flags {0};
};

/// The property's value: the action from its name (unknown or NO_SOUND -> -1) and the LOOPING, SOFTRELEASE and
/// USESURFACE bits; ONLYONE is read and dropped. The slots keep the defaults above. A missing property is the
/// default: NO_SOUND.
[[nodiscard]] SoundAction ReadSoundAction(const Object& object, std::string_view key);

/// The value of a name in Data\SoundAction.h, parsed once; -1 if the name is not there
[[nodiscard]] int32_t SoundActionValue(std::string_view name);
/// The name of a value, for the logs ("?" if none)
[[nodiscard]] std::string SoundActionName(int32_t value);
/// The names of an enum header: each name's value (the last one wins) and each value's first name
struct EnumNames
{
	std::map<std::string, int32_t, std::less<>> byName;
	std::map<int32_t, std::string> byValue;
};
/// The enum header Data\<file>, read once into the resource caches; empty without a file system. `loadedNow` is set
/// when this call read it
[[nodiscard]] const EnumNames& EnumHeaderNames(const std::string& file, bool* loadedNow = nullptr);
/// Data\<file> read and parsed, uncached (EnumHeaderLoader's read); empty, with a warning, when it cannot be read
[[nodiscard]] std::shared_ptr<EnumNames> ReadEnumHeader(const std::string& file);
/// The enum of a C header: each NAME with its "= value", or the previous value + 1
[[nodiscard]] std::vector<std::pair<std::string, int32_t>> ParseEnumHeader(std::string_view text);

} // namespace openblack::psys
