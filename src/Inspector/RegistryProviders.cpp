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
#include <cstdint>

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include <InspectorQuery.h>
#include <fmt/format.h>
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
			if (reflection::ShortTypeName(reflection::StorageType(storage)) == name)
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
		// Where it is drawn, as the search afterwards measures it
		const auto position = DrawnPosition(registry, entity);
		if (!position.has_value())
		{
			return false;
		}
		const auto& near = *options.near;
		const glm::dvec3 point(near.point[0], near.point[1], near.point[2]);
		glm::dvec3 offset = glm::dvec3(*position) - point;
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
	    {.name = "hash",
	     .description = "A hash of where every entity is (its id and transform), and of a component's fields when one is "
	                    "named, to tell whether two runs ended the same",
	     .parameters = {{.name = "component",
	                     .type = "string",
	                     .description = "Also hash this component's fields on each entity that has it",
	                     .required = false},
	                    {.name = "exclude",
	                     .type = "array",
	                     .description = "Leave out the entities with any of these components, e.g. [\"HandGrab\"]: the "
	                                    "hand eases towards the pointer every frame drawn, paused or not",
	                     .required = false}},
	     .kind = ResultKind::Object,
	     .needsNear = false},
	    {.name = "references",
	     .description = "Every component field that holds an entity (an entity, an optional one or a list of them), "
	                    "gone or not: the holder, its label, the component and the field's path",
	     .parameters = {{.name = "id", .type = "integer", .description = "The entity's id", .required = true}},
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
	if (query == "hash")
	{
		return Hash(*registry, context.params);
	}
	if (query == "references")
	{
		return References(*registry, context.params);
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
		// "all" asks for every component, also when a client sends it as a list (the parameter is a list or "all")
		const bool all = (it->is_string() && it->get<std::string>() == "all") ||
		                 (it->is_array() && std::ranges::find(*it, Json("all")) != it->end());
		const auto names = all ? std::vector<std::string> {} : Names(params, "components");
		if (all || !names.empty())
		{
			result["data"] = ComponentsToJson(registry, _context, *entity, names);
		}
	}
	return QueryResult::Value(std::move(result));
}

QueryResult RegistryProvider::Hash(const ecs::Registry& registry, const Json& params) const
{
	// FNV-1a over the entities in the order of their ids, each with its transform's floats as they are held
	constexpr uint64_t k_Offset = 14695981039346656037ull;
	constexpr uint64_t k_Prime = 1099511628211ull;
	uint64_t hash = k_Offset;
	const auto mix = [&hash](const void* data, size_t size) {
		const auto* bytes = static_cast<const unsigned char*>(data);
		for (size_t i = 0; i < size; ++i)
		{
			hash = (hash ^ bytes[i]) * k_Prime;
		}
	};
	const entt::sparse_set* extra = nullptr;
	if (const auto name = StringMember(params, "component"); name.has_value())
	{
		extra = reflection::FindStorage(registry.Underlying(), *name);
		if (extra == nullptr)
		{
			return QueryResult::Error("no component " + *name + "; ask ecs.components for their names");
		}
	}
	std::vector<const entt::sparse_set*> excluded;
	if (const auto it = params.find("exclude"); it != params.end())
	{
		if (!it->is_array())
		{
			return QueryResult::Error("exclude is a list of component names");
		}
		for (const auto& name : *it)
		{
			const auto* storage =
			    name.is_string() ? reflection::FindStorage(registry.Underlying(), name.get<std::string>()) : nullptr;
			if (storage == nullptr)
			{
				return QueryResult::Error("no component " + Dump(name) + " to exclude; ask ecs.components for their names");
			}
			excluded.push_back(storage);
		}
	}
	std::vector<entt::entity> entities;
	registry.Each<const ecs::components::Transform>(
	    [&entities, &excluded](entt::entity entity, const ecs::components::Transform&) {
		    if (std::ranges::none_of(excluded, [entity](const auto* storage) { return storage->contains(entity); }))
		    {
			    entities.push_back(entity);
		    }
	    });
	std::ranges::sort(entities);
	for (const auto entity : entities)
	{
		const auto id = entt::to_integral(entity);
		mix(&id, sizeof(id));
		const auto& transform = registry.Get<const ecs::components::Transform>(entity);
		mix(&transform.position, sizeof(transform.position));
		mix(&transform.rotation, sizeof(transform.rotation));
		mix(&transform.scale, sizeof(transform.scale));
		if (extra != nullptr && extra->contains(entity))
		{
			const auto fields =
			    Dump(reflection::ComponentToJson(_context, reflection::StorageType(*extra), extra->value(entity)));
			mix(fields.data(), fields.size());
		}
	}
	return QueryResult::Value({{"hash", fmt::format("{:016x}", hash)}, {"entities", entities.size()}});
}

QueryResult RegistryProvider::References(const ecs::Registry& registry, const Json& params) const
{
	const auto id = params.find("id");
	const auto target = id == params.end() ? std::nullopt : FromId(*id);
	if (!target.has_value())
	{
		return QueryResult::Error("ecs.references needs the id of an entity, gone or not");
	}
	const auto* info = _sources.info ? _sources.info() : nullptr;
	Json items = Json::array();
	for (const auto& [storageId, storage] : registry.Underlying().storage())
	{
		if (reflection::StorageType(storage) == entt::type_id<entt::entity>() ||
		    !reflection::IsReflected(_context, reflection::StorageType(storage)))
		{
			continue;
		}
		const auto component = reflection::ShortTypeName(reflection::StorageType(storage));
		for (const auto holder : storage)
		{
			for (auto& field :
			     reflection::References(_context, reflection::StorageType(storage), storage.value(holder), *target))
			{
				items.push_back({
				    {"id", entt::to_integral(holder)},
				    {"label", inspector::Describe(registry, holder, info).label},
				    {"component", component},
				    {"field", std::move(field)},
				});
			}
		}
	}
	return QueryResult::Value(std::move(items));
}

QueryResult RegistryProvider::Components(const ecs::Registry& registry) const
{
	Json items = Json::array();
	for (const auto& [id, storage] : registry.Underlying().storage())
	{
		if (reflection::StorageType(storage) == entt::type_id<entt::entity>() || storage.empty())
		{
			continue;
		}
		items.push_back({
		    {"name", reflection::ShortTypeName(reflection::StorageType(storage))},
		    {"count", storage.size()},
		    {"reflected", reflection::IsReflected(_context, reflection::StorageType(storage))},
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
