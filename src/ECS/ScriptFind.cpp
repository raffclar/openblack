/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ScriptFind.h"

#include <cmath>

#include <limits>

#include "Common/GUtilsDistance.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/AnimatedStatic.h"
#include "ECS/Components/Ball.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/DeadTree.h"
#include "ECS/Components/Feature.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/Flowers.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/OneOffSpellSeed.h"
#include "ECS/Components/Pot.h"
#include "ECS/Components/Reward.h"
#include "ECS/Components/SpellDispenser.h"
#include "ECS/Components/StreetLantern.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Villager.h"
#include "ECS/Registry.h"
#include "InfoConstants.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;
using ScriptType = openblack::script::ObjectType;

namespace
{
/// The row of a kind in its table as a subtype; none for a kind with no row
template <typename Info>
std::optional<uint32_t> Row(Info info)
{
	const auto row = static_cast<int32_t>(info);
	if (row < 0)
	{
		return std::nullopt;
	}
	return static_cast<uint32_t>(row);
}

/// The birds are animals of their own type to the scripts: the crow, the dove, the swallow, the pigeon, the seagull,
/// the bat, the vulture and the miracles' doves and bats. The temple's doves and bats aren't known to be birds or animals
std::optional<ScriptType> AnimalType(AnimalInfo info)
{
	switch (info)
	{
	case AnimalInfo::Crow:
	case AnimalInfo::Dove:
	case AnimalInfo::Swallow:
	case AnimalInfo::Pigeon:
	case AnimalInfo::Seagull:
	case AnimalInfo::Bat:
	case AnimalInfo::Vulture:
	case AnimalInfo::SpellDove:
	case AnimalInfo::SpellBat:
		return ScriptType::Bird;
	case AnimalInfo::CitadelDove:
	case AnimalInfo::CitadelBat:
	case AnimalInfo::None:
	case AnimalInfo::_COUNT:
		return std::nullopt;
	default:
		return ScriptType::Animal;
	}
}

/// A 16.16 value of a search's edge: the position in metres less or plus the reach, back in 16.16, truncated. The game
/// works it at the FPU's full precision
int32_t EdgeFixed(int32_t fixed, float offset)
{
	const double metres = static_cast<double>(fixed) * 10.0 * (1.0 / 65536.0) + static_cast<double>(offset);
	const double value = metres * 65536.0 / 10.0;
	if (!(value > -2147483648.0 && value < 2147483648.0))
	{
		return static_cast<int32_t>(0x80000000u);
	}
	return static_cast<int32_t>(value);
}
} // namespace

bool script_find::Matches(const Kind& kind, ScriptType type, uint32_t subtype)
{
	if (kind.type != type)
	{
		return false;
	}
	if (subtype == k_AnySubtype)
	{
		return true;
	}
	return kind.subtype.has_value() && *kind.subtype == subtype;
}

