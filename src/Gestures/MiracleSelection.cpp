/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "MiracleSelection.h"

using namespace openblack;
using namespace openblack::gesture;

namespace
{
size_t IndexOf(GestureType gesture)
{
	return static_cast<size_t>(gesture);
}
} // namespace

bool MiracleSelection::Open(GestureType opening, std::span<const SelectionOffer> offers)
{
	_waiting = {};
	_candidates = {};
	bool found = false;
	for (const auto& offer : offers)
	{
		const auto seed = static_cast<size_t>(offer.seed);
		if (offer.chain.front() == GestureType::None || offer.chain.front() != opening || seed >= _candidates.size())
		{
			continue;
		}
		_candidates.at(seed) = offer.chain;
		if (IndexOf(offer.chain.at(1)) < _waiting.size())
		{
			_waiting.at(IndexOf(offer.chain.at(1))) = true;
		}
		found = true;
	}
	_stage = 1;
	_open = found;
	_seconds = 0.0f;
	_openedWith = opening;
	return found;
}

void MiracleSelection::Close()
{
	_open = false;
}

void MiracleSelection::Update(float seconds, float timeoutSeconds)
{
	if (!_open)
	{
		return;
	}
	_seconds += seconds;
	if (_seconds > timeoutSeconds)
	{
		_open = false;
	}
}

std::vector<SelectionWait> MiracleSelection::Waiting() const
{
	std::vector<SelectionWait> waits;
	if (!_open)
	{
		return waits;
	}
	// Gesture 0 is no gesture, so it is never waited for
	for (size_t gesture = 1; gesture < _waiting.size(); ++gesture)
	{
		if (!_waiting.at(gesture))
		{
			continue;
		}
		SelectionWait wait {.gesture = static_cast<GestureType>(gesture)};
		for (size_t seed = 0; seed < _candidates.size(); ++seed)
		{
			const auto& chain = _candidates.at(seed);
			if (chain.has_value() && IndexOf(chain->at(_stage)) == gesture)
			{
				wait.seed = static_cast<SpellSeedType>(seed);
			}
		}
		waits.push_back(wait);
	}
	return waits;
}

MiracleSelection::Outcome MiracleSelection::Recognised(GestureType gesture,
                                                       const std::function<bool(SpellSeedType)>& stillValid)
{
	Outcome outcome;
	for (size_t seed = 0; seed < _candidates.size(); ++seed)
	{
		auto& chain = _candidates.at(seed);
		if (!chain.has_value())
		{
			continue;
		}
		const auto type = static_cast<SpellSeedType>(seed);
		if (chain->at(_stage) != gesture || !stillValid(type))
		{
			// This miracle does not have the gesture, so the selection stops to offer it
			chain.reset();
			continue;
		}
		// The first miracle that matches starts the time limit again
		if (outcome.kind == Outcome::Kind::Unmatched)
		{
			outcome.kind = Outcome::Kind::Advanced;
			_seconds = 0.0f;
		}
		// The last gesture of the chain chooses the miracle
		if (_stage + 1 == k_SelectionStages || chain->at(_stage + 1) == GestureType::None)
		{
			_open = false;
			return {.kind = Outcome::Kind::Chosen, .seed = type};
		}
	}

	// The miracles that stay wait for the next gesture of their chains. A miracle that is no longer valid stops.
	++_stage;
	_waiting = {};
	// A chain has no stage after its last. The game's chains always end before it.
	if (_stage >= k_SelectionStages)
	{
		_open = false;
		return outcome;
	}
	bool any = false;
	for (size_t seed = 0; seed < _candidates.size(); ++seed)
	{
		auto& chain = _candidates.at(seed);
		if (!chain.has_value())
		{
			continue;
		}
		if (!stillValid(static_cast<SpellSeedType>(seed)))
		{
			chain.reset();
			continue;
		}
		if (IndexOf(chain->at(_stage)) < _waiting.size())
		{
			_waiting.at(IndexOf(chain->at(_stage))) = true;
		}
		any = true;
	}
	if (!any)
	{
		_open = false;
	}
	return outcome;
}
