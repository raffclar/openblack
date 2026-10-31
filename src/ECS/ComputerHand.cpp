/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ComputerHand.h"

#include <cmath>
#include <cstdint>

#include <array>
#include <optional>
#include <utility>
#include <vector>

#include "3D/MapCoords.h"
#include "ECS/ComputerHandSteps.h"

using namespace openblack;
using namespace openblack::computer_hand;
using namespace openblack::computer_hand::steps;

namespace
{

/// The move that each kind of step needs first, by the game's table
constexpr std::array<std::optional<StepKind>, static_cast<size_t>(StepKind::_Count)> k_MoveFirst {{
    StepKind::MoveHandToObject, // PickUpObject
    std::nullopt,               // MoveHandToPos
    std::nullopt,               // MoveHandToObject
    std::nullopt,               // MoveHandToCreatureHead
    std::nullopt,               // MoveHandNearPos
    std::nullopt,               // MoveHandNearObject
    std::nullopt,               // MoveHandToPosStopWhenOutOfInfluence
    StepKind::MoveHandNearPos,  // ThrowObjectInHandAtPos
    StepKind::MoveHandToPos,    // DropObjectInHandAtPos
    std::nullopt,               // SayText
    StepKind::MoveHandToObject, // GiveObjectInHandToCreature
    std::nullopt,               // SetLeash
    std::nullopt,               // AttachLeashToObject
    StepKind::MoveHandToObject, // SlapCreature
    StepKind::MoveHandToObject, // StrokeCreature
    StepKind::MoveHandToObject, // CastSpellAtObject
    StepKind::MoveHandToPos,    // CastSpellAtPos
    std::nullopt,               // CastSpellTowardPos
    StepKind::MoveHandToPos,    // CastShieldAtPos
    std::nullopt,               // CloseDownCurrentSpell
    StepKind::MoveHandToObject, // RaiseTotem
    StepKind::MoveHandToObject, // LowerTotem
    std::nullopt,               // MakeGesture
    std::nullopt,               // Wait
    StepKind::MoveHandToObject, // DropObjectInHandOnObject
    StepKind::MoveHandToObject, // ForceCreatureToPerformAction
    StepKind::MoveHandToObject, // ForceCreatureDominantDesire
    StepKind::MoveHandToObject, // WaitForObjectToBeNearPos
    StepKind::MoveHandToObject, // WaitForSpellToWearOff
    StepKind::MoveHandToObject, // CreateFoodFromContainer
    StepKind::MoveHandToObject, // CreateWoodFromContainer
    StepKind::MoveHandToObject, // SetCreatureSpeedForJourney
    std::nullopt,               // MakeWorshippersChantForSpell
    std::nullopt,               // MakeWorshippersChantForSpellNoWait
    std::nullopt,               // SetHandToPos
    std::nullopt,               // MoveHandToPosDirectYInterpolation
    std::nullopt,               // SetHandToPosDirectY
}};

Result WaitStart(HandAction& action, const World& /*world*/, const Step& step)
{
	action.counter = static_cast<uint32_t>(map_coords::FtoL(static_cast<float>(k_TurnsPerSecond) * ArgumentAt(step, 0).value));
	return Result::Next;
}

Result WaitProcess(HandAction& action, const World& /*world*/, const Step& /*step*/)
{
	--action.counter;
	return action.counter != 0 ? Result::Continue : Result::Next;
}

/// The part of the first step that the hand is at, from the table of the steps
Result RunPart(HandAction& action, const World& world, const Step& step)
{
	const auto& row = Table().at(static_cast<size_t>(step.kind));
	if (!row.made)
	{
		// No action of the god uses the kinds of steps that are not made
		return Result::Abort;
	}
	const auto part = action.part == 0 ? row.start : (action.part == 1 ? row.process : row.end);
	return part != nullptr ? part(action, world, step) : Result::Next;
}

/// The first step goes to its next part, and after its end the step is done
void NextPart(HandAction& action)
{
	++action.part;
	if (action.part != 3)
	{
		return;
	}
	action.part = 0;
	action.steps.erase(action.steps.begin());
	if (action.insertedMove)
	{
		action.insertedMove = false;
	}
	else
	{
		action.moveFirst = true;
	}
}

/// Makes the table: each owner file puts its own steps in it
StepTable MakeTable()
{
	StepTable table {};
	AddCoreSteps(table);
	AddMoveSteps(table);
	AddObjectSteps(table);
	AddCastSteps(table);
	AddCreatureSteps(table);
	AddTotemSteps(table);
	return table;
}

} // namespace

