/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "VillagerClips.h"

#include <chrono>

#include <LNDFile.h>
#include <glm/vec2.hpp>

#include "3D/L3DAnim.h"
#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "Locator.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs;

std::optional<float> villager_clips::ClipMilliseconds(AnimId clip)
{
	if (!Locator::resources::has_value())
	{
		return std::nullopt;
	}
	auto& animations = Locator::resources::value().GetAnimations();
	const auto key = resources::HashIdentifier(static_cast<uint32_t>(clip));
	if (!animations.Contains(key))
	{
		return std::nullopt;
	}
	return static_cast<float>(animations.Handle(key)->GetPlayTime());
}

namespace
{
constexpr auto k_TurnMilliseconds =
    static_cast<uint32_t>(std::chrono::milliseconds(openblack::ecs::systems::TimeSystemInterface::k_TurnDuration).count());
} // namespace

bool villager_clips::ClipPlayed(const components::LivingAction& action, AnimId clip, uint32_t times)
{
	return HasPlayed(action.turnsSinceStateChange, k_TurnMilliseconds,
	                 static_cast<uint32_t>(ClipMilliseconds(clip).value_or(0.0f)), times);
}

villager_clips::DanceMoveClip villager_clips::OnDanceMove(const components::LivingAction& action, AnimId clip)
{
	return OnDanceMove(action.turnsSinceStateChange, k_TurnMilliseconds,
	                   static_cast<uint32_t>(ClipMilliseconds(clip).value_or(0.0f)));
}

bool villager_clips::IsWaterAt(glm::vec3 point, const std::function<const lnd::LNDCell*(glm::u16vec2)>& findCell)
{
	// Off the map and where the land has no block, the game takes the ground as water
	const auto cell = map_coords::CellOf(point);
	if (!map_coords::InBounds(cell))
	{
		return true;
	}
	const auto* land = findCell(glm::u16vec2(cell));
	return land == nullptr || land->properties.hasWater != 0;
}

bool villager_clips::IsOnWater(glm::vec3 point)
{
	if (!Locator::terrainSystem::has_value())
	{
		return false;
	}
	const auto& terrain = Locator::terrainSystem::value();
	return IsWaterAt(point, [&terrain](glm::u16vec2 cell) { return terrain.FindCell(cell); });
}
