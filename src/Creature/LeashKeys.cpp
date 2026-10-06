/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "LeashKeys.h"

using namespace openblack;
using namespace openblack::creature_leash;
using input::BindableActionMap;

std::optional<LeashKey> creature_leash::KeyFor(BindableActionMap action)
{
	switch (action)
	{
	case BindableActionMap::LEASH_UNLEASH_CREATURE:
		return LeashKey::Leash;
	case BindableActionMap::PREVIOUS_LEASH:
		return LeashKey::PreviousLeash;
	case BindableActionMap::NEXT_LEASH:
		return LeashKey::NextLeash;
	default:
		return std::nullopt;
	}
}

std::optional<LeashType> creature_leash::StepKnown(LeashType selected, const std::bitset<k_Types.size()>& known, bool forwards)
{
	const auto count = k_Types.size();
	// From the learning leash when none is picked
	auto index = IndexOf(selected).value_or(*IndexOf(LeashType::Rope));
	for (size_t tries = 1; tries < count; ++tries)
	{
		index = forwards ? (index + 1) % count : (index + count - 1) % count;
		if (known.test(index))
		{
			return k_Types.at(index);
		}
	}
	return std::nullopt;
}

KeyCommand creature_leash::CommandFor(LeashKey key, const KeyState& state)
{
	using Kind = KeyCommand::Kind;
	const auto knows = [&state](LeashType type) {
		const auto index = IndexOf(type);
		return index.has_value() && state.known.test(*index);
	};
	switch (key)
	{
	case LeashKey::Leash:
		if (state.worn && state.tied)
		{
			return {.kind = Kind::UntieToHand};
		}
		if (state.worn)
		{
			return {.kind = Kind::TakeOff};
		}
		if (!knows(LeashType::Rope))
		{
			return {};
		}
		return {.kind = Kind::PutOn, .type = knows(state.selected) ? state.selected : LeashType::Rope};
	case LeashKey::PreviousLeash:
	case LeashKey::NextLeash:
		if (const auto next = StepKnown(state.selected, state.known, key == LeashKey::NextLeash))
		{
			return {.kind = Kind::ChangeType, .type = *next};
		}
		return {};
	}
	return {};
}
