/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include <cmath>

#include <algorithm>
#include <iterator>

#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>

#include "3D/LandIslandInterface.h"
#include "Common/GameRandom.h"
#include "ECS/AnimalAIDetail.h"
#include "ECS/AnimalAnimations.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/AnimalBrain.h"
#include "ECS/Components/Flock.h"
#include "ECS/Components/Transform.h"
#include "ECS/Physics/PhysicsObjects.h"
#include "ECS/Registry.h"
#include "ECS/VillagerAnimations.h"
#include "InfoConstants.h"
#include "Locator.h"

/// The flying animals like the original (docs/bw1-notes/animals.md): crows, doves, swallows, pigeons, seagulls and bats
/// are all the Dove class. The leader flies legs of up to 80 m from where it is, climbing or sinking a little each turn;
/// the others keep near it in a formation. With info.sleep 0 for every bird they never land in the shipped game
/// (LAND_AT_POS / SLEEPS are dead data).
namespace openblack::ecs::animal_ai::detail
{
using components::Animal;
using components::AnimalBrain;
using components::Flock;
using components::Transform;

namespace
{
float Height(glm::vec2 p)
{
	return Locator::terrainSystem::value().GetHeightAt(p);
}

/// The flock's altitude, else the info's altitudeNormal
float FlockAltitude(const Context& ctx)
{
	const auto* flock = FlockOf(ctx.animal);
	return flock != nullptr && flock->altitude != 0.0f ? flock->altitude : ctx.info.altitudeNormal;
}

/// SetupMoveToPos with the goal's altitude
void SetupMoveTo(Context& ctx, glm::vec2 p, float altitude, AnimalState final)
{
	ctx.brain.goalAltitude = altitude;
	SetupMoveToPos(ctx, p, final);
}

/// Sets the top state only, the clip stays
void SetStateRaw(Context& ctx, AnimalState state)
{
	ctx.brain.topState = static_cast<uint8_t>(state);
	ctx.brain.turnsSinceStateChange = 0;
}

/// The horizontal step (MoveTo, turning towards the goal) and the altitude, relative to the land for a low goal, else in
/// absolute height; never under 2 m in flight. True when it got there.
bool MoveTo3D(Context& ctx)
{
	const float change = ctx.info.altitudeMovementChange;
	const glm::vec2 me = Xz(ctx.transform);
	const glm::vec2 goal = ctx.brain.goal;
	const float destination = ctx.brain.goalAltitude;
	const float before = Height(me) + ctx.brain.altitude;
	if (destination < 2.0f)
	{
		if (ctx.brain.altitude > destination + change)
		{
			ctx.brain.altitude -= change;
		}
		else if (ctx.brain.altitude < destination - change)
		{
			ctx.brain.altitude += change;
		}
	}
	// MoveTo, and FINAL_STEP counts as arrived at once (no snap)
	int r = MoveTo(ctx);
	if (ctx.brain.moveState == k_MoveFinalStep)
	{
		r = 0xA;
	}
	const bool arrived = r == 0xA;
	const glm::vec2 now = Xz(ctx.transform);
	if (destination >= 2.0f)
	{
		const float target = Height(goal) + destination;
		float absolute = before;
		if (target + change < before)
		{
			absolute = before - change;
		}
		else if (target - change > before)
		{
			absolute = before + change;
		}
		ctx.brain.altitude = std::max(absolute - Height(now), 2.0f);
	}
	ctx.transform.position.y = Height(now) + ctx.brain.altitude;
	return arrived;
}

/// Its index from the list's head: the newest member is 0, the leader (the tail, the first added) the last
int IndexInFlock(const Flock& flock, entt::entity entity)
{
	const auto found = std::find(flock.members.begin(), flock.members.end(), entity);
	if (found == flock.members.end())
	{
		return 0;
	}
	return static_cast<int>(flock.members.size()) - 1 - static_cast<int>(std::distance(flock.members.begin(), found));
}

/// The formation. On reaching the last goal, a slot by its place in the member list, turned to face the leader (the
/// axes as the code pairs them [read literally])
bool Formation(Context& ctx, const Flock& flock)
{
	const bool arrived = MoveTo3D(ctx);
	if (!arrived)
	{
		return false;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto leader = LeaderOf(flock);
	if (leader == entt::null)
	{
		return true;
	}
	int k = static_cast<int>(flock.members.size()) - IndexInFlock(flock, ctx.entity);
	int b = 1;
	if (k >= 1)
	{
		do
		{
			++b;
		} while (k >= b * b);
	}
	int s = 1;
	if (k % 2 == 0)
	{
		k -= 1;
		s = -1;
	}
	const int column = ((b * b - k) * s + 1) / 2;
	const int row = b - 5;
	const auto at = map_coords::FromMetres(Xz(registry.Get<const Transform>(leader)));
	// ConvertGameAngleTo3D(GetAngleFromDXDZ(row, column) + GetAngleFromXZ(me, leader) + 0x400), the mask the
	// conversion's own
	const float radians = gutils::ConvertGameAngleTo3D(
	    gutils::GetAngleFromDXDZ(row, column) + gutils::GetAngleFromXZ(map_coords::FromMetres(Xz(ctx.transform)), at) + 0x400);
	// Not AddDistanceFromAngle (row on x, column on z, and the integer multiply then x 10: two roundings):
	// x = ((cos(a) row) 10 + leader.x x 10 x 2^-16) x 65536 / 10 truncated toward zero, z the same with sin and column. The
	// cosine is extended precision and the multiply rounds it once: the cosine in double
	const auto axis = [](double trig, int32_t count, int32_t centre) {
		const auto scaled = static_cast<float>(trig * static_cast<double>(count)); // rounded to a float once
		const float metres = scaled * 10.0f;
		const float sum = metres + map_coords::ToMetres(centre); // the leader's coordinate in metres added
		return map_coords::ToFixedGUtils(sum);                   // back to map coordinates, truncated
	};
	const map_coords::MapCoords coords {axis(std::cos(static_cast<double>(radians)), row, at.x),
	                                    axis(std::sin(static_cast<double>(radians)), column, at.z), 0.0f};
	const glm::vec2 goal = map_coords::ToMetres(coords);
	const auto* leaderBrain = registry.TryGet<const AnimalBrain>(leader);
	SetupMoveTo(ctx, goal, leaderBrain != nullptr ? leaderBrain->altitude : ctx.brain.altitude, AnimalState::DecideWhatToDo);
	return true;
}
} // namespace

bool IsBird(AnimalInfo type)
{
	switch (type)
	{
	case AnimalInfo::Crow:
	case AnimalInfo::Dove:
	case AnimalInfo::Swallow:
	case AnimalInfo::Pigeon:
	case AnimalInfo::Seagull:
	case AnimalInfo::Bat:
	case AnimalInfo::SpellDove:
	case AnimalInfo::SpellBat:
		return true;
	default:
		return false;
	}
}

float TimeToBank(AnimalInfo type)
{
	if (type == AnimalInfo::SpellDove || type == AnimalInfo::SpellBat)
	{
		return 0.5f;
	}
	return IsBird(type) ? 2.0f : 0.0f;
}

float BankAngle(AnimalInfo type)
{
	return IsBird(type) ? 0.5f : 0.0f;
}

void BirdDecideWhatToDo(Context& ctx)
{
	auto* flock = FlockOf(ctx.animal);
	if (flock == nullptr)
	{
		return;
	}
	if (IsLeader(ctx))
	{
		if (BirdReactToAnimalNeeds(ctx) == k_Started)
		{
			return;
		}
		SetTopState(ctx, AnimalState::StartWander);
		BirdStartWander(ctx);
		return;
	}
	// a point within flockDistance of the leader, at the leader's altitude, then the flock's follow state
	auto& registry = Locator::entitiesRegistry::value();
	const auto leader = LeaderOf(*flock);
	const auto* leaderBrain = leader != entt::null ? registry.TryGet<const AnimalBrain>(leader) : nullptr;
	const auto p = CalcRandomPos(ctx, FlockPos(ctx), 0.0f, static_cast<float>(flock->flockDistance));
	SetupMoveTo(ctx, p, leaderBrain != nullptr ? leaderBrain->altitude : ctx.brain.altitude, AnimalState::DecideWhatToDo);
	SetSpeed(ctx, SpeedDefault(ctx));
	// before the leader's first leg the flock's state is still DECIDE (the flock constructors set it to 0x2B)
	SetStateRaw(ctx, static_cast<AnimalState>(flock->followState));
}

void BirdStartWander(Context& ctx)
{
	// The leader's next leg, up to domainRadius from where it is
	auto* flock = FlockOf(ctx.animal);
	if (flock == nullptr)
	{
		return;
	}
	SetSpeed(ctx, SpeedDefault(ctx));
	const auto p = CalcRandomPos(ctx, FlockPos(ctx), static_cast<float>(ctx.info.domainInnerRadius),
	                             static_cast<float>(flock->domainRadius));
	const float variance = ctx.info.altitudeVariance;
	// (altitude + variance) - GameFloatRand(2 variance)
	const float high = ctx.brain.altitude + variance;
	const float range = 2.0f * variance;
	const float altitude = high - game_random::GameFloatRand(range);
	const float base = FlockAltitude(ctx);
	const float goalAltitude =
	    base + ctx.info.altitudeMin < altitude && altitude < base + ctx.info.altitudeMax ? altitude : base;
	SetupMoveTo(ctx, p, goalAltitude, AnimalState::DecideWhatToDo);
	SetStateRaw(ctx, AnimalState::SpecialMoveToPos);
	flock->followState = static_cast<uint8_t>(AnimalState::FollowFlock);
	flock->followMode = 3;
	flock->afterMove = static_cast<uint8_t>(AnimalState::DecideWhatToDo);
	SpecialMoveToPos(ctx);
}

int BirdReactToAnimalNeeds(Context& ctx)
{
	// No hunger, no breeding; info.sleep is 0 for every bird, so no landing
	auto* flock = FlockOf(ctx.animal);
	if (flock != nullptr && flock->leaderTurns >= ctx.info.stayTime)
	{
		flock->leaderTurns = 0;
		SetTopState(ctx, AnimalState::StartWander);
		return k_Started;
	}
	return k_Nothing;
}

void SpecialMoveToPos(Context& ctx)
{
	// The leader re-steers every stayTime turns; a new leg on arrival
	const auto* flock = FlockOf(ctx.animal);
	if (flock != nullptr && IsLeader(ctx) && flock->followState == static_cast<uint8_t>(AnimalState::FollowFlock) &&
	    BirdReactToAnimalNeeds(ctx) == k_Started)
	{
		return;
	}
	if (MoveTo3D(ctx))
	{
		// The top state becomes the final one, then the flock's after-move state
		SetTopState(ctx, static_cast<AnimalState>(ctx.brain.finalState));
		if (flock != nullptr && flock->afterMove != 0)
		{
			SetStateRaw(ctx, static_cast<AnimalState>(flock->afterMove));
		}
	}
}

void FollowFlock(Context& ctx)
{
	if (IsLeader(ctx))
	{
		BirdDecideWhatToDo(ctx);
		return;
	}
	const auto* flock = FlockOf(ctx.animal);
	if (flock == nullptr || ctx.brain.topState != flock->followState)
	{
		return;
	}
	if (flock->followMode == 3)
	{
		Formation(ctx, *flock);
	}
	else
	{
		// mode 2 (the next member) is not set by the shipped birds; any other mode: nothing
		return;
	}
	// Once the clip has played, flap or glide again, every turn until the state changes (the counter is not
	// restarted)
	if (ctx.brain.topState == flock->followState && VillagerAnimationDone(ctx.entity, ctx.brain.turnsSinceStateChange))
	{
		SetAnimalStateAnim(ctx.entity);
	}
}

void BirdDying(Context& ctx)
{
	// Into the physics with its flight velocity along its heading: s = its heading as a Scawen angle, then
	// (sin(s) v, 0, -(cos(s) v)) (the sine and cosine extended, rounded once by the multiply: in double here)
	const double scawen = gutils::ConvertGameAngleToScawenAngle(ctx.brain.angle);
	const float speed = Metres(ctx.brain.speed) * 10.0f;
	const auto x = static_cast<float>(std::sin(scawen) * static_cast<double>(speed));
	const auto z = static_cast<float>(std::cos(scawen) * static_cast<double>(speed));
	const glm::vec3 velocity(x, 0.0f, -z);
	// The spin (5, 0, 0) is about the BODY's x axis: the original adds (w I) through the matrix rows and integrates the
	// other way to openblack's PhysicsBody, so here it is -(R (5, 0, 0))
	const glm::vec3 spin = -(ctx.transform.rotation * glm::vec3(5.0f, 0.0f, 0.0f));
	if (physics::PhysicsObjects::AddObject(ctx.entity, velocity, spin) == nullptr)
	{
		PlayAnimThenSetState(ctx, AnimalState::Dead);
	}
}

} // namespace openblack::ecs::animal_ai::detail