std::optional<script_find::Kind> script_find::KindOf(const Registry& registry, entt::entity entity)
{
	if (!registry.Valid(entity))
	{
		return std::nullopt;
	}
	if (const auto* villager = registry.TryGet<const Villager>(entity))
	{
		return Kind {.type =
		                 villager->lifeStage == Villager::LifeStage::Child ? ScriptType::VillagerChild : ScriptType::Villager,
		             .subtype = Row(GVillagerInfo::Find(villager->tribe, villager->number))};
	}
	if (const auto* animal = registry.TryGet<const Animal>(entity))
	{
		if (const auto type = AnimalType(animal->type))
		{
			return Kind {.type = *type, .subtype = Row(animal->type)};
		}
		return std::nullopt;
	}
	if (registry.AllOf<Creature>(entity))
	{
		return Kind {.type = ScriptType::Creature};
	}
	if (registry.AllOf<Town>(entity))
	{
		return Kind {.type = ScriptType::Town};
	}
	if (registry.AllOf<Temple>(entity))
	{
		return Kind {.type = ScriptType::Citadel};
	}
	if (const auto* abode = registry.TryGet<const Abode>(entity))
	{
		return Kind {.type = ScriptType::Abode, .subtype = Row(abode->info)};
	}
	if (const auto* dispenser = registry.TryGet<const SpellDispenser>(entity))
	{
		return Kind {.type = ScriptType::SpellDispenser, .subtype = Row(dispenser->building)};
	}
	// A field is a building to the scripts; the row of its building isn't kept
	if (registry.AllOf<Field>(entity))
	{
		return Kind {.type = ScriptType::Abode};
	}
	if (const auto* feature = registry.TryGet<const Feature>(entity))
	{
		return Kind {.type = ScriptType::Feature, .subtype = Row(feature->type)};
	}
	// The animated statics and the flowers are features whose rows are in other tables: their subtype is no feature's
	if (registry.AnyOf<AnimatedStatic, Flowers>(entity))
	{
		return Kind {.type = ScriptType::Feature};
	}
	if (const auto* tree = registry.TryGet<const Tree>(entity))
	{
		return Kind {.type = ScriptType::Tree, .subtype = Row(tree->type)};
	}
	if (registry.AllOf<DeadTree>(entity))
	{
		return Kind {.type = ScriptType::DeadTree};
	}
	if (const auto* pot = registry.TryGet<const Pot>(entity))
	{
		return Kind {.type = ScriptType::Store, .subtype = Row(pot->type)};
	}
	if (const auto* still = registry.TryGet<const MobileStatic>(entity))
	{
		return Kind {.type = registry.AllOf<Rock>(entity) ? ScriptType::Rock : ScriptType::MobileStatic,
		             .subtype = Row(still->type)};
	}
	// A street lantern stands as a mobile static, of a table of its own
	if (registry.AllOf<StreetLantern>(entity))
	{
		return Kind {.type = ScriptType::MobileStatic};
	}
	if (const auto* mobile = registry.TryGet<const MobileObject>(entity))
	{
		return Kind {.type = ScriptType::MobileObject, .subtype = Row(mobile->type)};
	}
	if (registry.AllOf<OneOffSpellSeed>(entity))
	{
		return Kind {.type = ScriptType::MobileObject};
	}
	if (registry.AllOf<Ball>(entity))
	{
		return Kind {.type = ScriptType::Ball};
	}
	if (const auto* reward = registry.TryGet<const Reward>(entity))
	{
		return Kind {.type = ScriptType::Reward, .subtype = Row(reward->type)};
	}
	return std::nullopt;
}

script_find::CellRange script_find::CellsAround(const map_coords::MapCoords& from, float reach)
{
	return CellRange {
	    .low = {map_coords::SignedCellOf(EdgeFixed(from.x, -reach)), map_coords::SignedCellOf(EdgeFixed(from.z, -reach))},
	    .high = {map_coords::SignedCellOf(EdgeFixed(from.x, reach)), map_coords::SignedCellOf(EdgeFixed(from.z, reach))}};
}

entt::entity script_find::FindNearest(const map_coords::MapCoords& from, float reach, const CellCandidates& inCell)
{
	const auto range = CellsAround(from, reach);
	entt::entity nearest = entt::null;
	float best = std::numeric_limits<float>::max();
	for (int x = range.low.x; x <= range.high.x; ++x)
	{
		for (int z = range.low.y; z <= range.high.y; ++z)
		{
			const glm::ivec2 cell {x, z};
			if (!map_coords::InBounds(cell))
			{
				continue;
			}
			for (const auto& candidate : inCell(cell))
			{
				const float distance = gutils::GetDistanceInMetres(from, candidate.at);
				if (distance < best)
				{
					best = distance;
					nearest = candidate.entity;
				}
			}
		}
	}
	return nearest;
}

entt::entity script_find::FindNearestTown(const map_coords::MapCoords& from, float reach, const std::vector<Candidate>& towns)
{
	entt::entity nearest = entt::null;
	float best = reach;
	for (const auto& town : towns)
	{
		const float distance = gutils::GetDistanceInMetres(from, town.at);
		if (distance < best)
		{
			best = distance;
			nearest = town.entity;
		}
	}
	return nearest;
}
