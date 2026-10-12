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
#include <vector>

#include <entt/entity/entity.hpp>

#include "Enums.h"
#include "Magic/WorshipBattery.h"

namespace openblack::ecs::systems
{

/// The miracles towns offer and the icons that show them. A town's land script gives it magic types; its village centre,
/// once working, shows each miracle on one of up to six stone hands, and its worship site keeps an icon for each miracle
/// its towns offer. The player owning a town may cast what it offers. A tap on a hand charges its miracle at the town's
/// worship site from the site's prayer power; when charged the miracle jumps into the hand.
class SpellIconSystemInterface
{
public:
	virtual ~SpellIconSystemInterface() = default;

	/// A land's script gives a town a magic type: its player may cast it, and its centre shows it
	virtual void TownHolds(entt::entity town, MagicType type) = 0;
	/// A town's village centre stands working: its hands show what the town offers
	virtual void CentreWorking(entt::entity centre) = 0;
	/// A town has changed hands: what it offers goes from its old player to its new one
	virtual void TownChangedHands(entt::entity town, PlayerNames previous) = 0;
	/// A town comes to worship at a site: the site gets an icon for each miracle the town's hands show that it has none of
	virtual void TownJoinedSite(entt::entity site, entt::entity town) = 0;
	/// A town no longer worships at a site: the site's icons of the miracles none of its other towns offer go
	virtual void TownLeftSite(entt::entity site, entt::entity town) = 0;
	/// A worship site goes (its temple destroyed): every icon of it goes with what it was charging, and the seed out from
	/// each, in a hand or cast, goes too. The village centres' hands stay with their towns.
	virtual void SiteRemoved(entt::entity site) = 0;

	/// The hand taps an icon: whether the tap counted
	virtual bool Tap(entt::entity icon, PlayerNames player) = 0;
	/// Whether a player has an icon for a seed at any of their worship sites
	[[nodiscard]] virtual bool HasIconFor(PlayerNames player, SpellSeedType seed) const = 0;
	/// Whether something is an icon or a stone hand
	[[nodiscard]] virtual bool IsIcon(entt::entity entity) const = 0;

	/// A worship site's icons as its turn shares out its prayer power, in a fixed order
	[[nodiscard]] virtual std::vector<magic::WorshipIconCharge> ChargesAt(entt::entity site) const = 0;
	/// What the site's turn gave its icons, in the order ChargesAt gave them; an icon charged full sends its miracle to the
	/// hand that asked for it
	virtual void ChargedAt(entt::entity site, std::span<const magic::WorshipIconCharge> charges) = 0;

	/// Once a frame: the seeds above the icons spin
	virtual void Update(float seconds) = 0;
	/// A new land
	virtual void Reset() = 0;
};

} // namespace openblack::ecs::systems
