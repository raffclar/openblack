/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "AnimalAI.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

#include <algorithm>
#include <array>
#include <limits>
#include <vector>

#include <LNDFile.h>
#include <glm/gtc/constants.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <spdlog/spdlog.h>

#include "3D/LandIslandInterface.h"
#include "3D/ObjectMatrix.h"
#include "Common/GUtilsDistance.h"
#include "Common/GameRandom.h"
#include "Debug/DebugEnv.h"
#include "ECS/AnimalAIDetail.h"
#include "ECS/AnimalAnimations.h"
#include "ECS/AnimalWallHug.h"
#include "ECS/Archetypes/AnimalArchetype.h"
#include "ECS/Components/Animal.h"
#include "ECS/Components/AnimalBrain.h"
#include "ECS/Components/Fixed.h"
#include "ECS/Components/Flock.h"
#include "ECS/Components/Life.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Villager.h"
#include "ECS/Components/WallHug.h"
#include "ECS/DisappearSmoke.h"
#include "ECS/Effects/Reactions.h"
#include "ECS/Flocks.h"
#include "ECS/LivingPhysics.h"
#include "ECS/LivingPos.h"
#include "ECS/Map.h"
#include "ECS/MapCells.h"
#include "ECS/MobileDrawing.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/Physics/PhysicsObjects.h"
#include "ECS/Registry.h"
#include "ECS/ScriptHeld.h"
#include "ECS/SeaCells.h"
#include "ECS/ToBeDeleted.h"
#include "ECS/Villager/VillagerDeath.h"
#include "ECS/Villager/VillagerScript.h"
#include "ECS/VillagerAnimations.h"
#include "GameClock.h"
#include "InfoConstants.h"
#include "Locator.h"

namespace openblack::ecs::animal_ai
{
using components::Animal;
using components::AnimalBrain;
using components::Fixed;
using components::Flock;
using components::Life;
using components::LivingAction;
using components::Mesh;
using components::Transform;
using components::Villager;

using game_random::GameFloatRand;
using game_random::GameRand;

namespace detail
{
const GAnimalInfo& InfoOf(const Animal& animal)
{
	return Locator::infoConstants::value().animal.at(static_cast<size_t>(animal.type));
}

const GAnimalStateTableInfo& StateInfo(uint8_t state)
{
	return Locator::infoConstants::value().animalStateTable.at(std::min<size_t>(state, 52));
}

/// the grazing class: sheep, tortoise, cow, horse, pig (and the puzzle sheep)
bool IsGrazer(AnimalInfo type)
{
	switch (type)
	{
	case AnimalInfo::Sheep:
	case AnimalInfo::Tortoise:
	case AnimalInfo::Cow:
	case AnimalInfo::Horse:
	case AnimalInfo::Pig:
	case AnimalInfo::PuzzleSheep:
		return true;
	default:
		return false;
	}
}

Hunter HunterOf(AnimalInfo type)
{
	switch (type)
	{
	case AnimalInfo::Lion:
	case AnimalInfo::Leopard:
	case AnimalInfo::PuzzleLion:
		return Hunter::Cat;
	case AnimalInfo::Tiger:
		return Hunter::Tiger;
	case AnimalInfo::Wolf:
	case AnimalInfo::PuzzleWolf:
		return Hunter::Wolf;
	default:
		return Hunter::None;
	}
}

systems::AnimalAISystemInterface& Shared()
{
	if (!Locator::animalAISystem::has_value())
	{
		std::fputs("ecs::animal_ai: no animal state in the locator (Locator::animalAISystem)\n", stderr);
		std::abort();
	}
	return Locator::animalAISystem::value();
}

float VisualTime()
{
	return Shared().VisualTime();
}

uint32_t Turn()
{
	return game_clock::Turn(); // the reactions' clock too
}

// ---- MapCoords and the angle tables ----

/// COS / SIN tables (gutils::Cos / Sin): 2048 entries per circle, 65536 = 1
int32_t Cos(uint16_t a)
{
	return gutils::Cos(a);
}
int32_t Sin(uint16_t a)
{
	return gutils::Sin(a);
}

/// the step of that speed along the angle: ((speed >> 4) * COS[a]) >> 12, MapCoords per turn
/// (gutils::StepFromAngle)
glm::ivec2 Step(uint16_t angle, uint32_t speed)
{
	return gutils::StepFromAngle(angle, static_cast<int32_t>(speed));
}

/// the game angle of a MapCoords offset (gutils::GetAngleFromDXDZ)
uint16_t AngleOfMapCoords(int32_t dx, int32_t dz)
{
	return gutils::GetAngleFromDXDZ(dx, dz);
}

/// v * num / den, truncated
int32_t Scale(int32_t v, int32_t num, int32_t den)
{
	return den == 0 ? 0 : static_cast<int32_t>(static_cast<double>(v) * num / den);
}

float Metres(uint32_t speed)
{
	return static_cast<float>(speed) / k_MapCoordsPerMetre;
}

glm::vec2 Xz(const Transform& transform)
{
	return {transform.position.x, transform.position.z};
}

MapInterface::CellId CellOf(glm::vec2 p)
{
	return MapInterface::GetGridCell(p); // map_coords::CellOf: off the map (>= 512, 0xFFFF when negative) stays off it
}

/// the drawn rotation of a mobile heading along the angle (as PathfindingSystem's InitializeStep)
void FaceAngle(Transform& transform, uint16_t angle)
{
	const float theta = static_cast<float>(angle) * glm::two_pi<float>() / k_Circle;
	transform.rotation = affine::AngleY(theta + glm::half_pi<float>()); // a quarter turn ahead of the game angle
}

// ---- the land ----

bool InBounds(glm::vec2 p)
{
	return living::InBounds(p); // ECS/LivingPos.h
}

/// whether the point collides with the collide type (inferred)
bool Collides(glm::vec2 p, uint32_t collideType)
{
	return living::Collides(p, collideType); // ECS/LivingPos.h
}

/// uniform in a square of that side around c (half - GameFloatRand(size) per axis)
glm::vec2 SquarePos(glm::vec2 c, float size)
{
	const float half = size * 0.5f;
	const float x = GameFloatRand(size);
	const float z = GameFloatRand(size);
	return c + glm::vec2(half - x, half - z);
}

/// any living is available unless it is being deleted (held or flying animals are available); a villager also needs
/// its final state not to be dying
bool Available(entt::entity entity)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!ecs::IsAvailable(entity) || !registry.AllOf<Transform>(entity))
	{
		return false;
	}
	return !registry.AllOf<Villager>(entity) || villager::IsAvailable(entity);
}

/// birds need a land block under the point (the tie rules are not read); every other living accepts any point
bool IsPosValidForMapCellExistance(const Context& ctx, glm::vec2 p)
{
	if (!IsBird(ctx.animal.type))
	{
		return true;
	}
	const auto cellCoords = glm::u16vec2(glm::max(p, glm::vec2(0.0f)) / 10.0f);
	const auto& cell = Locator::terrainSystem::value().GetCell(cellCoords);
	return !(cell.properties.fullWater && cell.r == 0 && cell.g == 0 && cell.b == 0 && cell.properties.country == 0);
}

bool IsPosValidForTurnAngle(const Context& ctx, glm::vec2 p)
{
	const float turn = gutils::ConvertGameAngleTo3D(static_cast<int32_t>(ctx.info.turnAngle));
	// R = whole metres of (2 x speed / turn), the speed in MapCoords per turn and the turn in radians. The turn is not
	// tested: with 0 the quotient is infinite (or NaN with no speed), the truncation gives INT_MIN, R = -327680 m and both
	// distances below exceed it (true)
	const float twice = static_cast<float>(ctx.brain.speed) * 2.0f;
	const float radius = gutils::ConvertWholeDistanceToMeters(map_coords::FtoL(twice / turn));
	// the two turning-circle centres, a quarter turn left and right of the heading at distance R (the step drops the low
	// 4 bits of R); the point must lie outside both
	const auto me = map_coords::FromMetres(Xz(ctx.transform));
	const auto at = map_coords::FromMetres(p);
	const auto left = static_cast<uint16_t>((ctx.brain.angle + 0x200) & 0x7FF);
	const auto right = static_cast<uint16_t>((ctx.brain.angle - 0x200) & 0x7FF);
	if (!(gutils::GetDistanceInMetres(at, me + gutils::GetPosFromGameAngle(left, radius)) > radius))
	{
		return false;
	}
	return gutils::GetDistanceInMetres(at, me + gutils::GetPosFromGameAngle(right, radius)) > radius;
}

