/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <entt/entity/entity.hpp>

#include "Enums.h"

namespace openblack::ecs::systems
{

/// The worship sites by the players' temples (components::WorshipSite): one for each tribe a player has towns of, made
/// as the player's towns come to need one, and the link between each of the player's towns and its tribe's site
class WorshipSiteSystemInterface
{
public:
	virtual ~WorshipSiteSystemInterface() = default;

	/// A temple is made: it takes its worship sites' places, facing the way it was set down. Made already standing, its
	/// player's towns are given their worship sites at once, each asked to build its site.
	virtual void AddTemple(entt::entity temple, float facing, bool standing) = 0;
	/// A temple still to be built is finished: its player's towns are given their worship sites, each asked to build
	/// its site
	virtual void TempleBuilt(entt::entity temple) = 0;
	/// A person joins a town: the town's first, with its player's temple, has it given its tribe's worship site
	virtual void PersonJoinedTown(entt::entity town) = 0;
	/// The land has been laid out: each town of a player with a temple and no worship site is given one
	virtual void LandLaidOut() = 0;
	/// The land's script puts down a worship site already built: the player's site of the tribe, made if it isn't
	/// there, is built and given to the first of the player's towns of that tribe. The site, none without the player's
	/// temple or a place for it.
	virtual entt::entity MakeBuiltSite(PlayerNames player, Tribe tribe) = 0;
	/// A script lets a town or a temple have worship sites made, or stops it
	virtual void SetCanHaveSites(entt::entity townOrTemple, bool can) = 0;

	/// A site is built up by a share of the whole, its altar with it. Reaching the whole, no town is asked to build it
	/// any more.
	virtual void BuildBy(entt::entity site, float share) = 0;
	/// Whether a site stands wholly built
	[[nodiscard]] virtual bool IsBuilt(entt::entity site) const = 0;
	/// The temple of a player, none without one
	[[nodiscard]] virtual entt::entity TempleOf(PlayerNames player) const = 0;
};

} // namespace openblack::ecs::systems
