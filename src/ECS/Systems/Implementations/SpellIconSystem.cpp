/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "SpellIconSystem.h"

#include <cmath>

#include <algorithm>
#include <array>
#include <optional>
#include <vector>

#include <PackFile.h>
#include <entt/core/hashed_string.hpp>
#include <fmt/format.h>
#include <glm/gtc/constants.hpp>

#include "Audio/AudioManagerInterface.h"
#include "Audio/GameSoundEffects.h"
#include "Audio/Sound.h"
#include "ECS/Archetypes/SpellIconArchetype.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Player.h"
#include "ECS/Components/SpellIcon.h"
#include "ECS/Components/SpellSeed.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/WorshipChants.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/PlayerMiracles.h"
#include "ECS/Registry.h"
#include "ECS/Systems/HandGrabSystemInterface.h"
#include "ECS/Systems/HandSystemInterface.h"
#include "ECS/Systems/MagicSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/Systems/WorshipSiteSystemInterface.h"
#include "ECS/WorldObjects.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/DispenserRules.h"
#include "Magic/MiracleVisuals.h"
#include "Magic/SpellSeedRules.h"
#include "Magic/WorshipBattery.h"

using namespace openblack;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;
using openblack::ecs::archetypes::SpellIconArchetype;
namespace icons = openblack::magic::spell_icons;

namespace
{
[[nodiscard]] ecs::Registry& Entities()
{
	return Locator::entitiesRegistry::value();
}

/// The player's own entity's record, none for a player the land has none for
[[nodiscard]] Player* PlayerNamed(PlayerNames name)
{
	Player* found = nullptr;
	Entities().Each<Player>([&](Player& player) {
		if (found == nullptr && player.name == name)
		{
			found = &player;
		}
	});
	return found;
}

/// The player whose hand this computer moves: only this player hears the icons
[[nodiscard]] PlayerNames OwnPlayer()
{
	return Locator::handSystem::has_value() ? Locator::handSystem::value().GetOwnPlayer() : PlayerNames::PLAYER_ONE;
}
} // namespace

std::optional<icons::SeedLevel> SpellIconSystem::SeedLevelOf(MagicType type) const
{
	if (!Locator::infoConstants::has_value())
	{
		return std::nullopt;
	}
	const auto& info = Locator::infoConstants::value();
	const auto seed = magic::FindFirstSpellSeedForMagicType(info, type);
	if (!seed.has_value())
	{
		return std::nullopt;
	}
	return icons::SeedLevel {.seed = *seed,
	                         .powerUp = magic::GetPowerUpFromMagicType(magic::GetSpellSeedInfo(info, *seed), type)};
}

entt::entity SpellIconSystem::WorkingCentreOf(entt::entity town) const
{
	auto& registry = Entities();
	const auto* component = registry.Valid(town) ? registry.TryGet<const Town>(town) : nullptr;
	if (component == nullptr)
	{
		return entt::null;
	}
	entt::entity centre = entt::null;
	registry.Each<const Abode, const CentreHands>([&](entt::entity entity, const Abode& abode, const CentreHands& /*hands*/) {
		if (centre == entt::null && abode.townId == component->id)
		{
			centre = entity;
		}
	});
	return centre;
}

bool SpellIconSystem::TownHolds(entt::entity town, MagicType type)
{
	auto& registry = Entities();
	if (!registry.Valid(town) || !registry.AllOf<Town>(town))
	{
		return false;
	}
	auto& magicHeld = registry.AnyOf<TownMagic>(town) ? registry.Get<TownMagic>(town) : registry.Assign<TownMagic>(town);
	if (!icons::Hold(magicHeld.held, type))
	{
		return false;
	}
	// Its player may cast it while the town is theirs
	SetEnabled(registry.Get<const Town>(town).owner, type, true);
	// A working centre shows it: the plain miracle on a hand of its own, a power-up level on its seed's hand
	const auto level = SeedLevelOf(type);
	const auto centre = WorkingCentreOf(town);
	if (!level.has_value() || centre == entt::null)
	{
		return true;
	}
	if (level->powerUp == magic::k_BasePowerUpLevel)
	{
		AddHand(centre, town, level->seed);
	}
	else
	{
		SetPowerUp(centre, level->seed, level->powerUp, true);
	}
	return true;
}

bool SpellIconSystem::TownReleases(entt::entity town, MagicType type)
{
	auto& registry = Entities();
	auto* magicHeld = registry.Valid(town) ? registry.TryGet<TownMagic>(town) : nullptr;
	if (magicHeld == nullptr || !icons::Release(magicHeld->held, type))
	{
		return false;
	}
	// Its player may cast it once fewer
	SetEnabled(registry.Get<const Town>(town).owner, type, false);
	// A working centre does not show it any more. The plain miracle loses its hand, and a power-up level loses its mark.
	const auto level = SeedLevelOf(type);
	const auto centre = WorkingCentreOf(town);
	if (!level.has_value() || centre == entt::null)
	{
		return true;
	}
	if (level->powerUp == magic::k_BasePowerUpLevel)
	{
		RemoveHands(centre, town, level->seed);
	}
	else
	{
		SetPowerUp(centre, level->seed, level->powerUp, false);
	}
	return true;
}

