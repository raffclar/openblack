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
#include <vector>

#include <entt/core/fwd.hpp>
#include <entt/entity/entity.hpp>

#include "Enums.h"

namespace openblack::ecs::components
{

/// The worship sites around a player's temple, on the temple's entity: up to six, one for each tribe the player has
/// towns of, each standing in a place of its own around the temple
struct CitadelWorship
{
	static constexpr size_t k_Places = 6;

	/// The site standing in each place around the temple, none where there is none
	std::array<entt::entity, k_Places> sites {entt::null, entt::null, entt::null, entt::null, entt::null, entt::null};
	/// Which way the temple was set down to face, in radians: the places go round from there
	float facing {0.0f};
	/// A script has stopped the temple from having new worship sites made
	bool cannotMakeSites {false};
	/// The temple stands. A temple that is not yet built makes no site for its towns. Only a site that a land script
	/// makes is made for it.
	bool standing {false};
	/// How many sites have been made for it: each new site is numbered by it, and the newest go first when the temple
	/// loses them
	uint32_t sitesMade {0};
	/// How loud the strain of its sites sounds, 0 to 1: each turn it goes towards the most strain of its sites. Only the
	/// temple of the player at this machine changes it.
	float strainSound {0.0f};
};

/// A worship site: the place by a player's temple where the people of one of the player's tribes go to worship. It
/// stands at the temple's own place, its model laid out to one side of the temple and turned to face out from its
/// place around it.
struct WorshipSite
{
	/// The temple it belongs to, and the temple's player
	entt::entity temple {entt::null};
	PlayerNames player {PlayerNames::NEUTRAL};
	/// The tribe whose people worship at it
	Tribe tribe {Tribe::NORSE};
	/// Its place around the temple, 0 to 5
	uint8_t place {0};
	/// Which of its temple's sites it was, counting the first made as 0
	uint32_t made {0};
	/// Which way it faces, in radians: the temple's facing and a seventh of a turn for each place round
	float facing {0.0f};
	/// The towns whose people worship at it, the newest first
	std::vector<entt::entity> towns;
	/// The altar of its tribe at the head of it
	entt::entity altar {entt::null};
	/// The dance its worshippers dance round the altar, whose dancers chant
	entt::entity dance {entt::null};
	/// The pot its worshippers eat from
	entt::entity foodPot {entt::null};
	/// Food counted as on its way to it from its towns. Nothing adds to it; food taken off it for supplies that never
	/// came can leave it below none.
	int32_t foodOnTheWay {0};
	/// Its worshippers, the newest first: the villagers dancing at it (from setting off to their places in the dance)
	/// and those worshipping hidden in it. The player's come into the temple with the player.
	std::vector<entt::entity> worshippers;
	/// A town asked to build it while it isn't built, and how much more than its other building the town wants to
	struct BuildRequest
	{
		entt::entity town {entt::null};
		float desireBoost {0.0f};
	};
	/// The towns asked to build it, the newest first; none once it is built
	std::vector<BuildRequest> buildRequests;
	/// What its three need flags show, 0 to 1, as each of its turns works them out: how much it wants food, how much
	/// its dancers want rest, and how many hide in it
	std::array<float, 3> needs {0.0f, 0.0f, 0.0f};
	/// Its three need flags, in the same order
	std::array<entt::entity, 3> needFlags {entt::null, entt::null, entt::null};
	/// The particle effect of the motes of prayer power that fly from its spell icons to the miracles they top up, 0 for
	/// none. It is made with the first mote, and goes when it ends.
	uint32_t manaPath {0};
};

/// A need flag of a worship site (see need_flag). The frames move it, thus only the frames read this.
struct WorshipSiteFlag
{
	entt::entity site {entt::null};
	/// Which of the site's needs it shows: 0 food, 1 rest, 2 hiding
	uint8_t need {0};
	/// How far up it flies, as a share of its full height, and how fast it moves
	float level {0.0f};
	float speed {0.0f};
	/// Whether it showed in the last frame
	bool shown {false};
};

/// A villager going to worship, or worshipping, at its town's worship site
struct Worshipper
{
	/// Counted among its town's worshippers: it dances, hides, eats or rests at the site
	bool atWorship {false};
	/// Set off for the site and not there yet: counted among those the site's dance waits for
	bool onTheWay {false};
};

/// A worship site's altar, the tribe's own, made with the site and built with it
struct WorshipAltar
{
	entt::entity site {entt::null};
	/// The model of the altar. The altar has this size and shape on the map from when the game makes the site, but the
	/// game shows the model only when the town builds the site.
	entt::id_type model {0};
	/// How much of the altar is built. Each share of work on its site also builds the altar by that share, the work
	/// that mends the site too. Nothing else changes it.
	float built {0.0f};
};

} // namespace openblack::ecs::components
