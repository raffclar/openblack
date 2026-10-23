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
#include <functional>
#include <memory>
#include <optional>
#include <variant>
#include <vector>

#include "CreatureRecord.h"
#include "ObjectStream.h"

// The whole records of the objects that a creature's part of a saved game can hold, in the game's layout. Each record
// starts with the type of its class. Then each class adds its values, the base class first.
//
// A reader knows only the classes in this file. A stream that writes another class at a reference cannot be read
// past that point, because the game writes no lengths.
namespace openblack::gamesave
{

/// The game's numbers of the classes in this file
inline constexpr uint32_t k_CreatureType = 0x69;
inline constexpr uint32_t k_LeashStatusType = 0x102;

/// The flag of a thing that a particle system carries
inline constexpr uint8_t k_ThingFlagCarriedByParticles = 0x10;
/// The flag of a positioned thing that the physics moves
inline constexpr uint16_t k_PositionFlagInPhysics = 0x40;

/// The start of every record: what the class writes for every object
struct ThingPart
{
	/// A word that the class writes, for example the number of a player
	uint32_t extra {0};
	/// The running sum of the stream at the record, as the file has it. A write puts the writer's own sum.
	uint32_t checksum {0};
	uint32_t destroyed {0};
	uint8_t flags {0};

	bool operator==(const ThingPart& other) const
	{
		return extra == other.extra && destroyed == other.destroyed && flags == other.flags;
	}
};

/// What a thing with a position adds
struct PositionPart
{
	MapCoords position {};
	uint16_t flags {0};

	bool operator==(const PositionPart&) const = default;
};

/// The body of an object that the physics moves at the save
struct PhysicsPart
{
	/// Three rows of the turn, then the position
	std::array<float, 12> matrix {1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f};
	Point velocity {};
	Point spin {};

	bool operator==(const PhysicsPart&) const = default;
};

/// What an object adds
struct ObjectPart
{
	/// The number of the object's kind in the game's tables of kinds
	uint32_t info {0};
	MapCoords coords {};
	uint32_t creationIndex {0};
	float life {1.0f};
	float scale {1.0f};
	float heading {0.0f};
	ObjectRef fire;
	/// Present only when the position flags say that the physics moves the object
	std::optional<PhysicsPart> physics;

	bool operator==(const ObjectPart&) const = default;
};

/// The move states for which the record keeps the object that the thing walks around
inline constexpr uint8_t k_FirstCircleState = 0x0C;
inline constexpr uint8_t k_LastCircleState = 0x12;
/// The move state for which the record keeps the turns until the step is made again
inline constexpr uint8_t k_StepThroughState = 0x0B;

/// The object that a thing walks around, and where
struct CircleHug
{
	uint8_t turnsToObject {0};
	uint32_t entryDistance {0};
	ObjectRef object;
	int32_t index {0};
	MapCoords coords {};

	bool operator==(const CircleHug&) const = default;
};

/// One step of a move: x and z in 16.16 parts of a cell, with the height between them
struct MoveStep
{
	int32_t x {0};
	float altitude {0.0f};
	int32_t z {0};

	bool operator==(const MoveStep&) const = default;
};
/// The three fields have four bytes each, so the value has no gaps
static_assert(sizeof(MoveStep) == 2 * sizeof(int32_t) + sizeof(float));
template <>
inline constexpr bool k_HasNoGaps<MoveStep> = true;

/// What a moving thing and a thing that walks around obstacles add
struct MovePart
{
	/// The word that every moving thing adds
	uint16_t mobileWord {0};
	int16_t turnsToStateChange {0};
	uint16_t speed {0};
	uint16_t angle {0};
	uint8_t moveState {0};
	/// Present only in the circle states
	std::optional<CircleHug> circle;
	/// Present only in the state that steps through
	std::optional<int8_t> turnsToStepRebuild;
	ObjectRef footpath;
	MapCoords goal {};
	ObjectRef target;
	MoveStep step {};

	bool operator==(const MovePart&) const = default;
};

/// A reaction that the living thing did, and the turn
struct ReactionDone
{
	uint32_t reaction {0};
	uint32_t turn {0};

	bool operator==(const ReactionDone&) const = default;
};

/// What a living thing adds
struct LivingPart
{
	ObjectRef footpath;
	ObjectRef footpathNode;
	/// Three state bytes, a byte that the game does not use, and the turns since the state changed
	std::array<uint8_t, 6> action {};
	ObjectRef reaction;
	uint32_t word9C {0};
	uint32_t birthTurn {0};
	uint8_t byteA8 {0};
	ObjectRef dataPath;
	ObjectRef scriptReminder;
	uint16_t status {0};
	ObjectRef flock;
	ObjectRef objectBC;
	ObjectRef footpathAgain;
	uint32_t wordC4 {0};
	/// Blue, green, red and alpha
	std::array<uint8_t, 4> specular {};
	uint8_t byteD4 {0};
	ObjectRef danceGroup;
	uint8_t byteDC {0};
	std::vector<ReactionDone> reactionsDone;

	bool operator==(const LivingPart&) const = default;
};

/// A creature's whole record
struct CreatureObject
{
	ThingPart thing;
	PositionPart position;
	ObjectPart object;
	MovePart move;
	LivingPart living;
	CreatureRecord creature;

	bool operator==(const CreatureObject&) const = default;
};

/// A player's leash on a creature, as the record that the player's interface writes
struct LeashStatusObject
{
	ThingPart thing;
	LeashRecord leash;

	bool operator==(const LeashStatusObject&) const = default;
};

/// The record of an object of a class that openblack reads and writes in the game's layout
struct SavedObject
{
	std::variant<CreatureObject, LeashStatusObject> value;

	bool operator==(const SavedObject&) const = default;
};

/// Writes the whole record of the object, from the type of its class
void Write(StreamWriter& writer, const SavedObject& object);
/// Reads the whole record of an object, from the type of its class. Gives nothing for a class that the reader does not
/// know (the reader then keeps the type), or bytes that stop or a reference that is not valid.
[[nodiscard]] std::shared_ptr<SavedObject> ReadObject(StreamReader& reader);

void Write(StreamWriter& writer, const CreatureObject& creature);
void Write(StreamWriter& writer, const LeashStatusObject& leash);

/// Calls the function for each reference of the creature's record, in the order that the game writes them. The
/// function does not go into the records that the references hold.
void ForEachReference(CreatureObject& creature, const std::function<void(ObjectRef&)>& function);

} // namespace openblack::gamesave
