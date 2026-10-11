/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "GrazerRules.h"

#include <cmath>
#include <cstdlib>

#include <algorithm>
#include <limits>

#include "3D/MapCoords.h"
#include "Animals/AnimalMove.h"
#include "Common/GUtilsAngle.h"
#include "Common/GUtilsDistance.h"

using namespace openblack;
using namespace openblack::animals;
using namespace openblack::animals::grazers;

namespace
{
/// One clip of each slot for a kind: the walk and run (and a horse's trot), the two grazing clips, and the rest
struct KindClips
{
	AnimId walk;
	AnimId trot;
	AnimId run;
	AnimId eat1;
	AnimId eat2;
	AnimId stand;
	AnimId startToEat;
	AnimId finishEating;
	AnimId inHand;
	AnimId thrown;
	AnimId dying;
};

KindClips ClipsOf(AnimalInfo type)
{
	switch (type)
	{
	case AnimalInfo::Sheep:
		return {AnimId::ASheepWalk,   AnimId::ASheepRun,    AnimId::ASheepRun,        AnimId::ASheepEat1,
		        AnimId::ASheepEat2,   AnimId::ASheepStand,  AnimId::ASheepGotoEat,    AnimId::ASheepOutOfEat,
		        AnimId::ASheepInHand, AnimId::ASheepThrown, AnimId::ASheepFallDeadlhs};
	case AnimalInfo::Horse:
		// A horse falls dead in the clip it then lies in
		return {AnimId::AHorseWalk,   AnimId::AHorseTrot,   AnimId::AHorseRun,     AnimId::AHorseEat1,
		        AnimId::AHorseEat2,   AnimId::AHorseStand,  AnimId::AHorseGotoEat, AnimId::AHorseUpFromEat,
		        AnimId::AHorseInHand, AnimId::AHorseThrown, AnimId::AHorseDeadlhs};
	case AnimalInfo::Pig:
		return {AnimId::APigWalk,   AnimId::APigRun,    AnimId::APigRun,        AnimId::APigEat1,
		        AnimId::APigEat2,   AnimId::APigStand,  AnimId::APigGotoEat,    AnimId::APigOutOfEat,
		        AnimId::APigInHand, AnimId::APigThrown, AnimId::APigFallDeadlhs};
	case AnimalInfo::Tortoise:
		// A tortoise walks, and stands for everything else
		return {AnimId::ATortoiseWalk,  AnimId::ATortoiseWalk,  AnimId::ATortoiseWalk,  AnimId::ATortoiseStand,
		        AnimId::ATortoiseStand, AnimId::ATortoiseStand, AnimId::ATortoiseStand, AnimId::ATortoiseStand,
		        AnimId::ATortoiseStand, AnimId::ATortoiseStand, AnimId::ATortoiseStand};
	case AnimalInfo::Cow:
	default:
		return {AnimId::ACowWalk,   AnimId::ACowRun,    AnimId::ACowRun,     AnimId::ACowEat_1,
		        AnimId::ACowEat_2,  AnimId::ACowStand,  AnimId::ACowGotoEat, AnimId::ACowUpfromEat,
		        AnimId::ACowInHand, AnimId::ACowThrown, AnimId::ACowDie};
	}
}

/// The truncation the game's float-to-whole conversion makes
int32_t Truncate(double value)
{
	return static_cast<int32_t>(std::trunc(value));
}

int32_t LargerSide(glm::ivec2 v)
{
	return std::max(std::abs(v.x), std::abs(v.y));
}
} // namespace

bool grazers::IsGrazer(AnimalInfo type)
{
	switch (type)
	{
	case AnimalInfo::Sheep:
	case AnimalInfo::Tortoise:
	case AnimalInfo::Cow:
	case AnimalInfo::Horse:
	case AnimalInfo::Pig:
		return true;
	default:
		return false;
	}
}

SpeedThreshold grazers::ThresholdRowOf(AnimalInfo type)
{
	return type == AnimalInfo::Horse ? SpeedThreshold::AnimalHorse : SpeedThreshold::AnimalCow;
}

AnimId grazers::Clip(AnimalInfo type, ClipSlot slot, uint16_t speed, SpeedThresholds thresholds,
                     const std::function<uint32_t(uint32_t)>& roll)
{
	const auto clips = ClipsOf(type);
	switch (slot)
	{
	case ClipSlot::Move:
		if (type == AnimalInfo::Tortoise)
		{
			return clips.walk;
		}
		if (type == AnimalInfo::Horse)
		{
			// A horse walks up to the walking speed, trots up to the running one and gallops past it
			if (speed <= thresholds.walk)
			{
				return clips.walk;
			}
			return speed > thresholds.run ? clips.run : clips.trot;
		}
		return speed > thresholds.walk ? clips.run : clips.walk;
	case ClipSlot::Eat:
		if (type == AnimalInfo::Tortoise)
		{
			return clips.eat1;
		}
		return roll(2) != 0 ? clips.eat1 : clips.eat2;
	case ClipSlot::StartToEat:
		return clips.startToEat;
	case ClipSlot::FinishEating:
		return clips.finishEating;
	case ClipSlot::InHand:
		return clips.inHand;
	case ClipSlot::Thrown:
		return clips.thrown;
	case ClipSlot::Dying:
		return clips.dying;
	case ClipSlot::Sleep:
	case ClipSlot::Stand:
	default:
		return clips.stand;
	}
}

