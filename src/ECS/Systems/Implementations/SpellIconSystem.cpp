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
#include "ECS/Components/WorshipSite.h"
#include "ECS/PlayerMiracles.h"
#include "ECS/Registry.h"
#include "ECS/Systems/HandGrabSystemInterface.h"
#include "ECS/Systems/MagicSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/Systems/WorshipSiteSystemInterface.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/DispenserRules.h"
#include "Magic/MiracleVisuals.h"

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

[[nodiscard]] PlayerNames LocalPlayer()
{
	return Locator::playerSystem::has_value() ? Locator::playerSystem::value().GetLocalPlayer() : PlayerNames::PLAYER_ONE;
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
		AddPowerUp(centre, level->seed, level->powerUp);
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
}

void SpellIconSystem::AddPowerUp(entt::entity centre, SpellSeedType seed, int powerUp)
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
			icon->powerUps.at(static_cast<size_t>(powerUp)) = true;
			// The seed above it shows the highest level with a ring for each
			if (auto* shown = registry.TryGet<IconSeed>(hand))
			{
				shown->powerUp = icons::ShownPowerUp(icon->powerUps);
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
				AddPowerUp(centre, level->seed, level->powerUp);
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
		if (SiteIconOf(site, seed) != entt::null)
		{
			continue;
		}
		if (const auto icon = SpellIconArchetype::CreateSiteIcon(site, seed); icon != entt::null)
		{
			auto& list = registry.AnyOf<SiteIcons>(site) ? registry.Get<SiteIcons>(site) : registry.Assign<SiteIcons>(site);
			list.icons.push_back(icon);
		}
	}
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
		// The seed made from it goes with it, from the hand or from the miracle it cast
		if (const auto seed = component->seedOut;
		    registry.Valid(seed) && registry.AllOf<SpellSeed>(seed) && Locator::magicSystem::has_value())
		{
			Locator::magicSystem::value().DeleteSeed(seed);
		}
	}
	registry.Destroy(icon);
}

void SpellIconSystem::SetEnabled(PlayerNames player, MagicType type, bool enable) const
{
	if (auto* component = PlayerNamed(player))
	{
		player_miracles::SetMagicTypeEnabled(component->miracles, type, enable);
	}
}

bool SpellIconSystem::IsEnabled(PlayerNames player, MagicType type) const
{
	const auto* component = PlayerNamed(player);
	return component != nullptr && player_miracles::IsMagicTypeEnabled(component->miracles, type);
}

bool SpellIconSystem::HandReady(PlayerNames player) const
{
	// Only the hand at this machine takes miracles from icons, and only while it holds nothing
	if (player != LocalPlayer() || !Locator::magicSystem::has_value())
	{
		return false;
	}
	const bool holdsThing = Locator::handGrabSystem::has_value() && Locator::handGrabSystem::value().IsBusy();
	return !holdsThing && !Locator::magicSystem::value().IsHandBusy();
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

icons::IconCharge SpellIconSystem::ChargeOf(const WorshipIcon& icon, PlayerNames tapping) const
{
	auto& registry = Entities();
	return {.charging = icon.charging,
	        .chargedByThisHand = icon.charging && icon.charger == tapping,
	        .seedOut = registry.Valid(icon.seedOut) && registry.AllOf<SpellSeed>(icon.seedOut),
	        .store = icon.store,
	        .required = Required(icon)};
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
	component.seedOut = seed;
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
		component.charging = true;
		component.powerUp = magic::k_BasePowerUpLevel;
		component.charger = player;
		component.chargeStartTurn = Locator::time::has_value() ? Locator::time::value().GetTurn() : 0;
		// Already full, it goes straight into the hand
		if (icons::ReadyForHand(ChargeOf(component, player), handReady))
		{
			GiveSeed(icon);
		}
		break;
	case icons::TapResult::Nothing:
		break;
	}
	return result;
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
	if (result != icons::TapResult::Nothing && player == LocalPlayer() && Locator::audio::has_value())
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
		charges.push_back(
		    {.charging = charge.charging, .seedOut = charge.seedOut, .required = charge.required, .store = charge.store});
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
		const auto charge = ChargeOf(component, component.charger);
		if (icons::Need(charge) > 0.0f)
		{
			continue;
		}
		// Charged full: its miracle jumps into the hand that asked for it if that hand can take it, and the announcer
		// names it to the player at this machine. Either way it stops charging.
		if (!charge.seedOut)
		{
			if (icons::ReadyForHand(charge, HandReady(component.charger)))
			{
				GiveSeed(made.at(i));
			}
			if (sitePlayer == LocalPlayer() && Locator::audio::has_value())
			{
				const auto voice = icons::FullyChargedVoice(component.seed);
				audio::StartGameSoundEffect(entt::hashed_string(fmt::format("SpellDialogue.sad/{}", voice).c_str()).value(),
				                            {});
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