std::optional<size_t> SpellIconSystem::CentreMiraclesShown(entt::entity town) const
{
	const auto centre = WorkingCentreOf(town);
	const auto* hands = centre != entt::null ? Entities().TryGet<const CentreHands>(centre) : nullptr;
	if (hands == nullptr)
	{
		return std::nullopt;
	}
	return static_cast<size_t>(std::ranges::count_if(hands->hands, [](entt::entity hand) { return hand != entt::null; }));
}

void SpellIconSystem::AddHand(entt::entity centre, entt::entity town, SpellSeedType seed)
{
	auto& registry = Entities();
	auto& hands = registry.Get<CentreHands>(centre);
	std::array<SpellSeedType, icons::k_CentreHands> shown {};
	for (size_t i = 0; i < hands.hands.size(); ++i)
	{
		const auto hand = hands.hands.at(i);
		const auto* icon = registry.Valid(hand) ? registry.TryGet<const TownCentreIcon>(hand) : nullptr;
		shown.at(i) = icon != nullptr ? icon->seed : SpellSeedType::None;
	}
	if (std::ranges::find(shown, seed) != shown.end())
	{
		return;
	}
	const auto free = icons::FreeHand(shown, SpellIconArchetype::CentreHandPlaces(centre));
	if (!free.has_value())
	{
		return;
	}
	hands.hands.at(*free) = SpellIconArchetype::CreateCentreHand(centre, town, *free, seed);
	// The town's worship site gets an icon of the new hand's miracle at once
	if (const auto site = registry.Get<const Town>(town).worshipSite; registry.Valid(site))
	{
		AddSiteIcon(site, seed);
	}
}

void SpellIconSystem::RemoveHands(entt::entity centre, entt::entity town, SpellSeedType seed)
{
	auto& registry = Entities();
	for (auto& hand : registry.Get<CentreHands>(centre).hands)
	{
		const auto* icon = registry.Valid(hand) ? registry.TryGet<const TownCentreIcon>(hand) : nullptr;
		if (icon != nullptr && icon->seed == seed)
		{
			registry.Destroy(hand);
			hand = entt::null;
		}
	}
	// The site keeps its icon of the seed while another of its towns still has a hand of it
	const auto site = registry.Get<const Town>(town).worshipSite;
	const auto* component = registry.Valid(site) ? registry.TryGet<const WorshipSite>(site) : nullptr;
	if (component == nullptr)
	{
		return;
	}
	std::vector<std::vector<SpellSeedType>> offered;
	for (const auto other : component->towns)
	{
		offered.push_back(SeedsOfTown(other));
	}
	const auto kept = icons::SiteSeeds(offered);
	if (std::ranges::find(kept, seed) != kept.end())
	{
		return;
	}
	if (const auto icon = SiteIconOf(site, seed); icon != entt::null)
	{
		Destroy(icon);
	}
}

void SpellIconSystem::SetPowerUp(entt::entity centre, SpellSeedType seed, int powerUp, bool shown)
{
	auto& registry = Entities();
	if (powerUp < 0 || static_cast<size_t>(powerUp) >= icons::k_PowerUpLevels)
	{
		return;
	}
	for (const auto hand : registry.Get<const CentreHands>(centre).hands)
	{
		auto* icon = registry.Valid(hand) ? registry.TryGet<TownCentreIcon>(hand) : nullptr;
		if (icon != nullptr && icon->seed == seed)
		{
			icon->powerUps.at(static_cast<size_t>(powerUp)) = shown;
			// The seed above it shows the highest level with a ring for each
			if (auto* graphic = registry.TryGet<IconSeed>(hand))
			{
				graphic->powerUp = icons::ShownPowerUp(icon->powerUps);
			}
		}
	}
}

void SpellIconSystem::CentreWorking(entt::entity centre)
{
	auto& registry = Entities();
	const auto* abode = registry.Valid(centre) ? registry.TryGet<const Abode>(centre) : nullptr;
	if (abode == nullptr || registry.AllOf<CentreHands>(centre))
	{
		return;
	}
	const auto& towns = registry.Context().towns;
	const auto found = towns.find(abode->townId);
	if (found == towns.end() || !registry.Valid(found->second))
	{
		return;
	}
	const auto town = found->second;
	registry.Assign<CentreHands>(centre);
	// Every magic type the town holds, in order: its seed on a hand, and its power-up level on that hand
	if (const auto* magicHeld = registry.TryGet<const TownMagic>(town))
	{
		const auto held = magicHeld->held;
		for (size_t i = 0; i < held.size(); ++i)
		{
			if (!held.at(i))
			{
				continue;
			}
			const auto level = SeedLevelOf(static_cast<MagicType>(i));
			if (!level.has_value())
			{
				continue;
			}
			AddHand(centre, town, level->seed);
			if (level->powerUp != magic::k_BasePowerUpLevel)
			{
				SetPowerUp(centre, level->seed, level->powerUp, true);
			}
		}
	}
	// The town's worship site keeps icons of them too
	if (const auto site = registry.Get<const Town>(town).worshipSite; registry.Valid(site))
	{
		TownJoinedSite(site, town);
	}
}

