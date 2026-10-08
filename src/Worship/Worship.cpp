/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "Worship.h"

#include <cstdio>
#include <cstdlib>

#include <vector>

#include "Citadel.h"
#include "ECS/Components/OneOffSpellSeed.h"
#include "ECS/Components/SpellIcon.h"
#include "ECS/Components/SpellSeed.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/Registry.h"
#include "ECS/Systems/WorshipStateInterface.h"
#include "ECS/ToBeDeleted.h"
#include "FireFlyReward.h"
#include "GestureIconProvider.h"
#include "InterfaceStatus.h"
#include "Locator.h"
#include "Magic/Core/OneOffSpellSeed.h"
#include "Magic/Core/SpellSeed.h"
#include "PlayerSpellIcons.h"
#include "SpellDispenser.h"
#include "SpellSeedGraphic.h"
#include "WorshipPercentage.h"
#include "WorshipSite.h"
#include "WorshipSpellIcon.h"

using namespace openblack;
using namespace openblack::worship;
using namespace openblack::ecs::components;

namespace
{
struct WorshipLoopState
{
	bool postLoadDone {false};
};

/// This module's state (Locator::worshipState)
WorshipLoopState& Loop()
{
	if (!Locator::worshipState::has_value())
	{
		std::fputs("worship: no worship state in the locator (Locator::worshipState)\n", stderr);
		std::abort();
	}
	return Locator::worshipState::value().Get<WorshipLoopState>();
}

auto& Registry()
{
	return Locator::entitiesRegistry::value();
}
} // namespace

void worship::OnLoadMap()
{
	Loop().postLoadDone = false;
	player::Reset();
	fire_fly::Reset(); // the fireflies go with the old map
	gesture_icons::Register();
	ResetDebugHooks();
}

void worship::ProcessSpellIcons()
{
	seed_graphic::ProcessTurn();
	player::ProcessSpellIcons();
}

void worship::ProcessTurn(uint32_t turn)
{
	if (!Locator::entitiesRegistry::has_value())
	{
		return;
	}
	if (auto& loop = Loop(); !loop.postLoadDone)
	{
		// The players' post-load cleanup, once the land's script has run
		loop.postLoadDone = true;
		citadel::PostLoadCleanup();
	}
	RunDebugHooks(turn);
}

void worship::Update(float seconds)
{
	if (!Locator::entitiesRegistry::has_value())
	{
		return;
	}
	const float milliseconds = seconds * 1000.0f;
	seed_graphic::UpdatePhase(milliseconds);
	seed_graphic::UpdateIconGraphics(milliseconds); // site and town icons
	auto& registry = Registry();
	std::vector<entt::entity> icons;
	registry.Each<const WorshipSpellIcon>([&](entt::entity icon, const WorshipSpellIcon&) { icons.push_back(icon); });
	for (const auto icon : icons)
	{
		icon::UpdateChargingVisual(icon, seed_graphic::Phase());
	}
	std::vector<entt::entity> sites;
	registry.Each<const WorshipSite>([&](entt::entity site, const WorshipSite&) { sites.push_back(site); });
	for (const auto site : sites)
	{
		site::UpdateStrainVisual(site, milliseconds);
	}
	percentage::UpdateTotems();
}

bool worship::InterfaceValidToTap(entt::entity object, PlayerNames player)
{
	auto& registry = Registry();
	if (object == entt::null || !ecs::IsAvailable(object))
	{
		return false;
	}
	if (registry.AllOf<SpellIcon>(object))
	{
		return icon::InterfaceValidToTap(object, player);
	}
	return registry.AllOf<OneOffSpellSeed>(object); // a one-shot orb can always be tapped
}

int worship::InterfaceTap(entt::entity object, PlayerNames player)
{
	if (!InterfaceValidToTap(object, player))
	{
		return 0;
	}
	if (Registry().AllOf<SpellIcon>(object))
	{
		return icon::InterfaceTap(object, player);
	}
	return magic::one_off::InterfaceTap(object, player);
}

void worship::OnPlacedInMagicHand(entt::entity object)
{
	// A one-shot orb picked up itself (held past the 225 ms of the grab) only marks its magic ever enabled
	if (object != entt::null && Registry().Valid(object) && Registry().AllOf<OneOffSpellSeed>(object))
	{
		magic::one_off::InterfaceSetInMagicHand(object, PlayerNames::PLAYER_ONE);
	}
	fire_fly::OnPlacedInMagicHand(object);
}

