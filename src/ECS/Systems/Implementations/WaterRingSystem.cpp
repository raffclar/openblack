/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS 1

#include "WaterRingSystem.h"

#include <algorithm>

#include "3D/LandIslandInterface.h"
#include "ECS/Reflection/BinaryArchive.h"
#include "Locator.h"

using namespace openblack::ecs::systems;

namespace
{
/// The rings on the water are made a little above the sea's level; the rings of rain lie higher, on the land
constexpr float k_HighestRingOnWater = 1.0f;
} // namespace

void WaterRingSystem::SaveState(reflection::BinaryWriter& out) const
{
	reflection::SavePresentation(out, _rings);
	reflection::SavePresentation(out, _pace);
}

bool WaterRingSystem::LoadState(reflection::BinaryReader& in)
{
	reflection::Load(in, _rings);
	reflection::Load(in, _pace);
	return !in.Failed();
}

bool WaterRingSystem::Add(const water_rings::Ring& ring)
{
	if (_rings.size() >= water_rings::k_MostRings)
	{
		return false;
	}
	// A ring on the water lies on the water's surface. On a lake above the sea of a generated land, that is the lake's
	// level.
	auto placed = ring;
	if (placed.position.y < k_HighestRingOnWater && Locator::terrainSystem::has_value())
	{
		placed.position.y += Locator::terrainSystem::value().GetWaterLevelAt({placed.position.x, placed.position.z});
	}
	_rings.push_back(placed);
	return true;
}

void WaterRingSystem::Update(std::chrono::milliseconds frameRealTime, std::chrono::milliseconds frameGameTime)
{
	const auto step = water_rings::Step(_pace, frameRealTime, frameGameTime);
	if (step.count() == 0)
	{
		return;
	}
	const auto milliseconds = static_cast<float>(step.count());
	std::erase_if(_rings, [milliseconds](water_rings::Ring& ring) { return !water_rings::Advance(ring, milliseconds); });
}
