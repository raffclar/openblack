/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "BirdRules.h"

#include <cmath>

using namespace openblack;
using namespace openblack::animals;

namespace
{
/// The land's birds tilt half a radian into a turn over two seconds, the miracles' over half a second
constexpr birds::Bank k_LandBank {.angle = 0.5f, .seconds = 2.0f};
constexpr birds::Bank k_SpellBank {.angle = 0.5f, .seconds = 0.5f};
/// A bird a land script makes at no age is this many years old, and up to so many more: fewer joining a flock
constexpr uint32_t k_YoungestBorn = 5;
constexpr uint32_t k_AgeRangeInFlock = 20;
constexpr uint32_t k_AgeRangeAlone = 40;
/// The temple's flock follows its model's height, scaled, 10 m above it
constexpr float k_TempleFlockAbove = 10.0f;
} // namespace

bool birds::IsBird(AnimalInfo type)
{
	switch (type)
	{
	case AnimalInfo::Vulture:
	case AnimalInfo::CitadelDove:
	case AnimalInfo::CitadelBat:
	case AnimalInfo::SpellDove:
	case AnimalInfo::SpellBat:
		return true;
	default:
		return IsLandBird(type);
	}
}

bool birds::IsLandBird(AnimalInfo type)
{
	switch (type)
	{
	case AnimalInfo::Crow:
	case AnimalInfo::Dove:
	case AnimalInfo::Swallow:
	case AnimalInfo::Pigeon:
	case AnimalInfo::Seagull:
	case AnimalInfo::Bat:
		return true;
	default:
		return false;
	}
}

AnimId birds::FlyingClip(AnimalInfo type, const std::function<uint32_t(uint32_t)>& roll)
{
	switch (type)
	{
	case AnimalInfo::Crow:
		return roll(2) != 0 ? AnimId::CrowGlide : AnimId::CrowFlap;
	case AnimalInfo::Dove:
		return roll(2) != 0 ? AnimId::DoveGlide : AnimId::DoveFlap;
	case AnimalInfo::Pigeon:
		return roll(2) != 0 ? AnimId::PigeonGlide : AnimId::PigeonFlap;
	case AnimalInfo::Seagull:
		return roll(2) != 0 ? AnimId::SeagullGlide : AnimId::SeagullFlap;
	case AnimalInfo::Swallow:
		switch (roll(3))
		{
		case 0:
			return AnimId::SwallowFlap;
		case 1:
			return AnimId::SwallowEraticglide;
		default:
			return AnimId::SwallowCalmglide;
		}
	case AnimalInfo::Bat:
	default:
		return AnimId::BatFlap;
	}
}

std::optional<AnimId> birds::DeadClip(AnimalInfo type)
{
	switch (type)
	{
	case AnimalInfo::Crow:
		return AnimId::CrowGlide;
	case AnimalInfo::Dove:
		return AnimId::DoveFlap;
	case AnimalInfo::Pigeon:
		return AnimId::PigeonGlide;
	case AnimalInfo::Seagull:
		return AnimId::SeagullGentleflap;
	case AnimalInfo::Swallow:
		return AnimId::SwallowEraticglide;
	case AnimalInfo::Bat:
		return AnimId::BatGlide;
	default:
		return std::nullopt;
	}
}

birds::Bank birds::BankOf(AnimalInfo type)
{
	return type == AnimalInfo::SpellDove || type == AnimalInfo::SpellBat ? k_SpellBank : k_LandBank;
}

bool birds::LeaderPicksNewLeg(uint32_t turnsOnLeg, uint32_t stayTime)
{
	return turnsOnLeg >= stayTime;
}

uint32_t birds::ScriptBirdAge(uint32_t scriptAge, bool joinsFlock, const std::function<uint32_t(uint32_t)>& roll)
{
	if (scriptAge != 0)
	{
		return scriptAge;
	}
	return roll(joinsFlock ? k_AgeRangeInFlock : k_AgeRangeAlone) + k_YoungestBorn;
}

float birds::BaseHeight(float flockHeight, float kindHeight)
{
	return flockHeight != 0.0f ? flockHeight : kindHeight;
}

AnimalInfo birds::TempleBirdKind(float alignment)
{
	return alignment >= 0.0f ? AnimalInfo::Dove : AnimalInfo::Bat;
}

uint32_t birds::TempleBirdCount(float alignment, uint32_t most, bool built)
{
	if (!built)
	{
		return 0;
	}
	// Cut down to a whole bird
	return static_cast<uint32_t>(std::abs(alignment) * static_cast<float>(most));
}

birds::TempleFlockStep birds::StepTowards(uint32_t members, uint32_t count)
{
	if (members < count)
	{
		return TempleFlockStep::AddOne;
	}
	return members > count ? TempleFlockStep::RemoveOne : TempleFlockStep::None;
}

float birds::TempleFlockHeight(float scale, float modelHeight)
{
	return scale * modelHeight + k_TempleFlockAbove;
}
