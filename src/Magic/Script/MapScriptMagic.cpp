/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "MapScriptMagic.h"

#include <spdlog/spdlog.h>

#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/Core/OneOffSpellSeed.h"
#include "Magic/Core/Spell.h"
#include "Magic/MagicTables.h"
#include "Worship/Citadel.h"
#include "Worship/FireFlyReward.h"
#include "Worship/SpellDispenser.h"
#include "Worship/TownCentreSpellIcon.h"
#include "Worship/TownMagic.h"

using namespace openblack;
using namespace openblack::magic;

namespace
{
/// The script's "x,z" as a point on the land (altitude 0)
glm::vec3 LandPoint(const glm::vec3& position)
{
	return ToWorld(glm::vec3(position.x, 0.0f, position.z));
}
} // namespace

void script::CreateOneShotSpell(const glm::vec3& position, const std::string& seed)
{
	// the seed by name (none: 30, which Create refuses)
	const int seedType = GetSpellSeedFromText(Locator::infoConstants::value(), seed).value_or(k_SpellSeedNotFound);
	if (one_off::Create(LandPoint(position), static_cast<SpellSeedType>(seedType), -1, 1.0f) == entt::null)
	{
		SPDLOG_LOGGER_WARN(spdlog::get("scripting"), "CREATE_ONE_SHOT_SPELL: no spell seed {}", seed);
	}
}

void script::CreateOneShotSpellPu(const glm::vec3& position, const std::string& magic)
{
	// GetInfoFromText, 0 < magic < 42, GetFirstSpellSeedForMagicType in 0..29, and its level
	const auto& tables = Locator::infoConstants::value();
	const auto found = GetInfoFromText(tables, magic);
	if (!found || *found <= 0 || *found >= static_cast<int>(k_MagicTypeCount))
	{
		return;
	}
	const int type = *found;
	const auto seedType = GetFirstSpellSeedForMagicType(tables, static_cast<MagicType>(type));
	const auto index = static_cast<int>(seedType);
	if (index <= -1 || index >= static_cast<int>(k_SpellSeedCount))
	{
		return;
	}
	const int powerUp = GetPowerUpFromMagicType(GetSpellSeedInfo(tables, seedType), static_cast<MagicType>(type));
	one_off::Create(LandPoint(position), seedType, powerUp, 1.0f);
}

void script::CreateTownSpell(int32_t townId, const std::string& seed)
{
	const auto& tables = Locator::infoConstants::value();
	const auto found = GetSpellSeedFromText(tables, seed);
	const auto town = worship::town::FromId(static_cast<uint32_t>(townId));
	if (town == entt::null || !found || *found < 0 || *found >= static_cast<int>(k_SpellSeedCount))
	{
		return;
	}
	const int seedType = *found;
	const auto& seedInfo = GetSpellSeedInfo(tables, static_cast<SpellSeedType>(seedType));
	worship::town::AddMagicTypesHeld(town, seedInfo.magicTypes[0]);
	if (const auto centre = worship::town::TownCentreOf(town); centre != entt::null)
	{
		worship::town_centre::AddSpell(centre, static_cast<SpellSeedType>(seedType));
	}
}

void script::CreateNewTownSpell(int32_t townId, const std::string& magic)
{
	const auto& tables = Locator::infoConstants::value();
	const auto town = worship::town::FromId(static_cast<uint32_t>(townId));
	if (town == entt::null)
	{
		return;
	}
	const auto found = GetInfoFromText(tables, magic);
	if (!found || *found >= static_cast<int>(k_MagicTypeCount) || *found <= 0)
	{
		return;
	}
	const int type = *found;
	const auto seed = GetFirstSpellSeedForMagicType(tables, static_cast<MagicType>(type));
	const auto base =
	    static_cast<int>(seed) >= 0 ? MagicTypeForPowerUpLevel(GetSpellSeedInfo(tables, seed), -1) : MagicType::None;
	worship::town::AddMagicTypesHeld(town, static_cast<MagicType>(type));
	if (!worship::town::IsMagicTypeHeld(town, base))
	{
		worship::town::AddMagicTypesHeld(town, base);
	}
}

void script::CreatePlannedSpellIcon(int32_t townId, const std::string& seed)
{
	const auto& tables = Locator::infoConstants::value();
	const auto found = GetSpellSeedFromText(tables, seed);
	const auto town = worship::town::FromId(static_cast<uint32_t>(townId));
	if (town == entt::null || !found || *found < 0 || *found >= static_cast<int>(k_SpellSeedCount))
	{
		return;
	}
	const int seedType = *found;
	worship::town::AddMagicTypesHeld(town, GetSpellSeedInfo(tables, static_cast<SpellSeedType>(seedType)).magicTypes[0]);
}

void script::CreateWorshipSite(PlayerNames player, Tribe tribe)
{
	// the player's citadel and its heart, for the tribe
	const auto citadel = worship::citadel::Of(player);
	if (citadel == entt::null || tribe == Tribe::NONE)
	{
		return;
	}
	worship::citadel::CreateBuiltWorshipSite(citadel, tribe);
}

void script::CreateSpellDispenser(int32_t townId, const glm::vec3& position, AbodeInfo abode, const std::string& magic,
                                  float yAngle, float scale, float period)
{
	// the land point, the magic by name, the town by id (none: the nearest)
	// none: 42, passed on as it is
	const int type = GetInfoFromText(Locator::infoConstants::value(), magic).value_or(k_MagicTypeNotFound);
	const auto dispenser = worship::dispenser::Create(LandPoint(position), abode, townId, yAngle, scale);
	if (dispenser == entt::null)
	{
		return;
	}
	// the magic, an orb at once, the period truncated to turns (0 deactivates it)
	worship::dispenser::SetMagicAndPeriod(dispenser, static_cast<MagicType>(type), static_cast<uint32_t>(period));
}

void script::FireFlySpellRewardProb(const std::string& magic, float probability)
{
	worship::fire_fly::SetRewardProbability(
	    static_cast<MagicType>(GetInfoFromText(Locator::infoConstants::value(), magic).value_or(k_MagicTypeNotFound)),
	    probability);
}
