/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CastRules.h"

#include <cmath>

#include <algorithm>
#include <vector>

#include <glm/geometric.hpp>
#include <spdlog/spdlog.h>

#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "Common/GUtilsDistance.h"
#include "Core/Spell.h"
#include "ECS/AnimalAI.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/Spell.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Effects/EffectValues.h"
#include "ECS/Influence/Influence.h"
#include "ECS/Map.h"
#include "ECS/MapCells.h"
#include "ECS/Registry.h"
#include "ECS/Systems/Implementations/HandSystemDetail.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/Objects/MagicTeleport.h"
#include "MagicTables.h"
#include "Particles/PSysManager.h"
#include "Spells/SpellForest.h"

using namespace openblack;
using namespace openblack::magic;

namespace
{
/// A living thing can be healed while it is not dead
bool CanBeHealedByHealSpell(entt::entity object)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (registry.AllOf<ecs::components::Villager>(object))
	{
		return true; // (inferred: dead villagers taken as gone from openblack's world; no IsDead test)
	}
	const auto* animal = registry.TryGet<const ecs::components::Animal>(object);
	if (animal == nullptr)
	{
		return false;
	}
	// no bird can be healed, not just the spell dove. The Vulture is a bird too but IsFlyingSpecies leaves it out, so
	// it is named here. The citadel dove and bat: their kind is not identified and openblack never makes them
	// (inferred: taken as healable). (inferred: no IsDead test for animals)
	return !ecs::animal_ai::IsFlyingSpecies(animal->type) && animal->type != AnimalInfo::Vulture;
}

/// The map's cells per side, 512 in the original (map_coords::k_MapCells)
/// (inferred: 512 when there is no terrain, an openblack fallback)
uint16_t MapSide()
{
	return Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetCellsPerSide()
	                                           : static_cast<uint16_t>(map_coords::k_MapCells);
}
} // namespace

bool cast_rules::InBounds(const glm::vec3& position)
{
	// the MapCoords' high words (the 10 m cells) as unsigned: a negative coordinate is out. The cast position is
	// (int)(x * 6553.6), truncated
	return map_coords::InBounds(position, MapSide());
}

bool cast_rules::IsLand(const glm::vec3& position)
{
	return ecs::systems::hand_detail::IsLand(position);
}

bool cast_rules::CanCastRule(const GMagicInfo& info, const glm::vec3& position, PlayerNames player)
{
	if (!InBounds(position))
	{
		return false;
	}
	const auto inInfluence = [&]() {
		return influence::CalculatePlayerInfluence(player, magic::ToWorld(position), influence::CalcType::Default, true) > 0.0f;
	};
	switch (info.castRuleType)
	{
	case CastRuleType::Anywhere:
		return true;
	case CastRuleType::OnLand:
		return IsLand(position);
	case CastRuleType::InInfluence:
		return inInfluence();
	case CastRuleType::OnLandInInfluence:
		return IsLand(position) && inInfluence();
	default:
		return true; // an unknown rule passes
	}
}

bool cast_rules::CanCastAt(MagicType type, const glm::vec3& position)
{
	switch (ClassOf(type))
	{
	case SpellClass::Heal:
		// each heal type checks with its own row (FindTargets of the type's GMagicHealInfo)
		return (type == MagicType::Heal || type == MagicType::HealPowerUpOne) &&
		       FindHealTargets(position, entt::null, type) > 0;
	case SpellClass::Resource:
		return IsLand(position);
	case SpellClass::Creature:
		return false;
	case SpellClass::Forest:
		return spell_forest::CanCastAt(position); // Spells/SpellForest.cpp
	case SpellClass::Teleport:
		// no MultiMapFixed (another stone, a building...) within 6 m, then the base check passes
		return !teleport::AnyMultiCellStaticNear(position, teleport::k_Radius);
	default:
		return true;
	}
}

bool cast_rules::CanCastOn(MagicType type, entt::entity object)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* transform = registry.TryGet<const ecs::components::Transform>(object);
	const glm::vec3 position = transform != nullptr ? ToMap(transform->position) : glm::vec3(0.0f);
	switch (ClassOf(type))
	{
	case SpellClass::Resource:
		return IsLand(position);
	case SpellClass::Forest:
	case SpellClass::Creature:
		return false; // TODO: a Creature whose mind allows it
	default:
		return CanCastAt(type, position);
	}
}

