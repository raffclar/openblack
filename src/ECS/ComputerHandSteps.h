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
#include <glm/vec3.hpp>

#include "3D/MapCoords.h"
#include "ECS/ComputerHand.h"

/// The table of the steps of the hand. Each kind of step has one owner file, which puts the parts of its steps in the
/// table. The core file lists the call of each owner, so that a new step changes only the file of its owner.
namespace openblack::computer_hand::steps
{

/// What a part of a step tells the hand to do next
enum class Result : uint8_t
{
	Next,
	Abort,
	Continue,
};

/// The owner file of a kind of step
enum class StepOwner : uint8_t
{
	/// ComputerHand.cpp: the steps that need no part of the world
	Core,
	/// ComputerHandMoves.cpp
	Moves,
	/// ComputerHandObjects.cpp
	Objects,
	/// ComputerHandCasts.cpp
	Casts,
	/// ComputerHandCreature.cpp
	Creature,
	/// ComputerHandTotem.cpp
	Totem,
};

/// The owner of each kind of step, in the order of the kinds
constexpr std::array<StepOwner, static_cast<size_t>(StepKind::_Count)> k_Owners {{
    StepOwner::Objects,  // PickUpObject
    StepOwner::Moves,    // MoveHandToPos
    StepOwner::Moves,    // MoveHandToObject
    StepOwner::Creature, // MoveHandToCreatureHead
    StepOwner::Moves,    // MoveHandNearPos
    StepOwner::Moves,    // MoveHandNearObject
    StepOwner::Moves,    // MoveHandToPosStopWhenOutOfInfluence
    StepOwner::Objects,  // ThrowObjectInHandAtPos
    StepOwner::Objects,  // DropObjectInHandAtPos
    StepOwner::Core,     // SayText
    StepOwner::Creature, // GiveObjectInHandToCreature
    StepOwner::Creature, // SetLeash
    StepOwner::Creature, // AttachLeashToObject
    StepOwner::Creature, // SlapCreature
    StepOwner::Creature, // StrokeCreature
    StepOwner::Casts,    // CastSpellAtObject
    StepOwner::Casts,    // CastSpellAtPos
    StepOwner::Casts,    // CastSpellTowardPos
    StepOwner::Casts,    // CastShieldAtPos
    StepOwner::Casts,    // CloseDownCurrentSpell
    StepOwner::Totem,    // RaiseTotem
    StepOwner::Totem,    // LowerTotem
    StepOwner::Casts,    // MakeGesture
    StepOwner::Core,     // Wait
    StepOwner::Objects,  // DropObjectInHandOnObject
    StepOwner::Creature, // ForceCreatureToPerformAction
    StepOwner::Creature, // ForceCreatureDominantDesire
    StepOwner::Moves,    // WaitForObjectToBeNearPos
    StepOwner::Casts,    // WaitForSpellToWearOff
    StepOwner::Objects,  // CreateFoodFromContainer
    StepOwner::Objects,  // CreateWoodFromContainer
    StepOwner::Creature, // SetCreatureSpeedForJourney
    StepOwner::Casts,    // MakeWorshippersChantForSpell
    StepOwner::Casts,    // MakeWorshippersChantForSpellNoWait
    StepOwner::Moves,    // SetHandToPos
    StepOwner::Moves,    // MoveHandToPosDirectYInterpolation
    StepOwner::Moves,    // SetHandToPosDirectY
}};

/// The owner of the kind of step
[[nodiscard]] constexpr StepOwner OwnerOf(StepKind kind)
{
	return k_Owners.at(static_cast<size_t>(kind));
}

/// One part of a step: its start, the part that the hand does each turn, or its end
using Part = Result (*)(HandAction& action, const World& world, const Step& step);

/// The three parts of a kind of step. With no function, the hand moves to the next part at once.
struct StepRow
{
	/// The hand knows the kind of step. A step that the hand does not know ends the action badly.
	bool made {false};
	Part start {nullptr};
	Part process {nullptr};
	Part end {nullptr};
};

using StepTable = std::array<StepRow, static_cast<size_t>(StepKind::_Count)>;

/// The owner puts the parts of a kind of step in the table. Each kind has one owner and one row.
void AddStep(StepTable& table, StepOwner owner, StepKind kind, StepRow row);

/// The steps of each owner file. ComputerHand.cpp calls each of them one time to make the table.
void AddCoreSteps(StepTable& table);
void AddMoveSteps(StepTable& table);
void AddObjectSteps(StepTable& table);
void AddCastSteps(StepTable& table);
void AddCreatureSteps(StepTable& table);
void AddTotemSteps(StepTable& table);

/// The table of all the steps
[[nodiscard]] const StepTable& Table();

/// The argument at the place, an empty one when the step has fewer
[[nodiscard]] Argument ArgumentAt(const Step& step, size_t index);
/// The place in metres of a map position
[[nodiscard]] glm::vec3 PlaceOf(const World& world, const map_coords::MapCoords& coords);
/// The map position under the hand, as the hand's height finds it
[[nodiscard]] map_coords::MapCoords UnderHand(const HandAction& action);
/// The hand moves a step toward its target over the land. It returns the distance that was left before the step.
float StepTowardTarget(HandAction& action);
/// The hand floats over the land
void FloatOverLand(HandAction& action, const World& world);
/// The hand moves toward a thing and floats over it at a height that the thing's height gives
void KeepHandOver(HandAction& action, const World& world, entt::entity thing);
/// The seconds that the step has counted, in whole seconds as the game counts them
[[nodiscard]] float WholeSeconds(uint32_t turns);

} // namespace openblack::computer_hand::steps
