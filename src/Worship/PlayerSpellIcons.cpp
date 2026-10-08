/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "PlayerSpellIcons.h"

#include <cstdio>
#include <cstdlib>

#include <algorithm>
#include <array>

#include "Citadel.h"
#include "ECS/Components/SpellIcon.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/Registry.h"
#include "ECS/Systems/WorshipStateInterface.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/MagicTables.h"
#include "WorshipSite.h"
#include "WorshipSpellIcon.h"

using namespace openblack;
using namespace openblack::worship;
using namespace openblack::ecs::components;

namespace
{
constexpr size_t k_Players = static_cast<size_t>(PlayerNames::_COUNT);

struct PlayerSpellIconsState
{
	std::array<SpellSeedType, k_Players> lastSeed {};
};

/// This module's state (Locator::worshipState)
PlayerSpellIconsState& SpellIcons()
{
	if (!Locator::worshipState::has_value())
	{
		std::fputs("worship::player: no worship state in the locator (Locator::worshipState)\n", stderr);
		std::abort();
	}
	return Locator::worshipState::value().Get<PlayerSpellIconsState>();
}

auto& Registry()
{
	return Locator::entitiesRegistry::value();
}

/// The six site slots of the player's citadel
std::array<entt::entity, CitadelWorship::k_Sites> SitesOf(PlayerNames player)
{
	std::array<entt::entity, CitadelWorship::k_Sites> sites {entt::null, entt::null, entt::null,
	                                                         entt::null, entt::null, entt::null};
	const auto citadel = citadel::Of(player);
	if (citadel == entt::null)
	{
		return sites;
	}
	for (size_t i = 0; i < sites.size(); ++i)
	{
		const auto site = Registry().Get<const CitadelWorship>(citadel).sites[i];
		if (site != entt::null && Registry().Valid(site))
		{
			sites[i] = site;
		}
	}
	return sites;
}
} // namespace

void player::ProcessSpellIcons()
{
	// the players in order, then the neutral one
	for (size_t p = 0; p < k_Players; ++p)
	{
		if (const auto citadel = citadel::Of(static_cast<PlayerNames>(p)); citadel != entt::null)
		{
			citadel::ProcessSpellIcons(citadel);
		}
	}
}

std::vector<entt::entity> player::Icons(PlayerNames player)
{
	std::vector<entt::entity> icons;
	for (const auto site : SitesOf(player))
	{
		if (site == entt::null)
		{
			continue;
		}
		const auto& list = Registry().Get<const WorshipSite>(site).icons;
		icons.insert(icons.end(), list.begin(), list.end());
	}
	return icons;
}

bool player::AnyIconChargingFor(PlayerNames player)
{
	return std::ranges::any_of(Icons(player), [player](const auto icon) { return icon::IsCharging(icon, player, false); });
}

float player::MaxChargeFraction(PlayerNames player)
{
	float best = 0.0f;
	for (const auto icon : Icons(player))
	{
		// the charge fraction while charging for that hand, else 0
		const float fraction = icon::IsCharging(icon, player, false) ? icon::ChargeFraction(icon) : 0.0f;
		if (!(fraction < best))
		{
			best = fraction;
		}
	}
	return best;
}

bool player::AnyRequestableIcon(PlayerNames player)
{
	return std::ranges::any_of(Icons(player),
	                           [player](const auto icon) { return icon::ValidForRequestSpell(icon, player, -1, true); });
}

bool player::AnyRequestableIconOfCategory(PlayerNames player, GestureType category)
{
	const auto& tables = Locator::infoConstants::value();
	return std::ranges::any_of(Icons(player), [&tables, player, category](const auto icon) {
		const auto& seedInfo = magic::GetSpellSeedInfo(tables, icon::SeedTypeOf(icon));
		return seedInfo.selectionGesture == category && icon::ValidForRequestSpell(icon, player, -1, true);
	});
}

bool player::IconValidForRequest(PlayerNames player, SpellSeedType seed)
{
	if (seed == SpellSeedType::None)
	{
		return false;
	}
	for (const auto site : SitesOf(player))
	{
		if (site == entt::null)
		{
			continue;
		}
		// the icon must be functional: openblack's icons always are
		const auto icon = site::GetSpellIconFromSeedType(site, seed);
		if (icon != entt::null && icon::ValidForRequestSpell(icon, player, -1, true))
		{
			return true;
		}
	}
	return false;
}

