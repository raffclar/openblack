/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "GameProviderShared.h"

#include <algorithm>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>

#include <InspectorQuery.h>

#include "Common/GameHeading.h"
#include "ECS/Components/ComputerPlayer.h"
#include "ECS/Components/Player.h"
#include "ECS/DrawnRegistry.h"
#include "ECS/Registry.h"
#include "InfoConstants.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::inspector;
using namespace openblack::ecs::components;

void openblack::to_json(inspector::Json& json, const GameHeading& heading)
{
	json = heading.Radians();
}

namespace openblack::inspector::game_providers
{

Json Point(const glm::vec3& point)
{
	return {point.x, point.y, point.z};
}

Json Point(const glm::vec2& point)
{
	return {point.x, point.y};
}

Json Id(entt::entity entity)
{
	return entity == entt::null ? Json(nullptr) : Json(ToId(entity));
}

Json Id(const std::optional<entt::entity>& entity)
{
	return entity.has_value() ? Id(*entity) : Json(nullptr);
}

const ecs::Registry* Registry()
{
	return Locator::entitiesRegistry::has_value() ? &Locator::entitiesRegistry::value() : nullptr;
}

const ecs::Registry* DrawnRegistry()
{
	return Locator::drawnRegistry::has_value() ? &Locator::drawnRegistry::value() : nullptr;
}

const InfoConstants* Info()
{
	return Locator::infoConstants::has_value() ? &Locator::infoConstants::value() : nullptr;
}

WorldSources World()
{
	return {.registry = &Registry, .info = &Info};
}

QueryDescription Query(std::string name, std::string description, std::vector<ParameterDescription> parameters, ResultKind kind,
                       bool needsNear)
{
	return {.name = std::move(name),
	        .description = std::move(description),
	        .parameters = std::move(parameters),
	        .kind = kind,
	        .needsNear = needsNear,
	        .writes = false};
}

ParameterDescription IdParameter(std::string description)
{
	return {.name = "id", .type = "integer", .description = std::move(description), .required = true};
}

ParameterDescription PositionParameter()
{
	return {.name = "position", .type = "point", .description = "[x, z] or [x, y, z]", .required = true};
}

std::optional<entt::entity> EntityParam(const Json& params)
{
	const auto* registry = Registry();
	const auto it = params.find("id");
	const auto entity = it == params.end() ? std::nullopt : FromId(*it);
	if (registry == nullptr || !entity.has_value() || !registry->Valid(*entity))
	{
		return std::nullopt;
	}
	return entity;
}

std::optional<glm::vec3> PositionParam(const Json& params)
{
	const auto it = params.find("position");
	if (it == params.end())
	{
		return std::nullopt;
	}
	const auto point = ReadPoint(*it);
	if (!point.has_value())
	{
		return std::nullopt;
	}
	return glm::vec3(static_cast<float>((*point)[0]), static_cast<float>((*point)[1]), static_cast<float>((*point)[2]));
}

PlayerNames PlayerParam(const Json& params)
{
	const auto number = static_cast<int>(NumberMember(params, "player").value_or(0.0));
	return static_cast<PlayerNames>(std::clamp(number, 0, static_cast<int>(PlayerNames::NEUTRAL)));
}

std::optional<PlayerNames> NamedPlayerParam(const Json& params)
{
	const auto it = params.find("player");
	if (it == params.end())
	{
		return std::nullopt;
	}
	if (it->is_string())
	{
		const auto& text = it->get_ref<const std::string&>();
		const auto found = std::ranges::find(k_PlayerNamesStrs, std::string_view(text));
		if (found == k_PlayerNamesStrs.end())
		{
			return std::nullopt;
		}
		return static_cast<PlayerNames>(found - k_PlayerNamesStrs.begin());
	}
	const auto number = NumberMember(params, "player");
	if (!number.has_value() || *number < 0.0 || *number >= static_cast<double>(PlayerNames::_COUNT))
	{
		return std::nullopt;
	}
	return static_cast<PlayerNames>(static_cast<int>(*number));
}

std::optional<entt::entity> PlayerEntityOf(const ecs::Registry& registry, PlayerNames name)
{
	std::optional<entt::entity> found;
	registry.Each<const Player>([&](entt::entity entity, const Player& player) {
		if (player.name == name)
		{
			found = entity;
		}
	});
	return found;
}

const computer_mind::NodeNames& SharedNodeNames(const ecs::Registry& registry)
{
	const computer_mind::NodeNames* names = &computer_mind::FirstNodeNames();
	registry.Each<const ecs::components::ComputerNodeNames>(
	    [&names](entt::entity /*entity*/, const ecs::components::ComputerNodeNames& shared) { names = &shared.names; });
	return *names;
}

Json Listed(const ecs::Registry& registry, entt::entity entity)
{
	return ToListItem(entity, Describe(registry, entity, Info()));
}

} // namespace openblack::inspector::game_providers