void openblack::computer_hand::steps::AddCoreSteps(StepTable& table)
{
	// SayText is not made yet
	AddStep(table, StepOwner::Core, StepKind::Wait, {.made = true, .start = WaitStart, .process = WaitProcess});
}

const StepTable& openblack::computer_hand::steps::Table()
{
	static const StepTable k_Table = MakeTable();
	return k_Table;
}

std::optional<StepKind> openblack::computer_hand::MoveFirst(StepKind kind)
{
	return k_MoveFirst.at(static_cast<size_t>(kind));
}

bool openblack::computer_hand::Busy(const HandAction& action)
{
	return !action.steps.empty();
}

void openblack::computer_hand::Add(HandAction& action, StepKind kind, std::vector<Argument> arguments)
{
	action.steps.push_back({.kind = kind, .arguments = std::move(arguments)});
}

void openblack::computer_hand::Clear(HandAction& action)
{
	action.locked = false;
	action.steps.clear();
	action.part = 0;
	action.moveFirst = true;
	action.insertedMove = false;
	action.counter = 0;
	action.castTarget = entt::null;
}

void openblack::computer_hand::ClearAndPutDown(HandAction& action, const World& world)
{
	Clear(action);
	if (!world.objects.HandHasRoom())
	{
		world.objects.PutDownHeld(action.hand);
	}
}

bool openblack::computer_hand::Process(HandAction& action, const World& world)
{
	action.coords = {.x = map_coords::ToFixed(action.hand.x), .z = map_coords::ToFixed(action.hand.z), .altitude = 0.0f};
	if (!Busy(action))
	{
		action.camera = action.hand;
		return true;
	}
	const auto& front = action.steps.front();
	auto move = MoveFirst(front.kind);
	if (action.moveFirst && move.has_value())
	{
		// A miracle that is felt as angry is cast from near its target rather than over it
		if (front.kind == StepKind::CastSpellAtObject || front.kind == StepKind::CastSpellAtPos)
		{
			if (world.casts.Angry(static_cast<MagicType>(ArgumentAt(front, 1).number)))
			{
				move = front.kind == StepKind::CastSpellAtObject ? StepKind::MoveHandNearObject : StepKind::MoveHandNearPos;
			}
		}
		Step inserted {.kind = *move, .arguments = {ArgumentAt(front, 0)}};
		action.steps.insert(action.steps.begin(), std::move(inserted));
		action.moveFirst = false;
		action.insertedMove = true;
		return true;
	}
	const auto step = action.steps.front();
	const auto result = RunPart(action, world, step);
	// The camera of the god follows the hand after each part, but not after the turn that puts a move in front
	action.camera = action.hand;
	switch (result)
	{
	case Result::Next:
		NextPart(action);
		return true;
	case Result::Abort:
		ClearAndPutDown(action, world);
		return false;
	case Result::Continue:
		return true;
	}
	return true;
}

glm::vec3 openblack::computer_hand::ThrowVelocity(glm::vec3 hand, glm::vec3 place)
{
	// The thing flies for a time that the distance gives, and gravity pulls it down for that time
	const float dx = place.x - hand.x;
	const float dy = place.y - hand.y;
	const float dz = place.z - hand.z;
	const float distance = std::sqrt(dz * dz + dy * dy + dx * dx);
	const float seconds = std::sqrt(distance * k_ThrowTimeScale);
	const float fall = seconds * seconds * 0.5f;
	const float drop = fall * k_ThrowGravity;
	const float inverse = 1.0f / seconds;
	return {(dx - 0.0f) * inverse, (dy - drop) * inverse, (dz - 0.0f) * inverse};
}

float openblack::computer_hand::GestureSharePerTurn(float length, float circleLength)
{
	if (!(length > 0.0f) || !(circleLength > 0.0f))
	{
		return 1.0f;
	}
	return 1.0f / ((length / circleLength) * k_GestureTurns);
}

glm::vec3 openblack::computer_hand::GestureHand(glm::vec3 corner, glm::vec3 shapePoint)
{
	return {shapePoint.x * k_GestureSize + corner.x, shapePoint.z * k_GestureSize + corner.y, corner.z};
}
