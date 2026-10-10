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
	/// The temple stands: one still to be built has no sites made for it, as their places are laid out from the
	/// standing temple's
	bool standing {false};
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
	/// A town asked to build it while it isn't built, and how much more than its other building the town wants to
	struct BuildRequest
	{
		entt::entity town {entt::null};
		float desireBoost {0.0f};
	};
	/// The towns asked to build it, the newest first; none once it is built
	std::vector<BuildRequest> buildRequests;
};

/// A worship site's altar, the tribe's own, made with the site and built with it
struct WorshipAltar
{
	entt::entity site {entt::null};
};

} // namespace openblack::ecs::components
