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

#include <functional>
#include <optional>

#include "3D/AllMeshes.h"
#include "Enums.h"

/// Which clip a villager plays in each of its states, the clips that take it into and out of some states, and what it
/// carries. A state's clip is chosen as the villager goes into it, not every frame: the random choices are made then,
/// on the game's shared stream, and walking or running is decided by the speed it was given. Pure, tested on made-up
/// villagers.
namespace openblack::ecs::villager_animation
{

/// Draws a whole number below a limit on the game's shared stream
using IntRandom = std::function<uint32_t(uint32_t)>;

/// A table clip that hides the villager: it is in a building or out of sight
constexpr int32_t k_HiddenClip = -4;

/// How a state chooses its clip when its table's clip is not enough
enum class ClipChoice : uint8_t
{
	Table,
	Walk,
	Thrown,
	Landed,
	Dying,
	Dead,
	Kissing,
	Forestering,
	Yawn,
	PauseForASecond,
	LookAtLargeObject,
	LookAtFlyingObject,
	PointAtFlyingObject,
	InspectCreature,
	RespectCreature,
	ControlledByCreature,
	AmazedByShield,
	TownEmergency,
	RandomCrowd,
	SitDown,
	Building,
	WatchFight,
	FootballWaitForKickOff,
	FootballMatchPaused,
	FootballGoalKeeper,
	FootballOutfield,
	FootballWatchMatch,
	Dance,
	Script,
};

/// The clips that take a villager into a state and out of it again
enum class Transition : uint8_t
{
	None,
	SleepOnTheGround,
	Mourn,
	Pray,
	ArrivesAtResource,
	Walk,
	SitDown,
	Building,
};

[[nodiscard]] ClipChoice ClipChoiceOf(VillagerStates state);
/// The states of dying: being set to die, dying, dead, drowning and brought down
[[nodiscard]] constexpr bool IsDeathState(VillagerStates state)
{
	return state > VillagerStates::LookAtFlyingObjectReaction && state < VillagerStates::BeingEaten;
}
[[nodiscard]] Transition TransitionOf(VillagerStates state);

/// What the walking clips depend on
struct WalkInputs
{
	CarriedObject carried {CarriedObject::None};
	/// A villager a script moves walks however it is hurt
	bool scriptControlled {false};
	float life {1.0f};
	float lifeWhenCrawlsWounded {0.15f};
	float lifeWhenWalksWounded {0.3f};
	bool female {false};
	/// Its speed and the fastest it walks and runs, all in the same units
	float speed {0.0f};
	float walkMax {0.0f};
	float runMax {0.0f};
};
/// The badly hurt crawl and the hurt limp; the rest walk, run or sprint by their speed, men and women in their own way,
/// and anyone carrying something big carries it walking or running
[[nodiscard]] AnimId WalkClip(const WalkInputs& in);

/// Whether a walking villager carries its load in its arms
[[nodiscard]] bool CarriesInArms(CarriedObject carried);

/// The fall of a dying villager: it sinks on water, falls back when it landed on its back, else drops
[[nodiscard]] AnimId DyingClip(bool onWater, bool onBack);
[[nodiscard]] AnimId DeadClip(bool onWater, bool onBack);
[[nodiscard]] AnimId KissingClip(bool female);
/// Yawns one of two ways
[[nodiscard]] AnimId YawnClip(const IntRandom& random);
/// A poisoned villager shows it, without a draw; the rest look worn out one of two ways
[[nodiscard]] AnimId PauseForASecondClip(bool poisoned, const IntRandom& random);
/// Looks at the hand or about it, by the two halves of the turns it has left
[[nodiscard]] AnimId LookAtLargeObjectClip(uint16_t turnsUntilStateChange);
/// Stays hidden when it was hidden before; looks up at a thing high in the air, else stands
[[nodiscard]] int32_t LookAtFlyingObjectClip(int32_t previousTableClip, float heightAboveLand);
/// Women and children are scared stiff one time in three; near the creature, one in three looks at it; else they talk
/// and point or stand
[[nodiscard]] AnimId InspectCreatureClip(bool womanOrChild, std::optional<float> distanceToTarget, const IntRandom& random);
[[nodiscard]] AnimId RespectCreatureClip(const IntRandom& random);
[[nodiscard]] AnimId ControlledByCreatureClip(const IntRandom& random);
[[nodiscard]] AnimId AmazedByShieldClip(const IntRandom& random);
[[nodiscard]] AnimId TownEmergencyClip(bool female, const IntRandom& random);
[[nodiscard]] AnimId RandomCrowdClip(const IntRandom& random);

/// The clip a sitting villager sits in, matched to the way it sat down when that clip is playing, else one of two ways
[[nodiscard]] AnimId SitDownClip(bool transitionPlaying, AnimId current, const IntRandom& random);

/// A builder hammers, saws or swings a mallet, carrying the tool of it
struct BuildingClipChoice
{
	AnimId clip;
	CarriedObject tool;
};
[[nodiscard]] BuildingClipChoice BuildingClip(bool transitionPlaying, AnimId current, const IntRandom& random);

[[nodiscard]] AnimId FootballWaitForKickOffClip(const IntRandom& random);
[[nodiscard]] AnimId FootballMatchPausedClip(const IntRandom& random);
/// The crowd at a match cheers a goal, chats while it goes on, or is unimpressed in between; with no match they gossip
[[nodiscard]] AnimId FootballWatchMatchClip(std::optional<uint32_t> matchState, const IntRandom& random);

/// The clip that takes a villager into a state (or out of it), if the state has one: by the state's transition and the
/// clip it plays now
[[nodiscard]] std::optional<AnimId> TransitionClip(Transition transition, bool into, bool destinationIsCurrent, bool deathState,
                                                   AnimId current);

/// What a villager carries in its hands
struct CarryInputs
{
	/// Its state is played by a script, which keeps what it has
	bool scriptHeld {false};
	float life {1.0f};
	float lifeWhenCrawlsWounded {0.15f};
	uint32_t wood {0};
	uint32_t food {0};
	uint32_t minWoodToShowGraphic {0};
	uint32_t minFoodToShowGraphic {0};
	/// The kind of log its wood came from, 0 for plain wood
	uint8_t woodKind {0};
	/// Its final state is building, where food isn't shown
	bool building {false};
	/// The states' own choices, 0 for none
	int32_t finalStateCarries {0};
	int32_t topStateCarries {0};
};
/// None when it keeps what it has
[[nodiscard]] std::optional<CarriedObject> CarriedObjectOf(const CarryInputs& in);
/// The log it shows for its wood, by the kind of tree it came from
[[nodiscard]] CarriedObject WoodCarriedObject(uint8_t woodKind);

} // namespace openblack::ecs::villager_animation
