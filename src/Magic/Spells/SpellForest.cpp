/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SpellForest.h"

#include <cmath>
#include <cstring>

#include <algorithm>
#include <vector>

#include <LNDFile.h>
#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>
#include <spdlog/spdlog.h>

#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "Common/GUtilsDistance.h"
#include "Common/GameRandom.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/Forest.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Effects/EffectValues.h"
#include "ECS/Fire/FireObjectTraits.h"
#include "ECS/Map.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Registry.h"
#include "ECS/Trees.h"
#include "ECS/Weather/Weather.h"
#include "ForestDebugHooks.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/CastRules.h"
#include "Magic/Core/Spell.h"
#include "Magic/Core/SpellEvent.h"
#include "Magic/MagicTables.h"
#include "Magic/Objects/MagicTree.h"
#include "SpellClasses.h"

using namespace openblack;
using namespace openblack::magic;
using namespace openblack::ecs::components;

namespace
{
const GMagicForestInfo& ForestInfoOf(entt::entity spell)
{
	const auto type = Locator::entitiesRegistry::value().Get<const Spell>(spell).magicType;
	return *GetMagicInfoAs<GMagicForestInfo>(Locator::infoConstants::value(), type);
}

SpellForestData& DataFor(entt::entity spell)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (auto* data = registry.TryGet<SpellForestData>(spell); data != nullptr)
	{
		return *data;
	}
	return registry.Assign<SpellForestData>(spell);
}

/// The forest the spell points at, when it still exists (ECS/Trees drops an empty forest after 2000 turns)
bool HasForest(const SpellForestData& data)
{
	return data.forestId != 0 && !data.forestDeleted && ecs::IsInForest(data.forestId);
}

/// The forest's trees (ECS/Trees ForestTreeCount)
uint32_t TreeCountOf(const SpellForestData& data)
{
	return HasForest(data) ? static_cast<uint32_t>(ecs::ForestTreeCount(data.forestId)) : 0;
}

/// A magic tree of the forest went (ECS/Trees' DeleteTree told MagicTree.cpp) and the forest has no tree left -> the
/// forest is deleted (DeleteForest)
bool ForestWentWithItsLastTree(const SpellForestData& data)
{
	if (data.forestId == 0 || !magic_tree::ForestLostAMagicTree(data.forestId) || ecs::ForestTreeCount(data.forestId) != 0)
	{
		return false;
	}
	ecs::DeleteForest(data.forestId);
	return true;
}

/// TreesWanted of a spell
int TreesWanted(entt::entity spell)
{
	return spell_forest::TreesWanted(GetSpellStrength(spell), DataFor(spell).maxTrees, ForestInfoOf(spell).finalNoTrees);
}

/// The cell of a point's MapCoords (the high words)
glm::ivec2 CellOf(const glm::vec3& position)
{
	return map_coords::CellOf(position);
}

/// IsFixed: the cell's first fixed object (where the newest is put) is a multi-cell fixed object (ecs::map_cells, the
/// ordered lists). A tree is in its own cell only; this event's trees are already at the heads
/// (map_cells::InsertMapObject in CreateTree)
bool IsFixedCell(const glm::vec3& position)
{
	return ecs::map_cells::IsFixed(CellOf(position));
}

/// No object of type ABODE in the fixed list of the position's cell has Get2DRadius > its distance to the point. A
/// field is type 18 FIELD (info.dat fieldType[].type), so it is not one of them
bool NoAbodeCovers(const glm::vec3& position)
{
	const auto cell = CellOf(position);
	for (auto object = ecs::map_cells::FindType(cell, ObjectType::Abode); object != entt::null;
	     object = ecs::map_cells::FindType(cell, ObjectType::Abode, object))
	{
		const auto centre = ecs::fire::traits::FireCentre(object);
		// the distance of the point and that centre
		const float distance = gutils::GetDistanceInMetres(position, centre);
		const float radius = ecs::object::Get2DRadius(object);
		if (radius > distance)
		{
			return false;
		}
	}
	return true;
}

