/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "WaterRings.h"

#include "3D/LandLight.h"
#include "3D/LandLightTable.h"
#include "ECS/Systems/WorldEffectsInterface.h"
#include "Locator.h"

using namespace openblack::ecs;

namespace
{
constexpr size_t k_MaxRings = 1024;
constexpr uint32_t k_RingLife = 700; // 0x2BC
/// What this module keeps between calls (Locator::worldEffects)
struct WaterRingsState
{
	std::vector<WaterRing> rings {};
};

WaterRingsState& WaterRingsData()
{
	return openblack::Locator::worldEffects::value().Get<WaterRingsState>();
}
} // namespace

uint32_t openblack::ecs::LandLightRgb(uint8_t index)
{
	return land_light::CurrentTable().GetRaw(index) & 0x00FFFFFFu;
}

bool openblack::ecs::AddWaterRing(const WaterRing& ring)
{
	auto& state = WaterRingsData();
	if (state.rings.size() >= k_MaxRings)
	{
		return false;
	}
	auto& added = state.rings.emplace_back(ring);
	if (added.seaLight)
	{
		added.argb = (added.argb & 0xFF000000u) | LandLightRgb(255);
		added.seaLight = false;
	}
	return true;
}

void openblack::ecs::UpdateWaterRings(float gameMilliseconds)
{
	auto& state = WaterRingsData();
	for (auto& ring : state.rings)
	{
		// g_game_time_inc x rate, truncated toward zero
		ring.age += static_cast<uint32_t>(gameMilliseconds * ring.rate);
	}
	std::erase_if(state.rings, [](const WaterRing& ring) { return ring.age >= k_RingLife; });
}

const std::vector<WaterRing>& openblack::ecs::GetWaterRings()
{
	return WaterRingsData().rings;
}
