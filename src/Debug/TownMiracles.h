/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <optional>
#include <string>
#include <vector>

#include <entt/entity/entity.hpp>

#include "Debug/TownMiracleRules.h"
#include "Enums.h"

/// A debug tool that gives a town a miracle or takes it away, through the same steps as the land's scripts. A town that
/// gets a miracle and has no worship site can then have its site, which goes up for the town's builders to build.
/// Only the debug window and the inspector use this tool. A normal game never does.
namespace openblack::debug::town_miracles
{

struct Request
{
	entt::entity town {entt::null};
	MagicType type {MagicType::None};
	/// Give the miracle, or take it away
	bool give {true};
	/// When it gives a power-up level, give the plain miracle too, as a land file line does
	bool withPlain {true};
	/// When it takes a miracle, take the plain miracle and every power-up level, as a creature that steals it does
	bool allLevels {false};
	/// When it gives a miracle to a town with no worship site, let the town have its site, as the scripts do
	bool openSite {true};
	/// Let the first land have the site too, which it never has in a normal game
	bool anyLand {true};
};

struct Outcome
{
	/// The magic types that changed, in the order they changed
	std::vector<MagicType> changed;
	/// The town's worship site after the change, none when it has none
	entt::entity site {entt::null};
	/// The tool made the site now
	bool siteIsNew {false};
	/// Why the town has no site, when the tool tried to let it have one
	std::optional<town_miracle_tool::NoSite> noSite;
	/// Why nothing changed, empty when the request was good
	std::string error;
};

/// Gives a town a miracle or takes it away, and lets the town have its worship site if the request asks
Outcome Apply(const Request& request);

/// The town's magic types, held now, by magic type
[[nodiscard]] std::vector<MagicType> HeldBy(entt::entity town);

/// The name of a magic type as the info tables give it
[[nodiscard]] std::string MagicName(MagicType type);

} // namespace openblack::debug::town_miracles
