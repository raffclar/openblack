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
#include <vector>

#include "SaveBytes.h"

// The creature's own part of its record in a saved game, in the layout and order that the game uses. The parts that
// every object of its classes writes (its position, its movement and its living state) come before it and are not
// here. The mind of the creature is not here either: the game keeps it in a mind file beside the world file.
//
// A reference to another object is a number: 0 for none, else the number of an object that the file has before.
// The game can also write a new object in full at its first reference. These functions do not: a reference is always
// a number only.
namespace openblack::gamesave
{

/// A reference to another object of the saved game, 0 for none
using Ref = uint32_t;

/// The version of the record that the game writes. The mind file of the same game has the same version.
inline constexpr uint32_t k_CreatureRecordVersion = 33;
/// The mind's processing word comes only in records of a newer version than this
inline constexpr uint32_t k_LastVersionWithoutProcessing = 25;
/// The number of creature spells, in the order of the game's tables (freeze first)
inline constexpr size_t k_CreatureSpellCount = 16;
/// The index or the player number of the creature when it has none
inline constexpr uint8_t k_None = 0xFF;
/// The first word of each spell slot, from its construction
inline constexpr uint32_t k_SpellSlotStart = 16;
/// The number of hand glow types that keep a creed power
inline constexpr size_t k_CreedPowerCount = 4;

using Point = std::array<float, 3>;

/// One creature spell: where it is and what it found
struct SpellSlot
{
	/// The game sets this word to 16 and does not change it
	uint32_t start {k_SpellSlotStart};
	/// 0 off, 1 waits to start, 2 starts, 3 holds, 4 finishes
	uint32_t phase {0};
	/// The turns that are left of the phase
	int32_t turnsLeft {0};
	/// The value that the spell found when it started, which it puts back when it ends
	float found {0.0f};
	/// The turns that the spell holds for when it started, or -1 for all time
	int32_t holdTurns {0};
	/// The miracle that made the spell
	Ref miracle {0};

	bool operator==(const SpellSlot&) const = default;
};

/// A spell that waits for another spell of its kind to finish
struct QueuedSpell
{
	uint32_t spell {0};
	/// The time that the spell holds for, in seconds, as the player cast it
	float seconds {0.0f};
	Ref miracle {0};

	bool operator==(const QueuedSpell&) const = default;
};

/// A step of the agenda of the creature's plan
struct SubAction
{
	uint32_t action {0};
	/// The object of the step's belief
	Ref object {0};
	uint32_t word10 {0};
	Point point {};
	uint32_t word20 {0};
	uint32_t word24 {0};
	uint32_t word28 {0};
	/// A second object of the step
	Ref other {0};
	uint32_t word30 {0};

	bool operator==(const SubAction&) const = default;
};

/// The agenda of the creature's plan: its steps and the step that it does now
struct Agenda
{
	uint32_t word08 {0};
	uint32_t current {0};
	uint32_t state {0};
	uint32_t count {0};
	uint32_t word18 {0};
	uint32_t word1C {0};
	uint32_t word20 {0};
	/// The steps from the current step to the last. The game writes no step before the current step.
	std::vector<SubAction> steps;
	/// The creature of the agenda
	Ref creature {0};
	Ref other {0};

	bool operator==(const Agenda&) const = default;
};

/// The plan of a creature that the game keeps in the save: what it wants, what it does and to what, and its agenda
struct KeptPlan
{
	uint32_t desire {0};
	uint32_t action {0};
	/// The objects of its activity, of its action and of what it uses
	std::array<Ref, 3> objects {};
	Agenda agenda;

	bool operator==(const KeptPlan&) const = default;
};

struct CreedPower
{
	float scale {0.0f};
	float power {0.0f};

