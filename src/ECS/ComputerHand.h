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

#include <optional>
#include <vector>

#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "3D/MapCoords.h"
#include "Enums.h"

/// The hand of a computer god. An action of the god is a list of steps that the hand does in turn: it moves, it makes the
/// worshippers chant for a miracle, it casts. Each step has a start, a part that the hand does each turn, and an end.
namespace openblack::computer_hand
{

/// The kinds of steps that a hand can do, in the order of the game's table
enum class StepKind : uint8_t
{
	PickUpObject,
	MoveHandToPos,
	MoveHandToObject,
	MoveHandToCreatureHead,
	MoveHandNearPos,
	MoveHandNearObject,
	MoveHandToPosStopWhenOutOfInfluence,
	ThrowObjectInHandAtPos,
	DropObjectInHandAtPos,
	SayText,
	GiveObjectInHandToCreature,
	SetLeash,
	AttachLeashToObject,
	SlapCreature,
	StrokeCreature,
	CastSpellAtObject,
	CastSpellAtPos,
	CastSpellTowardPos,
	CastShieldAtPos,
	CloseDownCurrentSpell,
	RaiseTotem,
	LowerTotem,
	MakeGesture,
	Wait,
	DropObjectInHandOnObject,
	ForceCreatureToPerformAction,
	ForceCreatureDominantDesire,
	WaitForObjectToBeNearPos,
	WaitForSpellToWearOff,
	CreateFoodFromContainer,
	CreateWoodFromContainer,
	SetCreatureSpeedForJourney,
	MakeWorshippersChantForSpell,
	MakeWorshippersChantForSpellNoWait,
	SetHandToPos,
	MoveHandToPosDirectYInterpolation,
	SetHandToPosDirectY,

