/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "RegistryProviders.h"

#include <cctype>

#include <algorithm>
#include <string>
#include <utility>

#include <InspectorQuery.h>
#include <glm/geometric.hpp>

#include "ComponentReflection.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"

using namespace openblack;
using namespace openblack::inspector;

namespace
{

constexpr std::string_view k_NoRegistry = "there is no registry: no land is loaded";

std::string Lower(std::string_view text)
{
	std::string lower(text);
	std::ranges::transform(lower, lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	return lower;
}

/// The names in a parameter that is a name or an array of names
std::vector<std::string> Names(const Json& params, std::string_view key)
{
	std::vector<std::string> names;
	const auto it = params.find(key);
	if (it == params.end())
	{
		return names;
	}
	if (it->is_string())
	{
		names.push_back(it->get<std::string>());
	}
	else if (it->is_array())
	{
		for (const auto& each : *it)
		{
			if (each.is_string())
			{
				names.push_back(each.get<std::string>());
			}
		}
	}
	return names;
}

std::vector<ParameterDescription> FindParameters()
{
	return {
	    {.name = "component",
	     .type = "string or array",
	     .description = "Only entities with this component (or all of these), by name, e.g. Tree",
	     .required = false},
	    {.name = "kind",
	     .type = "string",
	     .description = "Only entities of this kind, e.g. Villager, Abode, Tree, Animal, Creature",
	     .required = false},
	    {.name = "name",
	     .type = "string",
	     .description = "Only entities whose label holds this text, in any case",
	     .required = false},
	};
}

} // namespace

QueryResult openblack::inspector::FindEntities(const ecs::Registry& registry, const InfoConstants* info, const Json& params,
                                               const QueryOptions& options)
{
	const auto& underlying = registry.Underlying();

	// The storages of the components asked for, by their names
	std::vector<const entt::sparse_set*> required;
	for (const auto& name : Names(params, "component"))
	{
		const entt::sparse_set* found = nullptr;
		for (const auto& [id, storage] : underlying.storage())
		{
			if (reflection::ShortTypeName(storage.type()) == name)
			{
				found = &storage;
				break;
			}
		}
		if (found == nullptr)
		{
			return QueryResult::Error("no component " + name + "; ask ecs.components for their names");
		}
		required.push_back(found);
	}
	const auto kind = StringMember(params, "kind");
	auto name = StringMember(params, "name");
	if (name.has_value())
	{
		name = Lower(*name);
	}

	// Only what is near enough is described: the rest would be dropped by the search anyway
	const auto nearEnough = [&registry, &options](entt::entity entity) {
		if (!options.near.has_value())
		{
			return true;
		}
		const auto* transform = registry.TryGet<ecs::components::Transform>(entity);
		if (transform == nullptr)
		{
			return false;
		}
		const auto& near = *options.near;
		const glm::dvec3 point(near.point[0], near.point[1], near.point[2]);
		glm::dvec3 offset = glm::dvec3(transform->position) - point;
		if (near.planar)
		{
			offset.y = 0.0;
		}
		return glm::length(offset) <= near.radius;
	};

	Json items = Json::array();
	const auto consider = [&](entt::entity entity) {
		if (!std::ranges::all_of(required, [entity](const auto* storage) { return storage->contains(entity); }) ||
		    !nearEnough(entity))
		{
			return;
		}
		const auto description = Describe(registry, entity, info);
		if (kind.has_value() && Lower(description.kind) != Lower(*kind))
		{
			return;
		}
		if (name.has_value() && Lower(description.label).find(*name) == std::string::npos)
		{
			return;
		}
		items.push_back(ToListItem(entity, description));
	};

	if (!required.empty())
	{
		// Through the smallest of the storages asked for
		const auto* smallest = *std::ranges::min_element(required, {}, [](const auto* storage) { return storage->size(); });
		for (const auto entity : *smallest)
		{
			consider(entity);
		}
	}
	else if (const auto* entities = underlying.storage<entt::entity>(); entities != nullptr)
	{
		for (const auto [entity] : entities->each())
		{
			consider(entity);
		}
	}
	return QueryResult::Value(std::move(items));
}

RegistryProvider::RegistryProvider(RegistrySources sources, const entt::meta_ctx& context)
    : _sources(std::move(sources))
    , _context(context)
{
}

std::vector<QueryDescription> RegistryProvider::Describe() const
{
	return {
	    {.name = "entities",
	     .description = "Entities by component, kind or label: ids, kinds, labels and places",
	     .parameters = FindParameters(),
	     .kind = ResultKind::List,
	     .needsNear = false},
	    {.name = "entity",
	     .description = "One entity: its kind, label, place and component names; their fields on request",
	     .parameters = {{.name = "id", .type = "integer", .description = "The entity's id", .required = true},
	                    {.name = "components",
	                     .type = "array or \"all\"",
	                     .description = "The components to write out with their fields, by name",
	                     .required = false}},
	     .kind = ResultKind::Object,
	     .needsNear = false},
	    {.name = "components",
	     .description = "The component types there are, how many entities have each, and whether their fields are known",
	     .parameters = {},
	     .kind = ResultKind::List,
	     .needsNear = false},
	};
}

QueryResult RegistryProvider::Run(std::string_view query, const QueryContext& context)
{
	const auto* registry = _sources.registry ? _sources.registry() : nullptr;
	if (registry == nullptr)
	{
		return QueryResult::Error(std::string(k_NoRegistry));
	}
	if (query == "entities")
	{
		return FindEntities(*registry, _sources.info ? _sources.info() : nullptr, context.params, context.options);
	}
	if (query == "entity")
	{
		return Entity(*registry, context.params);
	}
	if (query == "components")
	{
		return Components(*registry);
	}
	return QueryResult::Error("no query ecs." + std::string(query));
}

QueryResult RegistryProvider::Entity(const ecs::Registry& registry, const Json& params) const
{
	const auto id = params.find("id");
	const auto entity = id == params.end() ? std::nullopt : FromId(*id);
	if (!entity.has_value() || !registry.Valid(*entity))
	{
		return QueryResult::Error("no entity with that id");
	}
	auto result = ToListItem(*entity, inspector::Describe(registry, *entity, _sources.info ? _sources.info() : nullptr));
	result["components"] = ComponentNames(registry, *entity);
	const auto it = params.find("components");
	if (it != params.end())
	{
		const bool all = it->is_string() && it->get<std::string>() == "all";
		const auto names = all ? std::vector<std::string> {} : Names(params, "components");
		if (all || !names.empty())
		{
			result["data"] = ComponentsToJson(registry, _context, *entity, names);
		}
	}
	return QueryResult::Value(std::move(result));
}

QueryResult RegistryProvider::Components(const ecs::Registry& registry) const
{
	Json items = Json::array();
	for (const auto& [id, storage] : registry.Underlying().storage())
	{
		if (storage.type() == entt::type_id<entt::entity>() || storage.empty())
		{
			continue;
		}
		items.push_back({
		    {"name", reflection::ShortTypeName(storage.type())},
		    {"count", storage.size()},
		    {"reflected", reflection::IsReflected(_context, storage.type())},
		});
	}
	std::stable_sort(items.begin(), items.end(), [](const Json& a, const Json& b) { return a["count"] > b["count"]; });
	return QueryResult::Value(std::move(items));
}

ObjectsProvider::ObjectsProvider(RegistrySources sources)
    : _sources(std::move(sources))
{
}

std::vector<QueryDescription> ObjectsProvider::Describe() const
{
	return {
	    {.name = "find",
	     .description = "Objects within the radius of a point, nearest first, by component, kind or label",
	     .parameters = FindParameters(),
	     .kind = ResultKind::List,
	     .needsNear = true},
	};
}

QueryResult ObjectsProvider::Run(std::string_view query, const QueryContext& context)
{
	if (query != "find")
	{
		return QueryResult::Error("no query objects." + std::string(query));
	}
	const auto* registry = _sources.registry ? _sources.registry() : nullptr;
	if (registry == nullptr)
	{
		return QueryResult::Error(std::string(k_NoRegistry));
	}
	return FindEntities(*registry, _sources.info ? _sources.info() : nullptr, context.params, context.options);
}