glm::vec2 CalcRandomPos(const Context& ctx, glm::vec2 c, float rMin, float rMax)
{
	// living::CalcRandomPos (ECS/LivingPos.h) with the animal's collide type and its two tests: the turning circle and
	// the bird's land check
	return living::CalcRandomPos(
	    c, rMin, rMax, static_cast<uint32_t>(ctx.info.collideType), Xz(ctx.transform),
	    [&ctx](glm::vec2 p) { return IsPosValidForTurnAngle(ctx, p); },
	    [&ctx](glm::vec2 p) { return IsPosValidForMapCellExistance(ctx, p); });
}

void SetDomainCentre(Flock& flock, glm::vec3 position)
{
	// ECS/Flocks.h
	flocks::SetDomainCentrePos(Locator::entitiesRegistry::value().ToEntity(flock), position);
}

// ---- the flock ----

Flock* FlockOf(const Animal& animal)
{
	auto& registry = Locator::entitiesRegistry::value();
	return registry.Valid(animal.flock) ? registry.TryGet<Flock>(animal.flock) : nullptr;
}

entt::entity LeaderOf(const Flock& flock)
{
	// the tail (ECS/Flocks.h: members.front())
	return flocks::Leader(Locator::entitiesRegistry::value().ToEntity(flock));
}

/// the leader's position, else the domain centre
glm::vec2 FlockPos(const Context& ctx)
{
	const auto* flock = FlockOf(ctx.animal);
	if (flock == nullptr)
	{
		return Xz(ctx.transform);
	}
	const auto leader = LeaderOf(*flock);
	if (leader == entt::null)
	{
		return {flock->domainCentre.x, flock->domainCentre.z};
	}
	return Xz(Locator::entitiesRegistry::value().Get<const Transform>(leader));
}

/// whether the point is inside the flock's domain (factor 1.0, ECS/Flocks.h): no flock -> false (openblack had true here)
bool PosWithinDomain(const Context& ctx, glm::vec2 p)
{
	return flocks::PosWithinDomain(ctx.entity, p, 1.0f);
}

uint16_t FlockDistance(const Context& ctx)
{
	const auto* flock = FlockOf(ctx.animal);
	return flock != nullptr ? flock->flockDistance : static_cast<uint16_t>(ctx.info.flockDistance);
}

uint16_t DomainRadius(const Context& ctx)
{
	const auto* flock = FlockOf(ctx.animal);
	return flock != nullptr ? flock->domainRadius : static_cast<uint16_t>(ctx.info.domainRadius);
}

void LeaveFlock(entt::entity entity, Animal& animal)
{
	if (auto* flock = FlockOf(animal); flock != nullptr)
	{
		auto& members = flock->members;
		members.erase(std::remove(members.begin(), members.end(), entity), members.end());
	}
	animal.flock = entt::null;
}

// ---- states ----

/// the top state if it is final, else the destination
uint8_t FinalStateOf(const AnimalBrain& brain)
{
	return StateInfo(brain.topState).field0xc != 0 ? brain.topState : brain.finalState;
}

/// the exit function of its FINAL state, with the new state
bool CallExitStateFunction(AnimalBrain& brain, AnimalState to)
{
	const bool death = to >= AnimalState::SetDying && to <= AnimalState::Downed;
	const auto from = FinalStateOf(brain);
	switch (from)
	{
	case 24:
		// in the hand: only thrown, landed or dying while held
		return to == AnimalState::Flying || to == AnimalState::Landed || death;
	case 10:
		// flying: caught, landed or dying
		return to == AnimalState::InHand || to == AnimalState::Landed || death;
	case 1:
	case 2:
	case 3:
	case 27:
	case 28:
	case 29:
	case 41:
	case 42:
	case 44:
		// the move states: the target is dropped
		brain.target = entt::null;
		return true;
	default:
		if (IsReactionState(from))
		{
			ExitReaction(brain, static_cast<uint8_t>(to));
		}
		return true;
	}
}

/// the exit filter, the state, turnsSinceStateChange = 0 and the state's clip (the state speed is not changed; there
/// are no into / out-of clips)
void SetTopState(entt::entity entity, AnimalBrain& brain, AnimalState state)
{
	// the exit test may refuse; the entry functions all accept
	if (!CallExitStateFunction(brain, state))
	{
		return;
	}
	static const bool trace = debug_env::AnimalTrace();
	if (trace)
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Animal {}: state {} -> {}", static_cast<uint32_t>(entity), brain.topState,
		                   static_cast<int>(state));
	}
	brain.topState = static_cast<uint8_t>(state);
	brain.turnsSinceStateChange = 0;
	SetAnimalStateAnim(entity);
}

void SetTopState(Context& ctx, AnimalState state)
{
	SetTopState(ctx.entity, ctx.brain, state);
}

/// WAIT_FOR_ANIMATION with the clip unchanged, then the state
void PlayAnimThenSetState(Context& ctx, AnimalState state)
{
	// the exit of its final state tested with the new final state, then raw sets
	if (!CallExitStateFunction(ctx.brain, state))
	{
		return;
	}
	ctx.brain.topState = static_cast<uint8_t>(AnimalState::WaitForAnimation);
	ctx.brain.finalState = static_cast<uint8_t>(state);
	ctx.brain.turnsSinceStateChange = 0;
}

/// the speed (0..0xFFFF), then the clip for it without a restart
void SetSpeed(Context& ctx, uint32_t speed)
{
	// the SpellWolf ignores it: only SetRunToFinalDest sets its speed
	if (ctx.animal.type == AnimalInfo::SpellWolf)
	{
		return;
	}
	ctx.brain.speed = static_cast<uint16_t>(std::min<uint32_t>(speed, 0xFFFF));
	SetAnimalAnim(ctx.entity, AnimalAnimId(ctx.entity), false);
}

uint32_t SpeedDefault(const Context& ctx)
{
	return static_cast<uint32_t>(ctx.info.speedGroup.speedDefault);
}

/// the info's move state now and the final state after, then SetupMobileMoveToPos(p): a STEP_THROUGH walk (no obstacle
/// hugging)
void SetupMoveToPos(Context& ctx, glm::vec2 p, AnimalState final)
{
	if (SetCurrentAndDestinationState(ctx, final))
	{
		SetupMobileMoveToPos(ctx, p);
	}
}

bool SetCurrentAndDestinationState(Context& ctx, AnimalState final)
{
	// the exit test against the destination, then both states and the clip
	if (!CallExitStateFunction(ctx.brain, final))
	{
		return false;
	}
	ctx.brain.topState = static_cast<uint8_t>(ctx.info.moveState);
	ctx.brain.finalState = static_cast<uint8_t>(final);
	ctx.brain.turnsSinceStateChange = 0;
	SetAnimalStateAnim(ctx.entity);
	return true;
}

/// within one turn's step of the goal
bool AreWeThere(const Context& ctx)
{
	const glm::vec2 d = ctx.brain.goal - Xz(ctx.transform);
	const float step = Metres(ctx.brain.speed);
	return glm::dot(d, d) <= step * step;
}

/// SetTowardsAngle at the goal (the species' turn limit) and the step along the new heading
void InitStepsXZ(Context& ctx)
{
	const glm::vec2 d = ctx.brain.goal - Xz(ctx.transform);
	SetTowardsAngle(ctx, gutils::GetAngleFromXZ(Xz(ctx.transform), ctx.brain.goal), glm::length(d));
	ctx.brain.step = Step(ctx.brain.angle, ctx.brain.speed);
	FaceAngle(ctx.transform, ctx.brain.angle);
}

namespace
{
/// the ARRIVED / FINAL_STEP snap: Pos = goal
int SnapToGoal(Context& ctx)
{
	const glm::vec2 goal = ctx.brain.goal;
	if (InBounds(goal))
	{
		ctx.brain.movedLastTurn += glm::distance(Xz(ctx.transform), goal);
		// the cell lists change only when the cell does
		map_cells::MoveMapObject(
		    ctx.entity, glm::vec3(goal.x, Locator::terrainSystem::value().GetHeightAt(goal) + ctx.brain.altitude, goal.y));
	}
	return 0xA;
}
} // namespace

void SetupMobileMoveToPos(Context& ctx, glm::vec2 p)
{
	ctx.brain.goal = p;
	InitStepsXZ(ctx);
	// the circle hug's goal distance is cleared (only ORBIT reads it). The STEP_THROUGH flag the original also sets is not
	// kept (inferred: not read on the animals' path)
	ctx.brain.hugGoalDistance = 0;
	if (AreWeThere(ctx))
	{
		ctx.brain.moveState = k_MoveArrived;
		return;
	}
	// no circle, turnsToObj 0xFF
	ctx.brain.hugCircle.set = false;
	ctx.brain.turnsToObj = 0xFF;
	ctx.brain.moveState = k_MoveStepThrough;
}