void SpellIconSystem::TownChangedHands(entt::entity town, PlayerNames previous)
{
	auto& registry = Entities();
	const auto* magicHeld = registry.Valid(town) ? registry.TryGet<const TownMagic>(town) : nullptr;
	if (magicHeld == nullptr)
	{
		return;
	}
	// What it offers goes from its old player to its new one; its hands follow its player by themselves
	const auto owner = registry.Get<const Town>(town).owner;
	for (size_t i = 0; i < magicHeld->held.size(); ++i)
	{
		if (magicHeld->held.at(i))
		{
			SetEnabled(previous, static_cast<MagicType>(i), false);
			SetEnabled(owner, static_cast<MagicType>(i), true);
		}
	}
}

void SpellIconSystem::PlayerMade(PlayerNames player)
{
	// The towns of the player gave their miracles to no one while the player had no entity
	auto& registry = Entities();
	std::vector<MagicType> offered;
	registry.Each<const Town, const TownMagic>([player, &offered](const Town& town, const TownMagic& magicHeld) {
		if (town.owner != player)
		{
			return;
		}
		for (size_t i = 0; i < magicHeld.held.size(); ++i)
		{
			if (magicHeld.held.at(i))
			{
				offered.push_back(static_cast<MagicType>(i));
			}
		}
	});
	for (const auto type : offered)
	{
		SetEnabled(player, type, true);
	}
}

std::vector<SpellSeedType> SpellIconSystem::SeedsOfTown(entt::entity town) const
{
	auto& registry = Entities();
	std::vector<SpellSeedType> seeds;
	const auto centre = WorkingCentreOf(town);
	if (centre == entt::null)
	{
		return seeds;
	}
	for (const auto hand : registry.Get<const CentreHands>(centre).hands)
	{
		if (const auto* icon = registry.Valid(hand) ? registry.TryGet<const TownCentreIcon>(hand) : nullptr)
		{
			seeds.push_back(icon->seed);
		}
	}
	return seeds;
}

entt::entity SpellIconSystem::SiteIconOf(entt::entity site, SpellSeedType seed) const
{
	auto& registry = Entities();
	const auto* list = registry.Valid(site) ? registry.TryGet<const SiteIcons>(site) : nullptr;
	if (list == nullptr)
	{
		return entt::null;
	}
	for (const auto icon : list->icons)
	{
		if (const auto* component = registry.Valid(icon) ? registry.TryGet<const WorshipIcon>(icon) : nullptr;
		    component != nullptr && component->seed == seed)
		{
			return icon;
		}
	}
	return entt::null;
}

void SpellIconSystem::TownJoinedSite(entt::entity site, entt::entity town)
{
	auto& registry = Entities();
	if (!registry.Valid(site) || !registry.AllOf<WorshipSite>(site))
	{
		return;
	}
	for (const auto seed : SeedsOfTown(town))
	{
		AddSiteIcon(site, seed);
	}
}

void SpellIconSystem::AddSiteIcon(entt::entity site, SpellSeedType seed)
{
	auto& registry = Entities();
	if (SiteIconOf(site, seed) != entt::null)
	{
		return;
	}
	if (const auto icon = SpellIconArchetype::CreateSiteIcon(site, seed); icon != entt::null)
	{
		auto& list = registry.AnyOf<SiteIcons>(site) ? registry.Get<SiteIcons>(site) : registry.Assign<SiteIcons>(site);
		list.icons.push_back(icon);
		ShowPlayersPowerUp(icon);
	}
}

void SpellIconSystem::ShowPlayersPowerUp(entt::entity icon) const
{
	auto& registry = Entities();
	const auto* component = registry.TryGet<const WorshipIcon>(icon);
	auto* graphic = registry.TryGet<IconSeed>(icon);
	if (component == nullptr || graphic == nullptr || !Locator::infoConstants::has_value())
	{
		return;
	}
	const auto* site = registry.Valid(component->site) ? registry.TryGet<const WorshipSite>(component->site) : nullptr;
	if (site == nullptr)
	{
		return;
	}
	// Unlike a village centre's hand, which shows what its town offers, a site's icon shows what its player may cast
	const auto& seedInfo = magic::GetSpellSeedInfo(Locator::infoConstants::value(), component->seed);
	std::array<MagicType, icons::k_PowerUpLevels> levels {};
	for (size_t level = 0; level < levels.size(); ++level)
	{
		levels.at(level) = magic::GetMagicTypeFromPowerUpLevel(seedInfo, static_cast<int>(level));
	}
	const auto player = site->player;
	graphic->powerUp = icons::SiteShownPowerUp(levels, [this, player](MagicType type) { return IsEnabled(player, type); });
}

