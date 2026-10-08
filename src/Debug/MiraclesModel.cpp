/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "MiraclesModel.h"

#include <algorithm>
#include <array>
#include <limits>

#include <fmt/format.h>
#include <glm/geometric.hpp>
#include <glm/vec2.hpp>

#include "Magic/MagicTables.h"

using namespace openblack;
using namespace openblack::debug;
using namespace openblack::debug::miracles;

namespace
{
constexpr std::array<std::string_view, 41> k_ReactionNames {
    "flee from object",
    "look at object",
    "follow object",
    "flee from spell",
    "look at spell",
    "follow spell",
    "react to creature",
    "react to food",
    "react to magic tree",
    "react to flying object",
    "react to fire",
    "react to ball",
    "react to wood",
    "react to magic shield",
    "react to gift from creature",
    "react to new building",
    "react to hand pick up",
    "react to hand using totem",
    "react to object crushed",
    "react to fight",
    "react to teleport",
    "look at nice spell",
    "react to hand putting stuff in storage pit",
    "react to death",
    "react to dropped by hand",
    "fainting",
    "confused",
    "avoid falling tree",
    "flee from predator",
    "crowd around",
    "react to breeder",
    "react to town celebration",
    "react to villager in hand",
    "react to burning object in hand",
    "react to magic water putting out fire",
    "react to magic shield struck",
    "react to magic shield destroyed",
    "react to impressive spell",
    "react to scaffold",
    "react to missionary",
    "react to fight won",
};
static_assert(k_ReactionNames.size() == static_cast<size_t>(Reaction::ReactToFightWon) + 1);
} // namespace

std::vector<SpellSeedType> miracles::CastableSeeds(const InfoConstants& info)
{
	std::vector<SpellSeedType> seeds;
	for (size_t i = 0; i < info.spellSeed.size(); ++i)
	{
		if (info.spellSeed.at(i).magicTypes[0] != MagicType::None)
		{
			seeds.push_back(static_cast<SpellSeedType>(i));
		}
	}
	return seeds;
}

std::vector<int> miracles::PowerUpLevels(const GSpellSeedInfo& seed)
{
	std::vector<int> levels;
	for (int level = k_BasePowerUpLevel; level < static_cast<int>(seed.powerUpGestures.size()); ++level)
	{
		if (magic::MagicTypeForPowerUpLevel(seed, level) != MagicType::None)
		{
			levels.push_back(level);
		}
	}
	return levels;
}

int miracles::KeepLevel(const GSpellSeedInfo& seed, int powerUpLevel)
{
	const auto levels = PowerUpLevels(seed);
	return std::ranges::find(levels, powerUpLevel) != levels.end() ? powerUpLevel : k_BasePowerUpLevel;
}

MagicType miracles::MagicTypeOf(const InfoConstants& info, Choice choice)
{
	if (choice.seed == SpellSeedType::None)
	{
		return MagicType::None;
	}
	return magic::MagicTypeForPowerUpLevel(magic::GetSpellSeedInfo(info, choice.seed), choice.powerUpLevel);
}

std::string miracles::LevelName(int powerUpLevel)
{
	if (powerUpLevel == k_BasePowerUpLevel)
	{
		return "plain";
	}
	return fmt::format("power-up {}", powerUpLevel + 1);
}

CastPlan miracles::PlanCast(const InfoConstants& info, Choice choice, float multiplier, glm::vec3 point,
                            glm::vec3 cameraForward)
{
	CastPlan plan;
	plan.type = MagicTypeOf(info, choice);
	if (plan.type == MagicType::None)
	{
		return plan;
	}
	float size = 1.0f;
	if (const auto* radius = magic::GetMagicInfoAs<GMagicRadiusSpellInfo>(info, plan.type))
	{
		size = radius->radiusForNormalCost;
	}
	plan.cast = {
	    .magnitude = choice.seed == SpellSeedType::Fire ? 1.0f : size,
	    .chants = magic::GetMagicEffectInfo(info, plan.type).initialChants * multiplier,
	    .duration = magic::GetTimerWhenPlayerCasting(info, plan.type) * multiplier,
	    .maxObjectsToCreate = -1,
	};
	plan.process = {.handPos = point + glm::vec3(0.0f, k_CastHeight, 0.0f),
	                .cameraForward = cameraForward,
	                .direction = glm::vec3(0.0f, -k_CastThrowSpeed, 0.0f)};
	return plan;
}

