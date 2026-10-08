/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "VillagerWorldQueries.h"

#include <entt/entity/entity.hpp>

#include "3D/DayNightClock.h"
#include "ECS/Systems/DayNightClockSystemInterface.h"
#include "ECS/Town/AbodeQueries.h"
#include "ECS/Town/Graveyard.h"
#include "Game.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::systems;

bool VillagerWorldQueries::IsVisualNight() const
{
	return Locator::dayNightClock::has_value() && Locator::dayNightClock::value().Clock().IsVisualNight();
}

std::optional<bool> VillagerWorldQueries::Graveyard(entt::entity town) const
{
	const auto yard = graveyard::GetGraveyard(town);
	if (yard == entt::null)
	{
		return std::nullopt;
	}
	return abode_queries::IsFunctional(yard);
}