	bool operator==(const CreedPower&) const = default;
};

/// The creature's own part of its record
struct CreatureRecord
{
	uint32_t version {k_CreatureRecordVersion};
	/// 1 when the game was saved in the temple. The game reads it and does not use it.
	uint32_t savedInTemple {0};
	/// The place of the creature in the game's list of creatures, which names its mind file
	uint8_t index {k_None};
	/// The number of the player that owns the creature
	uint8_t owner {k_None};
	std::array<SpellSlot, k_CreatureSpellCount> spells {};
	std::vector<QueuedSpell> queued;
	/// 0 while a spell or the lesson choice holds the mind still
	uint32_t mindProcessing {1};
	float largestSize {2.4f};
	float smallestSize {0.2f};
	uint8_t mayGoThroughVortex {0};
	uint8_t fizzedForGood {0};
	/// How far it fizzed out of sight, 0 to 1, the fizz that it goes to, and the share of the way a second
	float fizz {0.0f};
	float fizzTarget {0.0f};
	float fizzPerSecond {0.0f};
	float freeze {0.0f};
	/// The game sets this word to 0 and does not change it
	uint32_t word12C0 {0};
	/// The visual effects of the spells on it
	std::array<Ref, 3> spellVisuals {};
	/// The particles in its left hand and in its right hand while it casts
	std::array<Ref, 2> handParticles {};
	/// The point on the land at which a spell on the creature aims, its particles and the object that it aims at
	Point targetPoint {};
	Ref targetParticles {0};
	Ref targetObject {0};
	uint8_t hasTarget {0};
	Ref arena {0};
	/// Its pen
	Point home {};
	float fightHealth {1.0f};
	uint32_t autoFight {0};
	/// The villagers that dance with it, the first first
	std::vector<Ref> dancePartners;
	Ref dance {0};
	uint32_t canDie {1};
	float strength {0.5f};
	/// The size that it grew to
	float size {1.0f};
	float alignment {0.0f};
	/// The pace of its animations: 1, less while a freeze slows it
	float animationPace {1.0f};
	uint32_t autoScale {0};
	float autoScaleShare {0.0f};
	/// The plan and its agenda: kept only when a script controls the creature, it fights, or villagers dance with it
	std::optional<KeptPlan> plan;
	std::array<CreedPower, k_CreedPowerCount> creedPowers {};

	bool operator==(const CreatureRecord&) const = default;
};

/// Adds the creature part to the writer, in the game's layout
void Write(ByteWriter& writer, const CreatureRecord& record);
[[nodiscard]] std::vector<uint8_t> Write(const CreatureRecord& record);
/// Reads a creature part, or nothing when the bytes stop before its end
[[nodiscard]] std::optional<CreatureRecord> ReadCreatureRecord(ByteReader& reader);
[[nodiscard]] std::optional<CreatureRecord> ReadCreatureRecord(std::span<const uint8_t> bytes);

/// The leash of a player's creature, in the layout that the game keeps in the player's interface
struct LeashRecord
{
	uint32_t on {0};
	uint32_t works {1};
	/// 1 the evil leash, 2 the learning leash, 3 the good leash
	uint32_t type {2};
	uint8_t player {0};
	uint32_t word20 {0};
	/// The object that the leash is tied to
	Ref tiedTo {0};
	float length {0.0f};
	float fullLength {0.0f};
	/// The interface that holds the leash
	Ref holder {0};
	uint32_t word34 {0};

	bool operator==(const LeashRecord&) const = default;
};

void Write(ByteWriter& writer, const LeashRecord& record);
[[nodiscard]] std::optional<LeashRecord> ReadLeashRecord(ByteReader& reader);

/// The leash types that the local player's creature knows, as the one byte that the game keeps for them
struct KnownLeashes
{
	bool evil {false};
	bool rope {false};
	bool good {false};

	bool operator==(const KnownLeashes&) const = default;
};

[[nodiscard]] uint8_t ToByte(const KnownLeashes& known);
[[nodiscard]] KnownLeashes KnownLeashesFromByte(uint8_t byte);

} // namespace openblack::gamesave