int MoveTo(Context& ctx)
{
	switch (ctx.brain.moveState)
	{
	case k_MoveArrived:
		// there already, else on through STEP_THROUGH
		if (AreWeThere(ctx))
		{
			return SnapToGoal(ctx);
		}
		ctx.brain.moveState = k_MoveStepThrough;
		[[fallthrough]];
	case k_MoveStepThrough:
	{
		// an animal re-aims every turn and walks straight, no obstacle handling
		InitStepsXZ(ctx);
		const int r = MoveBy(ctx, ctx.brain.step) ? 7 : 6;
		if (AreWeThere(ctx))
		{
			ctx.brain.moveState = k_MoveFinalStep;
		}
		return r;
	}
	case k_MoveFinalStep:
		return SnapToGoal(ctx);
	default:
		// LINEAR / ORBIT / EXIT_CIRCLE: the circle hug of SetupMoveToWithHug (ECS/AnimalWallHug.cpp)
		return IsHugMoveState(ctx.brain.moveState) ? HugMoveTo(ctx) : 0;
	}
}

/// one step; true when it entered another 10 m map cell (7) rather than staying in its own (6)
bool MoveBy(Context& ctx, glm::ivec2 step)
{
	const glm::vec2 from = Xz(ctx.transform);
	const glm::vec2 to = from + glm::vec2(step) / k_MapCoordsPerMetre;
	if (!InBounds(to))
	{
		return false;
	}
	// out of the old cell and into the new one (at the head) only when the cell changes, the same test as the return
	// value
	map_cells::MoveMapObject(ctx.entity,
	                         glm::vec3(to.x, Locator::terrainSystem::value().GetHeightAt(to) + ctx.brain.altitude, to.y));
	ctx.brain.movedLastTurn += glm::distance(from, to);
	return CellOf(from) != CellOf(to);
}

/// adds v to out within the speed's budget (the largest axis); true when the budget is used up
bool AddSteer(const AnimalBrain& brain, glm::ivec2& out, glm::ivec2 v)
{
	const int32_t budget = static_cast<int32_t>(brain.speed) - std::max(std::abs(out.x), std::abs(out.y));
	if (budget <= 0)
	{
		return true;
	}
	const int32_t m = std::min(std::max(std::abs(v.x), std::abs(v.y)), budget);
	out.y += Scale(v.y, m, brain.speed);
	out.x += Scale(v.x, m, brain.speed);
	return m == budget;
}

