/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureAttitudeRules.h"

#include <cmath>
#include <cstdlib>

#include <algorithm>
#include <array>

#include "3D/MapCoords.h"
#include "Common/GUtilsAngle.h"
#include "Common/GUtilsDistance.h"

using namespace openblack;
namespace creature_attitude = openblack::ecs::creature_attitude;
using creature_attitude::Attitude;

namespace
{
// The actions of the creature's table whose finishing frightens a town: throwing and stomping, the destructive
// miracles, kicking, setting fire to things, playfully frightening the people
constexpr std::array<uint32_t, 22> k_FrighteningActions {15,  18,  45,  46,  47,  95,  96,  97,  99,  100, 103,
                                                         105, 185, 186, 197, 228, 232, 236, 252, 263, 283, 296};
// The actions that earn a town's respect: the gifts to its storage pit, worship site, building sites and workshop,
// helping build and repair, putting out fires, stroking, impressing, dancing and playing with the people, the kind
// miracles, watering and making disciples
constexpr std::array<uint32_t, 58> k_RespectedActions {
    28,  29,  30,  31,  32,  33,  34,  35,  36,  37,  38,  39,  40,  41,  48,  49,  53,  54,  98,  101,
    102, 104, 136, 137, 138, 182, 183, 184, 229, 230, 231, 233, 234, 235, 238, 239, 240, 241, 245, 246,
    247, 248, 262, 285, 286, 287, 288, 293, 294, 295, 300, 302, 304, 305, 309, 325, 326, 327};
// The actions of eating, and of handling something in the hand
constexpr std::array<uint32_t, 8> k_EatingActions {11, 12, 13, 14, 275, 276, 277, 278};

// A town's feeling lapses this many seconds after it was shown, by the feeling
constexpr float k_CuriousLapseSeconds = 15.0f;
constexpr float k_FearLapseSeconds = 30.0f;
constexpr float k_RespectLapseSeconds = 10.0f;

// Curiosity weighs half as much again as the creature row's priority, fear twice the priority of running away
constexpr float k_CuriousPriorityFactor = 1.5f;
constexpr float k_FearPriorityFactor = 2.0f;

// The reach the timings measure nearness against goes ten metres beyond the row's reach
constexpr float k_TimingReachExtra = 10.0f;
// Reacting to a creature lasts longer, and waiting to react again is shorter, the more impressive it is
constexpr float k_QuiteImpressiveLonger = 2.0f;
constexpr float k_VeryImpressiveLonger = 3.0f;
constexpr float k_QuiteImpressiveSooner = 0.5f;
constexpr float k_VeryImpressiveSooner = 1.0f / 3.0f;
// A town that fears a creature watches it four times as long
constexpr float k_FearLonger = 4.0f;

constexpr float k_MetresPerCell = 10.0f;
constexpr float k_UnitsPerCell = 65536.0f;

template <size_t N>
bool Contains(const std::array<uint32_t, N>& actions, uint32_t action)
{
	return std::ranges::find(actions, action) != actions.end();
}

uint8_t AsByte(int32_t value)
{
	return static_cast<uint8_t>(static_cast<uint32_t>(value) & 0xFFu);
}
} // namespace

creature_attitude::Shown creature_attitude::ShownBy(uint32_t action)
{
	if (Contains(k_FrighteningActions, action))
	{
		return Shown::Fear;
	}
	if (Contains(k_RespectedActions, action))
	{
		return Shown::Respect;
	}
	if (Contains(k_EatingActions, action))
	{
		return Shown::Eating;
	}
	return Shown::Nothing;
}

std::optional<Attitude> creature_attitude::AttitudeShown(uint32_t action, bool thingIsVillager)
{
	switch (ShownBy(action))
	{
	case Shown::Fear:
		return Attitude::Fear;
	case Shown::Respect:
		return Attitude::Respect;
	case Shown::Eating:
		return thingIsVillager ? std::optional(Attitude::Fear) : std::nullopt;
	default:
		return std::nullopt;
	}
}

creature_attitude::Feeling creature_attitude::StartWatching(uint32_t turn)
{
	return {.attitude = Attitude::Curious, .shownTurn = turn, .lapsedTurn = turn};
}

float creature_attitude::LapseSeconds(Attitude attitude)
{
	switch (attitude)
	{
	case Attitude::Curious:
		return k_CuriousLapseSeconds;
	case Attitude::Fear:
		return k_FearLapseSeconds;
	case Attitude::Respect:
		return k_RespectLapseSeconds;
	default:
		return 0.0f;
	}
}

bool creature_attitude::Turn(Feeling& feeling, std::optional<Attitude> shown, uint32_t turn, uint32_t turnsPerSecond)
{
	bool changed = false;
	if (shown.has_value() && (*shown != feeling.attitude || *shown == Attitude::Fear))
	{
		changed = *shown != feeling.attitude;
		feeling.attitude = *shown;
		feeling.shownTurn = turn;
	}
	if (feeling.attitude != Attitude::Indifferent && turnsPerSecond != 0)
	{
		const auto seconds = static_cast<float>((turn - feeling.shownTurn) / turnsPerSecond);
		if (seconds > LapseSeconds(feeling.attitude))
		{
			feeling.attitude = Attitude::Indifferent;
			feeling.lapsedTurn = turn;
		}
	}
	return changed;
}