void SpellIconSystem::TownLeftSite(entt::entity site, entt::entity town)
{
	auto& registry = Entities();
	const auto* component = registry.Valid(site) ? registry.TryGet<const WorshipSite>(site) : nullptr;
	if (component == nullptr)
	{
		return;
	}
	// A miracle still offered by another of its towns keeps its icon
	std::vector<std::vector<SpellSeedType>> others;
	for (const auto other : component->towns)
	{
		if (other != town)
		{
			others.push_back(SeedsOfTown(other));
		}
	}
	const auto kept = icons::SiteSeeds(others);
	for (const auto seed : SeedsOfTown(town))
	{
		if (std::ranges::find(kept, seed) == kept.end())
		{
			if (const auto icon = SiteIconOf(site, seed); icon != entt::null)
			{
				Destroy(icon);
			}
		}
	}
}

void SpellIconSystem::SiteRemoved(entt::entity site)
{
	auto& registry = Entities();
	auto* list = registry.Valid(site) ? registry.TryGet<SiteIcons>(site) : nullptr;
	if (list == nullptr)
	{
		return;
	}
	// Copied: each icon takes itself off the site's list as it goes
	const auto icons = list->icons;
	for (const auto icon : icons)
	{
		if (registry.Valid(icon))
		{
			Destroy(icon);
		}
	}
}

void SpellIconSystem::Destroy(entt::entity icon)
{
	auto& registry = Entities();
	if (const auto* component = registry.TryGet<const WorshipIcon>(icon))
	{
		if (auto* list = registry.Valid(component->site) ? registry.TryGet<SiteIcons>(component->site) : nullptr)
		{
			std::erase(list->icons, icon);
		}
		// The seeds made from it go with it, from the hand or from the miracles they cast
		for (const auto seed : component->seedsOut)
		{
			if (registry.Valid(seed) && registry.AllOf<SpellSeed>(seed) && Locator::magicSystem::has_value())
			{
				Locator::magicSystem::value().DeleteSeed(seed);
			}
		}
	}
	registry.Destroy(icon);
}

void SpellIconSystem::SetEnabled(PlayerNames player, MagicType type, bool enable) const
{
	auto* component = PlayerNamed(player);
	if (component == nullptr)
	{
		return;
	}
	player_miracles::SetMagicTypeEnabled(component->miracles, type, enable);
	// The icons of the player's worship sites show the power-up levels the player may cast now
	auto& registry = Entities();
	std::vector<entt::entity> siteIcons;
	registry.Each<const WorshipIcon>([&](entt::entity icon, const WorshipIcon& worship) {
		const auto* site = registry.Valid(worship.site) ? registry.TryGet<const WorshipSite>(worship.site) : nullptr;
		if (site != nullptr && site->player == player)
		{
			siteIcons.push_back(icon);
		}
	});
	for (const auto icon : siteIcons)
	{
		ShowPlayersPowerUp(icon);
	}
}

bool SpellIconSystem::IsEnabled(PlayerNames player, MagicType type) const
{
	const auto* component = PlayerNamed(player);
	return component != nullptr && player_miracles::IsMagicTypeEnabled(component->miracles, type);
}

bool SpellIconSystem::HandReady(PlayerNames player) const
{
	// Only the hand of a human player takes miracles from icons, and only while it holds nothing. Each computer reads the
	// same hands, so the turns may read this.
	if (!Locator::magicSystem::has_value())
	{
		return false;
	}
	// A computer god has hands too, but its hand never takes a miracle: the god casts from the store of the icon
	const bool computer = Locator::playerSystem::has_value() && Locator::playerSystem::value().IsComputerPlayer(player);
	const bool human = Locator::handSystem::has_value() ? Locator::handSystem::value().GetHandsOf(player).has_value()
	                                                    : player == PlayerNames::PLAYER_ONE;
	if (!human || computer)
	{
		return false;
	}
	const bool holdsThing = Locator::handGrabSystem::has_value() && Locator::handGrabSystem::value().IsBusyOf(player);
	return !holdsThing && !Locator::magicSystem::value().IsHandBusyOf(player);
}

float SpellIconSystem::Required(const WorshipIcon& icon) const
{
	if (!Locator::infoConstants::has_value() || icon.seed == SpellSeedType::None)
	{
		return 0.0f;
	}
	const auto& info = Locator::infoConstants::value();
	const auto type = magic::GetMagicTypeFromPowerUpLevel(magic::GetSpellSeedInfo(info, icon.seed), icon.powerUp);
	return magic::GetChantsRequiredToCreate(info, type);
}

float SpellIconSystem::SeedCost(const SpellSeed& seed) const
{
	if (!Locator::infoConstants::has_value())
	{
		return 0.0f;
	}
	const auto& info = Locator::infoConstants::value();
	return magic::GetChantsRequiredToCreate(
	    info, magic::GetMagicTypeFromPowerUpLevel(magic::GetSpellSeedInfo(info, seed.seedType), seed.powerUp));
}

