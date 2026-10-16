/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <array>
#include <optional>
#include <span>

#include <PackFile.h>

namespace openblack::audio
{

/// A request for a voice to play a bank sample on
struct VoiceRequest
{
	/// What the sample plays for: 0 for nothing, k_AtmosOwner for the atmosphere, else a key of a thing
	uint64_t owner {0};
	uint32_t bank {0};
	uint32_t sample {0};
	/// The voice group of the sample in its bank, 0 for none
	uint16_t group {0};
	/// The priority of the sample in its bank header, 0 to 9999. A sample of priority 0 cannot take a voice that plays.
	uint16_t priority {0};
	pack::AudioBankLoop playType {pack::AudioBankLoop::Restart};
};

/// One of the voices of the sample player, with what it played last
struct VoiceSlot
{
	bool playing {false};
	uint64_t owner {0};
	uint32_t bank {0};
	/// Empty when the voice forgot its sample at its end
	std::optional<uint32_t> sample;
	uint16_t group {0};
	/// The priority of the sample that the voice played last. The voice keeps it after the sample stops.
	uint16_t priority {0};
};

/// What a request gets from the voice pool
struct VoiceChoice
{
	/// The voice to use, empty when there is no voice for the sample
	std::optional<size_t> slot;
	/// False when the sample plays once and plays already in the slot: then do not start it again
	bool start {false};
};

/// The fixed set of voices that all the bank samples share: the sound effects, the animation effects, the speech
/// and the atmosphere. The music plays on voices of its own.
class VoicePool
{
public:
	/// The game asks for 16 voices
	static constexpr size_t k_VoiceCount = 16;
	/// The owner key of the atmosphere samples
	static constexpr uint64_t k_AtmosOwner = ~uint64_t {0};

	/// The voice for a new sample, by its play type:
	/// - Overlap takes the first free voice.
	/// - Once does nothing when the same sample, or one of its voice group, plays for the same owner.
	/// - Restart uses the first voice that played the same sample for the same owner, or else the first one of the
	///   voice group. If there is no such voice, it takes the first free voice.
	/// - Each play type that finds no voice, and None at once, takes a voice from a sample of lower priority.
	[[nodiscard]] VoiceChoice Choose(const VoiceRequest& request) const;
	/// Puts the sample on the voice
	void Occupy(size_t slot, const VoiceRequest& request);
	/// Marks the voice as free when its sample stops. A voice that played an MPEG sample forgets the sample.
	void Finish(size_t slot, bool forgetSample);
	[[nodiscard]] std::span<const VoiceSlot, k_VoiceCount> Slots() const { return _slots; }

private:
	/// The voice of the lowest priority if that priority is less than the priority of the request
	[[nodiscard]] std::optional<size_t> Steal(uint16_t priority) const;
	[[nodiscard]] std::optional<size_t> FirstFree() const;

	std::array<VoiceSlot, k_VoiceCount> _slots {};
};

} // namespace openblack::audio
