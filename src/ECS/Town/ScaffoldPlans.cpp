/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ScaffoldPlans.h"

#include <bit>

#include "Common/GUtilsDistance.h"
#include "Common/GameRandom.h"
#include "ECS/Components/Scaffold.h"
#include "ECS/MapCells.h"
#include "ECS/ObjectMetrics.h"
#include "ECS/Town/BuildingSites.h"
#include "ECS/Town/TownDesire.h"
#include "ECS/Town/TownPlacement.h"
#include "ECS/Town/TownQueries.h"
#include "ECS/Town/TownStats.h"
#include "InfoConstants.h"
#include "Resources/ResourceManager.h"

// The scaffold plans (ScaffoldPlans.h)

namespace openblack::ecs
{
using namespace components;

namespace
{
/// ChoosePlanForScaffold: the first angle is GameFloatRand(2 pi), each try turns it by pi / 4 (the exact floats)
constexpr float k_TwoPi = std::bit_cast<float>(0x40C90FDBu);
constexpr float k_QuarterPi = std::bit_cast<float>(0x3F490FDBu);
constexpr uint32_t k_PlanTries = 8;
/// GetNewPlannedBuilding: the abode numbers 0..15
constexpr int32_t k_AbodeNumbers = 0x10;
/// GetWonderPower: 15.0 (the radius factor and floor), 0.1 (the impressive value's share), 0.25 (the power's floor)
constexpr float k_WonderRadius = std::bit_cast<float>(0x41700000u);
constexpr float k_ImpressiveShare = std::bit_cast<float>(0x3DCCCCCDu);
constexpr float k_WonderPowerFloor = std::bit_cast<float>(0x3E800000u);
/// WonderScale: 5.0 (the power's cap), 0.1
constexpr float k_WonderPowerCap = std::bit_cast<float>(0x40A00000u);
constexpr float k_WonderScaleShare = std::bit_cast<float>(0x3DCCCCCDu);

/// An object's town artifact value and impressive value. (pending) neither is ported (town artifacts, scaffolds, the
/// abodes...): 0 for every object, so the wonder's power is the raw desire (at least 0.25)
float TownArtifactValue(entt::entity)
{
	return 0.0f;
}
float ImpressiveValue(entt::entity)
{
	return 0.0f;
}
} // namespace

float scaffold_plans::GetNewPlannedBuilding(entt::entity town, ScaffoldPlan& out, const map_coords::MapCoords& pos,
                                            const float& angle, float scale, uint32_t n, entt::entity under, Tribe tribe,
                                            int32_t limit, bool force)
{
	// best = 0; first = limit == -1 ? 0 : limit; end = limit == -1 ? 16 : limit + 1 (unsigned compares)
	float best = 0.0f;
	const auto first = static_cast<uint32_t>(limit == -1 ? 0 : limit);
	const auto end = static_cast<uint32_t>(limit == -1 ? k_AbodeNumbers : limit + 1);
	for (uint32_t number = first; number < end; ++number)
	{
		// s = the scale argument (every iteration); t = the town's tribe ((inferred) the same TRIBE_TYPE as
		// town_queries::TribeOf); WONDER (10): t = tribe when != -1, s = WonderScale(pos)
		float s = scale;
		Tribe t = town_queries::TribeOf(town);
		if (number == static_cast<uint32_t>(AbodeNumber::Wonder))
		{
			if (tribe != Tribe::NONE)
			{
				t = tribe;
			}
			s = WonderScale(town, pos);
		}
		// the abode info for (t, number); ScaffoldsRequired == 0 or > n (unsigned) -> next. (openblack,
		// guard) no record -> next (the original reads through the null)
		const auto* info = town_stats::FindAbodeInfo(t, static_cast<AbodeNumber>(number));
		if (info == nullptr || info->scaffoldsRequired == 0 || info->scaffoldsRequired > n)
		{
			continue;
		}
		// the info's mesh; !force -> the position must suit it (IsSuitableForFixedObject(mesh, *angle, s, under))
		if (!force && !town_placement::IsSuitableForFixedObject(pos, resources::HashIdentifier(info->meshId), angle, s, under))
		{
			continue;
		}
		// d = GetDesireToBeBuilt(info, n); d > best (strict) -> best, out's info, angle = *angle, scale = s
		const float d = plans::GetDesireToBeBuilt(town, *info, n);
		if (d > best)
		{
			best = d;
			out.info = info;
			out.yAngle = angle;
			out.scale = s;
		}
	}
	return best;
}

bool scaffold_plans::ChoosePlanForScaffold(entt::entity town, ScaffoldPlan& out, const map_coords::MapCoords& pos, uint32_t n,
                                           Tribe tribe, entt::entity under, int32_t limit, bool force)
{
	// a = GameFloatRand(2 pi); 8 times: GetNewPlannedBuilding(out, pos, &a, 1.0, n, under, tribe, limit, force) > 0
	// -> 1; a += pi / 4
	float a = game_random::GameFloatRand(k_TwoPi);
	for (uint32_t k = 0; k < k_PlanTries; ++k)
	{
		if (GetNewPlannedBuilding(town, out, pos, a, 1.0f, n, under, tribe, limit, force) > 0.0f)
		{
			return true;
		}
		a = a + k_QuarterPi;
	}
	return false;
}

float scaffold_plans::GetWonderPower(entt::entity town, const map_coords::MapCoords& pos)
{
	// raw = GetRawDesire(14 For_Wonder); r = 15 x raw > 15 ? 15 x raw : 15 (15 on <=, and on NaN)
	const float raw = town_desire::GetRawDesire(town, TownDesireInfo::ToBuildWonder);
	const float scaled = raw * k_WonderRadius;
	const float r = scaled > k_WonderRadius ? scaled : k_WonderRadius;
	float sum = raw;
	// the spiral's cell count for r. Literal: each step reads the cell of `pos` itself, not of the spiral's moving
	// copy: the same cell, the position's own, is walked n times. Each object (fixed then mobile) with
	// GetDistanceInMetres(pos, o) < r: sum += its town artifact value, then sum += its impressive value x 0.1
	const int32_t cells = map_coords::CellSpiralSize(r);
	const auto cell = map_coords::Cell(pos);
	for (int32_t i = 0; i < cells; ++i)
	{
		for (const auto o : map_cells::ObjectsInCell(cell))
		{
			if (!(gutils::GetDistanceInMetres(pos, object::MapCoordsOf(o)) < r))
			{
				continue;
			}
			sum = TownArtifactValue(o) + sum;
			const float impressive = ImpressiveValue(o) * k_ImpressiveShare;
			sum = impressive + sum;
		}
	}
	// 0.25 <= sum (NaN too) ? sum : 0.25
	return k_WonderPowerFloor <= sum || sum != sum ? sum : k_WonderPowerFloor;
}

float scaffold_plans::WonderScale(entt::entity town, const map_coords::MapCoords& pos)
{
	// GetWonderPower(pos) < 5 ? GetWonderPower(pos) (called again) : 5; then p x (1 - p x 0.1), one rounding per step
	const float p = GetWonderPower(town, pos) < k_WonderPowerCap ? GetWonderPower(town, pos) : k_WonderPowerCap;
	const float tenth = p * k_WonderScaleShare;
	const float rest = 1.0f - tenth;
	return p * rest;
}

} // namespace openblack::ecs