/// The terrain material of a cell: 27 under deep snow (snow >= 27 at the cell's point), else the land's material (the
/// cell's country, the second material of its altitude, that material's type; 0 off the map or without a block) with
/// 0 -> 1
uint32_t TerrainMaterial(glm::u16vec2 cell)
{
	const glm::vec3 corner(static_cast<float>(cell.x) * 10.0f, 0.0f, static_cast<float>(cell.y) * 10.0f);
	if (weather::GetSnowAt(ToWorld(corner)) >= 27.0f)
	{
		return 27;
	}
	uint32_t material = 0;
	if (Locator::terrainSystem::has_value() && cell.x <= 0x1FF && cell.y <= 0x1FF)
	{
		const auto& island = Locator::terrainSystem::value();
		if (cell.x < island.GetCellsPerSide() && cell.y < island.GetCellsPerSide())
		{
			const auto& landCell = island.GetCell(cell);
			lnd::LNDCell empty {};
			empty.properties.fullWater = true;
			const auto& countries = island.GetCountries();
			const auto& materials = island.GetMaterialInfo();
			if (std::memcmp(&landCell, &empty, sizeof(empty)) != 0 && landCell.properties.country < countries.size())
			{
				const auto& country = countries[landCell.properties.country];
				const auto altitude = std::min<uint16_t>(island.GetCellAltitude(landCell), 255);
				const auto index = country.materials[altitude].indices[1];
				if (index < materials.size())
				{
					material = materials[index].type;
				}
			}
		}
	}
	return material != 0 ? material : 1;
}

/// The Forest on the first tree, then a MagicTree at a random angle with its target scale
entt::entity CreateTree(entt::entity entity, const glm::vec3& position, TreeInfo type)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto& data = DataFor(entity);
	if (data.forestId == 0)
	{
		// a new forest for every cast (ECS/Trees). The creator's player is not kept here and is not needed: the
		// forest's process does not read it. The statistic the forest's new trees raise is for the most influential
		// player at the new tree, not for the forest's own player, and it is only kept in a multiplayer game.
		// (inferred) no other reader of a Forest's player was found.
		data.forestId = ecs::CreateForest(0, glm::vec3(position.x, 0.0f, position.z));
		data.forestCreated = true;
	}
	// a random angle in [0, 2 pi)
	const float angle = game_random::GameFloatRand(glm::two_pi<float>());
	const float woodMultiplier = ForestInfoOf(entity).woodValueMultiplier * GetTribalPower(entity);
	const auto tree = magic_tree::Create(position, entity, type, data.forestId, angle, 0.0f, woodMultiplier);
	if (tree != entt::null)
	{
		// the head of its cell's fixed list at once (so the next tree of this event sees it with IsFixed)
		ecs::map_cells::InsertMapObject(tree);
		const auto& castPos = registry.Get<const Spell>(entity).originalCastPos;
		// the distance of the tree's MapCoords and the cast one
		const float distance = gutils::GetDistanceInMetres(position, castPos);
		registry.Get<Tree>(tree).maxSize = spell_forest::TargetScale(distance);
	}
	return tree;
}

int InitWithPos(entt::entity spell, const glm::vec3& position, SpellCastData* castData, const psys::ProcessInfo& info)
{
	// the base InitWithPos on a cleared state, as at allocation
	DataFor(spell) = {};
	const int result = base::InitWithPos(spell, position, castData, info);
	// the requested maximum: -1 -> finalNoTrees
	auto& registry = Locator::entitiesRegistry::value();
	if (registry.Valid(spell) && registry.AllOf<Spell>(spell))
	{
		const int requested = registry.Get<const Spell>(spell).maxObjectsToCreate;
		DataFor(spell).maxTrees = requested == -1 ? static_cast<int>(ForestInfoOf(spell).finalNoTrees) : requested;
	}
	return result;
}

/// The seed has landed (type 3): the whole forest at once
int SpellEvent(entt::entity entity, const psys::SpellEventInfo& event)
{
	if (event.type == psys::SpellEventInfo::Type::Started || DataFor(entity).forestId != 0)
	{
		return 1;
	}
	// pays costPerEvent (1); the NATURE EffectValues (alignment 1) around the spell, the reaction
	if (spell_event::ApplyDefaultSpellEffect(entity, event) != 1)
	{
		return 1;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const int n = TreesWanted(entity);
	const auto castPos = registry.Get<const Spell>(entity).originalCastPos;
	int made = 0;
	for (int i = 0; i < n; ++i)
	{
		const auto point = spell_forest::ToMapCoords(glm::vec2(castPos.x, castPos.z) + spell_forest::SpiralOffset(i, n));
		const glm::vec3 position(point.x, 0.0f, point.y);
		// the forest has fewer trees than wanted
		if (!spell_forest::ValidPlaceForTree(position) || !(TreeCountOf(DataFor(entity)) < static_cast<uint32_t>(n)))
		{
			continue;
		}
		// GetRandomTreeInfo(castPos): the material under the cast point, a new GameRand(4) for each tree
		const auto tree = CreateTree(entity, position, spell_forest::RandomTreeType(castPos));
		if (tree != entt::null)
		{
			++made;
		}
	}
	forest_debug::OnLanded(entity, CurrentTurn()); // OPENBLACK_TEST_FOREST_SHOT
	if (TraceEnabled())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"),
		                   "Spell trace: spell {} SpellForest event {} at ({:.1f}, {:.1f}): {} trees wanted, {} made around "
		                   "({:.1f}, {:.1f}), material {}",
		                   static_cast<uint32_t>(entity), static_cast<int>(event.type), event.position.x, event.position.z, n,
		                   made, castPos.x, castPos.z, TerrainMaterial(CellOf(castPos)));
	}
	return 1;
}