/// the flock's pull (cohesion to the others' centre, the nearest member per axis, its step); true when the budget is
/// used up
bool FlockSteer(const Context& ctx, glm::ivec2& out)
{
	if (static_cast<int32_t>(ctx.brain.speed) - std::max(std::abs(out.x), std::abs(out.y)) <= 0)
	{
		return true;
	}
	const auto* flock = FlockOf(ctx.animal);
	if (flock == nullptr)
	{
		return false;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const glm::ivec2 me(Xz(ctx.transform) * k_MapCoordsPerMetre);
	glm::ivec2 sum(0);
	int32_t count = 0;
	int32_t nearestDistance = 0x7FFFFFFF;
	entt::entity nearest = entt::null;
	// the list from its head: the newest member first
	for (auto it = flock->members.rbegin(); it != flock->members.rend(); ++it)
	{
		const auto member = *it;
		if (member == ctx.entity || !registry.Valid(member) || !registry.AllOf<Transform>(member))
		{
			continue;
		}
		const glm::ivec2 them(Xz(registry.Get<const Transform>(member)) * k_MapCoordsPerMetre);
		// |dx| + |dz| (inferred)
		const int32_t d = std::abs(them.x - me.x) + std::abs(them.y - me.y);
		if (d < nearestDistance)
		{
			nearestDistance = d;
			nearest = member;
		}
		sum += them;
		++count;
	}
	if (nearest == entt::null)
	{
		return false;
	}
	const int32_t speed = ctx.brain.speed;
	// cohesion: a fifth of the speed towards the others' centre
	const auto centre = sum / count - me;
	const auto toCentre = Step(AngleOfMapCoords(centre.x, centre.y), ctx.brain.speed);
	glm::ivec2 v(Scale(toCentre.x, speed / 5, speed), Scale(toCentre.y, speed / 5, speed));
	if (AddSteer(ctx.brain, out, v))
	{
		return true;
	}
	// the nearest member, per axis: closer than FlockDistance away, farther towards it. The distance is compared with raw
	// MapCoords (6553.6 per metre), so in practice it always pulls; and the cohesion vector goes in a second time.
	const glm::ivec2 them(Xz(registry.Get<const Transform>(nearest)) * k_MapCoordsPerMetre);
	const glm::ivec2 d = them - me;
	const auto flockDistance = static_cast<int32_t>(FlockDistance(ctx));
	for (int axis = 0; axis < 2; ++axis)
	{
		const int32_t sign = d[axis] < 0 ? -1 : 1;
		if (std::abs(d[axis]) > flockDistance)
		{
			v[axis] += sign * ((speed / 5 * 8) / 5);
		}
		else if (std::abs(d[axis]) < flockDistance)
		{
			v[axis] += 2 * sign * -(speed / 5);
		}
	}
	if (AddSteer(ctx.brain, out, v))
	{
		return true;
	}
	// alignment: three fifths of the nearest member's step
	const auto* other = registry.TryGet<const AnimalBrain>(nearest);
	const glm::ivec2 theirStep = other != nullptr ? other->step : glm::ivec2(0);
	return AddSteer(ctx.brain, out, {Scale(theirStep.x, speed * 3 / 5, speed), Scale(theirStep.y, speed * 3 / 5, speed)});
}

/// the new straight step (towards / away from c, the flock, a random turn)
void SetNewWander(Context& ctx, glm::vec2 c, int32_t rMin, int32_t rMax)
{
	glm::ivec2 out(0);
	const glm::vec2 me = Xz(ctx.transform);
	// the distance in whole metres, compared as integers with the int arguments
	const int32_t d = map_coords::FtoL(gutils::GetDistanceInMetres(c, me));
	if (d > rMax || d < rMin)
	{
		const auto a = d > rMax ? gutils::GetAngleFromXZ(me, c) : gutils::GetAngleFromXZ(c, me);
		if (a != 0)
		{
			const auto full = Step(a, ctx.brain.speed);
			AddSteer(ctx.brain, out, {Scale(full.x, 9, 10), Scale(full.y, 9, 10)});
		}
	}
	if (!FlockSteer(ctx, out))
	{
		const auto turn = static_cast<int32_t>(ctx.info.turnAngle);
		// GameRand(turn), 0 for 0 without a draw
		const auto random = static_cast<int32_t>(GameRand(static_cast<uint16_t>(turn)));
		const auto a = static_cast<uint16_t>((ctx.brain.angle - turn / 2 + random) & 0x7FF);
		AddSteer(ctx.brain, out, Step(a, ctx.brain.speed));
	}
	ctx.brain.step = out;
	// the heading follows the step at once
	if (out != glm::ivec2(0))
	{
		ctx.brain.angle = AngleOfMapCoords(out.x, out.y);
		FaceAngle(ctx.transform, ctx.brain.angle);
	}
}

/// turns at most turnAngle a turn; with the goal inside its turning circle (R = 2 x speed / turnAngle in radians) it
/// turns |diff| - turnAngle x d / R instead, nearly all the way when close
void SetTowardsAngle(Context& ctx, uint16_t target, float distance)
{
	// the side and the size of the turn (half a circle turns +1, as the original)
	const int32_t direction = gutils::GetAngleSign(ctx.brain.angle, target);
	const auto absDiff = static_cast<int32_t>(gutils::GetAngleDifference(ctx.brain.angle, target));
	const int32_t diff = direction * absDiff;
	const auto turnAngle = static_cast<int32_t>(ctx.info.turnAngle);
	int32_t turn = std::min(absDiff, turnAngle);
	if (absDiff > turnAngle)
	{
		// a turnAngle of 0 gives an infinite R (as the original's float division), so the turn is the whole |diff|
		const float radians = static_cast<float>(turnAngle) * glm::two_pi<float>() / k_Circle;
		const float radius = turnAngle > 0 ? 2.0f * Metres(ctx.brain.speed) / radians : std::numeric_limits<float>::infinity();
		if (distance < radius)
		{
			// max(turnAngle, |diff|) - turnAngle x d / R, truncated, at most |diff|
			const float reduced = static_cast<float>(std::max(turnAngle, absDiff)) -
			                      (turnAngle > 0 ? static_cast<float>(turnAngle) * distance / radius : 0.0f);
			turn = std::min(static_cast<int32_t>(reduced), absDiff);
		}
	}
	ctx.brain.angle = static_cast<uint16_t>((ctx.brain.angle + (diff < 0 ? -turn : turn)) & 0x7FF);
	// the bank zoomer: level again when it needs no turn, else +-GetBankAngle, over GetTimeToBank
	const float time = TimeToBank(ctx.animal.type);
	const float bankTarget = diff == 0 ? 0.0f : (diff < 0 ? -BankAngle(ctx.animal.type) : BankAngle(ctx.animal.type));
	// the zoomer's own threshold settles it below 0.001
	ctx.brain.bank.SetDestinationWithSpeedAndTime(bankTarget, 0.0f, time);
}

// ---- needs ----

bool IsLeader(const Context& ctx)
{
	const auto* flock = FlockOf(ctx.animal);
	return flock != nullptr && LeaderOf(*flock) == ctx.entity;
}

uint32_t AgeOf(const AnimalBrain& brain)
{
	// 1500 game turns per year
	const int32_t turns = static_cast<int32_t>(Turn()) - brain.birthTurn;
	return turns > 0 ? static_cast<uint32_t>(turns / 1500) : 0;
}

/// animal_ai::ScaleForAge on the Transform's uniform scale
void SetScaleForAge(Context& ctx, uint32_t age)
{
	ctx.transform.scale = glm::vec3(ScaleForAge(ctx.info, age, ctx.transform.scale.x));
}

void InteractDecideWhatToDo(Context& ctx)
{
	// with a flock, LookForFlocksInSpiral(2 x domainRadius, merge = 1)
	if (FlockOf(ctx.animal) != nullptr)
	{
		LookForFlocksInSpiral(ctx, 2.0f * static_cast<float>(ctx.info.domainRadius), true);
	}
	// then StartWander: the birds their own, StartWander for the grazers, the hunters and the SpellWolf; the other
	// species' (pending) as DecideWhatToDo
	const bool walker =
	    IsGrazer(ctx.animal.type) || HunterOf(ctx.animal.type) != Hunter::None || ctx.animal.type == AnimalInfo::SpellWolf;
	if (IsBird(ctx.animal.type))
	{
		BirdStartWander(ctx);
	}
	else if (walker)
	{
		StartWander(ctx);
	}
	else
	{
		SetTopState(ctx, AnimalState::DecideWhatToDo);
	}
}

/// the breed, hunger and sleep counters grow by one a turn, and the leader's turns
void ProcessNeeds(Context& ctx)
{
	auto* flock = FlockOf(ctx.animal);
	// the age is GetAge, the live one
	if (ctx.info.needToBreed != 0 && flock != nullptr && flock->members.size() >= 2 &&
	    flock->members.size() < flock->maxMembers && ctx.brain.breed < static_cast<int32_t>(ctx.info.needToBreed) &&
	    AgeOf(ctx.brain) >= ctx.info.grownUpAge)
	{
		++ctx.brain.breed;
	}
	if (ctx.info.hunger != 0 && ctx.brain.hunger < static_cast<int32_t>(ctx.info.hunger))
	{
		++ctx.brain.hunger;
	}
	if (ctx.info.sleep != 0 && ctx.brain.sleep < static_cast<int32_t>(ctx.info.sleep))
	{
		++ctx.brain.sleep;
	}
	// the leader counts its turns when no shepherd guards the flock (openblack has no shepherds)
	if (flock != nullptr && IsLeader(ctx))
	{
		++flock->leaderTurns;
	}
}

/// the grazers' food search: the first free cell of a spiral of (domainRadius / 10)^2 map cells around it, ahead of
/// it, not its own and no member's
bool LookForFoodPos(const Context& ctx, glm::vec2& out)
{
	auto& registry = Locator::entitiesRegistry::value();
	const glm::vec2 me = Xz(ctx.transform);
	const auto myCell = CellOf(me);
	const auto* flock = FlockOf(ctx.animal);
	const int cells = (DomainRadius(ctx) / 10) * (DomainRadius(ctx) / 10);
	// a spiral over a copy of its own MapCoords: in the domain, in bounds and free, then on by whole cells
	Spiral spiral;
	map_coords::MapCoords coords = map_coords::FromMetres(me);
	for (int i = 0; i < cells; ++i)
	{
		const glm::vec2 c = map_coords::ToMetres(coords);
		const auto cell = CellOf(c);
		bool ok = cell != myCell && PosWithinDomain(ctx, c) && InBounds(c);
		// within viewAngle / 2 of its heading
		ok = ok && static_cast<int32_t>(gutils::GetAngleDifference(
		               ctx.brain.angle, gutils::GetAngleFromXZ(map_coords::FromMetres(me), coords))) <=
		               static_cast<int32_t>(ctx.info.viewAngle) / 2;
		// no other member of its flock stands there or goes there
		if (ok && flock != nullptr)
		{
			for (const auto member : flock->members)
			{
				if (member == ctx.entity || !registry.Valid(member))
				{
					continue;
				}
				const auto* brain = registry.TryGet<const AnimalBrain>(member);
				if (CellOf(Xz(registry.Get<const Transform>(member))) == cell ||
				    (brain != nullptr && brain->goal != glm::vec2(0.0f) && CellOf(brain->goal) == cell))
				{
					ok = false;
					break;
				}
			}
		}
		if (ok && !Collides(c, static_cast<uint32_t>(ctx.info.collideType)))
		{
			out = c;
			return true;
		}
		spiral.Advance(coords);
	}
	return false;
}

/// 3 ready to breed (else the breed counter restarts when the flock is full), 1 hungry, 2 sleepy, 0 none
int CheckNeeds(Context& ctx)
{
	if (ctx.info.needToBreed != 0 && ctx.brain.breed >= static_cast<int32_t>(ctx.info.needToBreed))
	{
		const auto* flock = FlockOf(ctx.animal);
		if (flock != nullptr && flock->maxMembers > flock->members.size())
		{
			return 3;
		}
		ctx.brain.breed = 0;
	}
	if (ctx.info.hunger != 0 && ctx.brain.hunger >= static_cast<int32_t>(ctx.info.hunger))
	{
		return 1;
	}
	if (ctx.info.sleep != 0 && ctx.brain.sleep >= static_cast<int32_t>(ctx.info.sleep))
	{
		return 2;
	}
	return 0;
}

/// the grazers' needs: breeding, then hunger, then sleep
int CowReactToAnimalNeeds(Context& ctx)
{
	const auto* flock = FlockOf(ctx.animal);
	// the age is GetAge, the live one
	if (ctx.info.needToBreed != 0 && AgeOf(ctx.brain) >= ctx.info.grownUpAge)
	{
		// the breed counter grows again here (ProcessNeeds counts it too)
		if (flock != nullptr && flock->members.size() >= 2 && flock->members.size() < flock->maxMembers &&
		    ctx.brain.breed < static_cast<int32_t>(ctx.info.needToBreed))
		{
			++ctx.brain.breed;
		}
		if (ctx.brain.breed >= static_cast<int32_t>(ctx.info.needToBreed))
		{
			ctx.brain.breed = 0;
			if (flock != nullptr && flock->maxMembers > flock->members.size())
			{
				SetTopState(ctx, AnimalState::GivesBirth);
				return k_Started;
			}
		}
	}
	if (ctx.info.hunger != 0 && ctx.brain.hunger >= static_cast<int32_t>(ctx.info.hunger))
	{
		glm::vec2 p;
		if (LookForFoodPos(ctx, p))
		{
			SetSpeed(ctx, SpeedDefault(ctx));
			SetupMoveToPos(ctx, p, AnimalState::StartToEat);
			return k_Started;
		}
		// no food found: the sleep test is skipped
		return k_Nothing;
	}
	if (ctx.info.sleep != 0 && ctx.brain.sleep >= static_cast<int32_t>(ctx.info.sleep) &&
	    ctx.brain.sleepCell != glm::u16vec2(0))
	{
		SetTopState(ctx, AnimalState::SeekSleep);
		return k_Started;
	}
	return k_Nothing;
}

/// the grazers', the predators' or the birds' ReactToAnimalNeeds
int ReactToAnimalNeeds(Context& ctx)
{
	if (IsBird(ctx.animal.type))
	{
		return BirdReactToAnimalNeeds(ctx);
	}
	return HunterOf(ctx.animal.type) != Hunter::None ? PredatorReactToAnimalNeeds(ctx) : CowReactToAnimalNeeds(ctx);
}

/// merges with another flock of its species nearby: the bigger flock keeps everyone
void LookForFlocksInSpiral(Context& ctx, float radius, bool merge)
{
	if (!merge)
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	auto* mine = FlockOf(ctx.animal);
	// a flock with a town or a shepherd never merges. openblack has no shepherds, so only the town is tested
	if (mine == nullptr || mine->town != entt::null)
	{
		return;
	}
	const auto mineEntity = ctx.animal.flock;
	const glm::vec2 me = Xz(ctx.transform);
	const int cells = std::max(1, static_cast<int>((radius / 10.0f) * (radius / 10.0f)));
	// the spiral's cells, in order, looking for another flock of my species: a spiral over a copy of its own MapCoords,
	// in bounds, then on by whole cells
	Spiral spiral;
	map_coords::MapCoords coords = map_coords::FromMetres(me);
	// one read filter for the spiral: the search ends at the first merge, and a merge moves no object in the hand or
	// into physics
	const map_cells::ReadBatch batch;
	for (int i = 0; i < cells; ++i)
	{
		const glm::vec2 c = map_coords::ToMetres(coords);
		if (InBounds(c))
		{
			// the ANIMAL objects of the cell's mobile list (type 4 does not count as fixed) from its head
			const auto cell = map_coords::Cell(coords);
			for (auto entity = map_cells::FindType(cell, ObjectType::Animal); entity != entt::null;
			     entity = map_cells::FindType(cell, ObjectType::Animal, entity))
			{
				if (entity == ctx.entity || !registry.Valid(entity) || !registry.AllOf<Animal>(entity))
				{
					continue;
				}
				const auto& other = registry.Get<const Animal>(entity);
				if (other.type != ctx.animal.type || other.flock == entt::null || other.flock == mineEntity ||
				    !registry.Valid(other.flock))
				{
					continue;
				}
				const auto* brain = registry.TryGet<const AnimalBrain>(entity);
				if (brain != nullptr && (brain->status & 1) != 0)
				{
					continue; // dead
				}
				auto& theirs = registry.Get<Flock>(other.flock);
				// the original also needs both flocks' players equal (openblack's Flock has no player: not tested,
				// approximate) and theirs without a shepherd (none in openblack); never two script flocks
				const bool theirsScript = script_held::IsControlledByScript(other.flock);
				if (theirsScript && script_held::IsControlledByScript(mineEntity))
				{
					continue;
				}
				if (mine->members.size() + theirs.members.size() > ctx.info.maxFlockSize)
				{
					continue;
				}
				// the flock with more members keeps them all; a script flock of theirs always keeps them
				if (mine->members.size() >= theirs.members.size() && !theirsScript)
				{
					flocks::Merge(mineEntity, other.flock);
				}
				else
				{
					flocks::Merge(other.flock, mineEntity);
				}
				return;
			}
		}
		spiral.Advance(coords);
	}
}

/// the leader takes the herd to a new place every stayTime turns
int KeepLeaderWithinDomain(Context& ctx)
{
	auto* flock = FlockOf(ctx.animal);
	if (flock == nullptr || !IsLeader(ctx))
	{
		return 0;
	}
	if (!PosWithinDomain(ctx, FlockPos(ctx)) || flock->leaderTurns >= ctx.info.stayTime)
	{
		const auto p = CalcRandomPos(ctx, {flock->domainCentre.x, flock->domainCentre.z},
		                             static_cast<float>(ctx.info.domainInnerRadius), static_cast<float>(flock->domainRadius));
		SetupMoveToPos(ctx, p, AnimalState::DecideWhatToDo);
		flock->leaderTurns = 0;
		return k_Started;
	}
	return 0;
}

/// back towards the leader when too far from it
int KeepFlockMemberWithinFlockArea(Context& ctx)
{
	const glm::vec2 me = Xz(ctx.transform);
	const glm::vec2 leader = FlockPos(ctx);
	const auto flockDistance = static_cast<float>(FlockDistance(ctx));
	// the distance in metres to the leader
	if (PosWithinDomain(ctx, me) && gutils::GetDistanceInMetres(leader, me) <= flockDistance)
	{
		return 1;
	}
	const auto p = CalcRandomPos(ctx, leader, 0.0f, flockDistance);
	if (PosWithinDomain(ctx, p) || !PosWithinDomain(ctx, leader))
	{
		SetupMoveToPos(ctx, p, AnimalState::DecideWhatToDo);
	}
	return k_Started;
}

// ---- the state functions ----

void StartWander(Context& ctx)
{
	ctx.brain.moveState = k_MoveWander;
	ctx.brain.step = Step(ctx.brain.angle, ctx.brain.speed);
	SetSpeed(ctx, SpeedDefault(ctx));
	SetTopState(ctx, AnimalState::Wander);
	const auto* flock = FlockOf(ctx.animal);
	SetNewWander(ctx, FlockPos(ctx), static_cast<int32_t>(ctx.info.domainInnerRadius),
	             static_cast<int32_t>(flock != nullptr ? flock->domainRadius : ctx.info.domainRadius));
}

/// the grazers' decision: breed, keep with the flock, react to needs, else wander
void DecideWhatToDo(Context& ctx)
{
	if (HunterOf(ctx.animal.type) != Hunter::None)
	{
		PredatorDecideWhatToDo(ctx);
		return;
	}
	if (IsBird(ctx.animal.type))
	{
		BirdDecideWhatToDo(ctx);
		return;
	}
	if (ctx.animal.type == AnimalInfo::SpellWolf)
	{
		SetRunToFinalDest(ctx);
		return;
	}
	if (!IsGrazer(ctx.animal.type))
	{
		return;
	}
	// ready to breed
	if (CheckNeeds(ctx) == 3)
	{
		ctx.brain.breed = 0;
		SetTopState(ctx, AnimalState::GivesBirth);
		return;
	}
	// LookForFlocksInSpiral with the info's flocksCanMerge: 0 for every grazer, nothing
	if (FlockOf(ctx.animal) != nullptr)
	{
		if (KeepLeaderWithinDomain(ctx) == k_Started)
		{
			return;
		}
		if (KeepFlockMemberWithinFlockArea(ctx) == k_Started)
		{
			return;
		}
	}
	if (ReactToAnimalNeeds(ctx) != k_Started)
	{
		SetTopState(ctx, AnimalState::StartWander);
	}
}

/// a straight line, re-steered in every new map cell
void Wander(Context& ctx)
{
	if (ReactToAnimalNeeds(ctx) == k_Started)
	{
		return;
	}
	if (!PosWithinDomain(ctx, Xz(ctx.transform)))
	{
		SetTopState(ctx, AnimalState::DecideWhatToDo);
		return;
	}
	if (MoveBy(ctx, ctx.brain.step))
	{
		SetNewWander(ctx, FlockPos(ctx), 0, static_cast<int32_t>(FlockDistance(ctx)));
	}
}

/// walks to the goal; once there (0xA) the final state follows. Inside its turning circle SetTowardsAngle turns
/// nearly straight at the goal.
void MoveToPos(Context& ctx)
{
	if (MoveTo(ctx) == 0xA)
	{
		SetTopState(ctx, static_cast<AnimalState>(ctx.brain.finalState));
	}
}

/// 15..24 eat clips; the grazers then make it 20..34
void StartToEat(Context& ctx)
{
	ctx.brain.counter = static_cast<int16_t>(GameRand(10) + 15);
	if (IsGrazer(ctx.animal.type))
	{
		ctx.brain.counter = static_cast<int16_t>(GameRand(15) + 20);
	}
	SetSpeed(ctx, SpeedDefault(ctx));
	PlayAnimThenSetState(ctx, AnimalState::Eat);
}

/// one eat clip per turn until the counter runs out
void Eat(Context& ctx)
{
	if (--ctx.brain.counter == 0)
	{
		PlayAnimThenSetState(ctx, AnimalState::FinishEating);
		ctx.brain.hunger = 0;
		ctx.brain.foodTarget = entt::null;
	}
	else
	{
		PlayAnimThenSetState(ctx, AnimalState::Eat);
	}
	// every predator (and the SpellWolf): its prey is gone (also once the meal is over: then the up-from-eat clip never
	// shows, straight to DECIDE)
	if ((HunterOf(ctx.animal.type) != Hunter::None || ctx.animal.type == AnimalInfo::SpellWolf) &&
	    !Available(ctx.brain.foodTarget))
	{
		ctx.brain.counter = 0;
		SetTopState(ctx, AnimalState::DecideWhatToDo);
	}
}

/// to a spot in a square around its sleep cell, 2 m per flock member
void SeekSleep(Context& ctx)
{
	if (ctx.brain.sleepCell == glm::u16vec2(0))
	{
		SetTopState(ctx, AnimalState::StartWander);
		return;
	}
	const auto* flock = FlockOf(ctx.animal);
	const float size = 2.0f * static_cast<float>(flock != nullptr ? flock->members.size() : 1);
	const auto centre = MapInterface::GetCellCenter(ctx.brain.sleepCell);
	SetupMoveToPos(ctx, SquarePos(centre, size), AnimalState::Sleeps);
}

/// the sleep counter runs down by 2 a turn (ProcessNeeds adds 1)
void Sleeps(Context& ctx)
{
	ctx.brain.sleep = static_cast<int16_t>(ctx.brain.sleep - 2);
	if (ctx.brain.sleep <= 0)
	{
		SetTopState(ctx, AnimalState::StartWander);
		ctx.brain.sleep = 0;
	}
}

/// a young one (age 1) of its kind joins the flock
AnimalBrain& Initialise(entt::entity entity, const Animal& animal, const Transform& transform);
void DecideWhatToDo(Context& ctx);

void GivesBirth(Context& ctx)
{
	const auto position = ctx.transform.position;
	const auto type = ctx.animal.type;
	const auto town = ctx.animal.town;
	const auto flock = ctx.animal.flock;
	// the newborn (age 1) runs DecideWhatToDo at once, then the mother goes to START_WANDER
	const auto born = archetypes::AnimalArchetype::Create(position, type, town, flock, 1);
	auto& registry = Locator::entitiesRegistry::value();
	if (born != entt::null && registry.AllOf<Animal, Transform>(born))
	{
		auto& bornAnimal = registry.Get<Animal>(born);
		auto& bornTransform = registry.Get<Transform>(born);
		auto& bornBrain = Initialise(born, bornAnimal, bornTransform);
		Context child {born, bornAnimal, bornBrain, bornTransform, InfoOf(bornAnimal)};
		DecideWhatToDo(child);
	}
	auto& mother = registry.Get<AnimalBrain>(ctx.entity);
	SetTopState(ctx.entity, mother, AnimalState::StartWander);
}

/// nothing while it flies
void SetDying(entt::entity entity, AnimalBrain& brain)
{
	// the species' own SetDying replaces the common one
	if (const auto* animal = Locator::entitiesRegistry::value().TryGet<Animal>(entity); animal != nullptr)
	{
		if (const auto* dying = Shared().SpeciesDying(static_cast<size_t>(animal->type)); dying != nullptr)
		{
			(*dying)(entity);
			return;
		}
	}
	if (physics::PhysicsObjects::IsFlying(entity))
	{
		return;
	}
	if ((brain.status & 1) == 0)
	{
		// a copy: a listener may add or remove listeners
		const auto listeners = Shared().GetDeathListeners();
		for (const auto& [id, listener] : listeners)
		{
			listener(entity);
		}
	}
	if ((brain.status & 1) == 0)
	{
		auto& registry = Locator::entitiesRegistry::value();
		(registry.AllOf<Life>(entity) ? registry.Get<Life>(entity) : registry.Assign<Life>(entity)).value = 0.0f;
		SetTopState(entity, brain, AnimalState::Dying);
		brain.status |= 0x31;
	}
	brain.counter = k_TurnsToDieOver;
}

/// off the flock; the corpse lies 600 turns, then its smoke puff and it goes. A script-controlled one never times
/// out. The counter is living::DeadTick (ECS/Villager/VillagerDeath.h), shared with the villagers; (pending) the death
/// reason is not kept, so the sacrifice rule never applies to an animal yet
bool Dead(Context& ctx)
{
	LeaveFlock(ctx.entity, ctx.animal);
	const auto tick = living::DeadTick(static_cast<uint16_t>(ctx.brain.counter), script_held::IsControlledByScript(ctx.entity),
	                                   DeathReason::None);
	ctx.brain.counter = static_cast<decltype(ctx.brain.counter)>(tick.counter);
	if (!tick.vanish)
	{
		return false;
	}
	// the white smoke puff at half its height
	const float height = 0.5f * ctx.transform.scale.y * 2.0f;
	DisappearSmoke::Create(ctx.transform.position + glm::vec3(0.0f, height, 0.0f), 1.0f);
	return true;
}

/// one turn of the animal's state machine; true when the animal is to be deleted
bool ProcessState(Context& ctx)
{
	++ctx.brain.turnsSinceStateChange;
	ctx.brain.movedLastTurn = 0.0f;
	// the flight from a predator ends after its turns or when it has gone
	ProcessReaction(ctx);
	// a target that is no longer available is dropped
	if (ctx.brain.target != entt::null && !Available(ctx.brain.target))
	{
		ctx.brain.target = entt::null;
	}
	if (StateInfo(ctx.brain.topState).field0xa4 != 0)
	{
		ProcessNeeds(ctx);
		// a young one grows four times a year (every 1500 / 4 turns)
		const auto age = AgeOf(ctx.brain);
		if (age < ctx.info.grownUpAge && Turn() % 375 == 0)
		{
			ctx.animal.age = age;
			SetScaleForAge(ctx, age);
		}
	}
	// what it was eating has gone
	if (ctx.brain.foodTarget != entt::null && !Available(ctx.brain.foodTarget))
	{
		ctx.brain.foodTarget = entt::null;
		ctx.brain.counter = 0;
		SetTopState(ctx, AnimalState::DecideWhatToDo);
		return false;
	}
	// (the SpellWolf starts its wander as the others do: after a hunt it wanders, and its Wander runs on)
	const bool walker =
	    IsGrazer(ctx.animal.type) || HunterOf(ctx.animal.type) != Hunter::None || ctx.animal.type == AnimalInfo::SpellWolf;
	switch (static_cast<AnimalState>(ctx.brain.topState))
	{
	case AnimalState::MoveToPos:
		MoveToPos(ctx);
		if (ctx.animal.type == AnimalInfo::SpellWolf)
		{
			SpellWolfMoveToPos(ctx);
		}
		break;
	case AnimalState::Landed:
		// CalculateLairPosition, the flock now centres where it landed (the predators: their lair)
		if (HunterOf(ctx.animal.type) != Hunter::None)
		{
			CalculateLairPosition(ctx);
		}
		else if (auto* flock = FlockOf(ctx.animal); flock != nullptr)
		{
			SetDomainCentre(*flock, ctx.transform.position);
		}
		PlayAnimThenSetState(ctx, AnimalState::InteractDecideWhatToDo);
		break;
	case AnimalState::SetDying:
		SetDying(ctx.entity, ctx.brain);
		break;
	case AnimalState::Dying:
	case AnimalState::Drowning:
		// a bird falls into the physics with its flight
		if (IsBird(ctx.animal.type) && ctx.brain.altitude > 0.0f)
		{
			BirdDying(ctx);
		}
		else
		{
			PlayAnimThenSetState(ctx, AnimalState::Dead);
		}
		break;
	case AnimalState::Dead:
		return Dead(ctx);
	case AnimalState::WaitForAnimation:
		// turns x 100 ms >= the clip's length
		if (VillagerAnimationDone(ctx.entity, ctx.brain.turnsSinceStateChange))
		{
			SetTopState(ctx, static_cast<AnimalState>(ctx.brain.finalState));
		}
		break;
	case AnimalState::MoveInFlock:
	case AnimalState::StartWander:
		if (IsBird(ctx.animal.type))
		{
			BirdStartWander(ctx);
		}
		else if (walker)
		{
			StartWander(ctx);
		}
		break;
	case AnimalState::SpecialMoveToPos:
		SpecialMoveToPos(ctx);
		break;
	case AnimalState::FollowFlock:
		FollowFlock(ctx);
		break;
	case AnimalState::HuntingMoveToPos:
		HuntingMoveToPos(ctx);
		break;
	case AnimalState::TargetPounce:
		TargetPounce(ctx);
		break;
	case AnimalState::Downed:
		// the Dying clip, then being eaten for 300 turns
		PlayAnimThenSetState(ctx, AnimalState::BeingEaten);
		ctx.brain.counter = 300;
		break;
	case AnimalState::BeingEaten:
		BeingEaten(ctx);
		break;
	case AnimalState::HideInLair:
		HideInLair(ctx);
		break;
	case AnimalState::FleeingFromPredatorReaction:
		FleeingFromPredatorReaction(ctx);
		break;
	case AnimalState::GotoFoodReaction:
		GotoFoodReaction(ctx);
		break;
	case AnimalState::ArrivesAtFoodReaction:
		ArrivesAtFoodReaction(ctx);
		break;
	case AnimalState::FleeingFromObjectReaction:
		FleeingFromObjectReaction(ctx);
		break;
	case AnimalState::FleeingAndLookingAtObjectReaction:
		FleeingAndLookingReaction(ctx);
		break;
	case AnimalState::Wander:
		if (ctx.animal.type == AnimalInfo::SpellWolf)
		{
			SetRunToFinalDest(ctx);
		}
		else
		{
			Wander(ctx);
		}
		break;
	case AnimalState::Eat:
		Eat(ctx);
		break;
	case AnimalState::SeekSleep:
		SeekSleep(ctx);
		break;
	case AnimalState::Sleeps:
		Sleeps(ctx);
		break;
	case AnimalState::StartToEat:
		StartToEat(ctx);
		break;
	case AnimalState::FinishEating:
		PlayAnimThenSetState(ctx, AnimalState::DecideWhatToDo);
		break;
	case AnimalState::DecideWhatToDo:
		DecideWhatToDo(ctx);
		break;
	case AnimalState::InteractDecideWhatToDo:
		InteractDecideWhatToDo(ctx);
		break;
	case AnimalState::GivesBirth:
		GivesBirth(ctx);
		break;
	default:
		// IN_HAND, FLYING and the states grazers never reach do nothing
		break;
	}
	return false;
}

/// counters 0, speedDefault, DECIDE_WHAT_TO_DO, the life of info.dat; the sleep place is the flock's domain centre
/// cell at creation
AnimalBrain& Initialise(entt::entity entity, const Animal& animal, const Transform& transform)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto& info = InfoOf(animal);
	auto& brain = registry.Assign<AnimalBrain>(entity);
	brain.speed = static_cast<uint16_t>(std::min<uint32_t>(static_cast<uint32_t>(info.speedGroup.speedDefault), 0xFFFF));
	// game angle 0, facing +x; nothing at creation sets another
	brain.angle = 0;
	// birthTurn = the turn it would have been born to be this old
	brain.birthTurn = static_cast<int32_t>(Turn()) - static_cast<int32_t>(animal.age) * 1500;
	// a bird's altitude (the archetype put it at the land + altitudeNormal)
	if (IsBird(animal.type) && Locator::terrainSystem::has_value())
	{
		brain.altitude = std::max(0.0f, transform.position.y - Locator::terrainSystem::value().GetHeightAt(Xz(transform)));
		brain.goalAltitude = brain.altitude;
	}
	if (const auto* flock = FlockOf(animal); flock != nullptr)
	{
		brain.sleepCell = CellOf({flock->domainCentre.x, flock->domainCentre.z});
	}
	if (!registry.AllOf<Life>(entity))
	{
		registry.Assign<Life>(entity).value = info.life;
	}
	return brain;
}

