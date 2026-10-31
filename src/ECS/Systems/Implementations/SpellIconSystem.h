/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#if !defined(LOCATOR_IMPLEMENTATIONS)
#error "Locator interface implementations should only be included in Locator.cpp"
#endif

#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

#include "ECS/Systems/SpellIconSystemInterface.h"
#include "Magic/IconChargeLooks.h"
#include "Magic/SpellIcons.h"

namespace openblack::particles
{
class LightSheet;
} // namespace openblack::particles

namespace openblack::ecs::components
{
struct SpellSeed;
struct WorshipIcon;
} // namespace openblack::ecs::components

namespace openblack::ecs::systems
{

class SpellIconSystem final: public SpellIconSystemInterface
{
public:
	bool TownHolds(entt::entity town, MagicType type) override;
	bool TownReleases(entt::entity town, MagicType type) override;
	[[nodiscard]] std::optional<size_t> CentreMiraclesShown(entt::entity town) const override;
	void CentreWorking(entt::entity centre) override;
	void TownChangedHands(entt::entity town, PlayerNames previous) override;
	void TownRemoved(entt::entity town) override;
	void PlayerMade(PlayerNames player) override;
	void TownJoinedSite(entt::entity site, entt::entity town) override;
	void TownLeftSite(entt::entity site, entt::entity town) override;
	void SiteRemoved(entt::entity site) override;

	[[nodiscard]] bool ValidToTap(entt::entity icon, PlayerNames player) const override;
	bool StartChargeForGod(entt::entity icon, PlayerNames player) override;
	[[nodiscard]] float ChantNeeded(entt::entity icon) const override;
	void TakeCastCharge(entt::entity icon) override;
	bool Tap(entt::entity icon, PlayerNames player) override;
	[[nodiscard]] entt::entity SiteOfIcon(entt::entity icon) const override;
	void EmptyIconStores(entt::entity site) override;
	void CancelChargesFor(entt::entity temple, PlayerNames charger) override;
	[[nodiscard]] bool ChargesFor(entt::entity temple, PlayerNames charger) const override;
	bool CancelLatestChargeFor(entt::entity temple, PlayerNames charger) override;
	[[nodiscard]] entt::entity IconInTemple(entt::entity temple, SpellSeedType seed) const override;
	[[nodiscard]] bool AnyIconStores(entt::entity temple, std::optional<MagicType> type) const override;
	[[nodiscard]] bool HasIconFor(PlayerNames player, SpellSeedType seed) const override;
	[[nodiscard]] bool IsIcon(entt::entity entity) const override;
	[[nodiscard]] bool IsHandReady(PlayerNames player) const override { return HandReady(player); }
	[[nodiscard]] std::vector<SpellSeedType> SeedsForSelection(PlayerNames player, GestureType opening) const override;
	[[nodiscard]] bool ValidForRequest(PlayerNames player, SpellSeedType seed) const override;
	bool RequestSeed(PlayerNames player, SpellSeedType seed) override;
	[[nodiscard]] SpellSeedType LastSeedGained(PlayerNames player) const override;
	[[nodiscard]] std::optional<float> ShownCharge(entt::entity icon) const override;

	[[nodiscard]] std::vector<magic::WorshipIconCharge> ChargesAt(entt::entity site) const override;
	void ChargedAt(entt::entity site, std::span<const magic::WorshipIconCharge> charges) override;