/// CoreProcess, then the trees
int Process(entt::entity entity)
{
	base::CoreProcess(entity);
	forest_debug::OnTurn(entity, CurrentTurn()); // OPENBLACK_TEST_FOREST_SHOT
	auto& data = DataFor(entity);
	auto& registry = Locator::entitiesRegistry::value();
	// the forest was deleted: forget it and CloseDown. It is deleted with its last magic tree
	// (ForestWentWithItsLastTree) or, empty, by ECS/Trees' forest turn.
	if (data.forestId != 0 && (data.forestDeleted || ForestWentWithItsLastTree(data) || !ecs::IsInForest(data.forestId)))
	{
		data.forestId = 0;
		data.forestDeleted = false;
		magic::CloseDown(entity);
	}
	if (data.forestId == 0)
	{
		return registry.Get<const Spell>(entity).psys != 0 ? 1 : 5;
	}
	const auto& info = ForestInfoOf(entity);
	const int wanted = TreesWanted(entity);
	const uint32_t count = TreeCountOf(data);
	float change = 0.0f;
	// an unsigned comparison
	const bool decay = static_cast<uint32_t>(wanted) < count;
	if (decay)
	{
		// ECS/Trees: a tree reaching 0 is ToBeDeleted (DeleteTree)
		change = ecs::ShrinkAllTrees(data.forestId, info.decaySpeed);
		// the last tree took the forest with it: CloseDown on the next turn, as the original
		data.forestDeleted = ForestWentWithItsLastTree(data);
	}
	else
	{
		change = ecs::GrowAllTrees(data.forestId, info.growSpeed);
	}
	if (TraceEnabled())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Spell trace: spell {} SpellForest trees {} wanted {}: {} {:.3f}",
		                   static_cast<uint32_t>(entity), count, wanted, decay ? "decay" : "grow", change);
	}
	return 1;
}

float CalculateCostToMaintain(entt::entity entity)
{
	const auto& effect = EffectInfoOf(entity);
	return spell_forest::CostToMaintain(base::CalculateCostToMaintain(entity), effect.costPerEvent,
	                                    TreeCountOf(DataFor(entity)));
}

/// (not named CloseDown: inside magic::RegisterForestSpell that name is magic::CloseDown, the dispatch)
void ForestCloseDown(entt::entity entity)
{
	// the base CloseDown, creator = NULL
	base::CloseDown(entity);
	Locator::entitiesRegistry::value().Get<Spell>(entity).creator = {};
}

void ToBeDeleted(entt::entity entity)
{
	// the forest (if not already going) goes with its trees (ECS/Trees' DeleteForest, each tree's ToBeDeleted), then
	// the spell's ToBeDeleted
	auto& data = DataFor(entity);
	if (HasForest(data))
	{
		ecs::DeleteForest(data.forestId);
	}
	data.forestId = 0;
}

bool HasEnoughChantsAndLifeForRecast(entt::entity spell)
{
	return spell_forest::GetMaxObjectsToCreate(spell) > 0;
}
} // namespace

int spell_forest::TreesWanted(float strength, int maxTrees, uint32_t finalNoTrees)
{
	const float alive = strength > 0.0f ? 1.0f : 0.0f;
	const int trees = maxTrees == -1 ? static_cast<int>(finalNoTrees) : maxTrees;
	return static_cast<int>(std::nearbyint(static_cast<float>(trees) * alive));
}