bool worship::IsSeedReturnPoint(entt::entity object, PlayerNames player)
{
	auto& registry = Registry();
	if (object == entt::null || !ecs::IsAvailable(object))
	{
		return false;
	}
	if (dispenser::IsDispenser(object))
	{
		return true; // no player check: a dispenser takes any seed
	}
	if (const auto* totem = registry.TryGet<const WorshipTotem>(object); totem != nullptr && totem->site != entt::null)
	{
		return registry.Get<const WorshipSite>(totem->site).player == player;
	}
	if (const auto* spellIcon = registry.TryGet<const SpellIcon>(object); spellIcon != nullptr)
	{
		return spellIcon->player == player;
	}
	return false;
}

int worship::ApplySeedToObject(entt::entity seedEntity, entt::entity object)
{
	auto& registry = Registry();
	if (!registry.Valid(seedEntity) || !registry.AllOf<SpellSeed>(seedEntity) || object == entt::null ||
	    !ecs::IsAvailable(object))
	{
		return 0;
	}
	if (dispenser::IsDispenser(object))
	{
		return dispenser::ApplySeed(object, seedEntity) ? 3 : 0;
	}
	// A WorshipTotem -> its site; a SpellIcon -> its site's battery, the seed goes, and an icon of another seed of the
	// same player gives its fully charged seed instead (the swap)
	if (const auto* totem = registry.TryGet<const WorshipTotem>(object); totem != nullptr && totem->site != entt::null)
	{
		auto& seed = registry.Get<SpellSeed>(seedEntity);
		registry.Get<WorshipSite>(totem->site).battery += seed.chantStore;
		magic::seed::SetChantStore(seed, 0.0f);
		magic::seed::ToBeDeleted(seedEntity);
		return 3;
	}
	const auto* spellIcon = registry.TryGet<const SpellIcon>(object);
	if (spellIcon == nullptr)
	{
		return 0;
	}
	auto& seed = registry.Get<SpellSeed>(seedEntity);
	const bool otherSeed = spellIcon->seedType != seed.seedType;
	const auto player = seed.creator.player;
	// The worship icon of the icon given to, and its site
	const auto target = icon::WorshipIconOf(object);
	const entt::entity site = target != entt::null ? registry.Get<const WorshipSpellIcon>(target).site : entt::null;
	if (site != entt::null && registry.Valid(site))
	{
		registry.Get<WorshipSite>(site).battery += seed.chantStore;
	}
	magic::seed::SetChantStore(seed, 0.0f);
	magic::seed::ToBeDeleted(seedEntity);
	if (otherSeed && spellIcon->player == player && target != entt::null &&
	    icon::ValidForPutFullyChargedSeedInHand(target, player))
	{
		icon::PutFullyChargedSeedInHand(target, player);
	}
	return 3;
}

int worship::ReturnSeedToItsSite(entt::entity seedEntity)
{
	auto& registry = Registry();
	if (!registry.Valid(seedEntity) || !registry.AllOf<SpellSeed>(seedEntity))
	{
		return 0;
	}
	auto& seed = registry.Get<SpellSeed>(seedEntity);
	if (seed.icon != entt::null && registry.Valid(seed.icon) && registry.AllOf<WorshipSpellIcon>(seed.icon))
	{
		const auto site = registry.Get<const WorshipSpellIcon>(seed.icon).site;
		if (site != entt::null && registry.Valid(site))
		{
			registry.Get<WorshipSite>(site).battery += seed.chantStore;
			magic::seed::SetChantStore(seed, 0.0f);
		}
	}
	magic::seed::ToBeDeleted(seedEntity);
	return 3;
}

void worship::OnSeedOutOfHand(entt::entity seedEntity, PlayerNames player)
{
	auto& registry = Registry();
	if (!registry.Valid(seedEntity) || !registry.AllOf<SpellSeed>(seedEntity))
	{
		return;
	}
	const auto& seed = registry.Get<const SpellSeed>(seedEntity);
	player::SetLastSeedType(player, seed.seedType);
	if (seed.icon != entt::null && registry.Valid(seed.icon) && registry.AllOf<WorshipSpellIcon>(seed.icon))
	{
		icon::CancelCharge(seed.icon, player);
	}
}

int worship::ApplySeedToPosition(entt::entity seedEntity, const glm::vec3& position)
{
	// The worship site at the position takes the seed (3), else 0x17
	const auto site = site::FindAt(position);
	auto& registry = Registry();
	if (site == entt::null)
	{
		return 0x17;
	}
	if (!registry.Valid(seedEntity) || !registry.AllOf<SpellSeed>(seedEntity))
	{
		return 0; // openblack's guard
	}
	auto& seed = registry.Get<SpellSeed>(seedEntity);
	registry.Get<WorshipSite>(site).battery += seed.chantStore;
	magic::seed::SetChantStore(seed, 0.0f);
	magic::seed::ToBeDeleted(seedEntity);
	return 3;
}