	void Update(float seconds) override;
	void UpdateChargeLooks(int32_t gameMilliseconds, bool landDrawn) override;
	[[nodiscard]] uint32_t ShownTint(entt::entity icon) const override;
	[[nodiscard]] std::vector<ChargePulse> ChargePulses() const override;
	void Reset() override;

private:
	/// The seed that shows a magic type, and the power-up level of that seed it is
	[[nodiscard]] std::optional<magic::spell_icons::SeedLevel> SeedLevelOf(MagicType type) const;
	/// The town's working village centre, none without one
	[[nodiscard]] entt::entity WorkingCentreOf(entt::entity town) const;
	/// A centre's hand shows a seed, if it has a free place and none shows it yet. The town's worship site then gets an
	/// icon of the seed if it has none.
	void AddHand(entt::entity centre, entt::entity town, SpellSeedType seed);
	/// The hand showing a seed shows a power-up level of it too, or stops
	void SetPowerUp(entt::entity centre, SpellSeedType seed, int powerUp, bool shown);
	/// Every hand of a centre that shows a seed goes. The site's icon of the seed goes too, unless another of the site's
	/// towns offers the seed.
	void RemoveHands(entt::entity centre, entt::entity town, SpellSeedType seed);
	/// A site gets an icon of a seed, if it has none
	void AddSiteIcon(entt::entity site, SpellSeedType seed);
	/// The seed above a site's icon shows the highest power-up level of it that the site's player may cast
	void ShowPlayersPowerUp(entt::entity icon) const;
	/// The seeds a town's centre's hands show
	[[nodiscard]] std::vector<SpellSeedType> SeedsOfTown(entt::entity town) const;
	/// A site's icon of a seed, none if it has none
	[[nodiscard]] entt::entity SiteIconOf(entt::entity site, SpellSeedType seed) const;
	/// A player's magic type is enabled, or one fewer thing enables it
	void SetEnabled(PlayerNames player, MagicType type, bool enable) const;
	[[nodiscard]] bool IsEnabled(PlayerNames player, MagicType type) const;
	/// The hand of a player can take a miracle now: only the hand at this machine, holding nothing
	[[nodiscard]] bool HandReady(PlayerNames player) const;
	/// What an icon's miracle needs to be charged full, at the level it charges
	[[nodiscard]] float Required(const components::WorshipIcon& icon) const;
	/// What the miracle of a seed costs to create, at the power-up level of the seed
	[[nodiscard]] float SeedCost(const components::SpellSeed& seed) const;
	/// The seed made from an icon that a hand of the icon's player holds, none if no hand holds one
	[[nodiscard]] entt::entity HeldSeedOf(const components::WorshipIcon& icon) const;
	/// What an icon needs to be charged full: its miracle's cost, or what its held seed still needs
	[[nodiscard]] float RequiredNow(const components::WorshipIcon& icon, entt::entity heldSeed) const;
	[[nodiscard]] magic::spell_icons::IconCharge ChargeOf(const components::WorshipIcon& icon, PlayerNames tapping) const;
	/// A site icon is tapped by a hand: what the tap did
	magic::spell_icons::TapResult TapSiteIcon(entt::entity icon, PlayerNames player);
	/// An icon's charged miracle goes into its player's hand, carrying what the icon stored
	bool GiveSeed(entt::entity icon);
	/// The icon starts to charge its miracle for the hand of a player. A charge that is full already goes into the hand.
	void StartCharge(entt::entity icon, PlayerNames player);
	/// The worship sites of a player, in the order of their places round the temple
	[[nodiscard]] std::vector<entt::entity> SitesOf(PlayerNames player) const;
	/// The worship sites of a temple, in the order of their places round the temple
	[[nodiscard]] std::vector<entt::entity> SitesOfTemple(entt::entity temple) const;
	/// An icon stops its charge: its store goes back to its site
	void CancelCharge(components::WorshipIcon& icon) const;
	/// Whether a gesture of a player may ask a site's icon for its miracle
	[[nodiscard]] bool IconValidForRequest(entt::entity icon, PlayerNames player) const;
	/// Deletes an icon and forgets it, with the seeds out from it; what it was charging and what it stored are lost
	void Destroy(entt::entity icon);

	/// A worship site's icon that is on the screen shows its charge in this frame
	void ShowCharge(entt::entity entity, const components::WorshipIcon& icon, int32_t gameMilliseconds);

	/// What the frames keep of an icon's look. A saved game does not keep these.
	struct IconLook
	{
		/// The ring of light round the icon. The first frame that draws the icon makes it.
		std::shared_ptr<particles::LightSheet> ring;
		/// The colour that the icon adds to its model, from the last frame that drew it
		uint32_t tint {0};
		/// The colour that the model shows in this frame
		uint32_t shownTint {0};
	};
	std::unordered_map<entt::entity, IconLook> _looks;
	/// The clock of the shines and the pulses of all the icons
	magic::icon_charge_looks::DecayClock _decayClock;
	/// The pulse models of this frame
	std::vector<ChargePulse> _pulses;
};

} // namespace openblack::ecs::systems
