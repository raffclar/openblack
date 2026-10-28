/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "GameEndConditions.h"

#include "Common/X87.h"

namespace openblack::game_end
{
using ecs::components::GameStanding;

namespace
{
/// The town has a storage pit, a town centre and a house
struct TownParts
{
	bool storagePit {false};
	bool centre {false};
	bool house {false};

	[[nodiscard]] bool All() const { return storagePit && centre && house; }

	void Add(AbodeType type)
	{
		// Only these exact kinds count: a windmill is not a house
		if (type == AbodeType::StoragePit)
		{
			storagePit = true;
		}
		else if (type == AbodeType::TownCentre)
		{
			centre = true;
		}
		else if (type == AbodeType::LivingQuarters)
		{
			house = true;
		}
	}
};

/// The belief of a town is multiplied by this before the game makes it a whole number
constexpr float k_BeliefScale = 1000.0f;
} // namespace

void AddToCondition(GameStanding& standing, Condition condition, int32_t amount)
{
	if (auto& each = standing.conditions.at(static_cast<size_t>(condition)); each.has_value())
	{
		Add(*each, amount);
	}
}

void SetCondition(GameStanding& standing, Condition condition, int32_t value)
{
	if (auto& each = standing.conditions.at(static_cast<size_t>(condition)); each.has_value())
	{
		Set(*each, value);
	}
}

bool HasCondition(const GameStanding& standing, Condition condition)
{
	return standing.conditions.at(static_cast<size_t>(condition)).has_value();
}

bool CountsAsKilled(DeathReason reason)
{
	switch (reason)
	{
	case DeathReason::None:
	case DeathReason::Starving:
	case DeathReason::Animal:
	case DeathReason::Chant:
	case DeathReason::Exhaustion:
	case DeathReason::OldAge:
		return false;
	default:
		return true;
	}
}

std::optional<Condition> SmashedCondition(const BuildingHarm& harm)
{
	if (harm.lifeAfter >= 1.0f)
	{
		return std::nullopt;
	}
	const bool stopsWorking = harm.lifeBefore > harm.stopsWorkingAt && harm.lifeAfter <= harm.stopsWorkingAt;
	const bool counts = stopsWorking ? harm.byPlayer && harm.age >= k_SettledAge : harm.lifeBefore >= 1.0f;
	if (!counts)
	{
		return std::nullopt;
	}
	return harm.byCreature ? Condition::BuildingsSmashedByCreature : Condition::BuildingsSmashedByPlayer;
}

bool CompletesNewTown(std::span<const TownBuilding> others, AbodeType starts)
{
	TownParts parts;
	for (const auto& other : others)
	{
		if (other.built)
		{
			parts.Add(other.type);
		}
	}
	if (parts.All())
	{
		return false;
	}
	parts.Add(starts);
	return parts.All();
}

int32_t AmountInStores(std::span<const uint32_t> piles)
{
	int32_t total = 0;
	for (const auto pile : piles)
	{
		total += static_cast<int32_t>(pile);
	}
	return total;
}

int32_t PrayerPower(std::span<const float> sites)
{
	float total = 0.0f;
	for (const auto site : sites)
	{
		total = x87::Add(site, total);
	}
	return static_cast<int32_t>(total);
}

int32_t BeliefInWorld(std::span<const float> towns)
{
	float total = 0.0f;
	for (const auto belief : towns)
	{
		const auto whole = static_cast<int32_t>(x87::Mul(belief, k_BeliefScale));
		total = x87::Add(static_cast<double>(whole), total);
	}
	return static_cast<int32_t>(total);
}

} // namespace openblack::game_end
