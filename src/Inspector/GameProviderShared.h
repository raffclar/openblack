/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstddef>

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include <InspectorJson.h>
#include <InspectorProvider.h>
#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "ECS/ComputerMind.h"
#include "ECS/Registry.h"
#include "Enums.h"
#include "SystemProviders.h"

namespace openblack
{
class GameHeading;

/// A heading is written to the inspector as its radians
void to_json(inspector::Json& json, const GameHeading& heading);
} // namespace openblack

namespace openblack::inspector
{
class GameSeamInterface;
class ScriptTargetInterface;
} // namespace openblack::inspector

/// The helpers that the providers of the game's state share, and the providers that each file of the game's state makes
namespace openblack::inspector::game_providers
{

inline constexpr std::string_view k_NoRegistry = "there is no registry: no land is loaded";

[[nodiscard]] Json Point(const glm::vec3& point);
[[nodiscard]] Json Point(const glm::vec2& point);
[[nodiscard]] Json Id(entt::entity entity);
[[nodiscard]] Json Id(const std::optional<entt::entity>& entity);

template <typename Value>
[[nodiscard]] Json Optional(const std::optional<Value>& value)
{
	if (!value.has_value())
	{
		return nullptr;
	}
	if constexpr (std::is_same_v<Value, glm::vec3> || std::is_same_v<Value, glm::vec2>)
	{
		return Point(*value);
	}
	else
	{
		return *value;
	}
}

[[nodiscard]] const ecs::Registry* Registry();

/// The things that only the frames make, out of the synced world
[[nodiscard]] const ecs::Registry* DrawnRegistry();

[[nodiscard]] const InfoConstants* Info();
[[nodiscard]] WorldSources World();

/// How many entities have a component
template <typename Component>
[[nodiscard]] size_t CountOf(const ecs::Registry& registry)
{
	const auto* storage = registry.Underlying().storage<Component>();
	return storage != nullptr ? storage->size() : 0;
}

[[nodiscard]] QueryDescription Query(std::string name, std::string description,
                                     std::vector<ParameterDescription> parameters = {}, ResultKind kind = ResultKind::Object,
                                     bool needsNear = false);
[[nodiscard]] ParameterDescription IdParameter(std::string description);
[[nodiscard]] ParameterDescription PositionParameter();

/// The entity a request names, if it exists
[[nodiscard]] std::optional<entt::entity> EntityParam(const Json& params);

[[nodiscard]] std::optional<glm::vec3> PositionParam(const Json& params);
[[nodiscard]] PlayerNames PlayerParam(const Json& params);

/// The player that a request names, by number (0 to 7) or by name (PLAYER_TWO): none when it names no player
[[nodiscard]] std::optional<PlayerNames> NamedPlayerParam(const Json& params);

/// The entity of the player, if the land has it
[[nodiscard]] std::optional<entt::entity> PlayerEntityOf(const ecs::Registry& registry, PlayerNames name);

/// The names that the nodes of every computer mind have in this game: a personality load can change them
[[nodiscard]] const computer_mind::NodeNames& SharedNodeNames(const ecs::Registry& registry);

/// A query of one locator service, answered only while the service is there
template <typename Service, typename Read>
[[nodiscard]] FunctionProvider::Function Serve(std::string_view name, Read read)
{
	return [name, read](const QueryContext& context) -> QueryResult {
		if (!Service::has_value())
		{
			return QueryResult::Error(std::string(name) + " isn't there: no land is loaded, or this mode doesn't make it");
		}
		if constexpr (std::is_same_v<std::invoke_result_t<Read, decltype(Service::value()), const QueryContext&>, QueryResult>)
		{
			return read(Service::value(), context);
		}
		else
		{
			return QueryResult::Value(read(Service::value(), context));
		}
	};
}

/// A query of the registry, answered only while there is one
template <typename Read>
[[nodiscard]] FunctionProvider::Function ServeRegistry(Read read)
{
	return [read](const QueryContext& context) -> QueryResult {
		const auto* registry = Registry();
		if (registry == nullptr)
		{
			return QueryResult::Error(std::string(k_NoRegistry));
		}
		if constexpr (std::is_same_v<std::invoke_result_t<Read, const ecs::Registry&, const QueryContext&>, QueryResult>)
		{
			return read(*registry, context);
		}
		else
		{
			return QueryResult::Value(read(*registry, context));
		}
	};
}

[[nodiscard]] Json Listed(const ecs::Registry& registry, entt::entity entity);

// The providers, one for each area of the game's state

/// The engine's services
[[nodiscard]] std::unique_ptr<ProviderInterface> EngineProvider();
/// The land, the sea and the weather
[[nodiscard]] std::unique_ptr<ProviderInterface> LandProvider();
[[nodiscard]] std::unique_ptr<ProviderInterface> SkyStateProvider(std::unique_ptr<ProviderInterface> sky);
/// The things living on the land
[[nodiscard]] std::unique_ptr<ProviderInterface> LivingProvider();
/// The players, their hands and their input
[[nodiscard]] std::unique_ptr<ProviderInterface> PlayersProvider();
/// Miracles, fire and what reacts to them
[[nodiscard]] std::unique_ptr<ProviderInterface> MagicProvider();
/// The temple
[[nodiscard]] std::unique_ptr<ProviderInterface> TempleProvider();
/// The camera's helpers, the editor and the cinematics
[[nodiscard]] std::unique_ptr<ProviderInterface> ViewProvider(const GameSeamInterface* seam);
/// The advisors
[[nodiscard]] std::unique_ptr<ProviderInterface> AdvisorsProvider();


} // namespace openblack::inspector::game_providers