uint8_t creature_attitude::VillagerPriority(const Watch& watch)
{
	if (watch.dancing || watch.creatureInvisible || !watch.withinReach)
	{
		return 0;
	}
	if (!watch.hasTown)
	{
		return AsByte(static_cast<int32_t>(watch.creaturePriority));
	}
	switch (watch.attitude)
	{
	case Attitude::Curious:
		return AsByte(static_cast<int32_t>(static_cast<float>(watch.creaturePriority) * k_CuriousPriorityFactor));
	case Attitude::Fear:
		return AsByte(static_cast<int32_t>(static_cast<float>(watch.fleePriority) * k_FearPriorityFactor));
	case Attitude::Respect:
		if (!watch.sameOwner)
		{
			return AsByte(static_cast<int32_t>(watch.followPriority));
		}
		if (watch.disciple || (watch.wanted.has_value() && *watch.wanted > 0.0f))
		{
			return 0;
		}
		return AsByte(static_cast<int32_t>(watch.followPriority));
	default:
		return 0;
	}
}

uint32_t creature_attitude::TurnsToReact(const Timing& timing)
{
	const float reach = timing.maxReactionDistance + k_TimingReachExtra;
	const float importance = timing.howImportantIsDistance;
	float factor = (reach - timing.distance) * importance / reach + (1.0f - importance);
	if (timing.impressive == ImpressiveType::CreatureQuiteImpressive)
	{
		factor *= k_QuiteImpressiveLonger;
	}
	else if (timing.impressive == ImpressiveType::CreatureVeryImpressive)
	{
		factor *= k_VeryImpressiveLonger;
	}
	if (timing.attitude == Attitude::Fear)
	{
		factor *= k_FearLonger;
	}
	return static_cast<uint32_t>(static_cast<int32_t>(static_cast<float>(timing.tableTurns) * factor));
}

uint32_t creature_attitude::TurnsBeforeReactingAgain(const Timing& timing, bool meansSomethingImpressive)
{
	const float reach = timing.maxReactionDistance + k_TimingReachExtra;
	const float importance = timing.howImportantIsDistance;
	float factor = importance * timing.distance / reach + (1.0f - importance);
	if (timing.impressive == ImpressiveType::CreatureQuiteImpressive)
	{
		factor *= k_QuiteImpressiveSooner;
	}
	else if (timing.impressive == ImpressiveType::CreatureVeryImpressive)
	{
		factor *= k_VeryImpressiveSooner;
	}
	if (meansSomethingImpressive ||
	    (timing.attitude.has_value() && (*timing.attitude == Attitude::Fear || *timing.attitude == Attitude::Respect)))
	{
		return 0;
	}
	return static_cast<uint32_t>(static_cast<int32_t>(static_cast<float>(timing.tableTurns) * factor));
}

int32_t creature_attitude::TurnsIn(float seconds, uint32_t turnsPerSecond)
{
	return static_cast<int32_t>(static_cast<float>(turnsPerSecond) * seconds);
}

std::optional<glm::vec2> creature_attitude::StepCloser(glm::vec2 villager, glm::vec2 creature, float keep)
{
	const float distance = gutils::GetDistanceInMetres(villager, creature);
	if (!(distance - keep > k_CloseEnoughMetres))
	{
		return std::nullopt;
	}
	const auto angle = gutils::GetAngleFromXZ(villager, creature);
	// The step is in whole metres, made map units
	const auto metres = static_cast<float>(static_cast<int32_t>(distance - keep));
	const auto whole = static_cast<int32_t>(metres / k_MetresPerCell * k_UnitsPerCell);
	auto at = map_coords::FromMetres(villager);
	at += map_coords::MapCoords {gutils::GetXByAngleHugeDistance(angle, whole), gutils::GetZByAngleHugeDistance(angle, whole),
	                             0.0f};
	return map_coords::ToMetres(at);
}

glm::vec2 creature_attitude::BowingPlace(glm::vec2 villager, glm::vec2 creature, float villagerRadius, float creatureRadius)
{
	auto at = map_coords::FromMetres(villager);
	at += gutils::GetPosFromAngle(gutils::Get3DAngleFromXZ(villager, creature), villagerRadius + creatureRadius);
	return map_coords::ToMetres(at);
}

glm::vec2 creature_attitude::FleePoint(glm::vec2 villager, glm::vec2 creature, float distance)
{
	const auto away = villager - creature;
	if (away == glm::vec2(0.0f))
	{
		return villager;
	}
	return villager + away * (distance / std::sqrt(away.x * away.x + away.y * away.y));
}

bool creature_attitude::DriftedFrom(uint16_t facing, uint16_t towardsCreature)
{
	return std::abs(static_cast<int32_t>(towardsCreature) - static_cast<int32_t>(facing)) > k_RespectDrift;
}
