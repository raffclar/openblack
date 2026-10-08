/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <unordered_map>
#include <vector>

#include <entt/entity/entity.hpp>
#include <entt/entity/fwd.hpp>

#include "Components/Stream.h"

namespace openblack::ecs
{
struct RegistryContext
{
	std::unordered_map<components::Stream::Id, entt::entity> streams;
	std::unordered_map<uint32_t, entt::entity> towns;
	/// (openblack) the last Town::ownerListStamp given (town_queries::NextOwnerListStamp): 0 again with every land
	uint32_t ownerListCounter {0};
	/// (openblack) the last Town::creationStamp given (TownArchetype::Create): 0 again with every land
	uint32_t townCreationCounter {0};
	/// (openblack) the last Footpath::creationStamp given (footpaths::Create): 0 again with every land
	uint32_t footpathCreationCounter {0};
	/// CREATE_FLOCK's flocks by their script id. The original's list is newest first, so a repeated id finds the last
	/// one made.
	std::unordered_map<int32_t, entt::entity> flocks;
};
} // namespace openblack::ecs