void Delete(entt::entity entity)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (auto* animal = registry.TryGet<Animal>(entity); animal != nullptr)
	{
		LeaveFlock(entity, *animal);
	}
	physics::PhysicsObjects::RemoveObject(entity);
	// out of its cell's mobile list
	map_cells::RemoveMapObject(entity);
	registry.Destroy(entity);
	registry.SetDirty();
}
} // namespace detail

using namespace detail;

bool BeginAnimalsTurn(float visualTime)
{
	if (!Locator::infoConstants::has_value() || !Locator::terrainSystem::has_value())
	{
		return false;
	}
	Shared().SetVisualTime(visualTime);
	return true;
}

void ProcessAnimal(entt::entity entity)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(entity) || !registry.AllOf<Animal, Transform>(entity))
	{
		return;
	}
	auto& animal = registry.Get<Animal>(entity);
	auto& transform = registry.Get<Transform>(entity);
	auto* brain = registry.TryGet<AnimalBrain>(entity);
	const bool fresh = brain == nullptr;
	if (fresh)
	{
		brain = &Initialise(entity, animal, transform);
	}
	Context ctx {entity, animal, *brain, transform, InfoOf(animal)};
	// a predator made without a flock gets its lair (CalculateLairPosition), which is also its sleep place
	if (const auto* flock = FlockOf(animal);
	    fresh && flock != nullptr && flock->id < 0 && flock->members.size() == 1 && HunterOf(animal.type) != Hunter::None)
	{
		CalculateLairPosition(ctx);
		brain->sleepCell = CellOf({flock->domainCentre.x, flock->domainCentre.z});
	}
	if (ProcessState(ctx))
	{
		// the vanish deletes it in its own turn, off the list for the livings after it (the loop read its next first:
		// living_turn::WalkList)
		Delete(entity);
	}
}

