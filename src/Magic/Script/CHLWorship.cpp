/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The CHL natives of worship and miracle supply (CHLApi.cpp forwards to them). The arguments are popped in the
// handlers' order.

#include "CHLWorship.h"

#include <LHVM.h>
#include <spdlog/spdlog.h>

#include "ECS/Components/SpellIcon.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/TownDeaths.h"
#include "ECS/Components/TownMagic.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/Registry.h"
#include "Locator.h"
#include "Magic/Core/OneOffSpellSeed.h"
#include "Magic/Core/Players.h"
#include "Magic/Script/ScriptPlayer.h"
#include "Worship/Citadel.h"
#include "Worship/PlayerSpellIcons.h"
#include "Worship/SpellDispenser.h"
#include "Worship/TownMagic.h"
#include "Worship/WorshipSite.h"

using namespace openblack;
using namespace openblack::magic;
using namespace openblack::ecs::components;

namespace
{
constexpr int k_MagicTypes = 42;

auto& Registry()
{
	return Locator::entitiesRegistry::value();
}

/// The object id is the entity
entt::entity ScriptThing(uint32_t id)
{
	const auto entity = static_cast<entt::entity>(id);
	if (id == 0 || !Registry().Valid(entity))
	{
		return entt::null;
	}
	return entity;
}

/// Truncated, then ScriptPlayerToGamePlayer (Magic/Script/ScriptPlayer.h): 0 the neutral player, n game player n - 1
bool ScriptPlayer(float value, PlayerNames& player)
{
	return magic::ScriptPlayerToGamePlayer(static_cast<int32_t>(value), player);
}

bool IsTown(entt::entity thing)
{
	return thing != entt::null && Registry().AllOf<Town, TownMagic>(thing);
}

/// A worship site, or the site of a thing with one (a town, a citadel part...)
entt::entity WorshipSiteOf(entt::entity thing)
{
	if (thing == entt::null)
	{
		return entt::null;
	}
	if (Registry().AllOf<WorshipSite>(thing))
	{
		return thing;
	}
	if (const auto* magic = Registry().TryGet<const TownMagic>(thing); magic != nullptr)
	{
		return magic->worshipSite;
	}
	if (const auto* icon = Registry().TryGet<const WorshipSpellIcon>(thing); icon != nullptr)
	{
		return icon->site;
	}
	if (const auto* totem = Registry().TryGet<const WorshipTotem>(thing); totem != nullptr)
	{
		return totem->site;
	}
	return entt::null;
}
} // namespace

void script::GameSetMana()
{
	auto& vm = Locator::vm::value();
	const float chants = vm.Popf();
	const auto thing = ScriptThing(vm.Pop().uintVal);
	const auto site = WorshipSiteOf(thing);
	if (site == entt::null)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "GAME_SET_MANA: Thing not valid!");
		return;
	}
	worship::site::SetMana(site, chants);
}

void script::GetMana()
{
	auto& vm = Locator::vm::value();
	const auto thing = ScriptThing(vm.Pop().uintVal);
	if (thing == entt::null || !Registry().AllOf<WorshipSite>(thing))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "GET_MANA: Thing not valid!");
		vm.Pushf(0.0f);
		return;
	}
	vm.Pushf(Registry().Get<const WorshipSite>(thing).battery);
}

void script::SetMagicInObject()
{
	auto& vm = Locator::vm::value();
	const auto thing = ScriptThing(vm.Pop().uintVal);
	const auto magic = vm.Pop().intVal;
	const bool on = vm.Pop().intVal != 0;
	if (thing == entt::null)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "SET_MAGIC_IN_OBJECT: Thing not valid");
		return;
	}
	if (!IsTown(thing))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "SET_MAGIC_IN_OBJECT: Object should be town");
		return;
	}
	if (on)
	{
		worship::town::AddMagicTypesHeld(thing, static_cast<MagicType>(magic));
	}
	else
	{
		worship::town::RemoveMagicTypesHeld(thing, static_cast<MagicType>(magic));
	}
}

void script::SetCanBuildWorshipsite()
{
	auto& vm = Locator::vm::value();
	const auto thing = ScriptThing(vm.Pop().uintVal);
	const bool on = vm.Pop().intVal != 0;
	if (thing == entt::null)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "SET_CAN_BUILD_WORSHIPSITE: Must be town");
		return;
	}
	if (IsTown(thing))
	{
		Registry().Get<TownMagic>(thing).forbidWorshipSite = !on;
		if (!on)
		{
			return;
		}
		worship::town::CheckAddWorshipSite(thing);
		const auto citadel = worship::citadel::Of(worship::town::OwnerOf(thing));
		if (citadel != entt::null)
		{
			Registry().Get<CitadelWorship>(citadel).cannotCreateSites = false; // = !on
			worship::citadel::FindOrCreateWorshipSite(citadel, thing);
		}
		return;
	}
	if (Registry().AllOf<Temple, CitadelWorship>(thing)) // the citadel's heart
	{
		Registry().Get<CitadelWorship>(thing).cannotCreateSites = !on;
		if (on)
		{
			worship::citadel::OpenWorshipSites(thing);
		}
		return;
	}
	SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "SET_CAN_BUILD_WORSHIPSITE: Must be town");
}