entt::entity SpellIconSystem::HeldSeedOf(const WorshipIcon& icon) const
{
	auto& registry = Entities();
	const auto* site = registry.Valid(icon.site) ? registry.TryGet<const WorshipSite>(icon.site) : nullptr;
	if (site == nullptr)
	{
		return entt::null;
	}
	// Only the hand of the icon's player counts
	const auto held =
	    Locator::magicSystem::has_value() ? Locator::magicSystem::value().GetHeldSeedOf(site->player) : std::nullopt;
	if (!held.has_value() || !registry.Valid(*held) || std::ranges::find(icon.seedsOut, *held) == icon.seedsOut.end())
	{
		return entt::null;
	}
	const auto* seed = registry.TryGet<const SpellSeed>(*held);
	return seed != nullptr && seed->player == site->player ? *held : entt::null;
}

float SpellIconSystem::RequiredNow(const WorshipIcon& icon, entt::entity heldSeed) const
{
	if (heldSeed == entt::null)
	{
		return Required(icon);
	}
	return magic::ChantNeeded(Required(icon), Entities().Get<const SpellSeed>(heldSeed).chantStore);
}

icons::IconCharge SpellIconSystem::ChargeOf(const WorshipIcon& icon, PlayerNames tapping) const
{
	const auto held = HeldSeedOf(icon);
	return {.charging = icon.charging,
	        .chargedByThisHand = icon.charging && icon.charger == tapping,
	        .seedHeld = held != entt::null,
	        .store = icon.store,
	        .required = RequiredNow(icon, held)};
}

bool SpellIconSystem::GiveSeed(entt::entity icon)
{
	auto& registry = Entities();
	auto& component = registry.Get<WorshipIcon>(icon);
	if (!Locator::magicSystem::has_value())
	{
		return false;
	}
	// The seed takes what it needs of the icon's store, which is all of it once the icon is charged full
	const float carried = std::min(Required(component), component.store);
	const auto seed = Locator::magicSystem::value().TakeIconSeed(component.site, component.seed, component.powerUp, carried);
	if (seed == entt::null)
	{
		return false;
	}
	component.store -= carried;
	// Seeds made before that are gone no longer count as out
	std::erase_if(component.seedsOut,
	              [&registry](entt::entity out) { return !registry.Valid(out) || !registry.AllOf<SpellSeed>(out); });
	component.seedsOut.push_back(seed);
	return true;
}

icons::TapResult SpellIconSystem::TapSiteIcon(entt::entity icon, PlayerNames player)
{
	auto& registry = Entities();
	auto& component = registry.Get<WorshipIcon>(icon);
	const auto* site = registry.Valid(component.site) ? registry.TryGet<const WorshipSite>(component.site) : nullptr;
	if (site == nullptr || !Locator::infoConstants::has_value())
	{
		return icons::TapResult::Nothing;
	}
	const auto& info = Locator::infoConstants::value();
	const auto plain =
	    magic::GetMagicTypeFromPowerUpLevel(magic::GetSpellSeedInfo(info, component.seed), magic::k_BasePowerUpLevel);
	const bool handReady = HandReady(player);
	const auto result = icons::Tap(ChargeOf(component, player), handReady, IsEnabled(player, plain), true);
	switch (result)
	{
	case icons::TapResult::PutInHand:
		GiveSeed(icon);
		break;
	case icons::TapResult::Cancelled:
		// Called off, its prayer power goes back to its site
		component.charging = false;
		component.powerUp = magic::k_BasePowerUpLevel;
		component.charger = PlayerNames::NEUTRAL;
		if (Locator::worshipSiteSystem::has_value() && component.store != 0.0f)
		{
			Locator::worshipSiteSystem::value().ReturnChants(component.site, component.store);
		}
		component.store = 0.0f;
		break;
	case icons::TapResult::StartedCharge:
		StartCharge(icon, player);
		break;
	case icons::TapResult::Nothing:
		break;
	}
	return result;
}

bool SpellIconSystem::StartChargeForGod(entt::entity icon, PlayerNames player)
{
	auto& registry = Entities();
	auto* component = registry.Valid(icon) ? registry.TryGet<WorshipIcon>(icon) : nullptr;
	if (component == nullptr || component->charging || HeldSeedOf(*component) != entt::null ||
	    !Locator::infoConstants::has_value())
	{
		return false;
	}
	const auto& info = Locator::infoConstants::value();
	const auto plain =
	    magic::GetMagicTypeFromPowerUpLevel(magic::GetSpellSeedInfo(info, component->seed), magic::k_BasePowerUpLevel);
	if (!IsEnabled(player, plain))
	{
		return false;
	}
	// The site must have prayer power to give this turn, or the icon must have some stored
	const auto* chants = registry.Valid(component->site) ? registry.TryGet<const WorshipChants>(component->site) : nullptr;
	if (chants != nullptr)
	{
		const float available = chants->infinite ? magic::k_InfiniteChants : chants->available;
		if (!(available > 0.0f) && component->store == 0.0f)
		{
			return false;
		}
	}
	else if (component->store == 0.0f)
	{
		return false;
	}
	component->charging = true;
	component->powerUp = magic::k_BasePowerUpLevel;
	component->charger = player;
	component->chargeStartTurn = Locator::time::has_value() ? Locator::time::value().GetTurn() : 0;
	return true;
}

