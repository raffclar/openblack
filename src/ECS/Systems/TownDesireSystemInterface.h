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

#include <functional>
#include <optional>

#include <entt/entity/fwd.hpp>

#include "Enums.h"

namespace openblack::ecs::systems
{

/// What the towns want (components::TownDesire, see town_desire)
class TownDesireSystemInterface
{
public:
	virtual ~TownDesireSystemInterface() = default;
	/// Once a game turn: every town counts its people and buildings and works out its desires
	virtual void ProcessTurn() = 0;
	/// Offers a villager to its town's desires, most wanted first, past its trigger; `satisfy` tries a desire and
	/// returns 1 when the villager took it up. 1 when one did.
	virtual uint32_t OfferVillager(entt::entity town, bool child, float trigger,
	                               const std::function<uint32_t(TownDesireInfo)>& satisfy) = 0;
	/// A desire of a town as the villagers see it, and as the town feels it; none without a town
	[[nodiscard]] virtual float GetDesire(entt::entity town, TownDesireInfo desire) const = 0;
	[[nodiscard]] virtual float GetRawDesire(entt::entity town, TownDesireInfo desire) const = 0;
	/// The town works out one desire afresh from how it is now, as it does when a store is given or loses a resource. The
	/// desire it then has.
	virtual float RecomputeDesire(entt::entity town, TownDesireInfo desire) = 0;
	/// The desire a town wants most this turn, None without a town
	[[nodiscard]] virtual TownDesireInfo GetMostWanted(entt::entity town) const = 0;
	/// The scripts' boost to a desire; `resort` puts the town's order right at once
	virtual void SetBoost(entt::entity town, TownDesireInfo desire, float boost, bool resort) = 0;

	/// Once a frame, by the game's time passed in it: each town's store flies a flag for some of its desires, each as
	/// high as the town wants that thing, turned to the camera, while the store stands built
	virtual void UpdateFlags(float seconds) = 0;
	/// What the hand shows over a flag: the flag's own words, among the game's tooltips, and its town's desire in
	/// hundredths. None for anything not a flag showing.
	struct FlagToolTip
	{
		uint32_t words;
		float percent;
	};
	[[nodiscard]] virtual std::optional<FlagToolTip> GetFlagToolTip(entt::entity flag) const = 0;
};

} // namespace openblack::ecs::systems