void script::IsSpellCharging()
{
	auto& vm = Locator::vm::value();
	PlayerNames player;
	const bool valid = ScriptPlayer(vm.Popf(), player);
	vm.Pushb(valid && worship::player::AnySpellCharging(player));
}

void script::IsThatSpellCharging()
{
	auto& vm = Locator::vm::value();
	const auto magic = vm.Pop().intVal;
	PlayerNames player;
	const bool valid = ScriptPlayer(vm.Popf(), player);
	vm.Pushb(valid && worship::player::IsThatSpellCharging(player, static_cast<MagicType>(magic)));
}

void script::ClearPlayerSpellCharging()
{
	auto& vm = Locator::vm::value();
	PlayerNames player;
	// the local hand's charges of that player
	if (ScriptPlayer(vm.Popf(), player))
	{
		worship::player::CancelAllSpellsCharging(player);
	}
}

void script::GetSpellIconInTemple()
{
	auto& vm = Locator::vm::value();
	const auto thing = ScriptThing(vm.Pop().uintVal);
	const auto magic = vm.Pop().intVal;
	if (thing == entt::null || !Registry().AllOf<Temple, CitadelWorship>(thing) || magic <= 0 || magic >= k_MagicTypes)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "GET_SPELL_ICON_IN_TEMPLE: Invalid Temple");
		vm.Pusho(0);
		return;
	}
	const auto icon = worship::citadel::GetSpellIcon(thing, static_cast<MagicType>(magic));
	vm.Pusho(icon == entt::null ? 0 : static_cast<uint32_t>(icon));
}

void script::GetTownWorshipDeaths()
{
	auto& vm = Locator::vm::value();
	const auto thing = ScriptThing(vm.Pop().uintVal);
	if (!IsTown(thing))
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "GET_TOWN_WORSHIP_DEATHS: Town invalid!");
		vm.Pushf(0.0f);
		return;
	}
	// the town's deaths from worshipping (ECS/Components/TownDeaths.h, written when a villager dies; none before the
	// town's first death)
	const auto* deaths = Registry().TryGet<const TownDeaths>(thing);
	vm.Pushf(static_cast<float>(deaths != nullptr ? deaths->byReason.at(static_cast<size_t>(DeathReason::Chant)) : 0u));
}

void script::SetMagicProperties()
{
	auto& vm = Locator::vm::value();
	const float seconds = vm.Popf();
	const auto magic = vm.Pop().intVal;
	const auto thing = ScriptThing(vm.Pop().uintVal);
	if (thing == entt::null)
	{
		SPDLOG_LOGGER_ERROR(spdlog::get("scripting"), "SET_MAGIC_PROPERTIES: Thing not valid");
		return;
	}
	worship::dispenser::SetMagicProperties(thing, static_cast<MagicType>(magic), seconds);
}

bool script::SetDispenserActive(entt::entity object, bool active)
{
	if (!worship::dispenser::IsDispenser(object))
	{
		return false;
	}
	worship::dispenser::SetActive(object, active);
	return true;
}

bool script::SetDispenserTimerTime(entt::entity object, float seconds)
{
	if (!worship::dispenser::IsDispenser(object))
	{
		return false;
	}
	worship::dispenser::SetTimerTime(object, seconds);
	return true;
}

entt::entity script::CreateOneShotSpell(uint32_t seed, const glm::vec3& position)
{
	return one_off::Create(position, static_cast<SpellSeedType>(seed), -1, 1.0f);
}

entt::entity script::CreateOneShotSpellInHand(uint32_t seed)
{
	// the local player's hand (inferred: the local interface is the first human player; the original reads its
	// interface)
	for (int p = 0; p < static_cast<int>(PlayerNames::_COUNT); ++p)
	{
		if (players::IsHuman(static_cast<PlayerNames>(p)))
		{
			return one_off::CreateSpellIntoHand(static_cast<PlayerNames>(p), static_cast<SpellSeedType>(seed), -1, 1.0f);
		}
	}
	return entt::null;
}

entt::entity script::CreateSpellDispenser(uint32_t abodeInfo, const glm::vec3& position, float yAngle, float scale)
{
	return worship::dispenser::Create(position, static_cast<AbodeInfo>(abodeInfo), -1, yAngle, scale);
}
