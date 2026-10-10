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

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <entt/entity/entity.hpp>

#include "3D/MapCoords.h"
#include "Enums.h"

namespace openblack::dance
{
struct DanceFile;
}

namespace openblack::ecs::components
{

/// A group of a dance's dancers, dancing one part of it
struct DanceGroup
{
	/// Which dancers a group takes: men, women, or both (anything not a villager counts as both)
	static constexpr uint32_t k_Men = 1;
	static constexpr uint32_t k_Women = 2;
	static constexpr uint32_t k_AnySex = k_Men | k_Women;

	std::string name;
	/// Its dancers in the order they joined: a dancer's place in the group is its place here
	std::vector<entt::entity> dancers;
	/// Taking a fixed number of dancers: `quota` of them. Else a share of the dance's dancers: `quota` in a hundred.
	bool limited {false};
	uint32_t quota {100};
	/// Its share of each round of newcomers, its quota over what the shared groups' quotas have in common
	uint32_t weight {100};
	/// How many of its dancers it took as a group with a fixed number
	uint32_t limitedDancers {0};
	uint32_t danceType {0};
	uint32_t sexes {k_AnySex};
	/// Its shape about the dance's place; a group with one keeps its membership
	uint32_t formation {0};
};

/// A dance's groups, in the order they were made, and the order newcomers try them in
struct DanceGroups
{
	std::vector<DanceGroup> all;
	/// The groups with a fixed number of dancers, tried first in this order
	std::vector<std::size_t> limited;
	/// The groups sharing the dancers between them, in this order, a newcomer going to each in turn by its weight
	std::vector<std::size_t> shared;
	/// Where the round of newcomers is, and how long a round is
	uint8_t round {0};
	uint32_t roundLength {0};
};

/// A dance villagers gather to dance about a place: the one at each worship site, a town's, or one a script makes. Its
/// dancers dance in groups as its dance file sets out, on the dance's clock.
struct Dance
{
	enum class State : uint8_t
	{
		Dancing,
		Stopped,
	};

	/// Which dance of the info table it is
	DanceInfo type {DanceInfo::None};
	/// The place it is danced about
	map_coords::MapCoords place;
	/// What it is danced for or about, such as a worship site or a script's villager; the dance ends when that is gone
	entt::entity owner {entt::null};
	State state {State::Stopped};
	/// Whether it starts by itself once its dancers have come
	bool autostart {false};
	/// Made by a script, rather than by a town or a site
	bool madeByScript {false};
	/// How fast it is asked to go, 0 to 1, as the worship sets it; every dance is made at a quarter
	float speed {0.0f};
	/// How many times their keyed speed its groups move at, from its speed in steps of 0.4: 0 to 4
	float rate {1.0f};
	/// The rate it last started dancing at: a new rate while dancing starts the dance over
	float dancingRate {1.0f};
	/// Its clock, a beat a game turn, starting over once the dance's loop has gone by
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
	/// Its groups, their order of choosing and its dancers
	DanceGroups groups;
	/// Its choreography; none when its file can't be read
	std::shared_ptr<const dance::DanceFile> file;
};

/// A living dancing in a dance: the dance and its group there; its place in the group is its place in the group's list
struct Dancer
{
	entt::entity dance {entt::null};
	std::size_t group {0};
};

} // namespace openblack::ecs::components
