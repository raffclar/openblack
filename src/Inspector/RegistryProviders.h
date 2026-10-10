/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <functional>
#include <string_view>
#include <vector>

#include <InspectorProvider.h>
#include <entt/meta/context.hpp>

#include "EntityDescription.h"

namespace openblack::inspector
{

/// Where the providers find the registry and the info tables as they are asked: either may be missing, as between
/// lands or in a test
struct RegistrySources
{
	std::function<const ecs::Registry*()> registry;
	std::function<const InfoConstants*()> info;
};

/// The entities and their components:
///   ecs.entities  {component?, kind?, name?}  ids, kinds, labels and places
///   ecs.entity    {id, components?}            one entity, its components' names, their fields on request
///   ecs.components                             the component types there are, how many of each, which are reflected
///   ecs.references {id}                        the component fields that hold an entity, gone or not
///   ecs.hash      {component?}                 a hash of where everything is, to compare runs
class RegistryProvider final: public ProviderInterface
{
public:
	RegistryProvider(RegistrySources sources, const entt::meta_ctx& context);

	[[nodiscard]] std::string_view Name() const override { return "ecs"; }
	[[nodiscard]] std::vector<QueryDescription> Describe() const override;
	[[nodiscard]] QueryResult Run(std::string_view query, const QueryContext& context) override;

private:
	[[nodiscard]] QueryResult Entity(const ecs::Registry& registry, const Json& params) const;
	[[nodiscard]] QueryResult Components(const ecs::Registry& registry) const;
	[[nodiscard]] QueryResult References(const ecs::Registry& registry, const Json& params) const;
	[[nodiscard]] QueryResult Hash(const ecs::Registry& registry, const Json& params) const;

	RegistrySources _sources;
	const entt::meta_ctx& _context;
};

/// Objects in the world by where they are:
///   objects.find  {component?, kind?, name?} near, radius   the matching objects within the radius, nearest first
class ObjectsProvider final: public ProviderInterface
{
public:
	explicit ObjectsProvider(RegistrySources sources);

	[[nodiscard]] std::string_view Name() const override { return "objects"; }
	[[nodiscard]] std::vector<QueryDescription> Describe() const override;
	[[nodiscard]] QueryResult Run(std::string_view query, const QueryContext& context) override;

private:
	RegistrySources _sources;
};

/// The entities that have the components, are of the kind and whose labels hold the name (any case), each with its id,
/// kind, label and place; only those within the search's radius when one is given. Shared by the two providers.
[[nodiscard]] QueryResult FindEntities(const ecs::Registry& registry, const InfoConstants* info, const Json& params,
                                       const QueryOptions& options);

} // namespace openblack::inspector
