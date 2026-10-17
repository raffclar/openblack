/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureStomp.h"

#include <cmath>

#include <algorithm>
#include <numbers>

#include <glm/geometric.hpp>

#include "Common/X87.h"
#include "Common/X87Vector.h"

using namespace openblack;
using namespace openblack::creature_stomp;

namespace
{
/// The clips of the creature spec file that change the spheres
constexpr size_t k_StartSleep = 28;
constexpr size_t k_Sleep = 29;
constexpr size_t k_EndSleep = 30;
constexpr size_t k_Faint = 106;
constexpr size_t k_FightStartCast = 110;
constexpr size_t k_FightStart = 122;
constexpr size_t k_FightStance = 123;

/// The head is this big at size 0, and gets smaller by this for each unit of size up to size 2
constexpr float k_HeadScaleAtZero = 1.8f;
constexpr float k_HeadScalePerSize = 0.5f;
/// The hands and feet are this big at size 0, and get smaller by this for each unit of size up to size 2
constexpr float k_LimbScaleAtZero = 1.5f;
constexpr float k_LimbScalePerSize = 0.25f;
constexpr float k_MaxScaleSize = 2.0f;
/// The sphere of a part is this share of the height of the creature: an eighth, and a sixth for the groin
constexpr float k_PartShare = 0.125f;
constexpr float k_GroinShare = 1.0f / 6.0f;

/// The kick turns the second blow up to a quarter turn from the way the creature faces, in tenths of a quarter turn
constexpr uint32_t k_SideMiddle = 5;
constexpr float k_SideStep = 0.1f;
constexpr float k_HeightStep = 0.01f;
} // namespace

bool creature_stomp::CanBeStompedOn(const StompTarget& target, float creatureSize)
{
	const auto creatureHeight = k_HeightPerSize * creatureSize;
	switch (target.rule)
	{
	case StompRule::Never:
		return false;
	case StompRule::Always:
		return true;
	case StompRule::WhileAlive:
		return !target.dead;
	case StompRule::LowBird:
		return target.heightAboveLand < creatureHeight * k_LowShare;
	case StompRule::Short:
		return target.onMap && target.height < k_ShortShare * creatureHeight;
	case StompRule::LowBuilding:
		return target.height < creatureHeight * k_LowShare;
	}
	return false;
}

bool creature_stomp::CanBeKicked(const KickTarget& target, float creatureWeight)
{
	switch (target.rule)
	{
	case KickRule::Never:
		return false;
	case KickRule::Building:
		return !target.onFire && target.built;
	case KickRule::Thing:
		break;
	}
	if (target.leashTied)
	{
		return false;
	}
	if (target.creature)
	{
		return true;
	}
	return target.canFly && !target.immovable && target.weight < k_KickWeightTimes * creatureWeight && !target.indestructible &&
	       !target.heldByKicker;
}

float creature_stomp::KickHitStrength(float kickerHeight, float otherHeight)
{
	const auto ratio = otherHeight > 0.0f ? std::clamp(kickerHeight / otherHeight, 0.0f, 1.0f) : 1.0f;
	return (ratio + 1.0f) * k_KickHitShare;
}

glm::vec3 creature_stomp::KickMiddle(glm::vec3 creature, glm::vec3 foot)
{
	return ((foot - creature) * 0.5f) + creature;
}

std::array<Blow, 2> creature_stomp::KickBlows(const BuildingKick& kick, float landHeight)
{
	auto middle = KickMiddle(kick.creature, kick.foot);
	middle.y = std::max(middle.y, landHeight);
	const auto reach = kick.creatureSize * k_BlowReachPerSize;
	const auto above = std::min(middle.y - landHeight, reach);
	const auto side = (static_cast<float>(kick.sideDraw) - static_cast<float>(k_SideMiddle)) * k_SideStep;
	const auto angle = (side * (std::numbers::pi_v<float> / 2.0f)) + kick.heading.Radians();
	const auto blowReach = std::min(kick.buildingRadius * k_KickReachShare, reach);
	const auto at = [&](uint32_t draw) {
		return glm::vec3 {middle.x, landHeight + (above * (static_cast<float>(draw) * k_HeightStep)), middle.z};
	};
	return {
	    Blow {.point = at(kick.firstHeightDraw), .velocity = kick.direction * k_KickBlowSpeed, .reach = blowReach},
	    Blow {.point = at(kick.secondHeightDraw),
	          // The FPU's wide sine and cosine go into the products
	          .velocity = {x87::Mul(x87::Sin(angle), k_KickBlowSpeed), 0.0f, x87::Mul(x87::Cos(angle), -k_KickBlowSpeed)},
	          .reach = blowReach},
	};
}