int cast_rules::FindHealTargets(const glm::vec3& position, entt::entity spell, MagicType typeWithoutSpell)
{
	const auto& tables = Locator::infoConstants::value();
	auto& registry = Locator::entitiesRegistry::value();
	const auto magicType = spell != entt::null ? registry.Get<ecs::components::Spell>(spell).magicType : typeWithoutSpell;
	const auto* info = GetMagicInfoAs<GMagicHealInfo>(tables, magicType);
	if (info == nullptr)
	{
		return 0;
	}
	float radius = info->dummyVar;
	int maximum = static_cast<int>(info->maxToHeal);
	if (spell != entt::null)
	{
		const float power = GetTribalPower(spell);
		radius *= power;
		maximum = static_cast<int>(std::lrint(static_cast<float>(maximum) * power)); // rounded
	}
	const int side = static_cast<int>(std::ceil(2.0f * radius / 10.0f));
	int cells = side * side;
	const auto values = ecs::effects::EffectValues::FromEffectInfo(GetMagicEffectInfo(tables, magicType));
	// the spiral walks the MapCoords itself (a copy of the cast position): InBounds and the cell lookup are on it, and
	// each step is added to the high words only, so the fraction stays and the 16-bit add wraps (from cell 0xFFFF, left
	// of the map, a +1 step enters cell 0)
	auto coords = map_coords::FromMetres(glm::vec2(position.x, position.z));
	map_coords::Spiral spiral; // dir = count = 1
	int healed = 0;
	int looked = 0;
	while (cells != 0 && healed < maximum)
	{
		if (map_coords::InBounds(coords, MapSide()))
		{
			const auto cell = map_coords::Cell(coords);
			// the cell's mobile list only, from its head (ecs::map_cells)
			const auto objects = ecs::map_cells::MobileInCell(glm::ivec2(cell));
			for (const auto object : objects)
			{
				if (healed >= maximum)
				{
					break;
				}
				if (!registry.Valid(object))
				{
					continue; // openblack only: the original walks the cell's list
				}
				const auto* transform = registry.TryGet<const ecs::components::Transform>(object);
				if (transform == nullptr)
				{
					continue;
				}
				++looked;
				// both tests measure from the spiral's MapCoords (the one the steps move), not from the cast position:
				// the distance of coords and the object < R, available, IsEffectReceiver, a living thing that can be
				// healed, and then dx^2 + dz^2 < R^2 with dx = coords - object in metres (x 10 x 2^-16: an exact
				// square, not the first test again). (approximate) the object's MapCoords is its float position
				const auto at = map_coords::FromMetres(glm::vec2(transform->position.x, transform->position.z));
				const float dx = map_coords::ToMetres(coords.x) - map_coords::ToMetres(at.x);
				const float dz = map_coords::ToMetres(coords.z) - map_coords::ToMetres(at.z);
				const float squared = dz * dz + dx * dx;
				if (!(gutils::GetDistanceInMetres(coords, at) < radius) || !ecs::effects::IsEffectReceiver(object, values) ||
				    !CanBeHealedByHealSpell(object) || !(radius * radius > squared))
				{
					continue;
				}
				++healed;
				if (spell != entt::null)
				{
					// the object becomes a target of the spell's PSys
					if (auto* effect = psys::manager::Find(registry.Get<ecs::components::Spell>(spell).psys); effect != nullptr)
					{
						effect->AddTarget(object);
					}
				}
			}
		}
		--cells;
		map_coords::AddCells(coords, spiral.Next()); // the next spiral step
	}
	if (TraceEnabled())
	{
		SPDLOG_LOGGER_INFO(
		    spdlog::get("game"),
		    "Spell trace: FindTargets at ({:.1f}, {:.1f}) R {:.1f} max {}: {} cells, {} mobile objects looked at, {} "
		    "to heal",
		    position.x, position.z, radius, maximum, side * side, looked, healed);
	}
	return healed;
}