CastResult miracles::Cast(CasterInterface& caster, const CastPlan& plan, std::string_view name, PlayerNames player,
                          const Target& target)
{
	if (plan.type == MagicType::None)
	{
		return {.spell = entt::null, .message = "No miracle chosen"};
	}
	CastResult result;
	if (const auto* point = std::get_if<glm::vec3>(&target))
	{
		result.spell = caster.CastAtPoint(plan, player, *point);
		result.message = fmt::format("Cast {} at ({:.0f}, {:.0f})", name, point->x, point->z);
	}
	else
	{
		const auto thing = std::get<entt::entity>(target);
		result.spell = caster.CastOnObject(plan, player, thing);
		result.message = fmt::format("Cast {} on thing {}", name, static_cast<uint32_t>(thing));
	}
	if (result.spell == entt::null)
	{
		result.message = fmt::format("{} could not start", name);
	}
	return result;
}

std::vector<AbodeInfo> miracles::DispenserAbodes(const InfoConstants& info)
{
	std::vector<AbodeInfo> abodes;
	for (size_t i = 0; i < info.abode.size(); ++i)
	{
		if (info.abode.at(i).abodeType == AbodeType::SpellDispenser)
		{
			abodes.push_back(static_cast<AbodeInfo>(i));
		}
	}
	return abodes;
}

std::optional<AbodeInfo> miracles::DefaultDispenserAbode(std::span<const AbodeInfo> abodes)
{
	if (abodes.empty())
	{
		return std::nullopt;
	}
	if (std::ranges::find(abodes, AbodeInfo::NorseSpellDispenser) != abodes.end())
	{
		return AbodeInfo::NorseSpellDispenser;
	}
	return abodes.front();
}

uint32_t miracles::DefaultPeriod(const InfoConstants& info, AbodeInfo abode)
{
	const auto index = static_cast<size_t>(abode);
	if (index >= info.abode.size())
	{
		return 0;
	}
	return static_cast<uint32_t>(info.abode.at(index).timeEachMobileObjectTakesToProduce);
}

std::optional<glm::vec3> miracles::HandPoint(std::optional<glm::vec3> left, std::optional<glm::vec3> right)
{
	return right.has_value() ? right : left;
}

CastResult miracles::CreateDispenser(DispenserCreatorInterface& creator, const InfoConstants& info, const DispenserPlan& plan,
                                     std::string_view name, PlayerNames player, glm::vec3 point)
{
	const auto magic = MagicTypeOf(info, plan.choice);
	if (magic == MagicType::None)
	{
		return {.spell = entt::null, .message = "No miracle chosen"};
	}
	CastResult result;
	switch (plan.kind)
	{
	case DispenserKind::OneShot:
		result.spell = creator.CreateOneShot(plan.choice.seed, plan.choice.powerUpLevel, point);
		result.message = fmt::format("A one-shot {} at ({:.0f}, {:.0f})", name, point.x, point.z);
		break;
	case DispenserKind::OneShotInHand:
		result.spell = creator.CreateOneShotInHand(plan.choice.seed, plan.choice.powerUpLevel, player);
		result.message = fmt::format("A one-shot {} in the hand", name);
		break;
	case DispenserKind::Permanent:
		result.spell = creator.CreatePermanent(plan.abode, magic, plan.periodTurns, point);
		result.message =
		    fmt::format("A {} dispenser at ({:.0f}, {:.0f}), every {} turns", name, point.x, point.z, plan.periodTurns);
		break;
	}
	if (result.spell == entt::null)
	{
		result.message = fmt::format("The {} could not be made", name);
	}
	return result;
}

