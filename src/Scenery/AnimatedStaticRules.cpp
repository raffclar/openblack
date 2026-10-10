/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "AnimatedStaticRules.h"

#include <algorithm>

using namespace openblack;
using namespace openblack::animated_static;

namespace
{
/// The stones sink by this many of their own heights over the plinth's clip
constexpr float k_StoneSinkHeights = 7.15f;
/// The stones sit this far into the plinth's top
constexpr float k_StoneSeat = 0.2f;
/// The sinking runs over the clip's play time less these milliseconds
constexpr int32_t k_SinkTimeShort = 3;
/// The step between the gate's circles, of half its model's width
constexpr float k_GateRouteStep = 0.13333334f;
} // namespace

uint32_t animated_static::OpenRestingPlace(uint32_t playTime, size_t frameCount)
{
	const auto count = static_cast<int64_t>(frameCount);
	if (count <= 0)
	{
		return 0;
	}
	return static_cast<uint32_t>(std::max<int64_t>(0, ((count - 2) * static_cast<int64_t>(playTime)) / count));
}

uint32_t animated_static::StepClip(int32_t openState, uint32_t place, uint32_t elapsed, uint32_t restingPlace)
{
	if (openState == k_Open)
	{
		return std::min(place + elapsed, restingPlace);
	}
	return place > elapsed ? place - elapsed : 0;
}

bool animated_static::IsMoving(int32_t openState, uint32_t place, uint32_t restingPlace)
{
	if (openState == k_Closed && place == restingPlace)
	{
		return true;
	}
	if (openState == k_Open && place == 0)
	{
		return true;
	}
	return place != 0 && place != restingPlace;
}

bool animated_static::IsGateStoneKind(MobileStaticInfo kind)
{
	return kind == MobileStaticInfo::GateTotemApe;
}

bool animated_static::AddGateStone(GateStones& stones, MeshId stone)
{
	const auto empty = std::ranges::find_if(stones, [](const auto& slot) { return !slot.has_value(); });
	if (empty == stones.end())
	{
		return false;
	}
	*empty = stone;
	return true;
}

uint32_t animated_static::GateStoneValue(const GateStones& stones)
{
	uint32_t value = 0;
	for (const auto& stone : stones)
	{
		if (stone == MeshId::ObjectGateTotemApe)
		{
			value += 1;
		}
		else if (stone == MeshId::ObjectGateTotemTiger)
		{
			value += 2;
		}
		else if (stone == MeshId::ObjectGateTotemCow)
		{
			value += 4;
		}
	}
	return value;
}

std::optional<MeshId> animated_static::CollisionModel(AnimatedStaticInfo type, int32_t openState, const GateStones& stones)
{
	switch (type)
	{
	case AnimatedStaticInfo::NorseGate:
		return openState == k_Open ? MeshId::NorseGatePhys2 : MeshId::NorseGatePhys1;
	case AnimatedStaticInfo::GateStonePlinth:
		if (openState != k_Open && stones[0].has_value())
		{
			return stones[1].has_value() ? MeshId::GateTotemPlinthePhys3 : MeshId::GateTotemPlinthePhys2;
		}
		return MeshId::GateTotemPlinthePhys1;
	case AnimatedStaticInfo::PhoneBox:
		return MeshId::GateTotemPlinthePhys1;
	case AnimatedStaticInfo::PiperCaveEntrance:
		return MeshId::PiperEntrancePhys1;
	default:
		return std::nullopt;
	}
}

std::vector<StoneDraw> animated_static::PlinthStoneDraws(const PlinthLook& look)
{
	std::vector<StoneDraw> draws;
	const float plinthTop = look.plinthHalfHeight + look.plinthHalfHeight;
	if (look.moving)
	{
		const float sunk =
		    static_cast<float>(look.place) / static_cast<float>(static_cast<int32_t>(look.playTime) - k_SinkTimeShort);
		for (size_t slot = 0; slot < k_GateStoneSlots; ++slot)
		{
			if (!look.stoneHalfHeights[slot].has_value())
			{
				continue;
			}
			const float height = *look.stoneHalfHeights[slot] + *look.stoneHalfHeights[slot];
			const float offset = (static_cast<float>(slot) * height) - ((k_StoneSinkHeights * height) * sunk);
			draws.push_back({.slot = slot, .lift = (plinthTop + offset) - k_StoneSeat, .pickable = false});
		}
	}
	if (look.openState == k_Closed)
	{
		for (size_t slot = 0; slot < k_GateStoneSlots; ++slot)
		{
			if (!look.stoneHalfHeights[slot].has_value())
			{
				continue;
			}
			const float stacked = static_cast<float>(slot) * *look.stoneHalfHeights[slot];
			draws.push_back({.slot = slot, .lift = (plinthTop + (stacked + stacked)) - k_StoneSeat, .pickable = true});
		}
	}
	return draws;
}

std::vector<RouteCircle> animated_static::GateRouteCircles(glm::vec2 middle, glm::vec2 across, float halfWidth, float size,
                                                           bool openAndStill)
{
	std::vector<RouteCircle> circles;
	circles.reserve(k_GateRouteCircles);
	const float step = size * halfWidth * k_GateRouteStep;
	float out = 0.0f;
	for (size_t i = 0; i < k_GateRouteCircles; ++i)
	{
		// The odd circles go a step further out on one side, the even ones mirror the last on the other
		float along = -out;
		if ((i & 1u) != 0)
		{
			out += step;
			along = out;
		}
		if (openAndStill && i < k_GateRouteGap)
		{
			continue;
		}
		circles.push_back({.centre = middle + across * along, .radius = k_GateRouteRadius});
	}
	return circles;
}