float creature_stomp::SphereRadius(Part part, float creatureSize)
{
	const auto limited = std::clamp(creatureSize, 0.0f, k_MaxScaleSize);
	const auto height = k_HeightPerSize * creatureSize;
	switch (part)
	{
	case Part::Head:
		return (k_HeadScaleAtZero - (k_HeadScalePerSize * limited)) * height * k_PartShare;
	case Part::Groin:
		return height * k_GroinShare;
	case Part::RightHand:
	case Part::LeftHand:
	case Part::RightFoot:
	case Part::LeftFoot:
		return (k_LimbScaleAtZero - (k_LimbScalePerSize * limited)) * height * k_PartShare;
	case Part::RightArmpit:
	case Part::LeftArmpit:
		return height * k_PartShare;
	}
	return 0.0f;
}

Switch creature_stomp::SwitchFor(const BodyStart& start)
{
	if (start.stands)
	{
		return Switch::Off;
	}
	if (!start.animation.has_value())
	{
		return Switch::Keep;
	}
	using Phase = BodyStart::Phase;
	switch (*start.animation)
	{
	case k_StartSleep:
	case k_EndSleep:
	case k_FightStart:
	case k_FightStartCast:
		return Switch::On;
	case k_Sleep:
	case k_FightStance:
		return Switch::Off;
	case k_Faint:
		// The fall turns them on; the creature that lies in the faint knocks nothing
		return start.phase == Phase::Start ? Switch::On : start.phase == Phase::Loop ? Switch::Off : Switch::Keep;
	default:
		break;
	}
	return Switch::Keep;
}

bool creature_stomp::MovedEnough(const glm::vec3& from, const glm::vec3& to, float creatureSize)
{
	const auto move = to - from;
	const auto limit = creatureSize * k_MoveShare;
	// The squares add x, z, then y
	return x87::Dot<x87::SumOrder::Xzy>(move, move) > limit * limit;
}

Victim creature_stomp::VictimOf(const VictimFacts& facts)
{
	if (!facts.hasModel || facts.indestructible || facts.passedBy)
	{
		return Victim::None;
	}
	if (facts.villager || facts.animal)
	{
		return facts.immovable ? Victim::None : Victim::Living;
	}
	if (facts.living)
	{
		return Victim::None;
	}
	if (facts.building)
	{
		return Victim::Building;
	}
	return facts.canFly && !facts.immovable ? Victim::Thing : Victim::None;
}

bool creature_stomp::InReach(Victim victim, float groundDistanceSquared, float sphereRadius, float thingRadius)
{
	// The game compares the squares, with no square root
	const auto within = [groundDistanceSquared](float reach) { return groundDistanceSquared < reach * reach; };
	switch (victim)
	{
	case Victim::None:
		return false;
	case Victim::Building:
		return within(thingRadius + sphereRadius);
	case Victim::Living:
	case Victim::Thing:
		return within(thingRadius + (sphereRadius * k_KnockReachShare));
	}
	return false;
}

glm::vec3 creature_stomp::KnockVelocity(const glm::vec3& move)
{
	return move * k_KnockSpeedPerMove;
}

Blow creature_stomp::TrampleBlow(const glm::vec3& point, const glm::vec3& move, float sphereRadius)
{
	// The length adds the squares of x, z, then y. The speed is the length times the move's scale and share, so a move
	// of no length gives no speed.
	if (move == glm::vec3(0.0f))
	{
		return {.point = point, .velocity = glm::vec3(0.0f), .reach = sphereRadius};
	}
	const float length = x87::Length<x87::SumOrder::Xzy>(move);
	const auto way = move * (1.0f / length);
	const auto speed = (length * k_TrampleBlowPerMove) * k_TrampleBlowShare;
	return {.point = point, .velocity = way * speed, .reach = sphereRadius};
}
