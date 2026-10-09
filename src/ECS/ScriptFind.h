/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <functional>
#include <optional>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>

#include "3D/MapCoords.h"
#include "ScriptHeaders/ScriptEnums.h"

namespace openblack::ecs
{
class Registry;
}

/// How the scripts' "get ... at" and "get ... at ... radius" find a thing of a script type and subtype about a place
namespace openblack::ecs::script_find
{

/// A subtype that matches any thing of the type
inline constexpr uint32_t k_AnySubtype = 5000;
/// How far from the place a plain "get ... at" looks, in metres
inline constexpr float k_AtReach = 1.0f;
/// How far from the place a "get town at" looks, in metres
inline constexpr float k_TownAtReach = 10.0f;

/// What a thing is to the scripts: its type, and its subtype, the row of its kind in its type's table. A thing whose
/// subtype the game can't tell (a dead tree, the football, the temple) has none, and only matches any subtype
struct Kind
{
	script::ObjectType type {script::ObjectType::None};
	std::optional<uint32_t> subtype;
};

/// Whether a thing of a kind is one asked for: the same type, and any subtype or its own
[[nodiscard]] bool Matches(const Kind& kind, script::ObjectType type, uint32_t subtype);

/// What a thing in the world is to the scripts; none for what they don't see as a thing
[[nodiscard]] std::optional<Kind> KindOf(const Registry& registry, entt::entity entity);

/// The cells a search about a place looks through: from the cell of the place less the reach to the cell of the place
/// and the reach, along x and along z
struct CellRange
{
	glm::ivec2 low {0};
	glm::ivec2 high {-1};
};
[[nodiscard]] CellRange CellsAround(const map_coords::MapCoords& from, float reach);

/// A thing in a cell that is one asked for, and where it is measured from
struct Candidate
{
	entt::entity entity {entt::null};
	map_coords::MapCoords at;
};

/// The things in a cell that are ones asked for, in the order the cell keeps them
using CellCandidates = std::function<std::vector<Candidate>(glm::ivec2 cell)>;

/// The nearest of the things asked for in the cells about a place: the cells x by x, and in each x by z, a thing
/// counting only when strictly nearer than the nearest so far, so the first met wins a tie. Cells off the map are
/// passed over. A thing farther than the reach still counts when its cell is looked through
[[nodiscard]] entt::entity FindNearest(const map_coords::MapCoords& from, float reach, const CellCandidates& inCell);

/// The nearest town strictly within the reach, the towns taken in the order given (each player's, then the neutral
/// ones), the first met winning a tie
[[nodiscard]] entt::entity FindNearestTown(const map_coords::MapCoords& from, float reach, const std::vector<Candidate>& towns);

} // namespace openblack::ecs::script_find
