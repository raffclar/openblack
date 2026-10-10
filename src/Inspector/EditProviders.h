/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include <InspectorProvider.h>
#include <entt/entity/entity.hpp>
#include <entt/meta/context.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "EntityDescription.h"

namespace openblack::inspector
{

/// How a thing is taken out of the world
enum class RemoveHow : uint8_t
{
	/// Gone at once, with its body in the physics, its sounds, its place in the map's cells, its building and town, and
	/// what it alone owns (a store's piles, a dispenser's bubble)
	Remove,
	/// As an effect that destroys things leaves it: a building burnt down flickers out, a villager dies, an animal falls
	/// dead, a field loses its crop, the temple starts its destruction
	Effect,
};

/// Where a thing is in the world's own bookkeeping, to confirm a write did what it should
struct Presence
{
	bool exists {false};
	/// The map's cell it stands in, and whether the cell lists it; none for what isn't on the map
	std::optional<glm::ivec2> cell;
	bool inCell {false};
	/// Whether the physics has a body for it
	bool inPhysics {false};
};

/// The game's own ways of making, moving and taking out things, which the edit provider writes through so that the map,
/// the physics, the effects, the sounds and the towns stay right
class WorldEditInterface
{
public:
	virtual ~WorldEditInterface() = default;

	/// The kinds of thing that can be made, and the names of each kind's types (by their numbers)
	[[nodiscard]] virtual std::vector<std::string> Kinds() const = 0;
	[[nodiscard]] virtual std::vector<std::string> TypeNames(std::string_view kind) const = 0;
	/// The land's height under a point
	[[nodiscard]] virtual float GroundHeight(glm::vec2 point) const = 0;

	/// Makes a thing of a kind and type at a point, facing an angle about the up axis in radians, through the
	/// archetype the game makes it with: the thing, or why not
	virtual std::variant<entt::entity, std::string> Create(std::string_view kind, int32_t type, glm::vec3 position,
	                                                       float yaw) = 0;
	/// Moves a thing to a point, or turns it to face an angle, as the game's tools do: empty, or why not
	virtual std::string Move(entt::entity entity, glm::vec3 position) = 0;
	virtual std::string Turn(entt::entity entity, float yaw) = 0;
	/// Takes a thing out of the world: empty, or why not
	virtual std::string Remove(entt::entity entity, RemoveHow how) = 0;
	/// A component of a thing was set, added or taken off by hand: what is drawn is updated, and the map's cells if it
	/// moved
	virtual void Changed(entt::entity entity, std::string_view component) = 0;

	/// Where a thing is in the world's bookkeeping; a thing gone is looked for where it was last
	[[nodiscard]] virtual Presence PresenceOf(entt::entity entity, std::optional<glm::vec3> lastPosition) const = 0;
};

struct EditSources
{
	std::function<ecs::Registry*()> registry;
	std::function<const InfoConstants*()> info;
};

/// Making, changing and removing things, each answered with the small state that results and logged:
///   edit.kinds            {kind?}                                 the kinds that can be made, or one kind's types
///   edit.create           {kind, type, position, yaw?}            makes a thing through its archetype
///   edit.move             {id, position}                          moves a thing as the game's tools do
///   edit.turn             {id, yaw}                               turns a thing about the up axis
///   edit.set              {id, component, field, value}           sets a field, its type checked
///   edit.add              {id, component, fields?}                adds a component, its fields set
///   edit.remove_component {id, component}                         takes a component off
///   edit.destroy          {id, how?}                              takes a thing out through the game's removal
class EditProvider final: public ProviderInterface
{
public:
	EditProvider(EditSources sources, const entt::meta_ctx& context, WorldEditInterface& world);

	[[nodiscard]] std::string_view Name() const override { return "edit"; }
	[[nodiscard]] std::vector<QueryDescription> Describe() const override;
	[[nodiscard]] QueryResult Run(std::string_view query, const QueryContext& context) override;

private:
	[[nodiscard]] QueryResult Kinds(const Json& params) const;
	[[nodiscard]] QueryResult Create(ecs::Registry& registry, const Json& params);
	[[nodiscard]] QueryResult Move(ecs::Registry& registry, const Json& params);
	[[nodiscard]] QueryResult Turn(ecs::Registry& registry, const Json& params);
	[[nodiscard]] QueryResult Set(ecs::Registry& registry, const Json& params);
	[[nodiscard]] QueryResult Add(ecs::Registry& registry, const Json& params);
	[[nodiscard]] QueryResult RemoveComponent(ecs::Registry& registry, const Json& params);
	[[nodiscard]] QueryResult Destroy(ecs::Registry& registry, const Json& params);

	/// An entity and its place, kind and label, as each write answers
	[[nodiscard]] Json Summary(const ecs::Registry& registry, entt::entity entity) const;
	[[nodiscard]] Json PresenceJson(entt::entity entity, std::optional<glm::vec3> lastPosition) const;

	EditSources _sources;
	const entt::meta_ctx& _context;
	WorldEditInterface& _world;
};

/// A point given as [x, z] on the land or [x, y, z], none for anything else
[[nodiscard]] std::optional<glm::vec3> ReadPosition(const Json& value, const WorldEditInterface& world);

} // namespace openblack::inspector