void EndAnimalsTurn()
{
	ProcessDownedVillagers();
	// (the reactions whose initiator went are pruned at the start of the turn: ECS/Effects/Reactions BeginTurn)
	RunDebugHooks(Turn());
}

AnimalState TopState(entt::entity entity)
{
	const auto* brain = Locator::entitiesRegistry::value().TryGet<const AnimalBrain>(entity);
	return brain != nullptr ? static_cast<AnimalState>(brain->topState) : AnimalState::DecideWhatToDo;
}

uint16_t LandType(entt::entity entity)
{
	const auto* brain = Locator::entitiesRegistry::value().TryGet<const AnimalBrain>(entity);
	return brain != nullptr ? (brain->status >> 4) & 3 : 3;
}

bool ValidForPlaceInHand(entt::entity entity)
{
	const auto* animal = Locator::entitiesRegistry::value().TryGet<const Animal>(entity);
	return animal == nullptr || !Locator::infoConstants::has_value() || InfoOf(*animal).playerCanPickUp != 0;
}

namespace detail
{
AnimalBrain* BrainOf(entt::entity entity)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(entity) || !registry.AllOf<Animal, Transform>(entity) || !Locator::infoConstants::has_value())
	{
		return nullptr;
	}
	if (auto* brain = registry.TryGet<AnimalBrain>(entity); brain != nullptr)
	{
		return brain;
	}
	return &Initialise(entity, registry.Get<const Animal>(entity), registry.Get<const Transform>(entity));
}
} // namespace detail