glm::vec2 spell_forest::SpiralOffset(int i, int n)
{
	const float step = n > 1 ? 1.0f / (static_cast<float>(n) - 1.0f) : 1.0f;
	// n x turns per tree x 2 pi, rounded to a float
	const float turns = static_cast<float>(static_cast<double>(n) * k_TurnsPerTree * static_cast<double>(glm::two_pi<float>()));
	const float f = static_cast<float>(i) * step; // rounded to a float
	const double g = 1.0 - static_cast<double>(f);
	const double r = k_InnerRadius + (k_ForestRadius - k_InnerRadius) * std::sqrt(1.0 - g * g);
	const double angle = static_cast<double>(f) * static_cast<double>(turns);
	return {static_cast<float>(r * std::cos(angle)), static_cast<float>(r * std::sin(angle))};
}

glm::vec2 spell_forest::ToMapCoords(glm::vec2 point)
{
	// x 6553.6 truncated to fixed point (map_coords::ToFixed), then x 10 / 65536 back to metres (map_coords::ToMetres)
	return map_coords::ToMetres(map_coords::FromMetres(point));
}

float spell_forest::TargetScale(float distance)
{
	return 1.0f - distance * 0.5f / k_ForestRadius;
}

int spell_forest::MaxObjectsToCreate(int maxTrees, uint32_t finalNoTrees, bool hasForest, uint32_t trees, bool created)
{
	int count = static_cast<int>(finalNoTrees);
	if (hasForest)
	{
		count = static_cast<int>(trees);
	}
	else if (created)
	{
		count = 0;
	}
	return count < maxTrees ? count : maxTrees;
}

float spell_forest::CostToMaintain(float costPerGameTurn, float costPerEvent, uint32_t trees)
{
	return costPerGameTurn + static_cast<float>(trees) * costPerEvent;
}

float spell_forest::AdjustSpellSeedAltitude(bool hasForest, float tallestTree, float altitude)
{
	if (!hasForest)
	{
		return -5.0f;
	}
	return altitude > tallestTree ? altitude : tallestTree;
}

float spell_forest::AdjustSpellSeedPos(entt::entity spell, float altitude)
{
	// no Forest -> -5; else at least the tallest tree (ECS/Trees)
	const auto& data = DataFor(spell);
	const bool hasForest = HasForest(data);
	return AdjustSpellSeedAltitude(hasForest, hasForest ? ecs::TallestTreeHeight(data.forestId) : 0.0f, altitude);
}

bool spell_forest::CanCastAt(const glm::vec3& position)
{
	return cast_rules::InBounds(position) && cast_rules::IsLand(position) && NoAbodeCovers(position) &&
	       ValidPlaceForTree(position);
}

bool spell_forest::ValidPlaceForTree(const glm::vec3& position)
{
	return cast_rules::InBounds(position) && cast_rules::IsLand(position) && !IsFixedCell(position);
}

TreeInfo spell_forest::RandomTreeType(const glm::vec3& position)
{
	const auto& materials = Locator::infoConstants::value().terrainMaterial;
	uint32_t material = TerrainMaterial(CellOf(position));
	// a material past the last one (0x2B) -> 0
	if (material > 0x2B || material >= materials.size())
	{
		material = 0;
	}
	const auto pick = game_random::GameRand(4);
	return materials[material].magicTreeTypes[pick];
}

const SpellForestData* spell_forest::DataOf(entt::entity spell)
{
	return Locator::entitiesRegistry::value().TryGet<const SpellForestData>(spell);
}

int spell_forest::GetMaxObjectsToCreate(entt::entity spell)
{
	const auto& data = DataFor(spell);
	return MaxObjectsToCreate(data.maxTrees, ForestInfoOf(spell).finalNoTrees, HasForest(data), TreeCountOf(data),
	                          data.forestCreated);
}

void openblack::magic::RegisterForestSpell()
{
	const SpellOps ops {.initWithPos = InitWithPos,
	                    .initWithObject = base::InitWithObject,
	                    .process = Process,
	                    .spellEvent = SpellEvent,
	                    .costToMaintain = CalculateCostToMaintain,
	                    .closeDown = ForestCloseDown,
	                    .toBeDeleted = ToBeDeleted,
	                    .hasEnoughChantsForRecast = HasEnoughChantsAndLifeForRecast,
	                    .particleType = base::GetParticleType,
	                    .maxObjectsToCreate = spell_forest::GetMaxObjectsToCreate};
	RegisterOps(SpellClass::Forest, ops);
}
