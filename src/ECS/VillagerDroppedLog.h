/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <entt/entity/entity.hpp>

#include "ECS/Systems/DynamicsSystemInterface.h"

/// The log a villager lets fall when it is thrown, launched or killed
namespace openblack::ecs::villager_dropped_log
{

/// A villager that shows something in its hand and carries more wood than shows lets all its wood fall as a log: a fallen
/// pine lying along the ground where the villager is, drawn as what the villager held and holding just its wood. The log
/// starts to fly as given, laid on the land and raised clear of what is under it, and passes through other things on
/// its way. The wood leaves the villager and its town's count of what its people carry. The log, or none.
entt::entity Drop(systems::DynamicsSystemInterface& dynamics, entt::entity villager, const systems::DroppedLogStart& start);

} // namespace openblack::ecs::villager_dropped_log