void PlaceInHand(entt::entity entity)
{
	auto* brain = BrainOf(entity);
	if (brain == nullptr)
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	auto& animal = registry.Get<Animal>(entity);
	if (auto* old = FlockOf(animal); old != nullptr)
	{
		const auto oldEntity = animal.flock;
		// a script flock gives up the script reference the flock took for the animal and is kept even empty
		const bool scriptFlock = script_held::IsControlledByScript(oldEntity);
		if (scriptFlock)
		{
			script_held::DecrementReference(entity);
		}
		// out of the old flock (deleted when emptied, unless a script's), then a flock of its own here (no town, max 0) with
		// the old radius and distance
		flocks::SeparateIntoNewFlock(oldEntity, entity, !scriptFlock);
	}
	SetTopState(entity, *brain, AnimalState::InHand);
	NotifyTeleported(entity);
}

void InitialisePhysics(entt::entity entity)
{
	if (auto* brain = BrainOf(entity); brain != nullptr)
	{
		SetTopState(entity, *brain, AnimalState::Flying);
		NotifyTeleported(entity);
	}
}

bool InitialisePhysicsWithoutBody(entt::entity entity)
{
	auto* brain = BrainOf(entity);
	if (brain == nullptr)
	{
		return false;
	}
	if (brain->topState == static_cast<uint8_t>(AnimalState::Flying))
	{
		return true; // straight to the physics
	}
	if (brain->topState != static_cast<uint8_t>(AnimalState::InHand))
	{
		brain->previousState = FinalStateOf(*brain); // stores its previous state
	}
	// SetTopState refuses only through the exit test
	SetTopState(entity, *brain, AnimalState::Flying);
	if (brain->topState != static_cast<uint8_t>(AnimalState::Flying))
	{
		return false;
	}
	// (openblack) its drawn pose restarts here: the original has no draw interpolation of its own
	NotifyTeleported(entity);
	return true;
}

void SetYAngle(entt::entity entity, float angle)
{
	auto* brain = BrainOf(entity);
	if (brain == nullptr)
	{
		return;
	}
	// drawn at the angle + pi / 2 (FaceAngle's convention)
	Locator::entitiesRegistry::value().Get<Transform>(entity).rotation = affine::AngleY(angle + glm::half_pi<float>());
	// the game angle of the heading
	brain->angle = static_cast<uint16_t>(gutils::ConvertAngle3DToGame(angle));
}

