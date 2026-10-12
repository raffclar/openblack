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

#include <optional>

#include "ECS/Systems/SpellIconSystemInterface.h"
#include "Magic/SpellIcons.h"

namespace openblack::ecs::components
{
struct WorshipIcon;
} // namespace openblack::ecs::components

namespace openblack::ecs::systems
{

class SpellIconSystem final: public SpellIconSystemInterface
{
public:
	bool TownHolds(entt::entity town, MagicType type) override;
	[[nodiscard]] std::optional<size_t> CentreMiraclesShown(entt::entity town) const override;
	void CentreWorking(entt::entity centre) override;
	void TownChangedHands(entt::entity town, PlayerNames previous) override;
	void TownJoinedSite(entt::entity site, entt::entity town) override;
	void TownLeftSite(entt::entity site, entt::entity town) override;
	void SiteRemoved(entt::entity site) override;

	bool Tap(entt::entity icon, PlayerNames player) override;
	[[nodiscard]] bool HasIconFor(PlayerNames player, SpellSeedType seed) const override;
	[[nodiscard]] bool IsIcon(entt::entity entity) const override;

	[[nodiscard]] std::vector<magic::WorshipIconCharge> ChargesAt(entt::entity site) const override;
	void ChargedAt(entt::entity site, std::span<const magic::WorshipIconCharge> charges) override;

	void Update(float seconds) override;
	void Reset() override;

private:
	/// The seed that shows a magic type, and the power-up level of that seed it is
	[[nodiscard]] std::optional<magic::spell_icons::SeedLevel> SeedLevelOf(MagicType type) const;
	/// The town's working village centre, none without one
	[[nodiscard]] entt::entity WorkingCentreOf(entt::entity town) const;
	/// A centre's hand shows a seed, if it has a free place and none shows it yet
	void AddHand(entt::entity centre, entt::entity town, SpellSeedType seed);
	/// The hand showing a seed shows a power-up level of it too
	void AddPowerUp(entt::entity centre, SpellSeedType seed, int powerUp);
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
	[[nodiscard]] magic::spell_icons::IconCharge ChargeOf(const components::WorshipIcon& icon, PlayerNames tapping) const;
	/// A site icon is tapped by a hand: what the tap did
	magic::spell_icons::TapResult TapSiteIcon(entt::entity icon, PlayerNames player);
	/// An icon's charged miracle goes into its player's hand, carrying what the icon stored
	bool GiveSeed(entt::entity icon);
	/// Deletes an icon and forgets it, with the seed out from it; what it was charging and what it stored are lost
	void Destroy(entt::entity icon);
};

} // namespace openblack::ecs::systems
