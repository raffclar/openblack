/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TownMiracles.h"

#include <cstring>

#include <array>

#include <fmt/format.h>

#include "ECS/Components/SpellIcon.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/Registry.h"
#include "ECS/Systems/SpellIconSystemInterface.h"
#include "ECS/Systems/WorshipSiteSystemInterface.h"
#include "ECS/WorshipSites.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/MagicTables.h"

using namespace openblack;
using namespace openblack::ecs::components;
namespace tool = openblack::town_miracle_tool;

namespace
{
[[nodiscard]] uint32_t PopulationOf(const ecs::Registry& registry, entt::entity town)
{
	auto population = static_cast<uint32_t>(registry.Get<const Town>(town).homelessVillagers.size());
	registry.Each<const Villager>([town, &population](const Villager& villager) {
		if (villager.town == town)
		{
			++population;
		}
	});
	return population;
}

/// The plain miracle of a magic type's seed, and the magic type of each power-up level of that seed
struct SeedTypes
{
	MagicType plain {MagicType::None};
	std::array<MagicType, 3> levels {MagicType::None, MagicType::None, MagicType::None};
};

[[nodiscard]] SeedTypes SeedTypesOf(MagicType type)
{
	SeedTypes types;
	if (!Locator::infoConstants::has_value())
	{
		return types;
	}
	const auto& info = Locator::infoConstants::value();
	const auto seed = magic::FindFirstSpellSeedForMagicType(info, type);
	if (!seed.has_value())
	{
		return types;
	}
	const auto& seedInfo = magic::GetSpellSeedInfo(info, *seed);
	types.plain = magic::GetMagicTypeFromPowerUpLevel(seedInfo, magic::k_BasePowerUpLevel);
	for (size_t level = 0; level < types.levels.size(); ++level)
	{
		types.levels.at(level) = magic::GetMagicTypeFromPowerUpLevel(seedInfo, static_cast<int>(level));
	}
	return types;
}

[[nodiscard]] tool::SiteChecks ChecksFor(const ecs::Registry& registry, entt::entity town, bool anyLand)
{
	const auto& component = registry.Get<const Town>(town);
	tool::SiteChecks checks {
	    .hasSite = registry.Valid(component.worshipSite),
	    .neutral = component.owner == PlayerNames::NEUTRAL,
	    .landWithoutSites = registry.Context().mapScriptGlobals.landNumber == ecs::worship_site::k_LandWithoutSites,
	    .anyLand = anyLand,
	    .population = PopulationOf(registry, town),
	};
	if (!checks.neutral && Locator::worshipSiteSystem::has_value())
	{
		const auto temple = Locator::worshipSiteSystem::value().TempleOf(component.owner);
		checks.hasTemple = temple != entt::null;
		const auto* worship = checks.hasTemple ? registry.TryGet<const CitadelWorship>(temple) : nullptr;
		checks.templeBuilt = worship != nullptr && worship->standing;
	}
	return checks;
}
} // namespace

namespace openblack::debug::town_miracles
{

Outcome Apply(const Request& request)
{
	Outcome outcome;
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(request.town) || !registry.AllOf<Town>(request.town))
	{
		outcome.error = "that is not a town";
		return outcome;
	}
	if (request.type == MagicType::None || static_cast<size_t>(request.type) >= magic::k_MagicTypeCount)
	{
		outcome.error = "that is not a magic type";
		return outcome;
	}
	if (!Locator::spellIconSystem::has_value())
	{
		outcome.error = "the game has no town miracles";
		return outcome;
	}
	auto& icons = Locator::spellIconSystem::value();
	const auto types = SeedTypesOf(request.type);
	if (request.give)
	{
		// The same steps as a land file line, or as the script command when it is only the one magic type
		for (const auto type : tool::TypesToGive(request.type, types.plain, request.withPlain))
		{
			if (icons.TownHolds(request.town, type))
			{
				outcome.changed.push_back(type);
			}
		}
	}
	else
	{
		// The same steps as the script command, or as a creature that steals every level of the miracle
		for (const auto type : tool::TypesToTake(request.type, types.plain, types.levels, request.allLevels))
		{
			if (icons.TownReleases(request.town, type))
			{
				outcome.changed.push_back(type);
			}
		}
	}

	const auto siteBefore = registry.Get<const Town>(request.town).worshipSite;
	outcome.site = siteBefore;
	if (request.give && request.openSite && !registry.Valid(siteBefore) && Locator::worshipSiteSystem::has_value())
	{
		outcome.site = Locator::worshipSiteSystem::value().DebugAllowSite(request.town, request.anyLand);
		outcome.siteIsNew = registry.Valid(outcome.site);
		outcome.noSite = tool::WhyNoSite(ChecksFor(registry, request.town, request.anyLand));
	}
	return outcome;
}

std::vector<MagicType> HeldBy(entt::entity town)
{
	std::vector<MagicType> held;
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* magicHeld = registry.Valid(town) ? registry.TryGet<const TownMagic>(town) : nullptr;
	if (magicHeld == nullptr)
	{
		return held;
	}
	for (size_t i = 0; i < magicHeld->held.size(); ++i)
	{
		if (magicHeld->held.at(i))
		{
			held.push_back(static_cast<MagicType>(i));
		}
	}
	return held;
}

std::string MagicName(MagicType type)
{
	if (!Locator::infoConstants::has_value() || static_cast<size_t>(type) >= magic::k_MagicTypeCount)
	{
		return fmt::format("{}", static_cast<int>(type));
	}
	const auto& name = magic::GetMagicEffectInfo(Locator::infoConstants::value(), type).debugString;
	return {name.data(), strnlen(name.data(), name.size())};
}

} // namespace openblack::debug::town_miracles
