/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <utility>

#include <entt/entity/fwd.hpp>
#include <fmt/format.h>

#include "ECS/Villager/VillagerCore.h"

namespace openblack::ecs::villager
{
/// A trace line for a villager (OPENBLACK_VILLAGER_TRACE), formatted only when that villager is traced: the arguments
/// are still evaluated as they were, only the text is not made for the villagers nobody traces
template <typename... Args>
void TraceFormatted(entt::entity villager, fmt::format_string<Args...> format, Args&&... args)
{
	if (TraceOn(villager))
	{
		Trace(villager, fmt::format(format, std::forward<Args>(args)...));
	}
}
} // namespace openblack::ecs::villager