entt::entity player::FindBestSpellIconForSpellSeed(PlayerNames player, SpellSeedType seed)
{
	if (seed == SpellSeedType::None)
	{
		return entt::null;
	}
	float best = -1.0f; // strictly more wins, so the first site wins a tie
	entt::entity found = entt::null;
	for (const auto site : SitesOf(player))
	{
		if (site == entt::null)
		{
			continue;
		}
		const float chants = site::AvailableForIcons(Registry().Get<const WorshipSite>(site), false);
		const auto icon = site::GetSpellIconFromSeedType(site, seed);
		if (icon != entt::null && icon::ValidForRequestSpell(icon, player, -1, true) && chants > best)
		{
			best = chants;
			found = icon;
		}
	}
	return found;
}

bool player::RequestSpell(PlayerNames player, SpellSeedType seed)
{
	if (seed == SpellSeedType::None)
	{
		return false;
	}
	const auto icon = FindBestSpellIconForSpellSeed(player, seed);
	return icon != entt::null && icon::RequestSpell(icon, player, -1, true);
}

bool player::CanRepeat(PlayerNames player)
{
	return IconValidForRequest(player, LastSeedType(player));
}

bool player::RepeatLastSpell(PlayerNames player)
{
	return CanRepeat(player) && RequestSpell(player, LastSeedType(player));
}

void player::SetLastSeedType(PlayerNames player, SpellSeedType seed)
{
	SpellIcons().lastSeed.at(static_cast<size_t>(player)) = seed;
}

SpellSeedType player::LastSeedType(PlayerNames player)
{
	return SpellIcons().lastSeed.at(static_cast<size_t>(player));
}

bool player::SetChargingPowerUp(entt::entity icon, PlayerNames player, int powerUp)
{
	return icon != entt::null && Registry().Valid(icon) && icon::SetChargingPowerUp(icon, player, powerUp);
}

bool player::CancelMostRecentCharge(PlayerNames player)
{
	// (inferred): the original function is not decoded; its start value and tie rule are openblack's (a charge
	// started on turn 0 is never the most recent)
	entt::entity latest = entt::null;
	uint32_t latestTurn = 0;
	for (const auto icon : Icons(player))
	{
		if (!icon::IsCharging(icon, player, false))
		{
			continue;
		}
		const auto turn = Registry().Get<const WorshipSpellIcon>(icon).chargeStartTurn;
		if (turn > latestTurn)
		{
			latestTurn = turn;
			latest = icon;
		}
	}
	if (latest == entt::null)
	{
		return false;
	}
	icon::CancelCharge(latest, player);
	return true;
}

bool player::CancelAllSpellsCharging(PlayerNames player)
{
	if (citadel::Of(player) == entt::null)
	{
		return false;
	}
	for (const auto icon : Icons(player))
	{
		if (icon::IsCharging(icon, player, false))
		{
			icon::CancelCharge(icon, player);
		}
	}
	return true;
}

bool player::AnySpellCharging(PlayerNames player)
{
	return std::ranges::any_of(Icons(player),
	                           [](const auto icon) { return Registry().Get<const WorshipSpellIcon>(icon).chantStore > 0.0f; });
}

bool player::IsThatSpellCharging(PlayerNames player, MagicType type)
{
	const auto& tables = Locator::infoConstants::value();
	for (const auto icon : Icons(player))
	{
		// the icon's seed's base magic
		const auto base = magic::MagicTypeForPowerUpLevel(magic::GetSpellSeedInfo(tables, icon::SeedTypeOf(icon)), -1);
		if (base == type && Registry().Get<const WorshipSpellIcon>(icon).chantStore > 0.0f)
		{
			return true;
		}
	}
	return false;
}

void player::OnMagicTypesChanged(PlayerNames player)
{
	if (!Locator::entitiesRegistry::has_value())
	{
		return;
	}
	for (const auto icon : Icons(player))
	{
		icon::UpdatePowerUpGraphics(icon);
	}
}

void player::Reset()
{
	SpellIcons().lastSeed.fill(SpellSeedType::None);
}
