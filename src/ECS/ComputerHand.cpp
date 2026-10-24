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
#include "Common/GUtilsDistance.h"

using namespace openblack;
using namespace openblack::computer_hand;

namespace
{

/// What a part of a step tells the hand to do next
enum class Result : uint8_t
{
	Next,
	Abort,
	Continue,
};

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

/// The argument at the place, an empty one when the step has fewer
Argument ArgumentAt(const Step& step, size_t index)
{
	return index < step.arguments.size() ? step.arguments[index] : Argument {};
}

/// The place in metres of a map position
glm::vec3 PlaceOf(const World& world, const map_coords::MapCoords& coords)
{
	return {map_coords::ToMetres(coords.x), world.LandHeight(coords) + coords.altitude, map_coords::ToMetres(coords.z)};
}

/// The map position under the hand, as the hand's height finds it
map_coords::MapCoords UnderHand(const HandAction& action)
{
	return {.x = map_coords::FtoL(action.hand.x * 65536.0f * 0.1f),
	        .z = map_coords::FtoL(action.hand.z * 65536.0f * 0.1f),
	        .altitude = 0.0f};
}

/// The hand moves a step toward its target over the land. It returns the distance that was left before the step.
float StepTowardTarget(HandAction& action)
{
	float dx = action.target.x - action.hand.x;
	float dz = action.target.z - action.hand.z;
	const float distance = std::sqrt(dz * dz + dx * dx);
	if (dx != 0.0f || dz != 0.0f)
	{
		const float inverse = 1.0f / distance;
		dx = inverse * dx;
		dz = dz * inverse;
	}
	// Near the target the hand moves at its speed. Further away it moves a fixed step each turn.
	float step = action.speed / static_cast<float>(k_TurnsPerSecond);
	if (distance < step)
	{
		step = distance;
	}
	if (distance > k_FarDistance)
	{
		step = k_FarStep;
	}
	else if (distance > k_MiddleDistance)
	{
		step = k_MiddleStep;
	}
	action.hand.x = dx * step + action.hand.x;
	action.hand.y = step * 0.0f + action.hand.y;
	action.hand.z = dz * step + action.hand.z;
	return distance;
}

/// The hand floats over the land
void FloatOverLand(HandAction& action, const World& world)
{
	action.hand.y = world.LandHeight(UnderHand(action)) + k_HandHeight;
}

/// The hand moves toward a thing and floats over it at a height that the thing's height gives
void KeepHandOver(HandAction& action, const World& world, entt::entity thing)
{
	const auto coords = world.CoordsOf(thing);
	if (!coords.has_value())
	{
		return;
	}
	action.target = PlaceOf(world, *coords);
	action.target.y = world.HeightOf(thing) + coords->altitude;
	StepTowardTarget(action);
	const float over = world.HeightOf(thing) * k_OverThingHeights;
	action.hand.y = world.LandHeight(UnderHand(action)) + over;
}

Result MoveToObjectStart(HandAction& action, const World& world, const Step& step)
{
	if (const auto coords = world.CoordsOf(ArgumentAt(step, 0).object))
	{
		action.target = PlaceOf(world, *coords);
	}
	return Result::Next;
}

Result MoveToTarget(HandAction& action, const World& world)
{
	StepTowardTarget(action);
	const float restX = action.hand.x - action.target.x;
	const float restZ = action.hand.z - action.target.z;
	FloatOverLand(action, world);
	return std::sqrt(restZ * restZ + restX * restX) <= k_ArrivedDistance ? Result::Next : Result::Continue;
}

Result ChantStart(HandAction& action, const World& world, const Step& step)
{
	action.counter = 0;
	return world.IconHasSite(ArgumentAt(step, 1).object) ? Result::Next : Result::Abort;
}

/// The seconds that the step has counted, in whole seconds as the game counts them
float WholeSeconds(uint32_t turns)
{
	return static_cast<float>(turns / k_TurnsPerSecond);
}

Result ChantProcess(HandAction& action, World& world, const Step& step)
{
	const auto type = static_cast<MagicType>(ArgumentAt(step, 0).number);
	const auto icon = ArgumentAt(step, 1).object;
	++action.counter;
	if (icon == entt::null)
	{
		return Result::Abort;
	}
	// The worshippers already chant at the icon, or they start now
	if (world.IconCharging(icon) || world.StartChant(icon, type))
	{
		return Result::Next;
	}
	if (!world.IconHasSite(icon))
	{
		return Result::Abort;
	}
	return WholeSeconds(action.counter) > k_ChantStartSeconds ? Result::Abort : Result::Continue;
}

Result ChantEnd(HandAction& action, const World& world, const Step& step)
{
	const auto icon = ArgumentAt(step, 1).object;
	if (icon == entt::null)
	{
		return Result::Abort;
	}
	if (world.ChantNeeded(icon) <= 0.0f)
	{
		return Result::Next;
	}
	++action.counter;
	return WholeSeconds(action.counter) > k_ChantFinishSeconds ? Result::Abort : Result::Continue;
}

Result CastStart(HandAction& action, const Step& step)
{
	action.castTarget = ArgumentAt(step, 0).object;
	return Result::Next;
}

Result CastProcess(HandAction& action, World& world, const Step& step)
{
	const auto target = ArgumentAt(step, 0).object;
	const auto type = static_cast<MagicType>(ArgumentAt(step, 1).number);
	const auto icon = ArgumentAt(step, 2).object;
	// The hand stays over the thing, except for an angry miracle
	if (!world.Angry(type))
	{
		KeepHandOver(action, world, target);
	}
	if (!world.Cast(type, target, icon, action.hand))
	{
		return Result::Abort;
	}
	action.counter = static_cast<uint32_t>(map_coords::FtoL(static_cast<float>(k_TurnsPerSecond) * world.CastSeconds(type)));
	return Result::Next;
}

Result CastEnd(HandAction& action, const World& world, const Step& step)
{
	--action.counter;
	if (action.counter == 0)
	{
		return Result::Next;
	}
	KeepHandOver(action, world, ArgumentAt(step, 0).object);
	return Result::Continue;
}

Result PickUpStart(HandAction& action, const World& world, const Step& step)
{
	if (const auto coords = world.CoordsOf(ArgumentAt(step, 0).object))
	{
		action.target = PlaceOf(world, *coords);
	}
	return Result::Next;
}

Result PickUpProcess(HandAction& action, World& world, const Step& step)
{
	// A full hand puts down what it holds first, and tries again on the next turn
	if (!world.HandHasRoom())
	{
		world.PutDownHeld(action.hand);
		return Result::Continue;
	}
	const auto thing = ArgumentAt(step, 0).object;
	if (!world.CanPickUp(thing))
	{
		return Result::Abort;
	}
	world.PickUp(thing, action.hand);
	return Result::Next;
}

Result MoveToPlaceStart(HandAction& action, const Step& step)
{
	action.target = ArgumentAt(step, 0).place;
	return Result::Next;
}

Result MoveToPlace(HandAction& action, const World& world)
{
	// The hand moves at its speed and stops at the place when it is nearer than a step
	const float distance = gutils::GetDistance(action.hand, action.target);
	float dx = action.target.x - action.hand.x;
	float dz = action.target.z - action.hand.z;
	if (dx != 0.0f || dz != 0.0f)
	{
		const float inverse = 1.0f / std::sqrt(dx * dx + dz * dz);
		dx = dx * inverse;
		dz = inverse * dz;
	}
	const float step = action.speed / static_cast<float>(k_TurnsPerSecond);
	if (!(distance > step))
	{
		action.hand = action.target;
		FloatOverLand(action, world);
		return Result::Next;
	}
	action.hand.x = dx * step + action.hand.x;
	action.hand.y = 0.0f * step + action.hand.y;
	action.hand.z = dz * step + action.hand.z;
	FloatOverLand(action, world);
	return Result::Continue;
}

Result WaitStart(HandAction& action, const Step& step)
{
	action.counter = static_cast<uint32_t>(map_coords::FtoL(static_cast<float>(k_TurnsPerSecond) * ArgumentAt(step, 0).value));
	return Result::Next;
}

Result WaitProcess(HandAction& action)
{
	--action.counter;
	return action.counter != 0 ? Result::Continue : Result::Next;
}

Result DropHeldProcess(HandAction& action, World& world)
{
	world.DropHeld(action.hand);
	return Result::Next;
}

Result UseHeldProcess(HandAction& action, World& world, const Step& step)
{
	world.UseHeldOn(ArgumentAt(step, 0).object, action.hand);
	return Result::Next;
}

/// The gesture that a step draws, from its magic type and its stage
uint32_t GestureOf(const World& world, const Step& step)
{
	return world.SeedGesture(static_cast<MagicType>(ArgumentAt(step, 0).number), ArgumentAt(step, 1).number);
}

Result GestureStart(HandAction& action, const World& world, const Step& step)
{
	action.kept = action.hand;
	action.progress = 0.0f;
	action.hand = GestureHand(action.kept, world.GesturePoint(GestureOf(world, step), 0.0f));
	return Result::Next;
}

Result GestureProcess(HandAction& action, const World& world, const Step& step)
{
	const auto gesture = GestureOf(world, step);
	const float share = GestureSharePerTurn(world.GestureLength(gesture), world.GestureLength(k_GestureCircle));
	action.progress = share + action.progress;
	if (!(action.progress < 1.0f))
	{
		action.progress = 1.0f;
		return Result::Next;
	}
	action.hand = GestureHand(action.kept, world.GesturePoint(gesture, action.progress));
	return Result::Continue;
}

Result GestureEnd(HandAction& action, World& world, const Step& step)
{
	// The hand goes back to where it started, and the gesture shows on the land there
	action.hand = action.kept;
	world.ShowGesture(GestureOf(world, step), action.hand, k_GestureSize);
	return Result::Next;
}

/// The part of the first step, for the kinds of steps that the hand knows
Result RunPart(HandAction& action, World& world, const Step& step)
{
	switch (step.kind)
	{
	case StepKind::MoveHandToObject:
		if (action.part == 0)
		{
			return MoveToObjectStart(action, world, step);
		}
		return action.part == 1 ? MoveToTarget(action, world) : Result::Next;
	case StepKind::MakeWorshippersChantForSpell:
	case StepKind::MakeWorshippersChantForSpellNoWait:
		if (action.part == 0)
		{
			return ChantStart(action, world, step);
		}
		if (action.part == 1)
		{
			return ChantProcess(action, world, step);
		}
		return step.kind == StepKind::MakeWorshippersChantForSpell ? ChantEnd(action, world, step) : Result::Next;
	case StepKind::CastSpellAtObject:
		if (action.part == 0)
		{
			return CastStart(action, step);
		}
		return action.part == 1 ? CastProcess(action, world, step) : CastEnd(action, world, step);
	case StepKind::PickUpObject:
		if (action.part == 0)
		{
			return PickUpStart(action, world, step);
		}
		return action.part == 1 ? PickUpProcess(action, world, step) : Result::Next;
	case StepKind::MoveHandToPos:
		if (action.part == 0)
		{
			return MoveToPlaceStart(action, step);
		}
		return action.part == 1 ? MoveToPlace(action, world) : Result::Next;
	case StepKind::Wait:
		if (action.part == 0)
		{
			return WaitStart(action, step);
		}
		return action.part == 1 ? WaitProcess(action) : Result::Next;
	case StepKind::DropObjectInHandAtPos:
		return action.part == 1 ? DropHeldProcess(action, world) : Result::Next;
	case StepKind::DropObjectInHandOnObject:
		return action.part == 1 ? UseHeldProcess(action, world, step) : Result::Next;
	case StepKind::MakeGesture:
		if (action.part == 0)
		{
			return GestureStart(action, world, step);
		}
		return action.part == 1 ? GestureProcess(action, world, step) : GestureEnd(action, world, step);
	default:
		// No action of the god uses the other kinds yet
		return Result::Abort;
	}
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

} // namespace

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

bool openblack::computer_hand::Process(HandAction& action, World& world)
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
			if (world.Angry(static_cast<MagicType>(ArgumentAt(front, 1).number)))
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
		Clear(action);
		return false;
	case Result::Continue:
		return true;
	}
	return true;
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
