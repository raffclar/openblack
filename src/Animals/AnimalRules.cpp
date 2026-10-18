/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "AnimalRules.h"

#include <cmath>
#include <cstdlib>

#include <algorithm>
#include <numbers>

#include <glm/geometric.hpp>
#include <glm/gtx/euler_angles.hpp>

#include "3D/MapCoords.h"
#include "Common/GUtilsAngle.h"
#include "Common/X87.h"
#include "InfoConstants.h"

using namespace openblack;

namespace
{
constexpr float k_Pi = std::numbers::pi_v<float>;
constexpr float k_TwoPi = 2.0f * k_Pi;
/// The tables count a full turn as this
constexpr float k_FullTurn = 2048.0f;
} // namespace

float animals::TurnAngleRadians(int turnAngle)
{
	return static_cast<float>(turnAngle) * k_TwoPi / k_FullTurn;
}

bool animals::FrightensCreatures(AnimalInfo type)
{
	switch (type)
	{
	case AnimalInfo::Bat:
	case AnimalInfo::SpellBat:
	case AnimalInfo::Vulture:
	case AnimalInfo::Lion:
	case AnimalInfo::Tiger:
	case AnimalInfo::Leopard:
	case AnimalInfo::Wolf:
	case AnimalInfo::SpellWolf:
		return true;
	default:
		return false;
	}
}

std::optional<AnimId> animals::DyingClip(AnimalInfo type)
{
	switch (type)
	{
	case AnimalInfo::Lion:
		return AnimId::ALionDie;
	case AnimalInfo::Leopard:
		return AnimId::ALeopardDie;
	case AnimalInfo::Tiger:
		return AnimId::ATigerDie;
	case AnimalInfo::Wolf:
		return AnimId::AWolfDie;
	case AnimalInfo::Cow:
		return AnimId::ACowDie;
	case AnimalInfo::Sheep:
		return AnimId::ASheepFallDeadlhs;
	case AnimalInfo::Horse:
		return AnimId::AHorseDeadlhs;
	case AnimalInfo::Pig:
		return AnimId::APigFallDeadlhs;
	case AnimalInfo::Tortoise:
		return AnimId::ATortoiseStand;
	default:
		return std::nullopt;
	}
}

std::optional<AnimId> animals::DeadClip(AnimalInfo type, bool onRightSide)
{
	// The big cats and wolves lie in their sleeping clip. The farm animals lie on their right side only when they last came
	// down on their right side. Else they lie on their left side
	switch (type)
	{
	case AnimalInfo::Lion:
		return AnimId::ALionSleep;
	case AnimalInfo::Leopard:
		return AnimId::ALeopardSleep;
	case AnimalInfo::Tiger:
		return AnimId::ATigerSleep;
	case AnimalInfo::Wolf:
		return AnimId::AWolfSleep;
	case AnimalInfo::Cow:
		return onRightSide ? AnimId::ACowDeadOnRhs : AnimId::ACowDeadOnLhs;
	case AnimalInfo::Sheep:
		return onRightSide ? AnimId::ASheepDeadRhs : AnimId::ASheepDeadLhs;
	case AnimalInfo::Horse:
		return onRightSide ? AnimId::AHorseDeadrhs : AnimId::AHorseDeadlhs;
	case AnimalInfo::Pig:
		return onRightSide ? AnimId::APigDeadrhs : AnimId::APigDeadlhs;
	case AnimalInfo::Tortoise:
		return AnimId::ATortoiseStand;
	default:
		return std::nullopt;
	}
}

float animals::BirthScale(uint32_t age, uint32_t grownUpAge, std::span<const float> ageToScale,
                          const std::function<float(float)>& random)
{
	constexpr float k_GrownSize = 0.9f;
	constexpr float k_GrownSpread = 0.05f;
	constexpr float k_GrownSpreadRange = 0.1f;
	constexpr double k_GrowingShare = 0.75;
	if (age < grownUpAge && age >= 1 && age + 1 < ageToScale.size())
	{
		// The table's size of its age, grown up to three quarters of the way to the size two years on
		const float size = ageToScale[age - 1];
		const auto towards = static_cast<float>(static_cast<double>(ageToScale[age + 1] - size) * k_GrowingShare);
		return size + random(towards);
	}
	// Grown: a size about a whole, which beats nine tenths, so it is drawn again
	float size = k_GrownSize;
	const float first = (k_GrownSpread - random(k_GrownSpreadRange)) + 1.0f;
	if (!(first <= size))
	{
		size = (k_GrownSpread - random(k_GrownSpreadRange)) + 1.0f;
	}
	return size;
}

