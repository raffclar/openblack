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

/// The kinds of step that the game's agendas use, and the stages that each kind has
namespace openblack::creature_sub_states
{
/// A kind of agenda step, by the number that the game gives it
enum class SubState : uint8_t
{
	Pickup = 0,
	Discard = 1,
	Eat = 2,
	HeldObjectAction = 3,
	StaticAction = 4,
	Throw = 5,
	TurnToFaceObject = 6,
	TurnToFacePos = 7,
	MoveToPos = 8,
	RunAway = 9,
	IndividualAction = 10,
	ChooseObjectToHurl = 11,
	AddVillagersToDance = 12,
	FacialAnimation = 13,
	Faint = 14,
	Poo = 15,
	MoveToThrowPos = 16,
	Stomp = 17,
	LookAtHand = 18,
	RenderImmobile = 19,
	AddToShape = 20,
	MoveToDropPos = 21,
	TurnToFaceAwayFromObject = 22,
	Dance = 23,
	Wait = 24,
	WaitForDancersToStopMoving = 25,
	WaitForDancersToStartDancing = 26,
	Heal = 27,
	Fight = 28,
	StopMoving = 29,
	FollowCamera = 30,
	TurnToFaceCamera = 31,
	CommunicateToPlayer = 32,
	ReduceDesire = 33,
	Loop = 34,
	WalkAlongCoast = 35,
	CheckValue = 36,
	Repair = 37,
	DestroyObject = 38,
	CastSpellAtObject = 39,
	CastSpellAtPos = 40,
	PutOutFire = 41,
	DancingDuringEditor = 42,
	CastSpotVisual = 43,
	StartTheDance = 44,
	TellVillagersToGoToPos = 45,
	TellVillagersToDance = 46,
	StartLooking = 47,
	StopLooking = 48,
	RaiseTotem = 49,
	LowerTotem = 50,
	Drink = 51,
	HealHimself = 52,
	InspectCreature = 53,
	CreateHandObjectFromObject = 54,
	PickupCreatedObject = 55,
	ThrowAtPos = 56,
	AddReactionToObject = 57,
	RequestCreatureDoSomethingWithObject = 58,
	RunAwayFromPlayer = 59,
	StartFire = 60,
	GoNearObject = 61,
	ObserveObject = 62,
	FollowInterestingObject = 63,
	InScript = 64,
	LookAtFlyingObject = 65,
	LookAtCamera = 66,
	FollowHand = 67,
	FaceDownSlope = 68,
	AddToHome = 69,
	WaitForObject = 70,
	SeeWhoWinsGame = 71,
	RespondAfterFight = 72,
	CreateFootPrint = 73,
	OrderCreature = 74,
	WaitForCreatureToPickUp = 75,
	DemobiliseObject = 76,
	RemobiliseObject = 77,
	WaitForever = 78,
	Kiss = 79,
	PointAtPoint = 80,
	PointAtObject = 81,
	DoTeleport = 82,
	StopReacting = 83,
	SetSpeed = 84,
	TurnToFaceHand = 85,
	PickUpFromHand = 86,
	TurnToFaceOtherPlayersCamera = 87,
	Gesture = 88,
	RequestPartner = 89,
	WaitForCreatureToBeFree = 90,
	ActOnObjectInHand = 91,
	CreateFishFromSea = 92,
	TransportedByTeleport = 93,
	PracticeDance = 94,
	ThrowInPile = 95,
	LoopAction = 96,
	ConsiderActionCompleted = 97,
	CreateArena = 98,
	MoveToArena = 99,
	TellVillagersToEnterState = 100,
	Resurrect = 101,
	SwapMindWithOtherCreature = 102,
	Fail = 103,
	WaitForObjectToBeInMap = 104,
	SetPos = 105,
	HideObjectInHand = 106,
	DestroyObjectToUse = 107,
	ClearObjectToActOn = 108,
	ClearObjectToUse = 109,
	Catch = 110,
	KickObject = 111,
	TransportHome = 112,
	RemoveAndCreateSpellFromTotemStatue = 113,
	AddAndDestroySpellFromTotemStatue = 114,
	MoveToThrowAtPos = 115,
	Lightbulb = 116,
	ShatterRock = 117,
	PickUpObjectCreatedByCreature = 118,
	DestroyCreatedObject = 119,
	CreatedHandObjectExNihilo = 120,
	ApplyDesireToObject = 121,
	MoveToWater = 122,
	StopControlledByTeleport = 123,
	WatchPlayerWhileHeHasYourAttention = 124,
	MakeDisciple = 125,
	SetDance = 126,
	SingleKiss = 127,
	EatCreatedObject = 128,
	PointAtArena = 129,
	RestToGetBetter = 130,
	TellVillagersToGoToStartDancePos = 131,
	Sleep = 132,
	WaitUntilFree = 133,
	StopBeingOnFire = 134,
	GetAwayFromObject = 135,
	LookAtCameraInWideScreen = 136,
	Talk = 137,
	CreatePickUpThenRemove = 138,
	WaitForSpellsToWearOff = 139,
	CreateMagicFood = 140,
	Build = 141,
	EatNearestFoodPile = 142,
};

/// The number of kinds of agenda step
constexpr size_t k_SubStateCount = 143;

/// The stages of a kind of step: a start, a stage for each turn, and an end. A kind of step can have no code for a
/// stage. The agenda then goes past that stage at once, in the turn that the stage comes.
struct SubStateStages
{
	bool start {false};
	bool turn {false};
	bool end {false};
};

/// The stages of each kind of step, from the game's table of steps
constexpr std::array<SubStateStages, k_SubStateCount> k_SubStateStages {
    SubStateStages {.start = true, .turn = true, .end = false},   // Pickup
    SubStateStages {.start = true, .turn = true, .end = true},    // Discard
    SubStateStages {.start = true, .turn = true, .end = true},    // Eat
    SubStateStages {.start = true, .turn = true, .end = false},   // HeldObjectAction
    SubStateStages {.start = true, .turn = true, .end = true},    // StaticAction
    SubStateStages {.start = true, .turn = true, .end = true},    // Throw
    SubStateStages {.start = true, .turn = true, .end = true},    // TurnToFaceObject
    SubStateStages {.start = true, .turn = true, .end = false},   // TurnToFacePos
    SubStateStages {.start = true, .turn = false, .end = true},   // MoveToPos
    SubStateStages {.start = true, .turn = true, .end = false},   // RunAway
    SubStateStages {.start = true, .turn = true, .end = false},   // IndividualAction
    SubStateStages {.start = false, .turn = true, .end = false},  // ChooseObjectToHurl
    SubStateStages {.start = false, .turn = true, .end = false},  // AddVillagersToDance
    SubStateStages {.start = false, .turn = true, .end = false},  // FacialAnimation
    SubStateStages {.start = true, .turn = true, .end = false},   // Faint
    SubStateStages {.start = false, .turn = false, .end = true},  // Poo
    SubStateStages {.start = true, .turn = true, .end = false},   // MoveToThrowPos
    SubStateStages {.start = true, .turn = true, .end = false},   // Stomp
    SubStateStages {.start = false, .turn = true, .end = false},  // LookAtHand
    SubStateStages {.start = false, .turn = false, .end = true},  // RenderImmobile
    SubStateStages {.start = false, .turn = false, .end = true},  // AddToShape
    SubStateStages {.start = false, .turn = true, .end = false},  // MoveToDropPos
    SubStateStages {.start = true, .turn = true, .end = false},   // TurnToFaceAwayFromObject
    SubStateStages {.start = true, .turn = true, .end = true},    // Dance
    SubStateStages {.start = false, .turn = true, .end = false},  // Wait
    SubStateStages {.start = false, .turn = true, .end = false},  // WaitForDancersToStopMoving
    SubStateStages {.start = false, .turn = true, .end = false},  // WaitForDancersToStartDancing
    SubStateStages {.start = false, .turn = true, .end = false},  // Heal
    SubStateStages {.start = true, .turn = true, .end = true},    // Fight
    SubStateStages {.start = true, .turn = true, .end = false},   // StopMoving
    SubStateStages {.start = true, .turn = true, .end = false},   // FollowCamera
    SubStateStages {.start = true, .turn = true, .end = false},   // TurnToFaceCamera
    SubStateStages {.start = true, .turn = true, .end = false},   // CommunicateToPlayer
    SubStateStages {.start = false, .turn = true, .end = false},  // ReduceDesire
    SubStateStages {.start = false, .turn = true, .end = false},  // Loop
    SubStateStages {.start = false, .turn = false, .end = false}, // WalkAlongCoast
    SubStateStages {.start = false, .turn = true, .end = false},  // CheckValue
    SubStateStages {.start = false, .turn = true, .end = false},  // Repair
    SubStateStages {.start = false, .turn = true, .end = false},  // DestroyObject
    SubStateStages {.start = true, .turn = true, .end = true},    // CastSpellAtObject
    SubStateStages {.start = true, .turn = true, .end = true},    // CastSpellAtPos
    SubStateStages {.start = false, .turn = true, .end = false},  // PutOutFire
    SubStateStages {.start = true, .turn = true, .end = false},   // DancingDuringEditor
    SubStateStages {.start = true, .turn = true, .end = true},    // CastSpotVisual
    SubStateStages {.start = false, .turn = true, .end = false},  // StartTheDance
    SubStateStages {.start = false, .turn = true, .end = false},  // TellVillagersToGoToPos
    SubStateStages {.start = false, .turn = true, .end = false},  // TellVillagersToDance
    SubStateStages {.start = false, .turn = true, .end = false},  // StartLooking
    SubStateStages {.start = false, .turn = true, .end = false},  // StopLooking
    SubStateStages {.start = true, .turn = true, .end = false},   // RaiseTotem
    SubStateStages {.start = true, .turn = true, .end = false},   // LowerTotem
    SubStateStages {.start = false, .turn = true, .end = false},  // Drink
    SubStateStages {.start = false, .turn = true, .end = false},  // HealHimself
    SubStateStages {.start = true, .turn = true, .end = false},   // InspectCreature
    SubStateStages {.start = false, .turn = true, .end = false},  // CreateHandObjectFromObject
    SubStateStages {.start = true, .turn = true, .end = false},   // PickupCreatedObject
    SubStateStages {.start = true, .turn = true, .end = true},    // ThrowAtPos
    SubStateStages {.start = false, .turn = true, .end = false},  // AddReactionToObject
    SubStateStages {.start = false, .turn = true, .end = false},  // RequestCreatureDoSomethingWithObject
    SubStateStages {.start = true, .turn = true, .end = false},   // RunAwayFromPlayer
    SubStateStages {.start = false, .turn = true, .end = false},  // StartFire
    SubStateStages {.start = true, .turn = true, .end = false},   // GoNearObject
    SubStateStages {.start = false, .turn = true, .end = false},  // ObserveObject
    SubStateStages {.start = false, .turn = true, .end = true},   // FollowInterestingObject
    SubStateStages {.start = false, .turn = true, .end = false},  // InScript
    SubStateStages {.start = false, .turn = true, .end = false},  // LookAtFlyingObject
    SubStateStages {.start = false, .turn = true, .end = false},  // LookAtCamera
    SubStateStages {.start = true, .turn = true, .end = false},   // FollowHand
    SubStateStages {.start = true, .turn = true, .end = false},   // FaceDownSlope
    SubStateStages {.start = false, .turn = true, .end = false},  // AddToHome
    SubStateStages {.start = false, .turn = true, .end = false},  // WaitForObject
    SubStateStages {.start = true, .turn = true, .end = false},   // SeeWhoWinsGame
    SubStateStages {.start = true, .turn = true, .end = false},   // RespondAfterFight
    SubStateStages {.start = false, .turn = true, .end = false},  // CreateFootPrint
    SubStateStages {.start = false, .turn = true, .end = false},  // OrderCreature
    SubStateStages {.start = false, .turn = true, .end = false},  // WaitForCreatureToPickUp
    SubStateStages {.start = false, .turn = true, .end = false},  // DemobiliseObject
    SubStateStages {.start = false, .turn = true, .end = false},  // RemobiliseObject
    SubStateStages {.start = false, .turn = true, .end = false},  // WaitForever
    SubStateStages {.start = false, .turn = true, .end = true},   // Kiss
    SubStateStages {.start = true, .turn = true, .end = true},    // PointAtPoint
    SubStateStages {.start = true, .turn = true, .end = true},    // PointAtObject
    SubStateStages {.start = false, .turn = true, .end = false},  // DoTeleport
    SubStateStages {.start = false, .turn = true, .end = false},  // StopReacting
    SubStateStages {.start = false, .turn = true, .end = false},  // SetSpeed
    SubStateStages {.start = true, .turn = true, .end = false},   // TurnToFaceHand
    SubStateStages {.start = true, .turn = true, .end = false},   // PickUpFromHand
    SubStateStages {.start = true, .turn = true, .end = false},   // TurnToFaceOtherPlayersCamera
    SubStateStages {.start = true, .turn = true, .end = true},    // Gesture
    SubStateStages {.start = false, .turn = true, .end = false},  // RequestPartner
    SubStateStages {.start = true, .turn = true, .end = false},   // WaitForCreatureToBeFree
    SubStateStages {.start = false, .turn = true, .end = false},  // ActOnObjectInHand
    SubStateStages {.start = false, .turn = true, .end = false},  // CreateFishFromSea
    SubStateStages {.start = true, .turn = true, .end = true},    // TransportedByTeleport
    SubStateStages {.start = true, .turn = true, .end = false},   // PracticeDance
    SubStateStages {.start = true, .turn = true, .end = true},    // ThrowInPile
    SubStateStages {.start = false, .turn = true, .end = false},  // LoopAction
    SubStateStages {.start = false, .turn = true, .end = false},  // ConsiderActionCompleted
    SubStateStages {.start = false, .turn = true, .end = false},  // CreateArena
    SubStateStages {.start = true, .turn = false, .end = true},   // MoveToArena
    SubStateStages {.start = false, .turn = true, .end = false},  // TellVillagersToEnterState
    SubStateStages {.start = false, .turn = true, .end = false},  // Resurrect
    SubStateStages {.start = true, .turn = true, .end = true},    // SwapMindWithOtherCreature
    SubStateStages {.start = false, .turn = true, .end = false},  // Fail
    SubStateStages {.start = false, .turn = true, .end = false},  // WaitForObjectToBeInMap
    SubStateStages {.start = false, .turn = true, .end = false},  // SetPos
    SubStateStages {.start = false, .turn = true, .end = false},  // HideObjectInHand
    SubStateStages {.start = false, .turn = true, .end = false},  // DestroyObjectToUse
    SubStateStages {.start = false, .turn = true, .end = false},  // ClearObjectToActOn
    SubStateStages {.start = false, .turn = true, .end = false},  // ClearObjectToUse
    SubStateStages {.start = false, .turn = true, .end = false},  // Catch
    SubStateStages {.start = false, .turn = true, .end = false},  // KickObject
    SubStateStages {.start = true, .turn = true, .end = true},    // TransportHome
    SubStateStages {.start = false, .turn = true, .end = false},  // RemoveAndCreateSpellFromTotemStatue
    SubStateStages {.start = false, .turn = true, .end = false},  // AddAndDestroySpellFromTotemStatue
    SubStateStages {.start = true, .turn = true, .end = false},   // MoveToThrowAtPos
    SubStateStages {.start = false, .turn = true, .end = false},  // Lightbulb
    SubStateStages {.start = false, .turn = true, .end = false},  // ShatterRock
    SubStateStages {.start = true, .turn = true, .end = false},   // PickUpObjectCreatedByCreature
    SubStateStages {.start = false, .turn = true, .end = false},  // DestroyCreatedObject
    SubStateStages {.start = false, .turn = true, .end = false},  // CreatedHandObjectExNihilo
    SubStateStages {.start = false, .turn = true, .end = false},  // ApplyDesireToObject
    SubStateStages {.start = true, .turn = false, .end = true},   // MoveToWater
    SubStateStages {.start = false, .turn = true, .end = false},  // StopControlledByTeleport
    SubStateStages {.start = false, .turn = true, .end = false},  // WatchPlayerWhileHeHasYourAttention
    SubStateStages {.start = false, .turn = true, .end = false},  // MakeDisciple
    SubStateStages {.start = true, .turn = true, .end = false},   // SetDance
    SubStateStages {.start = false, .turn = true, .end = true},   // SingleKiss
    SubStateStages {.start = true, .turn = true, .end = true},    // EatCreatedObject
    SubStateStages {.start = true, .turn = true, .end = true},    // PointAtArena
    SubStateStages {.start = true, .turn = true, .end = true},    // RestToGetBetter
    SubStateStages {.start = false, .turn = true, .end = false},  // TellVillagersToGoToStartDancePos
    SubStateStages {.start = true, .turn = true, .end = false},   // Sleep
    SubStateStages {.start = false, .turn = true, .end = false},  // WaitUntilFree
    SubStateStages {.start = false, .turn = true, .end = false},  // StopBeingOnFire
    SubStateStages {.start = true, .turn = true, .end = false},   // GetAwayFromObject
    SubStateStages {.start = false, .turn = true, .end = false},  // LookAtCameraInWideScreen
    SubStateStages {.start = true, .turn = true, .end = false},   // Talk
    SubStateStages {.start = true, .turn = true, .end = true},    // CreatePickUpThenRemove
    SubStateStages {.start = false, .turn = true, .end = false},  // WaitForSpellsToWearOff
    SubStateStages {.start = false, .turn = true, .end = false},  // CreateMagicFood
    SubStateStages {.start = false, .turn = true, .end = false},  // Build
    SubStateStages {.start = false, .turn = true, .end = false},  // EatNearestFoodPile
};

[[nodiscard]] constexpr SubStateStages StagesOf(SubState subState)
{
	return k_SubStateStages.at(static_cast<size_t>(subState));
}

/// Whether the look of a step stays for one more turn after the step ends. The head takes its look from the step that
/// the agenda is at, before the step of the turn runs. When the last stage of a kind has no code, the agenda stays at
/// the step until the next turn goes past the empty stage. A kind with no code in all its stages takes no turn, so the
/// head never takes its look.
[[nodiscard]] constexpr bool LookHoldsAfterEnd(SubState subState)
{
	const auto stages = StagesOf(subState);
	return !stages.end && (stages.start || stages.turn);
}
} // namespace openblack::creature_sub_states
