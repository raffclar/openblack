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

	/// A land's script, or a reward, gives a town a magic type: its player may cast it, and its centre shows it. Whether
	/// the town didn't hold it before.
	virtual bool TownHolds(entt::entity town, MagicType type) = 0;
	/// A script takes a magic type from a town: its player may cast it once fewer, and its centre stops showing it. Taking
	/// the plain miracle removes its hand, and the site's icon of it unless another of the site's towns offers it. Its
	/// power-up levels stay held. Whether the town held it.
	virtual bool TownReleases(entt::entity town, MagicType type) = 0;
	/// How many miracles a town's village centre shows on its hands; none when the town has no working centre
	[[nodiscard]] virtual std::optional<size_t> CentreMiraclesShown(entt::entity town) const = 0;
	/// A town's village centre stands working: its hands show what the town offers
	virtual void CentreWorking(entt::entity centre) = 0;
	/// A town has changed hands: what it offers goes from its old player to its new one
	virtual void TownChangedHands(entt::entity town, PlayerNames previous) = 0;
	/// A player's entity is made after the land made its towns, as a god that joins a network game: the player may cast
	/// what each of its towns offers, as if the player had been there when the towns were made
	virtual void PlayerMade(PlayerNames /*player*/) {}
	/// A town comes to worship at a site: the site gets an icon for each miracle the town's hands show that it has none of
	virtual void TownJoinedSite(entt::entity site, entt::entity town) = 0;
	/// A town no longer worships at a site: the site's icons of the miracles none of its other towns offer go
	virtual void TownLeftSite(entt::entity site, entt::entity town) = 0;
	/// A worship site goes (its temple destroyed): every icon of it goes with what it was charging, and the seed out from
	/// each, in a hand or cast, goes too. The village centres' hands stay with their towns.
	virtual void SiteRemoved(entt::entity site) = 0;

	/// Whether a player's hand may tap an icon or a stone hand: the tap counts for the help, whatever it then does
	[[nodiscard]] virtual bool ValidToTap(entt::entity icon, PlayerNames player) const = 0;
	/// The hand taps an icon: whether the tap counted
	virtual bool Tap(entt::entity icon, PlayerNames player) = 0;
	/// A computer god's hand makes the worshippers of an icon's site chant for its miracle. The icon must not charge,
	/// no hand may hold its seed, the god must be able to cast the miracle, and the site must have prayer power to give
	/// or the icon some stored. Whether the charge started.
	virtual bool StartChargeForGod(entt::entity /*icon*/, PlayerNames /*player*/) { return false; }
	/// The prayer power that an icon's miracle still needs to be charged full
	[[nodiscard]] virtual float ChantNeeded(entt::entity /*icon*/) const { return 0.0f; }
	/// A god cast the miracle of an icon: the icon's store loses the miracle's cost, never more than it holds
	virtual void TakeCastCharge(entt::entity /*icon*/) {}
	/// Whether a player has an icon for a seed at any of their worship sites
	[[nodiscard]] virtual bool HasIconFor(PlayerNames player, SpellSeedType seed) const = 0;
	/// Whether something is an icon or a stone hand
	[[nodiscard]] virtual bool IsIcon(entt::entity entity) const = 0;

	/// Whether the hand of a player has space for a miracle now: only the hand at this machine, while it holds nothing
	[[nodiscard]] virtual bool IsHandReady(PlayerNames player) const = 0;
	/// The seeds of the icons at a player's worship sites that a gesture may ask for, of the miracles whose chain of
	/// gestures in the miracle selection starts with a gesture
	[[nodiscard]] virtual std::vector<SpellSeedType> SeedsForSelection(PlayerNames player, GestureType opening) const = 0;
	/// Whether a gesture may ask an icon of a seed at one of a player's worship sites for its miracle
	[[nodiscard]] virtual bool ValidForRequest(PlayerNames player, SpellSeedType seed) const = 0;
	/// A gesture asks for the miracle of a seed. Of the player's icons of the seed that may give it, the icon at the
	/// site with the most prayer power left this turn gives it: the miracle goes into the hand when it is charged, else
	/// the icon starts to charge it. Whether an icon did something.
	virtual bool RequestSeed(PlayerNames player, SpellSeedType seed) = 0;
	/// The seed of the last miracle that went into a player's hand, none before the first one: the R gesture and the R
	/// key ask for it again
	[[nodiscard]] virtual SpellSeedType LastSeedGained(PlayerNames player) const = 0;
	/// The charge that a worship site's icon shows after the name of its miracle, in percent. None for a thing that is
	/// not such an icon, and for an icon whose charge is zero.
	[[nodiscard]] virtual std::optional<float> ShownCharge(entt::entity icon) const = 0;

	/// A worship site's icons as its turn shares out its prayer power, in a fixed order
	[[nodiscard]] virtual std::vector<magic::WorshipIconCharge> ChargesAt(entt::entity site) const = 0;
	/// What the site's turn gave its icons, in the order ChargesAt gave them. Then each charging icon does its own turn, in
	/// that order: it fills the seed of it that a hand holds, and when its charge is done it stops charging. A done icon
	/// sets the power-up level of its held seed, or else sends its miracle to the hand that asked for it.
	virtual void ChargedAt(entt::entity site, std::span<const magic::WorshipIconCharge> charges) = 0;

	/// Once a frame: the seeds above the icons spin
	virtual void Update(float seconds) = 0;
	/// A new land
	virtual void Reset() = 0;
};

} // namespace openblack::ecs::systems
