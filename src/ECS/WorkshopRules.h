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
#include <optional>
#include <span>

#include <entt/entity/entity.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "3D/MapCoords.h"
#include "ECS/Components/Workshop.h"
#include "Enums.h"

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

/// The wood that a scaffold gives to a storage pit: the wood of one scaffold for each scaffold that it is worth
[[nodiscard]] uint32_t WoodOfScaffold(uint32_t value, uint32_t scaffoldWood);

/// The worth that stays in the hand when the hand puts a scaffold down gently on a building that needs fewer scaffolds
/// than the scaffold is worth. None when the building needs all of the worth, or more.
[[nodiscard]] std::optional<uint32_t> RestInHand(uint32_t value, uint32_t required);

/// What the game knows of a scaffold when the hand taps it
struct TapFacts
{
	/// The number of scaffolds that the scaffold is worth
	uint32_t value {0};
	/// The scaffold is in the physics
	bool inPhysics {false};
	/// The scaffold is on a building site
	bool onSite {false};
	/// Its workshop still keeps it (see KeepsScaffold), thus the hand can take it from its site
	bool kept {false};
	/// A script gave the scaffold its building
	bool scriptBuilding {false};
};

/// Whether a tap breaks one scaffold off a scaffold: the scaffold is worth more than one, it is not in the physics, a
/// script did not give it its building, and it is on no building site or still in the time to take it from the site
[[nodiscard]] bool CanTapToBreak(const TapFacts& facts);

/// The angle about the up axis along which a tap moves the two parts of a scaffold apart: a random quarter turn, 0 to 3,
/// added to the angle of the scaffold
[[nodiscard]] float BreakAngle(uint32_t quarterTurns, float yAngle);

/// The scale of a scaffold that a script makes worth a size: the size cut to a whole number towards nought, then taken
/// as a number of scaffolds. A negative size wraps round to a very large number of scaffolds, as the game takes it.
[[nodiscard]] float ScaleOfScriptSize(float size);
/// The abode number of the building that a script gives a scaffold: none for -1, which is no building
[[nodiscard]] std::optional<AbodeNumber> ScriptBuildingOf(int32_t number);

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

/// The wood that a workshop requires, as its flag shows it: the wood for the scaffolds that it has room for, less the
/// wood that it holds. A workshop with no room requires no wood. The count is unsigned, thus a workshop that holds more
/// wood than its room needs gives a very large number, as the game shows it.
[[nodiscard]] uint32_t WoodRequired(int32_t room, uint32_t wood, uint32_t scaffoldWood);

/// Whether a thing is the wood pile of a workshop. Such a pile is part of its workshop, as a pile of a storage pit is.
[[nodiscard]] bool IsWoodPile(const Registry& registry, entt::entity pile);

/// What the game knows of a held scaffold and of a scaffold under the hand, when the hand can join the two
struct JoinFacts
{
	/// The number of scaffolds that each scaffold is worth
	uint32_t heldValue {0};
	uint32_t targetValue {0};
	/// The most scaffolds that a joined scaffold can be worth, from the scaffolds' table row
	uint16_t mostForCombining {0};
	/// The hidden football feature is on: then a joined scaffold can be worth one more
	bool football {false};
	/// A script gave the scaffold its building
	bool heldScriptBuilding {false};
	bool targetScriptBuilding {false};
	/// The scaffold under the hand is in the physics
	bool targetInPhysics {false};
	/// The scaffold under the hand is on a building site, and the share of the building that is built
	bool targetOnSite {false};
	float targetSiteBuilt {0.0f};
};

/// Whether the hand can join a held scaffold to a scaffold under it. The two scaffolds together must be worth less than
/// the most for a joined scaffold (or as much, with the football feature). No script gave either scaffold its
/// building. The scaffold under the hand is not in the physics, and its building site has no part built.
[[nodiscard]] bool CanJoin(const JoinFacts& facts);

/// The scaffold whose town a joined scaffold builds for
enum class JoinedTown : uint8_t
{
	Held,
	Target,
};

/// What the game knows of the towns of two scaffolds that the hand joins
struct JoinedTownFacts
{
	/// The scaffold has a town
	bool heldHasTown {false};
	bool targetHasTown {false};
	uint32_t heldValue {0};
	uint32_t targetValue {0};
	/// The player whose hand holds the held scaffold has the town of the held scaffold
	bool holderOwnsHeldTown {false};
};

/// The town of a joined scaffold. When only one scaffold has a town, that town. Else the town of the scaffold that is
/// worth more. When the two are worth the same, the town of the held scaffold only when the player who holds it has
/// that town.
[[nodiscard]] JoinedTown TownOfJoined(const JoinedTownFacts& facts);

/// The id that the game gives to each town that a scaffold founds
inline constexpr uint32_t k_FoundedTownId = 703058;

/// The id of a new town that a scaffold founds: the id of the game, or the first free id after it. The game gives all
/// these towns the same id. openblack finds the buildings of a town by its id, thus each town must have its own id.
[[nodiscard]] uint32_t FoundedTownId(std::span<const uint32_t> usedIds);

/// The cells that a scaffold which clears its ground looks at, in a spiral from its own cell
inline constexpr int32_t k_ClearingCells = 36;

/// A thing is in the reach of a scaffold that clears its ground when its distance, less its own 2D radius, is not more
/// than the reach
[[nodiscard]] bool InClearingReach(float distance, float thingRadius, float reach);

/// How a scaffold that clears its ground pushes a thing out of its way
struct ClearingPush
{
	/// The speed of the thing at the start of its flight
	glm::vec3 velocity {0.0f};
	/// Where the thing goes from the middle of the scaffold, in metres
	glm::vec2 offset {0.0f};
};

/// The push of a thing from the middle of the scaffold `centre`, by the edge point `edge` towards the thing (both in
/// metres). The thing goes twice as far as the edge point. Its speed divides each part by the larger part, with the
/// sign. Thus a thing at low x and low z can get a very high speed, as in the game. When the larger part is 0, the
/// divide uses the larger size, so a thing straight to the west or the south flies straight away.
[[nodiscard]] ClearingPush PushOutOfWay(glm::vec2 centre, glm::vec2 edge);

/// The height that a pushed thing starts at above the height of the scaffold, in metres
inline constexpr float k_PushStartRise = 0.5f;

/// The new place on the map of a thing that a scaffold which clears its ground pushes, from the middle of the scaffold
/// `centre` in metres
[[nodiscard]] map_coords::MapCoords PushStart(glm::vec2 centre, const ClearingPush& push);

/// The world height at which a pushed thing starts: the height of the scaffold above the land `scaffoldAltitude`, plus
/// half a metre. This is not a height above the land, so the thing starts under the land where the land is higher, as
/// in the game.
[[nodiscard]] float PushStartHeight(float scaffoldAltitude);

/// What a scaffold which clears its ground does to a thing in its reach
enum class ClearingAction
{
	/// The thing flies out of the way
	Push,
	/// The thing goes from the game
	Remove,
	/// Nothing happens to the thing
	LeaveAlone,
};

/// The facts about a thing in the reach of a scaffold which clears its ground
struct ClearingFacts
{
	/// The thing is a creature
	bool creature {false};
	/// A hand holds the thing, or a script holds it still
	bool held {false};
	/// The thing can become a physics object
	bool canFly {false};
};

/// What the scaffold does to the thing. It pushes a thing that can fly and that nothing holds, and it removes the other
/// things. It does not touch a creature (openblack's fix: the game removes the creature from the game).
[[nodiscard]] ClearingAction ClearingActionFor(const ClearingFacts& facts);

} // namespace openblack::ecs::workshop
