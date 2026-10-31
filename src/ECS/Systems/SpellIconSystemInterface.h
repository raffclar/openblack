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

#include <chrono>
#include <optional>
#include <span>
#include <vector>

#include <entt/core/fwd.hpp>
#include <entt/entity/entity.hpp>
#include <glm/mat4x4.hpp>

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
	/// A town goes from the game: its player counts one town less that offers each miracle that the town offers, and
	/// the stone hands of the town go
	virtual void TownRemoved(entt::entity town) = 0;
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

	/// One more thing enables a magic type for a player, or one fewer does. The icons of the player's worship sites then
	/// show the power-up levels that the player can cast.
	virtual void SetMagicTypeEnabled(PlayerNames /*player*/, MagicType /*type*/, bool /*enable*/) {}

	/// Whether a player's hand may tap an icon or a stone hand: the tap counts for the help, whatever it then does
	[[nodiscard]] virtual bool ValidToTap(entt::entity icon, PlayerNames player) const = 0;
	/// The hand taps an icon: whether the tap counted
	virtual bool Tap(entt::entity icon, PlayerNames player) = 0;
	/// A computer god's hand makes the worshippers of an icon's site chant for its miracle. The icon must not charge,
	/// no hand may hold its seed, the god must be able to cast the miracle, and the site must have prayer power to give
	/// or the icon some stored. An icon that is full already then gives its miracle to the hand, as a tap does. Only the
	/// hand of a human player takes it, so a computer god keeps its miracle in the icon. Whether the charge started.
	virtual bool StartChargeForGod(entt::entity /*icon*/, PlayerNames /*player*/) { return false; }
	/// The worship site icon that made a seed, none for a seed that no icon made or whose icon is gone
	[[nodiscard]] virtual entt::entity IconOfSeed(entt::entity /*seed*/) const { return entt::null; }
	/// The icon that makes the seed of a one-shot miracle for a player. It is the icon of the seed that can give the
	/// miracle now, at the player's site with the most prayer power left this turn. The first site wins a tie. None
	/// when no icon can give it.
	[[nodiscard]] virtual entt::entity BestIconFor(PlayerNames /*player*/, SpellSeedType /*seed*/) const { return entt::null; }
	/// A seed that an icon made is now out from the icon
	virtual void SeedMadeAt(entt::entity /*icon*/, entt::entity /*seed*/) {}
	/// Whether an icon can charge the seed of it in a hand to a power-up level for a player: the hand of the icon's
	/// player holds a seed of the icon, and the player may cast the miracle of that level. Level -1 is the plain miracle.
	[[nodiscard]] virtual bool ValidForPowerUp(entt::entity /*icon*/, PlayerNames /*player*/, int /*powerUp*/) const
	{
		return false;
	}
	/// A power-up gesture asks an icon to charge the seed of it in a hand to a power-up level, for the hand of a player.
	/// The icon charges from then on, also when it charged something before. When the seed holds what the level costs,
	/// it takes the level and gives what it holds past the cost back to the site. Level -1 calls a power-up off. Whether
	/// the charge started.
	virtual bool StartPowerUpCharge(entt::entity /*icon*/, PlayerNames /*player*/, int /*powerUp*/) { return false; }
	/// The prayer power that an icon's miracle still needs to be charged full
	[[nodiscard]] virtual float ChantNeeded(entt::entity /*icon*/) const { return 0.0f; }
	/// A god cast the miracle of an icon: the icon's store loses the miracle's cost, never more than it holds
	virtual void TakeCastCharge(entt::entity /*icon*/) {}
	/// The worship site of an icon: the site of a worship site's icon, or the site of the town of a stone hand. None for a
	/// thing that is not an icon, and for a stone hand whose town has no site.
	[[nodiscard]] virtual entt::entity SiteOfIcon(entt::entity icon) const = 0;
	/// A script sets the prayer power of a site: each icon of the site loses all the prayer power that it stored
	virtual void EmptyIconStores(entt::entity site) = 0;
	/// A script stops the charges for the hand of a player at the worship sites of a temple. Each icon that charges for
	/// that hand stops and gives its store back to its site.
	virtual void CancelChargesFor(entt::entity temple, PlayerNames charger) = 0;
	/// Whether an icon at the worship sites of a temple charges a miracle for the hand of a player
	[[nodiscard]] virtual bool ChargesFor(entt::entity /*temple*/, PlayerNames /*charger*/) const { return false; }
	/// A scribble of the empty hand stops one charge for the hand of a player at the worship sites of a temple: the
	/// charge that started last. Of charges that started in the same turn, the first in the order of the places stops.
	/// It returns whether a charge stopped.
	virtual bool CancelLatestChargeFor(entt::entity /*temple*/, PlayerNames /*charger*/) { return false; }
	/// The icon of a seed at the worship sites of a temple. The first site in the order of the places round the temple
	/// gives it. None when no site of the temple has an icon of the seed.
	[[nodiscard]] virtual entt::entity IconInTemple(entt::entity temple, SpellSeedType seed) const = 0;
	/// Whether an icon at the worship sites of a temple has prayer power in its store, as the scripts ask if a miracle
	/// charges. With a magic type, only an icon whose plain miracle is of that type counts.
	[[nodiscard]] virtual bool AnyIconStores(entt::entity temple, std::optional<MagicType> type) const = 0;
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

	/// The pulse model that a charging icon shows in a frame: where it is drawn, and its colour and alpha as 0xAARRGGBB
	struct ChargePulse
	{
		glm::mat4 model {1.0f};
		uint32_t argb {0};
	};
	/// Once a frame, while the frame draws the land: each icon at a worship site that is on the screen shows its charge.
	/// Its ring of light fills and moves on, its shine changes, and its pulse model shrinks into it while it charges.
	virtual void UpdateChargeLooks(int32_t /*gameMilliseconds*/, bool /*landDrawn*/) {}
	/// Once a frame: the stone hands of the village centres and the icons at the worship sites show the effect of the
	/// holder of their seeds. The effect of an icon shows and moves on, by the game time of the frame, only in a frame that
	/// draws the icon on the screen.
	virtual void UpdateHolderEffects(std::chrono::milliseconds /*frameRealTime*/, std::chrono::milliseconds /*frameGameTime*/,
	                                 bool /*landDrawn*/)
	{
	}
	/// The seed's model that the effect of the holder over an icon acts on: the model and where it was last drawn
	struct HolderTarget
	{
		entt::id_type model {0};
		glm::mat4 placement {1.0f};
	};
	/// The target of the effect of the holder over an icon, none for an icon with no such effect
	[[nodiscard]] virtual std::optional<HolderTarget> FindHolderTarget(entt::entity /*icon*/) const { return std::nullopt; }
	/// The colour that an icon adds to its model this frame (0xRRGGBB), 0 for none. The model shows the shine of the
	/// frame before, because the game draws the model before it changes the shine.
	[[nodiscard]] virtual uint32_t ShownTint(entt::entity /*icon*/) const { return 0; }
	/// The pulse models of the charging icons on the screen this frame
	[[nodiscard]] virtual std::vector<ChargePulse> ChargePulses() const { return {}; }
	/// A new land
	virtual void Reset() = 0;
};

} // namespace openblack::ecs::systems