std::optional<entt::entity> miracles::NearestThing(std::span<const ThingAt> things, glm::vec3 point, float maxDistance)
{
	std::optional<entt::entity> nearest;
	float best = std::numeric_limits<float>::max();
	for (const auto& thing : things)
	{
		const float distance = glm::distance(glm::vec2(thing.position.x, thing.position.z), glm::vec2(point.x, point.z));
		if (distance <= maxDistance && distance < best)
		{
			best = distance;
			nearest = thing.entity;
		}
	}
	return nearest;
}

void InfinitePrayer::Apply(entt::entity site, PrayerCheats& cheats)
{
	if (!_on)
	{
		return;
	}
	if (std::ranges::find(_kept, site, &Kept::site) == _kept.end())
	{
		_kept.push_back({.site = site, .cheats = cheats});
	}
	cheats = {.infinite = true, .freeMaintenance = true};
}

void InfinitePrayer::TurnOff(const std::function<void(entt::entity site, PrayerCheats cheats)>& restore)
{
	for (const auto& kept : _kept)
	{
		restore(kept.site, kept.cheats);
	}
	_kept.clear();
	_on = false;
}

std::string_view miracles::ReactionName(Reaction type)
{
	const auto index = static_cast<int>(type);
	if (index < 0 || index >= static_cast<int>(k_ReactionNames.size()))
	{
		return "none";
	}
	return k_ReactionNames.at(static_cast<size_t>(index));
}

std::vector<ReactionRow> miracles::ReactionRows(std::span<const ecs::effects::reactions::Reaction> reactions, bool all,
                                                const std::function<bool(entt::entity initiator)>& isMiracle,
                                                const std::function<uint32_t(uint32_t reaction)>& followers)
{
	std::vector<ReactionRow> rows;
	for (const auto& reaction : reactions)
	{
		const bool fromMiracle = isMiracle(reaction.initiator);
		if (!all && !fromMiracle)
		{
			continue;
		}
		rows.push_back({.id = reaction.id,
		                .name = ReactionName(reaction.type),
		                .player = reaction.player,
		                .radius = reaction.radius,
		                .followers = followers(reaction.id),
		                .fromMiracle = fromMiracle});
	}
	return rows;
}

std::string miracles::SpellAge(float age, float duration)
{
	return duration >= 0.0f ? fmt::format("{:.1f}/{:.0f} s", age, duration) : fmt::format("{:.1f} s", age);
}

std::string_view miracles::SpellState(bool closedDown)
{
	return closedDown ? "closing" : "running";
}

std::string_view miracles::DispenserState(bool hasOrb, bool active)
{
	return hasOrb ? "orb ready" : (active ? "making one" : "inactive");
}

std::string_view miracles::CreatureSpellName(creature_spells::Spell spell)
{
	constexpr std::array<std::string_view, creature_spells::k_SpellCount> k_Names {
	    "Freeze", "Small", "Big",    "Weak",       "Strong", "Fat", "Thin",    "Invisible",
	    "Nice",   "Nasty", "Hungry", "Frightened", "Tired",  "Ill", "Thirsty", "Itchy",
	};
	const auto index = static_cast<size_t>(spell);
	return index < k_Names.size() ? k_Names.at(index) : std::string_view("?");
}

std::string_view miracles::CreatureSpellPhaseName(creature_spells::Phase phase)
{
	switch (phase)
	{
	case creature_spells::Phase::Off:
		return "off";
	case creature_spells::Phase::Waiting:
		return "waiting";
	case creature_spells::Phase::Starting:
		return "starting";
	case creature_spells::Phase::Holding:
		return "holding";
	case creature_spells::Phase::Finishing:
		return "finishing";
	}
	return "?";
}

std::string miracles::CreatureSpellsLine(uint32_t creature, const creature_spells::Spells& spells)
{
	std::string line = fmt::format("Creature {}:", creature);
	for (size_t i = 0; i < creature_spells::k_SpellCount; ++i)
	{
		const auto& slot = spells.slots.at(i);
		if (slot.phase != creature_spells::Phase::Off)
		{
			line += fmt::format(" {} ({}, {} turns)", CreatureSpellName(static_cast<creature_spells::Spell>(i)),
			                    CreatureSpellPhaseName(slot.phase), slot.turnsLeft);
		}
	}
	return line;
}