float SpellIconSystem::ChantNeeded(entt::entity icon) const
{
	const auto& registry = Entities();
	const auto* component = registry.Valid(icon) ? registry.TryGet<const WorshipIcon>(icon) : nullptr;
	return component != nullptr ? icons::Need(ChargeOf(*component, PlayerNames::NEUTRAL)) : 0.0f;
}

void SpellIconSystem::TakeCastCharge(entt::entity icon)
{
	auto& registry = Entities();
	auto* component = registry.Valid(icon) ? registry.TryGet<WorshipIcon>(icon) : nullptr;
	if (component == nullptr)
	{
		return;
	}
	const float cost = RequiredNow(*component, HeldSeedOf(*component));
	component->store -= std::min(cost, component->store);
}

void SpellIconSystem::StartCharge(entt::entity icon, PlayerNames player)
{
	auto& component = Entities().Get<WorshipIcon>(icon);
	component.charging = true;
	component.powerUp = magic::k_BasePowerUpLevel;
	component.charger = player;
	component.chargeStartTurn = Locator::time::has_value() ? Locator::time::value().GetTurn() : 0;
	// Already full, it goes straight into the hand
	if (icons::ReadyForHand(ChargeOf(component, player), HandReady(player)))
	{
		GiveSeed(icon);
	}
}

std::vector<entt::entity> SpellIconSystem::SitesOf(PlayerNames player) const
{
	std::vector<entt::entity> sites;
	if (!Locator::worshipSiteSystem::has_value())
	{
		return sites;
	}
	const auto& registry = Entities();
	const auto temple = Locator::worshipSiteSystem::value().TempleOf(player);
	const auto* worship = registry.Valid(temple) ? registry.TryGet<const CitadelWorship>(temple) : nullptr;
	if (worship == nullptr)
	{
		return sites;
	}
	for (const auto site : worship->sites)
	{
		if (registry.Valid(site) && registry.AllOf<WorshipSite>(site))
		{
			sites.push_back(site);
		}
	}
	return sites;
}

bool SpellIconSystem::IconValidForRequest(entt::entity icon, PlayerNames player) const
{
	const auto& registry = Entities();
	const auto* component = registry.Valid(icon) ? registry.TryGet<const WorshipIcon>(icon) : nullptr;
	if (component == nullptr || !Locator::infoConstants::has_value() || !Locator::worshipSiteSystem::has_value())
	{
		return false;
	}
	const auto& sites = Locator::worshipSiteSystem::value();
	const bool functional = icons::Functional(ecs::world_objects::LifeOf(icon), sites.IsBuilt(component->site));
	const auto plain = magic::GetMagicTypeFromPowerUpLevel(
	    magic::GetSpellSeedInfo(Locator::infoConstants::value(), component->seed), magic::k_BasePowerUpLevel);
	return icons::ValidForRequest(ChargeOf(*component, player), functional, HandReady(player), IsEnabled(player, plain),
	                              sites.TotalChantsAvailable(component->site));
}

std::vector<SpellSeedType> SpellIconSystem::SeedsForSelection(PlayerNames player, GestureType opening) const
{
	std::vector<SpellSeedType> seeds;
	if (opening == GestureType::None || !Locator::infoConstants::has_value())
	{
		return seeds;
	}
	const auto& info = Locator::infoConstants::value();
	const auto& registry = Entities();
	for (const auto site : SitesOf(player))
	{
		const auto* list = registry.TryGet<const SiteIcons>(site);
		if (list == nullptr)
		{
			continue;
		}
		for (const auto icon : list->icons)
		{
			const auto* component = registry.Valid(icon) ? registry.TryGet<const WorshipIcon>(icon) : nullptr;
			if (component != nullptr && magic::GetSpellSeedInfo(info, component->seed).selectionGesture == opening &&
			    IconValidForRequest(icon, player))
			{
				seeds.push_back(component->seed);
			}
		}
	}
	return seeds;
}

bool SpellIconSystem::ValidForRequest(PlayerNames player, SpellSeedType seed) const
{
	if (seed == SpellSeedType::None)
	{
		return false;
	}
	return std::ranges::any_of(SitesOf(player), [this, player, seed](entt::entity site) {
		return IconValidForRequest(SiteIconOf(site, seed), player);
	});
}

SpellSeedType SpellIconSystem::LastSeedGained(PlayerNames player) const
{
	auto seed = SpellSeedType::None;
	Entities().Each<const ecs::components::Player>([player, &seed](entt::entity, const ecs::components::Player& record) {
		if (record.name == player)
		{
			seed = record.lastSeedGained;
		}
	});
	return seed;
}

