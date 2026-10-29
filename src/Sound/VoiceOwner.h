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

#include <optional>

namespace openblack::audio
{

/// Whose sound a sample is to the voice rules. The rules for "once" and "restart" find the voice of the same sample
/// only for the same owner.
enum class VoiceOwner : uint8_t
{
	/// The owner entity of the sound, or no owner when it has none
	Entity,
	/// An owner that no other sound has, as a sound tag has: the sound never finds the voice of another sound
	OwnSound,
	/// A number that the caller gives. Number 0 is the same as no owner.
	Number,
};

/// The owner keys of the kinds above do not overlap. Key 0 is the sound without an owner.
constexpr uint64_t k_NumberOwnerKeys = uint64_t {1} << 61U;
constexpr uint64_t k_OwnSoundKeys = uint64_t {1} << 62U;

/// The owner key that the voice pool compares.
/// @param entity the integral value of the owner entity, empty for no owner
/// @param number the owner number for the number kind
/// @param serial a number that the caller gives a different value for each sound
[[nodiscard]] constexpr uint64_t VoiceOwnerKey(VoiceOwner kind, std::optional<uint32_t> entity, uint32_t number,
                                               uint64_t serial)
{
	switch (kind)
	{
	case VoiceOwner::OwnSound:
		return k_OwnSoundKeys + serial;
	case VoiceOwner::Number:
		return number == 0 ? 0 : k_NumberOwnerKeys + number;
	case VoiceOwner::Entity:
		break;
	}
	return entity.has_value() ? static_cast<uint64_t>(*entity) + 1 : 0;
}

} // namespace openblack::audio
