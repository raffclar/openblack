/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "EditProviders.h"

#include <cctype>

#include <algorithm>
#include <iterator>
#include <string>
#include <utility>

#include <InspectorQuery.h>
#include <glm/trigonometric.hpp>

#include "ComponentReflection.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"

using namespace openblack;
using namespace openblack::inspector;

namespace
{

constexpr std::string_view k_NoRegistry = "there is no registry: no land is loaded";

/// A member of an object, null when it is missing (reading a missing member of a const object would abort)
const Json& Member(const Json& object, std::string_view key)
{
	static const Json k_Null;
	if (!object.is_object())
	{
		return k_Null;
	}
	const auto it = object.find(key);
	return it == object.end() ? k_Null : *it;
}

std::string Lower(std::string_view text)
{
	std::string lower(text);
	std::ranges::transform(lower, lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	return lower;
}

ParameterDescription IdParameter()
{
	return {.name = "id", .type = "integer", .description = "The entity's id", .required = true};
}

ParameterDescription ComponentParameter(std::string description)
{
	return {.name = "component", .type = "string", .description = std::move(description), .required = true};
}

QueryDescription Write(std::string name, std::string description, std::vector<ParameterDescription> parameters)
{
	return {.name = std::move(name),
	        .description = std::move(description),
	        .parameters = std::move(parameters),
	        .kind = ResultKind::Object,
	        .needsNear = false,
	        .writes = true};
}

/// The entity a request names by its id, if it exists
std::optional<entt::entity> EntityOf(const ecs::Registry& registry, const Json& params)
{
	const auto id = params.find("id");
	const auto entity = id == params.end() ? std::nullopt : FromId(*id);
	if (!entity.has_value() || !registry.Valid(*entity))
	{
		return std::nullopt;
	}
	return entity;
}

std::optional<glm::vec3> PositionOf(const ecs::Registry& registry, entt::entity entity)
{
	if (const auto* transform = registry.TryGet<const ecs::components::Transform>(entity); transform != nullptr)
	{
		return transform->position;
	}
	return std::nullopt;
}

} // namespace

std::optional<glm::vec3> openblack::inspector::ReadPosition(const Json& value, const WorldEditInterface& world)
{
	bool planar = false;
	const auto point = ReadPoint(value, &planar);
	if (!point.has_value())
	{
		return std::nullopt;
	}
	const auto x = static_cast<float>((*point)[0]);
	const auto z = static_cast<float>((*point)[2]);
	if (planar)
	{
		return glm::vec3(x, world.GroundHeight({x, z}), z);
	}
	return glm::vec3(x, static_cast<float>((*point)[1]), z);
}

EditProvider::EditProvider(EditSources sources, const entt::meta_ctx& context, WorldEditInterface& world)
    : _sources(std::move(sources))
    , _context(context)
    , _world(world)
{
}

std::vector<QueryDescription> EditProvider::Describe() const
{
	const ParameterDescription position {
	    .name = "position", .type = "point", .description = "[x, z] on the land, or [x, y, z]", .required = true};
	return {
	    {.name = "kinds",
	     .description = "The kinds of thing edit.create makes, or the types of one kind by their numbers",
	     .parameters = {{.name = "kind", .type = "string", .description = "A kind, to list its types", .required = false}},
	     .kind = ResultKind::List,
	     .needsNear = false,
	     .writes = false},
	    Write("create", "Makes a thing through the archetype the game makes it with; answers it and where it went",
	          {{.name = "kind", .type = "string", .description = "A kind from edit.kinds", .required = true},
	           {.name = "type",
	            .type = "integer or string",
	            .description = "The type within the kind, by its number or name (edit.kinds {kind})",
	            .required = true},
	           position,
	           {.name = "yaw",
	            .type = "number",
	            .description = "The angle it faces about the up axis, in degrees",
	            .required = false}}),
	    Write("move", "Moves a thing as the game's tools do; answers where it is and its map cell", {IdParameter(), position}),
	    Write("turn", "Turns a thing about the up axis by an angle",
	          {IdParameter(),
	           {.name = "yaw", .type = "number", .description = "The angle to turn by, in degrees", .required = true}}),
	    Write("set", "Sets one field of a component, its type checked; answers the field as it now is",
	          {IdParameter(),
	           ComponentParameter("The component, by name (ecs.components)"),
	           {.name = "field",
	            .type = "string",
	            .description = "The field, or a dotted path into a nested value",
	            .required = true},
	           {.name = "value",
	            .type = "any",
	            .description = "The new value, of the field's type; an object sets a nested value's fields",
	            .required = true}}),
	    Write("add", "Adds a component, made as it starts, then its fields set; answers the component",
	          {IdParameter(),
	           ComponentParameter("The component, by name"),
	           {.name = "fields",
	            .type = "object",
	            .description = "Fields to set once it is added, by name",
	            .required = false}}),
	    Write("remove_component", "Takes a component off an entity; answers the components it has left",
	          {IdParameter(), ComponentParameter("The component, by name")}),
	    Write("destroy",
	          "Takes a thing out of the world through the game's own removal: its physics, sounds, map cells, building, "
	          "town and what it alone owns; answers whether it is gone from each",
	          {IdParameter(),
	           {.name = "how",
	            .type = "string",
	            .description = "\"remove\" (gone at once, the default) or \"effect\" (as an effect that destroys things "
	                           "leaves it: a villager dies, a building burns down)",
	            .required = false}}),
	};
}

QueryResult EditProvider::Run(std::string_view query, const QueryContext& context)
{
	if (query == "kinds")
	{
		return Kinds(context.params);
	}
	auto* registry = _sources.registry ? _sources.registry() : nullptr;
	if (registry == nullptr)
	{
		return QueryResult::Error(std::string(k_NoRegistry));
	}
	const auto& params = context.params;
	if (query == "create")
	{
		return Create(*registry, params);
	}
	if (query == "move")
	{
		return Move(*registry, params);
	}
	if (query == "turn")
	{
		return Turn(*registry, params);
	}
	if (query == "set")
	{
		return Set(*registry, params);
	}
	if (query == "add")
	{
		return Add(*registry, params);
	}
	if (query == "remove_component")
	{
		return RemoveComponent(*registry, params);
	}
	if (query == "destroy")
	{
		return Destroy(*registry, params);
	}
	return QueryResult::Error("no query edit." + std::string(query));
}

QueryResult EditProvider::Kinds(const Json& params) const
{
	Json items = Json::array();
	if (const auto kind = StringMember(params, "kind"); kind.has_value())
	{
		const auto kinds = _world.Kinds();
		if (std::ranges::find(kinds, *kind) == kinds.end())
		{
			return QueryResult::Error("no kind " + *kind + "; ask edit.kinds for them");
		}
		const auto names = _world.TypeNames(*kind);
		for (size_t i = 0; i < names.size(); ++i)
		{
			items.push_back({{"type", i}, {"name", names[i]}});
		}
		return QueryResult::Value(std::move(items));
	}
	for (const auto& kind : _world.Kinds())
	{
		items.push_back({{"kind", kind}, {"types", _world.TypeNames(kind).size()}});
	}
	return QueryResult::Value(std::move(items));
}

Json EditProvider::Summary(const ecs::Registry& registry, entt::entity entity) const
{
	auto summary = ToListItem(entity, inspector::Describe(registry, entity, _sources.info ? _sources.info() : nullptr));
	summary["components"] = ComponentNames(registry, entity);
	return summary;
}

Json EditProvider::PresenceJson(entt::entity entity, std::optional<glm::vec3> lastPosition) const
{
	const auto presence = _world.PresenceOf(entity, lastPosition);
	Json result = {{"exists", presence.exists}, {"in_physics", presence.inPhysics}};
	if (presence.cell.has_value())
	{
		result["cell"] = {presence.cell->x, presence.cell->y};
		result["in_cell"] = presence.inCell;
	}
	else
	{
		result["cell"] = nullptr;
	}
	return result;
}

QueryResult EditProvider::Create(ecs::Registry& registry, const Json& params)
{
	const auto kind = StringMember(params, "kind");
	const auto kinds = _world.Kinds();
	if (!kind.has_value() || std::ranges::find(kinds, *kind) == kinds.end())
	{
		return QueryResult::Error("kind must be one of edit.kinds");
	}
	std::optional<int32_t> type;
	const auto& typeParam = Member(params, "type");
	if (typeParam.is_number_integer() && std::in_range<int32_t>(typeParam.get<int64_t>()))
	{
		type = static_cast<int32_t>(typeParam.get<int64_t>());
	}
	else if (typeParam.is_string())
	{
		const auto wanted = Lower(typeParam.get<std::string>());
		const auto names = _world.TypeNames(*kind);
		const auto found = std::ranges::find_if(names, [&wanted](const auto& name) { return Lower(name) == wanted; });
		if (found != names.end())
		{
			type = static_cast<int32_t>(std::distance(names.begin(), found));
		}
	}
	if (!type.has_value() || *type < 0 || static_cast<size_t>(*type) >= _world.TypeNames(*kind).size())
	{
		return QueryResult::Error("no such type of " + *kind + "; ask edit.kinds {\"kind\": \"" + *kind + "\"}");
	}
	const auto position = ReadPosition(Member(params, "position"), _world);
	if (!position.has_value())
	{
		return QueryResult::Error("position must be [x, z] or [x, y, z]");
	}
	const auto yaw = glm::radians(static_cast<float>(NumberMember(params, "yaw").value_or(0.0)));
	auto made = _world.Create(*kind, *type, *position, yaw);
	if (auto* problem = std::get_if<std::string>(&made); problem != nullptr)
	{
		return QueryResult::Error(*problem);
	}
	const auto entity = std::get<entt::entity>(made);
	auto result = Summary(registry, entity);
	result["presence"] = PresenceJson(entity, std::nullopt);
	return QueryResult::Value(std::move(result));
}

QueryResult EditProvider::Move(ecs::Registry& registry, const Json& params)
{
	const auto entity = EntityOf(registry, params);
	if (!entity.has_value())
	{
		return QueryResult::Error("no entity with that id");
	}
	const auto position = ReadPosition(Member(params, "position"), _world);
	if (!position.has_value())
	{
		return QueryResult::Error("position must be [x, z] or [x, y, z]");
	}
	if (auto problem = _world.Move(*entity, *position); !problem.empty())
	{
		return QueryResult::Error(problem);
	}
	const auto now = PositionOf(registry, *entity);
	return QueryResult::Value({
	    {"id", ToId(*entity)},
	    {"position", now.has_value() ? Json {now->x, now->y, now->z} : Json(nullptr)},
	    {"presence", PresenceJson(*entity, std::nullopt)},
	});
}

QueryResult EditProvider::Turn(ecs::Registry& registry, const Json& params)
{
	const auto entity = EntityOf(registry, params);
	if (!entity.has_value())
	{
		return QueryResult::Error("no entity with that id");
	}
	const auto yaw = NumberMember(params, "yaw");
	if (!yaw.has_value())
	{
		return QueryResult::Error("yaw must be a number of degrees");
	}
	if (auto problem = _world.Turn(*entity, glm::radians(static_cast<float>(*yaw))); !problem.empty())
	{
		return QueryResult::Error(problem);
	}
	return QueryResult::Value(
	    {{"id", ToId(*entity)},
	     {"rotation", reflection::ReadField(_context, entt::type_id<ecs::components::Transform>(),
	                                        registry.TryGet<ecs::components::Transform>(*entity), "rotation")
	                      .value_or(nullptr)}});
}

QueryResult EditProvider::Set(ecs::Registry& registry, const Json& params)
{
	const auto entity = EntityOf(registry, params);
	if (!entity.has_value())
	{
		return QueryResult::Error("no entity with that id");
	}
	const auto component = StringMember(params, "component");
	const auto field = StringMember(params, "field");
	if (!component.has_value() || !field.has_value())
	{
		return QueryResult::Error("component and field must be strings");
	}
	auto* storage = reflection::FindStorage(registry.Underlying(), _context, *component);
	if (storage == nullptr || !storage->contains(*entity))
	{
		return QueryResult::Error("the entity has no " + *component + "; ask ecs.entity for its components");
	}
	auto* data = storage->value(*entity);
	if (auto problem = reflection::SetField(_context, reflection::StorageType(*storage), data, *field, Member(params, "value"));
	    !problem.empty())
	{
		return QueryResult::Error(*component + "." + problem);
	}
	_world.Changed(*entity, *component);
	return QueryResult::Value({
	    {"id", ToId(*entity)},
	    {"component", *component},
	    {"field", *field},
	    {"value", reflection::ReadField(_context, reflection::StorageType(*storage), data, *field).value_or(nullptr)},
	});
}

QueryResult EditProvider::Add(ecs::Registry& registry, const Json& params)
{
	const auto entity = EntityOf(registry, params);
	if (!entity.has_value())
	{
		return QueryResult::Error("no entity with that id");
	}
	const auto component = StringMember(params, "component");
	if (!component.has_value())
	{
		return QueryResult::Error("component must be a string");
	}
	auto* storage = reflection::FindStorage(registry.Underlying(), _context, *component);
	if (storage == nullptr)
	{
		return QueryResult::Error("no component " + *component + "; ask ecs.components for their names");
	}
	if (storage->contains(*entity))
	{
		return QueryResult::Error("the entity has " + *component + " already: set its fields with edit.set");
	}
	const auto fields = params.find("fields");
	if (fields != params.end() && !fields->is_object())
	{
		return QueryResult::Error("fields must be an object of field names and values");
	}
	if (storage->push(*entity) == storage->end())
	{
		return QueryResult::Error(*component + " can't be made without values: it has no starting state");
	}
	if (fields != params.end())
	{
		for (const auto& [name, value] : fields->items())
		{
			auto problem =
			    reflection::SetField(_context, reflection::StorageType(*storage), storage->value(*entity), name, value);
			if (!problem.empty())
			{
				// Nothing half made is left behind
				storage->remove(*entity);
				_world.Changed(*entity, *component);
				return QueryResult::Error(*component + "." + problem);
			}
		}
	}
	_world.Changed(*entity, *component);
	return QueryResult::Value({
	    {"id", ToId(*entity)},
	    {"component", *component},
	    {"value", reflection::ComponentToJson(_context, reflection::StorageType(*storage), storage->value(*entity))},
	});
}

QueryResult EditProvider::RemoveComponent(ecs::Registry& registry, const Json& params)
{
	const auto entity = EntityOf(registry, params);
	if (!entity.has_value())
	{
		return QueryResult::Error("no entity with that id");
	}
	const auto component = StringMember(params, "component");
	if (!component.has_value())
	{
		return QueryResult::Error("component must be a string");
	}
	auto* storage = reflection::FindStorage(registry.Underlying(), _context, *component);
	if (storage == nullptr || !storage->contains(*entity))
	{
		return QueryResult::Error("the entity has no " + *component);
	}
	storage->remove(*entity);
	_world.Changed(*entity, *component);
	return QueryResult::Value({{"id", ToId(*entity)}, {"components", ComponentNames(registry, *entity)}});
}

QueryResult EditProvider::Destroy(ecs::Registry& registry, const Json& params)
{
	const auto entity = EntityOf(registry, params);
	if (!entity.has_value())
	{
		return QueryResult::Error("no entity with that id");
	}
	auto how = RemoveHow::Remove;
	if (const auto text = StringMember(params, "how"); text.has_value())
	{
		if (*text == "effect")
		{
			how = RemoveHow::Effect;
		}
		else if (*text != "remove")
		{
			return QueryResult::Error("how must be \"remove\" or \"effect\"");
		}
	}
	const auto before = Summary(registry, *entity);
	const auto lastPosition = PositionOf(registry, *entity);
	if (auto problem = _world.Remove(*entity, how); !problem.empty())
	{
		return QueryResult::Error(problem);
	}
	Json result = {
	    {"id", ToId(*entity)},
	    {"was", before["label"]},
	    {"presence", PresenceJson(*entity, lastPosition)},
	};
	if (registry.Valid(*entity))
	{
		// An effect may leave it, changed: a building with no life, a villager dying, an animal falling
		result["now"] = Summary(registry, *entity);
	}
	return QueryResult::Value(std::move(result));
}