bool SpellIconSystem::RequestSeed(PlayerNames player, SpellSeedType seed)
{
	if (seed == SpellSeedType::None || !Locator::worshipSiteSystem::has_value() || !Locator::infoConstants::has_value())
	{
		return false;
	}
	// The icon at the site with the most prayer power left this turn gives the miracle; the first site wins a tie
	entt::entity best = entt::null;
	float bestChants = -1.0f;
	for (const auto site : SitesOf(player))
	{
		const float chants = Locator::worshipSiteSystem::value().ChantsAvailable(site);
		const auto icon = SiteIconOf(site, seed);
		if (IconValidForRequest(icon, player) && chants > bestChants)
		{
			bestChants = chants;
			best = icon;
		}
	}
	if (best == entt::null)
	{
		return false;
	}
	const auto& component = Entities().Get<const WorshipIcon>(best);
	const auto plain = magic::GetMagicTypeFromPowerUpLevel(
	    magic::GetSpellSeedInfo(Locator::infoConstants::value(), component.seed), magic::k_BasePowerUpLevel);
	switch (icons::Request(ChargeOf(component, player), HandReady(player), IsEnabled(player, plain),
	                       Locator::worshipSiteSystem::value().TotalChantsAvailable(component.site)))
	{
	case icons::TapResult::PutInHand:
		return GiveSeed(best);
	case icons::TapResult::StartedCharge:
		StartCharge(best, player);
		return true;
	case icons::TapResult::Cancelled:
	case icons::TapResult::Nothing:
		break;
	}
	return false;
}

bool SpellIconSystem::ValidToTap(entt::entity icon, PlayerNames player) const
{
	const auto& registry = Entities();
	if (!registry.Valid(icon))
	{
		return false;
	}
	// A stone hand is its town's player's, a worship site's icon its site's player's. The icon must also work: a stone
	// hand has no building state, so it is always built. A worship site's icon is built when its site is built.
	const float life = ecs::world_objects::LifeOf(icon);
	if (const auto* centreHand = registry.TryGet<const TownCentreIcon>(icon))
	{
		const auto* town = registry.Valid(centreHand->town) ? registry.TryGet<const Town>(centreHand->town) : nullptr;
		return town != nullptr && icons::ValidToTap(town->owner, player, icons::Functional(life, true));
	}
	if (const auto* worshipIcon = registry.TryGet<const WorshipIcon>(icon))
	{
		const auto* site = registry.Valid(worshipIcon->site) ? registry.TryGet<const WorshipSite>(worshipIcon->site) : nullptr;
		const bool built =
		    Locator::worshipSiteSystem::has_value() && Locator::worshipSiteSystem::value().IsBuilt(worshipIcon->site);
		return site != nullptr && icons::ValidToTap(site->player, player, icons::Functional(life, built));
	}
	return false;
}

bool SpellIconSystem::Tap(entt::entity icon, PlayerNames player)
{
	auto& registry = Entities();
	if (!registry.Valid(icon))
	{
		return false;
	}
	// A village centre's hand taps its town's worship site's icon of its miracle; it counts only for the town's player,
	// and does nothing while the town has no site or the site no such icon
	entt::entity siteIcon = icon;
	size_t hand = 0;
	if (const auto* centreHand = registry.TryGet<const TownCentreIcon>(icon))
	{
		const auto* town = registry.Valid(centreHand->town) ? registry.TryGet<const Town>(centreHand->town) : nullptr;
		if (town == nullptr || town->owner != player || !registry.Valid(centreHand->centre))
		{
			return false;
		}
		siteIcon = SiteIconOf(town->worshipSite, centreHand->seed);
		hand = centreHand->hand;
	}
	else if (const auto* worshipIcon = registry.TryGet<const WorshipIcon>(icon))
	{
		const auto* site = registry.Valid(worshipIcon->site) ? registry.TryGet<const WorshipSite>(worshipIcon->site) : nullptr;
		if (site == nullptr || site->player != player)
		{
			return false;
		}
	}
	else
	{
		return false;
	}
	if (siteIcon == entt::null)
	{
		return false;
	}
	const auto result = TapSiteIcon(siteIcon, player);
	// A tap that did something clicks, faster the further along the centre its hand stands, for the hand at this machine
	if (result != icons::TapResult::Nothing && player == OwnPlayer() && Locator::audio::has_value())
	{
		audio::StartGameSoundEffect(static_cast<entt::id_type>(audio::SoundId::G_ClickOnSpell_01),
		                            {.pitchPercent = icons::ClickPitch(hand)});
	}
	return true;
}

bool SpellIconSystem::HasIconFor(PlayerNames player, SpellSeedType seed) const
{
	bool found = false;
	auto& registry = Entities();
	registry.Each<const WorshipIcon>([&](const WorshipIcon& icon) {
		const auto* site = registry.Valid(icon.site) ? registry.TryGet<const WorshipSite>(icon.site) : nullptr;
		found = found || (icon.seed == seed && site != nullptr && site->player == player);
	});
	return found;
}

bool SpellIconSystem::IsIcon(entt::entity entity) const
{
	auto& registry = Entities();
	return registry.Valid(entity) && registry.AnyOf<TownCentreIcon, WorshipIcon>(entity);
}

std::optional<float> SpellIconSystem::ShownCharge(entt::entity icon) const
{
	auto& registry = Entities();
	const auto* component = registry.Valid(icon) ? registry.TryGet<const WorshipIcon>(icon) : nullptr;
	if (component == nullptr)
	{
		return std::nullopt;
	}
	// The seed that the icon made counts only while a hand of the icon's player holds it
	std::optional<float> heldSeedCharge;
	if (const auto held = HeldSeedOf(*component); held != entt::null)
	{
		heldSeedCharge = registry.Get<const SpellSeed>(held).chantStore;
	}
	return icons::ShownCharge(component->store, icons::ShownRequired(Required(*component), heldSeedCharge));
}

