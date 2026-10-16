/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ThingModel.h"

#include "ECS/Components/Mesh.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/Registry.h"

using namespace openblack::ecs;
using namespace openblack::ecs::components;

std::optional<entt::id_type> thing_model::ModelOf(const Registry& registry, entt::entity thing)
{
	if (!registry.Valid(thing))
	{
		return std::nullopt;
	}
	if (const auto* mesh = registry.TryGet<const Mesh>(thing))
	{
		return mesh->id;
	}
	if (const auto* altar = registry.TryGet<const WorshipAltar>(thing); altar != nullptr && altar->model != 0)
	{
		return altar->model;
	}
	return std::nullopt;
}