	_Count
};

/// The step that the hand does first to come to the place of a step, none when the step needs no move
[[nodiscard]] std::optional<StepKind> MoveFirst(StepKind kind);

/// A value that a step works with: a thing, a number or a place
struct Argument
{
	entt::entity object {entt::null};
	int32_t number {0};
	float value {0.0f};
};

/// One step of an action
struct Step
{
	StepKind kind {StepKind::Wait};
	std::vector<Argument> arguments;
};

/// The hand of a computer god and the steps of its action
struct HandAction
{
	/// Where the hand is, in metres
	glm::vec3 hand {0.0f};
	/// The map position of the hand at the start of its last turn of an action. The hand starts in the middle of the
	/// land.
	map_coords::MapCoords coords {.x = 0x00808000, .z = 0x00808000, .altitude = 0.0f};
	/// Where the step takes the hand
	glm::vec3 target {0.0f};
	/// Where the hand was when it started a gesture
	glm::vec3 kept {0.0f};
	/// How fast the hand moves near its target, in metres a second
	float speed {90.0f};
	/// The steps still to do, the first now
	std::vector<Step> steps;
	/// The part of the first step: 0 its start, 1 each turn, 2 its end
	uint8_t part {0};
	/// The next step gets the move that brings the hand to it
	bool moveFirst {true};
	/// The first step is such a move
	bool insertedMove {false};
	/// A stronger need may not stop the action
	bool locked {false};
	/// The turns that the step counts
	uint32_t counter {0};
	/// How far along a gesture the hand is, 0 to 1
	float progress {0.0f};
	/// The thing that the hand casts at
	entt::entity castTarget {entt::null};
	/// Where the camera of the god is. It goes to the hand after each turn of a step, and stays there between actions.
	glm::vec3 camera {0.0f};
};

/// The turns in each second of the game
constexpr uint32_t k_TurnsPerSecond = 10;
/// Past this distance in metres the hand moves its far step each turn
constexpr float k_FarDistance = 300.0f;
constexpr float k_FarStep = 100.0f;
/// Past this distance in metres the hand moves its middle step each turn
constexpr float k_MiddleDistance = 100.0f;
constexpr float k_MiddleStep = 10.0f;
/// The hand is at its target when it is no further than this, in metres
constexpr float k_ArrivedDistance = 1.0f;
/// How high the hand floats over the land, in metres
constexpr float k_HandHeight = 10.0f;
/// How many times its height the hand floats over a thing that it casts at
constexpr float k_OverThingHeights = 2.5f;
/// The seconds that the worshippers have to start a chant, and to finish it
constexpr float k_ChantStartSeconds = 5.0f;
constexpr float k_ChantFinishSeconds = 60.0f;
/// The hand draws a gesture in the air on an upright square of this many metres, from its place as a corner. The
/// gesture's shape then shows on the land at the same size.
constexpr float k_GestureSize = 30.0f;
/// A gesture as long as the circle takes this many turns to draw. The other gestures take a time that their length gives.
constexpr float k_GestureTurns = 30.0f;
/// The gesture whose length measures the time of the others: the circle
constexpr uint32_t k_GestureCircle = 4;

/// What the steps of the hand read and change in the game
class World
{
public:
	virtual ~World() = default;
	/// The map position of the thing, none when it is gone
	[[nodiscard]] virtual std::optional<map_coords::MapCoords> CoordsOf(entt::entity thing) const = 0;
	/// The height of the thing
	[[nodiscard]] virtual float HeightOf(entt::entity thing) const = 0;
	/// The height of the land under a map position, in metres
	[[nodiscard]] virtual float LandHeight(const map_coords::MapCoords& coords) const = 0;
	/// The icon belongs to a worship site
	[[nodiscard]] virtual bool IconHasSite(entt::entity icon) const = 0;
	/// The worshippers chant at the icon now
	[[nodiscard]] virtual bool IconCharging(entt::entity icon) const = 0;
	/// The worshippers start to chant for the miracle at the icon. False when the icon cannot start.
	virtual bool StartChant(entt::entity icon, MagicType type) = 0;
	/// The prayer power that the miracle of the icon still needs
	[[nodiscard]] virtual float ChantNeeded(entt::entity icon) const = 0;
	/// The miracle is felt as an angry one
	[[nodiscard]] virtual bool Angry(MagicType type) const = 0;
	/// The god casts the miracle of the icon at the thing, from the hand. False when the miracle is not made.
	virtual bool Cast(MagicType type, entt::entity target, entt::entity icon, glm::vec3 hand) = 0;
	/// The seconds that the god keeps its hand over a miracle that it cast
	[[nodiscard]] virtual float CastSeconds(MagicType type) const = 0;
	/// The gesture of the seed of a miracle: for stage 0 the gesture that opens the choice of miracles, for stage 1 the
	/// gesture of the miracle. 0 when the miracle has no seed.
	[[nodiscard]] virtual uint32_t SeedGesture(MagicType type, int32_t stage) const = 0;
	/// The length of the shape of a gesture, in a square of side 1. 0 when the gesture has no shape.
	[[nodiscard]] virtual float GestureLength(uint32_t gesture) const = 0;
	/// The point of the shape of a gesture at a share of its length, from 0 to 1, in a square of side 1
	[[nodiscard]] virtual glm::vec3 GesturePoint(uint32_t gesture, float share) const = 0;
	/// The shape of the gesture shows on the land, as a recognised gesture shows. The square is so many metres across,
	/// from a corner.
	virtual void ShowGesture(uint32_t gesture, glm::vec3 corner, float size) = 0;
	/// The hand of the god has room for a thing
	[[nodiscard]] virtual bool HandHasRoom() const = 0;
	/// The hand puts down what it holds where the hand is
	virtual void PutDownHeld(glm::vec3 hand) = 0;
	/// The thing can go into the hand now
	[[nodiscard]] virtual bool CanPickUp(entt::entity thing) const = 0;
	/// The god places the thing in its hand. A field gives a handful of its food, then all the food that is left in it.
	virtual void PickUp(entt::entity thing, glm::vec3 hand) = 0;
	/// The hand uses what it holds on the thing: food or wood goes into a store
	virtual void UseHeldOn(entt::entity target, glm::vec3 hand) = 0;
};

/// The hand has steps left
[[nodiscard]] bool Busy(const HandAction& action);
/// Adds a step at the end of the action
void Add(HandAction& action, StepKind kind, std::vector<Argument> arguments);
/// Stops the action: the steps go and the counts start again
void Clear(HandAction& action);
/// The hand does a turn of its action. False when the action ended badly, which clears it.
bool Process(HandAction& action, World& world);

/// The share of a gesture that the hand draws in one turn, from the length of the gesture and of the circle. The hand
/// draws all of a gesture with no length in one turn.
[[nodiscard]] float GestureSharePerTurn(float length, float circleLength);
/// Where the hand is for a point of a gesture's shape: the shape's x goes east and its z goes up from the corner
[[nodiscard]] glm::vec3 GestureHand(glm::vec3 corner, glm::vec3 shapePoint);

} // namespace openblack::computer_hand
