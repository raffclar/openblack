/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VoicePool.h"

#include <algorithm>
#include <iterator>
#include <limits>

namespace openblack::audio
{

namespace
{
[[nodiscard]] bool SameSample(const VoiceSlot& slot, const VoiceRequest& request)
{
	return slot.bank == request.bank && slot.owner == request.owner && slot.sample == request.sample;
}

[[nodiscard]] bool SameGroup(const VoiceSlot& slot, const VoiceRequest& request)
{
	return request.group != 0 && slot.bank == request.bank && slot.owner == request.owner && slot.group == request.group;
}
} // namespace

VoiceChoice VoicePool::Choose(const VoiceRequest& request) const
{
	const auto index = [this](auto iter) { return static_cast<size_t>(std::distance(_slots.begin(), iter)); };
	const auto take = [](std::optional<size_t> slot) { return VoiceChoice {.slot = slot, .start = slot.has_value()}; };

	switch (request.playType)
	{
	case pack::AudioBankLoop::Overlap:
		if (const auto free = FirstFree())
		{
			return take(free);
		}
		break;
	case pack::AudioBankLoop::Once:
	{
		// The voice keeps the same sample, and the new sample does not start
		const auto playing = std::ranges::find_if(_slots, [&request](const VoiceSlot& slot) {
			return slot.playing && (SameSample(slot, request) || SameGroup(slot, request));
		});
		if (playing != _slots.end())
		{
			return {.slot = index(playing), .start = false};
		}
		if (const auto free = FirstFree())
		{
			return take(free);
		}
		break;
	}
	case pack::AudioBankLoop::Restart:
	{
		// A voice of the same sample matches also after the sample stopped
		const auto same = std::ranges::find_if(_slots, [&request](const VoiceSlot& slot) { return SameSample(slot, request); });
		if (same != _slots.end())
		{
			return take(index(same));
		}
		const auto group = std::ranges::find_if(_slots, [&request](const VoiceSlot& slot) { return SameGroup(slot, request); });
		if (group != _slots.end())
		{
			return take(index(group));
		}
		if (const auto free = FirstFree())
		{
			return take(free);
		}
		break;
	}
	default:
		break;
	}
	return take(Steal(request.priority));
}

void VoicePool::Occupy(size_t slot, const VoiceRequest& request)
{
	_slots.at(slot) = VoiceSlot {
	    .playing = true,
	    .owner = request.owner,
	    .bank = request.bank,
	    .sample = request.sample,
	    .group = request.group,
	    .priority = request.priority,
	};
}

void VoicePool::Finish(size_t slot, bool forgetSample)
{
	auto& voice = _slots.at(slot);
	voice.playing = false;
	if (forgetSample)
	{
		voice.sample.reset();
	}
}

std::optional<size_t> VoicePool::Steal(uint16_t priority) const
{
	if (priority == 0)
	{
		return std::nullopt;
	}
	// A voice of priority 0 goes first. Else the first voice of the lowest priority goes. The voice can be free or
	// not, because the voice keeps its priority after its sample stops.
	std::optional<size_t> lowest;
	auto lowestPriority = std::numeric_limits<uint32_t>::max();
	for (size_t i = 0; i < _slots.size(); ++i)
	{
		const auto voicePriority = static_cast<uint32_t>(_slots[i].priority);
		if (voicePriority == 0)
		{
			lowest = i;
			lowestPriority = 0;
			break;
		}
		if (voicePriority < lowestPriority)
		{
			lowest = i;
			lowestPriority = voicePriority;
		}
	}
	if (!lowest.has_value() || lowestPriority >= priority)
	{
		return std::nullopt;
	}
	return lowest;
}

std::vector<size_t> VoicePool::VoicesToStop(uint64_t owner, uint32_t bank, uint32_t sample) const
{
	std::vector<size_t> voices;
	if (sample == 0)
	{
		for (size_t i = 0; i < _slots.size(); ++i)
		{
			if (_slots[i].playing && _slots[i].bank == bank && _slots[i].owner == owner)
			{
				voices.push_back(i);
			}
		}
		return voices;
	}
	if (const auto first = FirstKeeping(owner, bank, sample); first.has_value() && _slots.at(*first).playing)
	{
		voices.push_back(*first);
	}
	return voices;
}

bool VoicePool::IsPlaying(uint64_t owner, uint32_t bank, uint32_t sample) const
{
	const auto first = FirstKeeping(owner, bank, sample);
	return first.has_value() && _slots.at(*first).playing;
}

std::optional<size_t> VoicePool::FirstKeeping(uint64_t owner, uint32_t bank, uint32_t sample) const
{
	const auto keeping = std::ranges::find_if(_slots, [owner, bank, sample](const VoiceSlot& slot) {
		return slot.bank == bank && slot.owner == owner && slot.sample == sample;
	});
	if (keeping == _slots.end())
	{
		return std::nullopt;
	}
	return static_cast<size_t>(std::distance(_slots.begin(), keeping));
}

std::optional<size_t> VoicePool::FirstFree() const
{
	const auto free = std::ranges::find_if(_slots, [](const VoiceSlot& slot) { return !slot.playing; });
	if (free == _slots.end())
	{
		return std::nullopt;
	}
	return static_cast<size_t>(std::distance(_slots.begin(), free));
}

} // namespace openblack::audio