std::vector<magic::WorshipIconCharge> SpellIconSystem::ChargesAt(entt::entity site) const
{
	auto& registry = Entities();
	std::vector<magic::WorshipIconCharge> charges;
	const auto* list = registry.Valid(site) ? registry.TryGet<const SiteIcons>(site) : nullptr;
	if (list == nullptr)
	{
		return charges;
	}
	for (const auto icon : list->icons)
	{
		const auto& component = registry.Get<const WorshipIcon>(icon);
		const auto charge = ChargeOf(component, component.charger);
		const bool out = std::ranges::any_of(component.seedsOut, [&registry](entt::entity seed) {
			return registry.Valid(seed) && registry.AllOf<SpellSeed>(seed);
		});
		charges.push_back({.charging = charge.charging,
		                   .seedOut = out,
		                   .seedHeld = charge.seedHeld,
		                   .required = charge.required,
		                   .store = charge.store});
	}
	return charges;
}

void SpellIconSystem::ChargedAt(entt::entity site, std::span<const magic::WorshipIconCharge> charges)
{
	auto& registry = Entities();
	auto* list = registry.Valid(site) ? registry.TryGet<SiteIcons>(site) : nullptr;
	if (list == nullptr)
	{
		return;
	}
	const auto sitePlayer = registry.Get<const WorshipSite>(site).player;
	const auto made = list->icons;
	for (size_t i = 0; i < made.size() && i < charges.size(); ++i)
	{
		auto& component = registry.Get<WorshipIcon>(made.at(i));
		component.store = charges[i].store;
		if (!component.charging)
		{
			continue;
		}
		// The seed of it that a hand holds takes what the icon stored, up to what the seed still needs
		const auto held = HeldSeedOf(component);
		auto* heldSeed = held != entt::null ? &registry.Get<SpellSeed>(held) : nullptr;
		const auto turn = icons::ChargingIconTurn(component.store, Required(component),
		                                          heldSeed != nullptr ? std::optional(heldSeed->chantStore) : std::nullopt);
		component.store = turn.store;
		if (heldSeed != nullptr && turn.seedStore.has_value())
		{
			heldSeed->chantStore = *turn.seedStore;
			heldSeed->power = magic::SeedPower(heldSeed->chantStore, SeedCost(*heldSeed));
		}
		if (!turn.full)
		{
			continue;
		}
		// The charge is done. The held seed takes the power-up level of the charge. Else the miracle goes into the hand
		// that asked for it if that hand can take it, and the announcer names it to the player at this machine.
		if (heldSeed != nullptr)
		{
			if (Locator::magicSystem::has_value())
			{
				Locator::magicSystem::value().SetIconSeedPowerUp(held, component.powerUp);
			}
		}
		else
		{
			const auto charge = ChargeOf(component, component.charger);
			if (icons::ReadyForHand(charge, HandReady(component.charger)))
			{
				GiveSeed(made.at(i));
			}
			if (sitePlayer == OwnPlayer() && Locator::audio::has_value())
			{
				const auto voice = icons::FullyChargedVoice(component.seed);
				audio::StartGameSoundEffect(entt::hashed_string(fmt::format("SpellDialogue.sad/{}", voice).c_str()).value(),
				                            {.voicePlayType = pack::AudioBankLoop::Once});
			}
		}
		component.charging = false;
		component.charger = PlayerNames::NEUTRAL;
		component.powerUp = magic::k_BasePowerUpLevel;
	}
}

void SpellIconSystem::Update(float seconds)
{
	if (!Locator::infoConstants::has_value())
	{
		return;
	}
	const auto& info = Locator::infoConstants::value();
	// The seeds spin as a globe's do, their rings round them, and their phials pulse
	Entities().Each<IconSeed>([&](IconSeed& shown) {
		shown.spin = std::fmod(shown.spin + (magic::k_OrbSeedSpin * seconds), glm::two_pi<float>());
		shown.ringSpin = std::fmod(shown.ringSpin + (magic::visuals::k_RingSpin * seconds), glm::two_pi<float>());
		if (magic::visuals::IsPhialSeed(shown.seed))
		{
			const auto* spell =
			    magic::GetMagicInfoAs<GMagicCreatureSpellInfo>(info, magic::GetSpellSeedInfo(info, shown.seed).magicTypes[0]);
			const int receiveType = spell != nullptr ? static_cast<int>(spell->creatureReceiveSpellType) : 0;
			shown.phialFrame = magic::visuals::StepPhialFrame(shown.phialFrame, seconds);
			shown.phialPhase = std::fmod(shown.phialPhase + (magic::visuals::PhialPulseSpeed(receiveType) * seconds), 1.0f);
		}
	});
}

void SpellIconSystem::Reset()
{
	// The icons are entities of the land and go with it
}