int32_t grazers::Scale(int32_t value, int32_t num, int32_t den)
{
	return Truncate(static_cast<double>(value) * static_cast<double>(num) / static_cast<double>(den));
}

bool grazers::AddSteer(glm::ivec2& step, glm::ivec2 push, uint16_t speed)
{
	const int32_t left = static_cast<int32_t>(speed) - LargerSide(step);
	if (left <= 0)
	{
		return true;
	}
	const int32_t taken = std::min(LargerSide(push), left);
	const auto share = [&](int32_t value) {
		return static_cast<double>(value) * static_cast<double>(taken) / static_cast<double>(speed);
	};
	// The game keeps the z share as a float before truncating it, and truncates the x share as it is
	step.y += Truncate(static_cast<double>(static_cast<float>(share(push.y))));
	step.x += Truncate(share(push.x));
	return taken == left;
}

bool grazers::HerdSteer(glm::ivec2& step, glm::ivec2 position, uint16_t speed, std::span<const HerdMate> mates,
                        uint16_t flockDistance)
{
	if (static_cast<int32_t>(speed) - LargerSide(step) <= 0)
	{
		return true;
	}
	if (mates.empty())
	{
		return false;
	}
	glm::ivec2 sum {0};
	int32_t nearestDistance = std::numeric_limits<int32_t>::max();
	const HerdMate* nearest = nullptr;
	for (const auto& mate : mates)
	{
		const int32_t distance = std::abs(position.x - mate.position.x) + std::abs(position.y - mate.position.y);
		if (distance < nearestDistance)
		{
			nearestDistance = distance;
			nearest = &mate;
		}
		sum += mate.position;
	}
	const auto count = static_cast<int32_t>(mates.size());
	const int32_t s = speed;
	// A fifth of its speed towards the middle of the others
	const auto towards = StepAlong(gutils::GetAngleFromXZ(position, sum / count), speed);
	glm::ivec2 push {Scale(towards.x, s / 5, s), Scale(towards.y, s / 5, s)};
	if (AddSteer(step, push, speed))
	{
		return true;
	}
	// Its nearest mate's pull, axis by axis, added to the first push, which so counts twice
	const glm::ivec2 apart = nearest->position - position;
	for (int axis = 0; axis < 2; ++axis)
	{
		const int32_t sign = apart[axis] < 0 ? -1 : 1;
		const auto off = static_cast<uint32_t>(std::abs(apart[axis]));
		if (off > flockDistance)
		{
			push[axis] += sign * ((s / 5 * 8) / 5);
		}
		else if (off < flockDistance)
		{
			push[axis] += 2 * sign * -(s / 5);
		}
	}
	if (AddSteer(step, push, speed))
	{
		return true;
	}
	// Three fifths of the nearest mate's step
	return AddSteer(step, {Scale(nearest->step.x, s * 3 / 5, s), Scale(nearest->step.y, s * 3 / 5, s)}, speed);
}

glm::ivec2 grazers::NewWanderStep(const WanderSetup& setup, std::span<const HerdMate> mates,
                                  const std::function<uint32_t(uint32_t)>& roll)
{
	glm::ivec2 step {0};
	const int32_t s = setup.speed;
	if (setup.centre.has_value())
	{
		// The distance in whole metres, compared with the whole distances
		const int32_t distance = Truncate(static_cast<double>(gutils::GetDistanceInMetres(*setup.centre, setup.position)));
		std::optional<uint16_t> angle;
		if (distance > setup.outer)
		{
			angle = gutils::GetAngleFromXZ(setup.position, *setup.centre);
		}
		else if (distance < setup.inner)
		{
			angle = gutils::GetAngleFromXZ(*setup.centre, setup.position);
		}
		// Straight along +x counts as no way to go
		if (angle.has_value() && *angle != 0)
		{
			const auto full = StepAlong(*angle, setup.speed);
			AddSteer(step, {Scale(full.x, s * 9 / 10, s), Scale(full.y, s * 9 / 10, s)}, setup.speed);
		}
	}
	if (!HerdSteer(step, setup.position, setup.speed, mates, setup.flockDistance))
	{
		const int32_t turn = setup.turnAngle;
		const auto random = static_cast<int32_t>(roll(static_cast<uint32_t>(turn)));
		const auto angle = static_cast<uint16_t>((setup.angle - (turn >> 1) + random) & gutils::k_GameAngleMask);
		AddSteer(step, StepAlong(angle, setup.speed), setup.speed);
	}
	return step;
}

