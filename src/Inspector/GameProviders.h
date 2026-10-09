/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <span>
#include <string_view>

#include <entt/meta/context.hpp>

namespace openblack::inspector
{
class GameProvider;
class Inspector;
class RunTargetInterface;
class WorldEditInterface;

/// Which provider's query answers for each of the locator's services: every service the locator holds is listed here,
/// so that a test finds one added without a query
struct LocatorCoverage
{
	/// The service's name in the locator
	std::string_view service;
	/// The query that inspects it
	std::string_view query;
};

/// The list, in the locator's order
[[nodiscard]] std::span<const LocatorCoverage> CoveredServices();

/// Adds every provider of the game's state, reading the locator's services as each query is asked (none need be there
/// when they are added). Answers the run control's provider, which is told of each frame.
GameProvider* AddGameProviders(Inspector& inspector, const entt::meta_ctx& reflection, RunTargetInterface& runTarget,
                               WorldEditInterface& worldEdit);

} // namespace openblack::inspector
