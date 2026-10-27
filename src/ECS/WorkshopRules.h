/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>
#include <cstdint>

#include <array>

#include <entt/entity/entity.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include "3D/MapCoords.h"
#include "ECS/Components/Workshop.h"

namespace openblack::ecs
{
class Registry;
} // namespace openblack::ecs

/// The rules of a workshop and its scaffolds: how a workshop makes a scaffold from its wood, how it keeps its scaffolds
/// in its yard, and what a scaffold is worth
namespace openblack::ecs::workshop
{

/// The special points of the workshop's model that mark the three places of its yard, in the order of the places
inline constexpr std::array<size_t, 3> k_PlacePoints {6, 7, 8};
/// The special point of the workshop's model where its wood pile stands
inline constexpr size_t k_WoodPilePoint = 4;
/// A scaffold that comes to rest nearer than this to a place of the yard goes into the yard, in metres
inline constexpr float k_SnapReach = 2.0f;

/// The place on the land of a special point of a building's model, and how the point is turned there
struct SpecialPlace
{
	/// The x and z of the point, at no altitude: the point stands on the land
	map_coords::MapCoords coords;
	/// The angles about x, y and z
	glm::vec3 angles {0.0f};
};

/// The matrix of a building: the game's turn of its angles with its scale in it, and its place
[[nodiscard]] glm::mat4 BuildingMatrix(const glm::vec3& angles, bool canLean, float scale, const glm::vec3& position);

/// The place on the land of a special point of a building's model. The point's matrix moves through the building's
/// matrix. The angles come from the moved matrix, as the game finds the angles of a matrix.
[[nodiscard]] SpecialPlace SpecialPlaceOf(const glm::mat4& point, const glm::mat4& building);

/// The world point of a special point of a building's model, with its height. The point's matrix moves through the
/// building's matrix, as for SpecialPlaceOf.
[[nodiscard]] glm::vec3 SpecialPointOf(const glm::mat4& point, const glm::mat4& building);

/// The scale of a scaffold that is worth a number of scaffolds
[[nodiscard]] float ScaleOfValue(uint32_t value);
/// The number of scaffolds that a scaffold of a scale is worth: the scale less a half, times five, rounded to the nearest
/// whole number
[[nodiscard]] uint32_t ValueOfScale(float scale);

/// The game turns that a workshop takes to make a scaffold, from the time in its row of the buildings' table. The game
/// cuts the fraction off.
[[nodiscard]] int32_t MakingTurns(float timeToMake);

/// What a workshop does in a game turn
enum class TurnEvent : uint8_t
{
	/// It continues as before
	None,
	/// It starts a new scaffold: it uses the wood of one scaffold and works for the turns that a scaffold takes
	Starts,
	/// It finishes the scaffold that it made
	Finishes,
};

/// What a workshop knows in a game turn
struct TurnFacts
{
	/// The workshop is built and works
	bool functional {false};
	/// Its room for more scaffolds (see disciple_work::WorkshopRoom)
	int32_t room {0};
	/// The wood that it holds, and the wood that a scaffold uses
	uint32_t wood {0};
	uint32_t scaffoldWood {0};
};

/// The game turn of a workshop. A workshop that works and makes no scaffold starts one when it has room and the wood for
/// one. A workshop that makes a scaffold counts its turns down, and finishes the scaffold at 0. The count does not change
/// while the workshop does not work.
[[nodiscard]] TurnEvent NextEvent(int32_t& makingTurns, const TurnFacts& facts);

/// The first free place of the yard. When no place is free, the first place.
[[nodiscard]] uint8_t FirstFreePlace(const std::array<components::Workshop::Place, 3>& places);

/// Whether a workshop still counts a scaffold as its own. The workshop lets the scaffold go when the scaffold has lain
/// for the given turns after a hand put it down, unless a script gave the scaffold its building.
[[nodiscard]] bool KeepsScaffold(uint32_t putDownTurn, uint32_t turn, uint32_t keepTurns, bool scriptBuilding);

/// The workshop counts a new scaffold as its own, the newest first
void AddScaffold(components::Workshop& workshop, entt::entity scaffold);
/// The workshop stops to count a scaffold, and frees its place. It returns true when the workshop had the scaffold and
/// now counts two, so that its yard is not full now. A workshop that does not have the scaffold does not change.
bool RemoveScaffold(components::Workshop& workshop, entt::entity scaffold, uint8_t place);

/// Whether a thing is a scaffold that stands in its place of its workshop's yard. Such a scaffold does not fly.
[[nodiscard]] bool StandsInYard(const Registry& registry, entt::entity scaffold);

/// How much the flag of a workshop shows that the workshop wants wood, 0 to 1: one less the share of the wood of a
/// scaffold that the workshop holds. A small amount added to the two counts keeps the division safe.
[[nodiscard]] float WoodWant(uint32_t wood, uint32_t scaffoldWood);

/// Whether a thing is the wood pile of a workshop. Such a pile is part of its workshop, as a pile of a storage pit is.
[[nodiscard]] bool IsWoodPile(const Registry& registry, entt::entity pile);

} // namespace openblack::ecs::workshop