glm::mat3 animals::Orientation(float heading, float bank)
{
	return glm::mat3(glm::eulerAngleY(-(heading + (k_Pi * 0.5f))) * glm::eulerAngleZ(bank));
}

animals::FormationSlot animals::FormationSlotOf(int place)
{
	int k = place;
	int b = 1;
	if (k >= 1)
	{
		do
		{
			++b;
		} while (k >= b * b);
	}
	int sign = 1;
	if (k % 2 == 0)
	{
		k -= 1;
		sign = -1;
	}
	return {.row = b - 5, .column = (((b * b) - k) * sign + 1) / 2};
}

glm::ivec2 animals::FormationGoal(glm::ivec2 leader, glm::ivec2 follower, FormationSlot slot)
{
	// The slot's angle plus the way from the leader to the follower, in game angles
	const auto slotAngle = gutils::GetAngleFromDXDZ(slot.row, slot.column);
	const auto away = gutils::GetAngleFromXZ(follower, leader) + 0x400;
	const auto angle = static_cast<double>(gutils::ConvertGameAngleTo3D(static_cast<int32_t>(slotAngle + away)));
	// The row's spacing along the angle's x and the column's along its z. The game keeps the spacing as a double, and
	// each product rounds to a float. The sum with the leader's metres truncates to a map position
	const auto spacing = static_cast<double>(k_FormationSpacing);
	const auto alongX = x87::Mul(x87::Mul(x87::Cos(angle), static_cast<double>(slot.row)), spacing);
	const auto alongZ = x87::Mul(x87::Mul(x87::Sin(angle), static_cast<double>(slot.column)), spacing);
	const auto x = map_coords::ToMetres(leader.x) + alongX;
	const auto z = map_coords::ToMetres(leader.y) + alongZ;
	return {map_coords::ToFixedGUtils(x), map_coords::ToFixedGUtils(z)};
}

bool animals::DeadBodyGoes(int16_t& turnsLeft, bool scriptControlled)
{
	if (scriptControlled)
	{
		return false;
	}
	const bool goes = turnsLeft == 0;
	turnsLeft = static_cast<int16_t>(static_cast<uint16_t>(turnsLeft) - 1u);
	return goes;
}

bool animals::TakesOwnTurn(const AnimalHolders& holders, bool dyingOrDead)
{
	if (holders.tornado || holders.playersHand || holders.physics)
	{
		return false;
	}
	return !holders.creature || dyingOrDead;
}

glm::vec3 animals::DeadBodySmokePlace(const glm::vec3& position, float heading, float height)
{
	const float out = height * 0.5f;
	return position + glm::vec3(std::cos(heading) * out, 0.0f, std::sin(heading) * out);
}

bool animals::InterestedInFood(const FoodInterest& facts)
{
	if (!facts.available || facts.inPhysics || facts.hungerLimit == 0)
	{
		return false;
	}
	// The game compares the limit and the hunger as signed numbers
	if (static_cast<int32_t>(facts.hungerLimit) > static_cast<int32_t>(facts.hunger))
	{
		return false;
	}
	return (static_cast<uint32_t>(facts.eats) & static_cast<uint32_t>(facts.food)) != 0;
}

uint32_t animals::FoodPriority(bool holdsFood, float distance, float furthest, bool interested, uint32_t tablePriority)
{
	if (!holdsFood || !(distance <= furthest))
	{
		return 0;
	}
	return interested ? tablePriority : 0;
}

uint16_t animals::LookTowards(uint16_t facing, uint16_t target, uint16_t step)
{
	constexpr int32_t k_HalfTurn = 0x400;
	constexpr int32_t k_AngleMask = 0x7FF;
	const int32_t difference = static_cast<int32_t>(target) - static_cast<int32_t>(facing);
	const int32_t size = std::abs(difference);
	if (size < static_cast<int32_t>(step))
	{
		return target;
	}
	// It turns forwards when the target is ahead by less than half a turn, or behind by more than half a turn
	const bool forwards = difference > 0 ? difference < k_HalfTurn : size >= k_HalfTurn;
	const int32_t change = forwards ? static_cast<int32_t>(step) : -static_cast<int32_t>(step);
	return static_cast<uint16_t>((static_cast<int32_t>(facing) + change) & k_AngleMask);
}

uint8_t animals::StateAfterReacting(uint8_t stateBefore, std::span<const GAnimalStateTableInfo> table)
{
	if (stateBefore >= table.size())
	{
		return 0;
	}
	return static_cast<uint8_t>(table[stateBefore].resumeState);
}
