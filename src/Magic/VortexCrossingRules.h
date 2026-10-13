/*******************************************************************************
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
#include <span>
#include <string>
#include <string_view>
#include <variant>

#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "3D/MapCoords.h"
#include "Enums.h"

// What a vortex that leads away from the land does with the things it takes, and how it remembers them for the next
// land. Each thing it takes is carried round and down into its middle by the effect that draws things in: drawn in faster and
// faster round the middle, lifted or lowered onto a funnel, and gone at the middle. As it is taken it is written to the
// crossing file as one line of the land scripts' language, placed relative to the vortex, so that the vortex the player arrives
// through on the next land can make it again. A pile it reaches gives up a pot of what it holds instead. Pure rules,
// tested without the game.

namespace openblack::vortex::crossing
{

/// A pile within reach gives up a pot of at most this much
inline constexpr uint32_t k_PileTake = 1000;
/// The pot is drawn at a share of its own size: 0.3 plus up to 0.3 of a random number times 0.4
inline constexpr float k_PileShareRandom = 0.3f;
[[nodiscard]] float PileShare(float random);

/// How the effect that draws things in moves them. The vortex's effect file sets none of these, so these are the values it runs
/// with.
struct AttractSettings
{
	/// How fast a thing is drawn in: the share of its distance it loses each second, once fully blended in
	float radialSpeed {1.0f};
	/// How fast it is swung round the middle
	float thetaSpeed {1.0f};
	/// Inside this distance of the middle it can be gone
	float deletionRadius {4.0f};
	/// It is never drawn nearer the middle than this
	float minRadius {3.0f};
	/// The seconds over which its own way of moving blends into the vortex's
	float blendTime {8.0f};
	/// The seconds over which its velocity blends from the one it was taken with into the vortex's
	float blendTimeVelocity {1.0f};
	/// The funnel's shape: its height above the land at a distance r from the middle is sqrt(r * yOffset^2 /
	/// rZeroHeight) + yOffset, so it is at the land's height at rZeroHeight and below it nearer the middle
	float yOffset {-80.0f};
	float rZeroHeight {10.0f};
	/// What a thing that can't be destroyed is thrown back out with, a scale of the thrown speeds
	float throwScale {2.0f};
};

/// A thing being carried in, as it stands before a step
struct AttractAtom
{
	glm::vec3 position {0.0f};
	/// The velocity it was taken with
	glm::vec3 takenVelocity {0.0f};
	/// How high above the land it stood when taken
	float startHeight {0.0f};
	/// Seconds since it was taken
	float age {0.0f};
	/// It can't be destroyed: it is thrown back out instead of being taken
	bool indestructible {false};
};

/// Where a step leaves a thing being carried in
struct AttractStep
{
	glm::vec3 position {0.0f};
	glm::vec3 velocity {0.0f};
};

/// The height of the land at a point across it
using GroundHeight = std::function<float(glm::vec2)>;

/// One step of dt seconds round a vortex whose middle is at the origin: where the thing goes, or none once it has
/// reached the middle and is gone (taken, or thrown back out if it can't be destroyed). Its distance shrinks, it swings
/// round faster the nearer it is, its velocity turns from the one it was taken with to the vortex's, and its height
/// blends from where it was taken to the funnel's.
[[nodiscard]] std::optional<AttractStep> Attract(const AttractSettings& settings, const AttractAtom& atom, glm::vec3 origin,
                                                 float dt, const GroundHeight& ground);

/// How a thing that can't be destroyed is thrown back out of the middle at an angle: outwards along (sin a, -cos a)
/// at 8 to 13 times the scale and upwards at 10 to 15 times it, the two random numbers each up to 5; it spins about the
/// level line across its flight, the faster the shorter it is
struct ThrowOut
{
	glm::vec3 velocity {0.0f};
	glm::vec3 spin {0.0f};
};
inline constexpr float k_ThrowOutRandom = 5.0f;
[[nodiscard]] ThrowOut ThrowOutOf(float angle, float outRandom, float upRandom, float scale, float height);

/// A line of the crossing file: one land-script command making a thing again, relative to the vortex
struct VillagerLine
{
	/// The town's number, -1 for none
	int32_t town {-1};
	map_coords::MapCoords offset;
	/// Its tribe and its job, as the land scripts name them ("CELTIC_FARMER")
	std::string villagerType;
	uint32_t age {0};
};
/// A villager of no tribe, such as a shepherd of a story, by its row of the special villagers
struct SpecialVillagerLine
{
	int32_t town {-1};
	map_coords::MapCoords offset;
	int32_t row {0};
	uint32_t age {0};
};
struct AnimalLine
{
	map_coords::MapCoords offset;
	/// Its kind's animal type
	int32_t type {0};
	/// Its flock's number and the flock's town's number, -1 for none
	int32_t flock {-1};
	int32_t town {-1};
	uint32_t age {0};
};
struct PotLine
{
	map_coords::MapCoords offset;
	int32_t row {0};
	/// Its kind of resource
	int32_t resource {0};
	uint32_t amount {0};
};
struct TreeLine
{
	/// A tree taken leaves its forest
	int32_t forest {-1};
	map_coords::MapCoords offset;
	int32_t row {0};
	bool nonScenic {false};
	float yAngle {0.0f};
	float scale {1.0f};
	float maxScale {1.0f};
};
/// A rock or any other static thing lying about
struct MobileStaticLine
{
	map_coords::MapCoords offset;
	int32_t row {0};
	float xAngle {0.0f};
	float yAngle {0.0f};
	float zAngle {0.0f};
	float scale {1.0f};
};
struct MobileObjectLine
{
	map_coords::MapCoords offset;
	int32_t row {0};
	float yAngle {0.0f};
	float scale {1.0f};
};
struct DeadTreeLine
{
	map_coords::MapCoords offset;
	/// The player it belongs to as the land scripts name them ("PLAYER_ONE"); the neutral player's when none
	std::string player;
	int32_t row {0};
	float life {1.0f};
	float xAngle {0.0f};
	float yAngle {0.0f};
	float zAngle {0.0f};
};
using Line = std::variant<VillagerLine, SpecialVillagerLine, AnimalLine, PotLine, TreeLine, MobileStaticLine, MobileObjectLine,
                          DeadTreeLine>;

/// The land scripts' text for a command: its name, then each parameter by its letter (A a place or plain text, N a whole
/// number, F a number, L quoted text), comma separated in brackets, ending the line
[[nodiscard]] std::string CommandFormat(std::string_view name, std::string_view parameters);
/// A place across the land relative to another, as the land scripts give one: "x,z" in metres to two places
[[nodiscard]] std::string PlaceText(const map_coords::MapCoords& offset);
/// The line, ending in a new line
[[nodiscard]] std::string Text(const Line& line);

// The vortex the player arrives through on the next land throws things out: every second game turn, at a random angle,
// the next line of the crossing file makes its thing again about a place 16 m out, which is then thrown out across the
// land; once the file is read, new villagers are made until 30 have come out.

/// The arriving vortex brings this many villagers, the crossing's and new ones together
inline constexpr uint32_t k_ArrivalVillagers = 30;
/// It takes the nearest town within this distance as its own when a script gave it none
inline constexpr float k_ArrivalTownReach = 200.0f;
/// Every fifth new villager is a child of 1 to 9; the others are adults of 16 to 21
inline constexpr uint32_t k_ChildAgeRandom = 9;
inline constexpr uint32_t k_AdultAgeRandom = 6;
/// Half the new villagers are housewives; the others take one of the five jobs after it
inline constexpr float k_HousewifeShare = 0.5f;
inline constexpr uint32_t k_JobRandom = 5;
/// Without a town, the new villagers are Norse
inline constexpr int32_t k_ArrivalTribeWithoutTown = 7;

/// Where things come out at an angle: the vortex's place moved about 16 m along (sin a, -cos a), by whole map cells, so
/// it keeps its place within its cell
[[nodiscard]] map_coords::MapCoords ThrowOutPlace(const map_coords::MapCoords& vortex, float angle);
/// The vortex keeps fading out from the start while its file is still being read or fewer than its villagers have
/// come out
[[nodiscard]] bool HoldsFade(bool reading, uint32_t villagersOut);
/// Whether a vortex still keeps a thing it threw out: only a vortex into the land throws things out and keeps them
[[nodiscard]] bool KeepsThrown(VortexType type, std::span<const entt::entity> thrown, entt::entity thing);
/// The random limit for the age of the new villager that brings the count to `count`, and its age from the number drawn
[[nodiscard]] uint32_t NewVillagerAgeLimit(uint32_t count);
[[nodiscard]] uint32_t NewVillagerAge(uint32_t count, uint32_t random);
/// The row of the villagers' table a new villager of a tribe takes: its tribe's housewife when the first number is under
/// a half, else one of the five jobs after it by the second
[[nodiscard]] int32_t NewVillagerRow(int32_t tribe, float housewifeRandom, uint32_t jobRandom);

} // namespace openblack::vortex::crossing