void EndPhysics(entt::entity entity, uint16_t landType)
{
	auto* brain = BrainOf(entity);
	if (brain == nullptr)
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	brain->step = glm::ivec2(0);
	// on the land
	brain->altitude = 0.0f;
	brain->bank.SetPosition(0.0f);
	// the drawn position snaps: no slide from where it was
	NotifyTeleported(entity);
	// the land type goes into status bits 4..5
	brain->status = static_cast<uint16_t>((brain->status & ~0x30) | ((landType & 3) << 4));
	static const bool trace = debug_env::AnimalTrace();
	if (trace)
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "Animal {}: end of physics, landType {}", static_cast<uint32_t>(entity),
		                   landType);
	}
	const auto* life = registry.TryGet<const Life>(entity);
	if (life == nullptr || life->value > 0.0f)
	{
		SetTopState(entity, *brain, AnimalState::Landed);
		return;
	}
	const bool wasDying = (brain->status & 1) != 0;
	if (!wasDying)
	{
		SetTopState(entity, *brain, AnimalState::Dying);
		brain->status |= 1;
	}
	brain->counter = k_TurnsToDieOver;
	brain->status = static_cast<uint16_t>((brain->status & ~0x30) | ((landType & 3) << 4));
	if (wasDying)
	{
		// a thrown corpse lies dead again
		SetTopState(entity, *brain, AnimalState::Dead);
	}
}

void PutDown(entt::entity entity)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (!registry.Valid(entity) || !registry.AllOf<Animal, Transform>(entity))
	{
		return;
	}
	// living::EndPhysicsWithoutBody (the one copy, shared with the body's EndPhysics)
	living::EndPhysicsWithoutBody(entity);
}

namespace
{
/// ageToScale: index -1 is the value just before the table (read at age 0), which is altitudeNormal; (openblack,
/// guard) past the 20 floats the last one
float AgeScale(const GAnimalInfo& info, int32_t i)
{
	const auto& values = info.ageToScale.values;
	if (i < 0)
	{
		return info.altitudeNormal;
	}
	return static_cast<size_t>(i) < values.size() ? values.at(static_cast<size_t>(i)) : values.back();
}
} // namespace

float InitialScaleForAge(const GAnimalInfo& info, uint32_t age)
{
	// age < grownUpAge -> ageToScale[age - 1]; else 0.9
	if (age < info.grownUpAge)
	{
		return AgeScale(info, static_cast<int32_t>(age) - 1);
	}
	return 0.9f;
}

float ScaleForAge(const GAnimalInfo& info, uint32_t age, float current)
{
	if (age < info.grownUpAge)
	{
		// (ageToScale[age + 1] - scale) x 0.75, rounded to float
		const float step = (AgeScale(info, static_cast<int32_t>(age) + 1) - current) * 0.75f;
		// scale + GameFloatRand(step), whatever the sign of step
		const float r = GameFloatRand(step);
		return current + r;
	}
	// t = (0.05 - GameFloatRand(0.1)) + 1, each operation rounded to float
	float t = 0.05f - GameFloatRand(0.1f);
	t += 1.0f;
	// scale < t -> a second roll; else the scale
	if (current < t)
	{
		float s = 0.05f - GameFloatRand(0.1f);
		s += 1.0f;
		return s;
	}
	return current;
}

void SetAge(entt::entity entity, uint32_t age)
{
	auto* brain = BrainOf(entity);
	if (brain == nullptr)
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	auto& animal = registry.Get<Animal>(entity);
	auto& transform = registry.Get<Transform>(entity);
	const auto& info = InfoOf(animal);
	// the initial scale, then the scale for the age from it
	transform.scale = glm::vec3(InitialScaleForAge(info, age));
	transform.scale = glm::vec3(ScaleForAge(info, age, transform.scale.x));
	// birth turn = turn - age x 1500
	brain->birthTurn = static_cast<int32_t>(Turn() - age * 1500u);
	// (openblack) the age the component keeps (the readers ask GetAge)
	animal.age = age;
}

uint32_t GetAge(entt::entity entity)
{
	const auto* brain = BrainOf(entity);
	return brain != nullptr ? AgeOf(*brain) : 0;
}

void SetSpeedInMetres(entt::entity entity, float metres)
{
	auto* brain = BrainOf(entity);
	if (brain == nullptr)
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	auto& animal = registry.Get<Animal>(entity);
	Context ctx {entity, animal, *brain, registry.Get<Transform>(entity), InfoOf(animal)};
	// whole MapCoords from metres; SetSpeed clamps to 0..0xFFFF
	SetSpeed(ctx, static_cast<uint32_t>(std::max(gutils::ConvertMetersToWholeDistance(metres), 0)));
}

float GetSpeedInMetres(entt::entity entity)
{
	const auto* brain = BrainOf(entity);
	return brain != nullptr ? gutils::ConvertWholeDistanceToMeters(brain->speed) : 0.0f;
}

bool IsDead(entt::entity entity)
{
	const auto* brain = BrainOf(entity);
	if (brain == nullptr)
	{
		return false;
	}
	// dying, or the TOP state DEAD
	if ((brain->status & 1) != 0 || brain->topState == static_cast<uint8_t>(AnimalState::Dead))
	{
		return true;
	}
	// dead unless functional: available and with some life. (approximate) available: openblack's dead list (ecs::IsAvailable)
	const auto* life = Locator::entitiesRegistry::value().TryGet<const Life>(entity);
	const bool functional = ecs::IsAvailable(entity) && !(life != nullptr && life->value == 0.0f);
	return !functional;
}

void SetScriptState(entt::entity entity, AnimalState state)
{
	auto* brain = BrainOf(entity);
	// a living that is available (not dying: status & 1) and on the map (inferred for an animal: not in the hand)
	if (brain == nullptr || brain->topState == static_cast<uint8_t>(AnimalState::InHand) || (brain->status & 1) != 0)
	{
		return;
	}
	// stores its final state; the exit function runs, its answer not read; no animal state the scripts set has an entry
	// function, so the state is set straight; then its clip and the counter 0
	brain->previousState = FinalStateOf(*brain);
	CallExitStateFunction(*brain, state);
	brain->topState = static_cast<uint8_t>(state);
	brain->turnsSinceStateChange = 0;
	SetAnimalStateAnim(entity);
	brain->counter = 0;
}

void ScriptMoveTo(entt::entity entity, glm::vec2 position)
{
	auto* brain = BrainOf(entity);
	// the script move on a living: on the map and not drowning (an animal never drowns), then already there ->
	// SetScriptState(IN_SCRIPT), else SetupMoveToPos(pos, IN_SCRIPT)
	if (brain == nullptr || brain->topState == static_cast<uint8_t>(AnimalState::InHand))
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	auto& animal = registry.Get<Animal>(entity);
	Context ctx {entity, animal, *brain, registry.Get<Transform>(entity), InfoOf(animal)};
	const glm::vec2 d = position - Xz(ctx.transform);
	const float step = Metres(brain->speed);
	if (glm::dot(d, d) <= step * step)
	{
		SetScriptState(entity, AnimalState::InScript);
		return;
	}
	SetupMoveToPos(ctx, position, AnimalState::InScript);
}

void DestroyedByEffect(entt::entity entity)
{
	if (auto* brain = BrainOf(entity); brain != nullptr)
	{
		SetDying(entity, *brain);
	}
}

void SetTown(entt::entity animal, entt::entity town)
{
	auto& registry = Locator::entitiesRegistry::value();
	auto* a = registry.TryGet<Animal>(animal);
	if (a == nullptr)
	{
		return;
	}
	// the animal's town; no town (or no animal) -> nothing more
	a->town = town;
	auto* t = town != entt::null ? registry.TryGet<components::Town>(town) : nullptr;
	if (t == nullptr)
	{
		return;
	}
	// at the head of the town's animal list
	t->animals.insert(t->animals.begin(), animal);
}

void Forget(entt::entity entity)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (auto* animal = registry.TryGet<Animal>(entity); animal != nullptr)
	{
		// off its town's list, no town, then off its flock. No prey / hunter link is cleared there: the readers ask
		// IsAvailable (detail::Available)
		auto* town = animal->town != entt::null ? registry.TryGet<components::Town>(animal->town) : nullptr;
		if (town != nullptr)
		{
			std::erase(town->animals, entity);
		}
		animal->town = entt::null;
		LeaveFlock(entity, *animal);
	}
	// (inferred) a deleted living leaves the living list, so its walk and the predator's hold on a downed villager stop:
	// openblack's walk tags and DownedVillager go
	registry.Remove<components::DownedVillager, components::MoveStateLinearTag, components::MoveStateOrbitTag,
	                components::MoveStateExitCircleTag, components::MoveStateStepThroughTag, components::MoveStateFinalStepTag,
	                components::MoveStateArrivedTag, components::WallHugObjectReference>(entity);
}

} // namespace openblack::ecs::animal_ai
