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

#include <entt/core/fwd.hpp>
#include <entt/entity/entity.hpp>

#include "Enums.h"

namespace openblack::ecs::components
{

/// A dance villagers gather to dance, such as the one at each worship site, on its own entity at its centre. Its dancers
/// move in groups as its dance file sets out, on the dance's clock.
struct Dance
{
	enum class State : uint8_t
	{
		Dancing,
		Stopped,
	};

	/// Which dance of the info table it is
	DanceInfo type {DanceInfo::None};
	/// What it is danced for, such as a worship site; the dance ends when that is gone
	entt::entity owner {entt::null};
	/// The dance file it follows (the dance resource cache's id)
	entt::id_type file {0};
	State state {State::Stopped};
	/// How fast it is asked to go, 0 to 1, as the worship sets it
	float speed {0.0f};
	/// How many times their keyed speed its groups move at, from its speed in steps of 0.4: 0 to 4
	float rate {1.0f};
	/// The rate it last started dancing at: a new rate while dancing starts the dance over
	float dancingRate {1.0f};
	/// Its clock, in game turns, starting over once the dance's loop has gone by
	float clock {0.0f};
	/// How many lengths of the clock it takes before it starts over, from its file
	uint32_t loopLength {1};
	/// How many turns it lasts once started, 0 for as long as it is wanted
	uint32_t duration {0};
	/// The turn it last started
	uint32_t startTurn {0};
	/// The villagers dancing it, and those on their way to
	uint32_t dancers {0};
	uint32_t onTheirWay {0};
	/// The turn its first dancer came, once one has
	std::optional<uint32_t> firstDancerTurn;
};

} // namespace openblack::ecs::components
