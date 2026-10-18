/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

// The values components hold whose data is private, which tools/inspector/generate_component_fields.py can't register:
// each is reached through its own functions, read only where it has no function to set it. The generator registers the
// types these give (its BY_HAND table). The saved world keeps the whole bytes of each value that is plain data.

#include <vector>

#include <LHVMTypes.h>

#include "3D/CameraTrack.h"
#include "3D/DayNightClock.h"
#include "3D/SkyDome.h"
#include "Animals/Zoomer.h"
#include "Common/Zoomer.h"
#include "ComponentReflection.h"
#include "Creature/CreatureFight.h"
#include "Creature/CreatureObjectBeliefs.h"
#include "Creature/CreatureRoute.h"
#include "Creature/CreatureRouteFollow.h"
#include "ECS/Components/WallHug.h"
#include "ECS/StatisticsRules.h"
#include "Enums.h"
#include "Hand/HandGrabRules.h"
#include "Magic/VillagerReactionRules.h"

namespace
{

namespace fight = openblack::creature_fight;

/// A fighter's queue of moves: the moves in order, each with its charge (none for a blow waiting to be let go)
std::vector<fight::QueuedMove> QueuedMoves(const fight::MoveQueue& queue)
{
	const auto moves = queue.Moves();
	return {moves.begin(), moves.end()};
}

void SetQueuedMoves(fight::MoveQueue& queue, const std::vector<fight::QueuedMove>& moves)
{
	queue.Assign(moves);
}

std::vector<fight::marks::Mark> FightMarks(const fight::marks::MarkList& list)
{
	const auto marks = list.Marks();
	return {marks.begin(), marks.end()};
}

namespace components = openblack::ecs::components;

/// A step of a villager's walk round an obstacle, one component for each state of the step
template <typename Tag>
void ReflectMoveState(entt::meta_ctx& context)
{
	openblack::ecs::reflection::Reflect<Tag>(context)
	    .template Field<&Tag::clockwise>("clockwise")
	    .template Field<&Tag::stepGoal>("stepGoal");
}

/// The highest and lowest of a figure of a player's statistics
template <typename Type>
void ReflectExtremes(entt::meta_ctx& context)
{
	using Extremes = openblack::statistics::Extremes<Type>;
	openblack::ecs::reflection::Reflect<Extremes>(context, openblack::ecs::reflection::ValueOnly {})
	    .template Field<&Extremes::highest>("highest")
	    .template Field<&Extremes::lowest>("lowest");
}

/// A smooth change's value, set as one at rest there
void ResetZoomer(openblack::Zoomer& zoomer, float value)
{
	zoomer.Reset(value);
}

} // namespace