void grazers::GrowNeeds(Needs& needs, const NeedLimits& limits, uint32_t age, std::optional<HerdSize> herd)
{
	if (limits.breed != 0 && herd.has_value() && herd->members > 1 && herd->members < herd->most &&
	    needs.breed < static_cast<int32_t>(limits.breed) && age >= limits.grownUpAge)
	{
		++needs.breed;
	}
	if (limits.hunger != 0 && needs.hunger < static_cast<int32_t>(limits.hunger))
	{
		++needs.hunger;
	}
	if (limits.sleep != 0 && needs.sleep < static_cast<int32_t>(limits.sleep))
	{
		++needs.sleep;
	}
}

bool grazers::ReadyToBreed(Needs& needs, const NeedLimits& limits, std::optional<HerdSize> herd)
{
	if (limits.breed == 0 || needs.breed < static_cast<int32_t>(limits.breed))
	{
		return false;
	}
	if (herd.has_value() && herd->members < herd->most)
	{
		return true;
	}
	needs.breed = 0;
	return false;
}

Need grazers::NeedToSee(Needs& needs, const NeedLimits& limits, uint32_t age, std::optional<HerdSize> herd,
                        bool hasSleepingPlace)
{
	if (limits.breed != 0 && age >= limits.grownUpAge)
	{
		++needs.breed;
		if (needs.breed >= static_cast<int32_t>(limits.breed))
		{
			needs.breed = 0;
			if (herd.has_value() && herd->most > herd->members)
			{
				return Need::Breed;
			}
		}
	}
	if (limits.hunger != 0 && needs.hunger >= static_cast<int32_t>(limits.hunger))
	{
		return Need::Graze;
	}
	if (limits.sleep != 0 && needs.sleep >= static_cast<int32_t>(limits.sleep) && hasSleepingPlace)
	{
		return Need::Sleep;
	}
	return Need::None;
}

int32_t grazers::GrazeSearchCells(uint16_t domainRadius)
{
	const float tenth = static_cast<float>(domainRadius) / 10.0f;
	return Truncate(static_cast<double>(tenth) * static_cast<double>(tenth));
}

std::optional<glm::ivec2> grazers::FindGrazeSpot(glm::ivec2 position, uint16_t domainRadius,
                                                 const std::function<bool(glm::ivec2)>& suits)
{
	map_coords::Spiral spiral;
	glm::ivec2 point = position;
	for (int32_t left = GrazeSearchCells(domainRadius); left > 0; --left)
	{
		if (suits(point))
		{
			return point;
		}
		const auto& next = spiral.Next();
		point += glm::ivec2(next.x, next.z) * map_coords::k_FixedPerCell;
	}
	return std::nullopt;
}

glm::ivec2 grazers::SleepSpot(glm::ivec2 cell, uint32_t members, const std::function<float(float)>& random)
{
	// The cell's corner, as a whole map position
	const auto corner =
	    glm::ivec2(map_coords::FtoL(static_cast<float>(cell.x) * map_coords::k_CellSize * map_coords::k_FixedPerMetre),
	               map_coords::FtoL(static_cast<float>(cell.y) * map_coords::k_CellSize * map_coords::k_FixedPerMetre));
	const float size = static_cast<float>(2 * members);
	const float half = size * 0.5f;
	const float dx = half - random(size);
	const float dz = half - random(size);
	const auto place = [](int32_t centre, float offset) {
		const float metres = static_cast<float>(centre) * 10.0f * (1.0f / 65536.0f) + offset;
		return map_coords::FtoL(metres * 65536.0f / 10.0f);
	};
	return {place(corner.x, dx), place(corner.y, dz)};
}

bool grazers::SleepTurn(Needs& needs)
{
	needs.sleep = static_cast<int16_t>(needs.sleep - 2);
	if (needs.sleep > 0)
	{
		return false;
	}
	needs.sleep = 0;
	return true;
}

float grazers::GrownScale(float scale, uint32_t age, std::span<const float> ageToScale,
                          const std::function<float(float)>& random)
{
	if (static_cast<size_t>(age) + 1 >= ageToScale.size())
	{
		return scale;
	}
	constexpr double k_GrowingShare = 0.75;
	const auto towards = static_cast<float>(static_cast<double>(ageToScale[age + 1] - scale) * k_GrowingShare);
	return scale + random(towards);
}

bool grazers::MergeKeepsLooker(uint32_t looker, uint32_t other, bool otherScripted)
{
	return other <= looker && !otherScripted;
}

uint32_t grazers::MergedMost(uint32_t keeperMost, uint32_t otherMost, uint32_t joining, uint32_t maxFlockSize)
{
	for (uint32_t i = 0; i < joining; ++i)
	{
		keeperMost = std::min(keeperMost + otherMost, maxFlockSize);
	}
	return keeperMost;
}