void openblack::ecs::reflection::RegisterHandWrittenFields(entt::meta_ctx& context)
{
	Reflect<fight::MoveQueue>(context, ValueOnly {})
	    .Property<&QueuedMoves, &SetQueuedMoves>("moves")
	    .Property<&fight::MoveQueue::HasWaiting, nullptr>("waiting")
	    .Codec<&SaveBytes<fight::MoveQueue>, &LoadBytes<fight::MoveQueue>>();
	Reflect<fight::marks::MarkList>(context, ValueOnly {})
	    .Property<&FightMarks, nullptr>("marks")
	    .Codec<&SaveBytes<fight::marks::MarkList>, &LoadBytes<fight::marks::MarkList>>();
	Reflect<DayNightClock>(context, ValueOnly {})
	    .Property<&DayNightClock::IsRunning, &DayNightClock::SetRunning>("running")
	    .Property<&DayNightClock::GetScriptTime, &DayNightClock::SetScriptTime>("scriptTime")
	    .Property<&DayNightClock::GetVisualTime, nullptr>("visualTime")
	    .Property<&DayNightClock::GetVisualTimes, nullptr>("visualTimes")
	    .Codec<&SaveBytes<DayNightClock>, &LoadBytes<DayNightClock>>();
	Reflect<Zoomer>(context, ValueOnly {})
	    .Property<&Zoomer::GetValue, &ResetZoomer>("value")
	    .Property<&Zoomer::GetSpeed, nullptr>("speed")
	    .Property<&Zoomer::GetDestination, nullptr>("destination")
	    .Codec<&SaveBytes<Zoomer>, &LoadBytes<Zoomer>>();
	Reflect<animals::Zoomer>(context, ValueOnly {})
	    .Property<&animals::Zoomer::Value, nullptr>("value")
	    .Property<&animals::Zoomer::Speed, nullptr>("speed")
	    .Property<&animals::Zoomer::Target, nullptr>("target")
	    .Property<&animals::Zoomer::Done, nullptr>("done")
	    .Codec<&SaveBytes<animals::Zoomer>, &LoadBytes<animals::Zoomer>>();
	Reflect<hand_grab::HandSpring>(context, ValueOnly {})
	    .Property<&hand_grab::HandSpring::Position, nullptr>("position")
	    .Property<&hand_grab::HandSpring::Velocity, nullptr>("velocity")
	    .Property<&hand_grab::HandSpring::StepsTaken, nullptr>("stepsTaken")
	    .Property<&hand_grab::HandSpring::CountedMs, nullptr>("countedMs")
	    .Property<&hand_grab::HandSpring::SteppedMs, nullptr>("steppedMs")
	    .Codec<&SaveBytes<hand_grab::HandSpring>, &LoadBytes<hand_grab::HandSpring>>();
	Reflect<sky_dome::Follow>(context, ValueOnly {})
	    .Property<&sky_dome::Follow::Following, nullptr>("following")
	    .Property<&sky_dome::Follow::RowsDone, nullptr>("rowsDone")
	    .Codec<&SaveBytes<sky_dome::Follow>, &LoadBytes<sky_dome::Follow>>();
	// A walk along a camera track keeps where it is on the track: the saved world keeps it whole
	Reflect<camera_track::WayRunner>(context, ValueOnly {})
	    .Codec<&SaveBytes<camera_track::WayRunner>, &LoadBytes<camera_track::WayRunner>>();
	// A town's tribe is a component of its own, which is only the tribe's number
	Reflect<Tribe>(context).Codec<&SaveBytes<Tribe>, &LoadBytes<Tribe>>();
	ReflectMoveState<components::MoveStateLinearTag>(context);
	ReflectMoveState<components::MoveStateOrbitTag>(context);
	ReflectMoveState<components::MoveStateExitCircleTag>(context);
	ReflectMoveState<components::MoveStateStepThroughTag>(context);
	ReflectMoveState<components::MoveStateFinalStepTag>(context);
	ReflectMoveState<components::MoveStateArrivedTag>(context);
	ReflectExtremes<uint32_t>(context);
	ReflectExtremes<float>(context);
	// A value that a script gave keeps its bits, whatever type the script gave it as
	Reflect<lhvm::VMValue>(context, ValueOnly {}).Codec<&SaveBytes<lhvm::VMValue>, &LoadBytes<lhvm::VMValue>>();
	namespace reaction = magic::villager_reaction;
	Reflect<reaction::Memory::Entry>(context, ValueOnly {})
	    .Field<&reaction::Memory::Entry::type>("type")
	    .Field<&reaction::Memory::Entry::turn>("turn");
	Reflect<reaction::Memory>(context, ValueOnly {})
	    .Property<&reaction::Memory::Records, &reaction::Memory::SetRecords>("records");
	Reflect<creature_object_beliefs::Store>(context, ValueOnly {})
	    .Property<&creature_object_beliefs::Store::Contents, &creature_object_beliefs::Store::SetContents>("contents");
	Reflect<creature_route::Follower>(context, ValueOnly {})
	    .Property<&creature_route::Follower::GetState, nullptr>("state")
	    .Property<&creature_route::Follower::GetRoute, nullptr>("route")
	    .Property<&creature_route::Follower::GetPosition, nullptr>("position")
	    .Property<&creature_route::Follower::GetSearchCount, nullptr>("searches");
}
