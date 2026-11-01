/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "ThingModel.h"

#include "3D/AllMeshes.h"
#include "ECS/Components/BuildingDamage.h"
#include "ECS/Components/Firefly.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/TeleportStone.h"
#include "ECS/Components/VillageTotem.h"
#include "ECS/Components/Vortex.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/Registry.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Resources/ResourceManager.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;

std::optional<entt::id_type> thing_model::ModelOf(const Registry& registry, entt::entity thing)
{
	if (!registry.Valid(thing))
	{
		return std::nullopt;
	}
	// A piece that broke off a building is a rock of the statics' rock row at its first size. The game shows the piece
	// as its broken triangles, but the rock model gives it its size and shape on the map.
	if (registry.AllOf<BuildingPiece>(thing))
	{
		if (!Locator::infoConstants::has_value())
		{
			return std::nullopt;
		}
		const auto& info = Locator::infoConstants::value().mobileStatic.at(static_cast<size_t>(MobileStaticInfo::Rock));
		return resources::HashIdentifier(info.meshId);
	}
	if (const auto* mesh = registry.TryGet<const Mesh>(thing))
	{
		return mesh->id;
	}
	if (const auto* altar = registry.TryGet<const WorshipAltar>(thing); altar != nullptr && altar->model != 0)
	{
		return altar->model;
	}
	// A firefly shows only a sprite, but in the map it has the plain model of a thing that has no model of its own
	if (registry.AllOf<Firefly>(thing))
	{
		return resources::HashIdentifier(MeshId::Dummy);
	}
	return std::nullopt;
}

std::optional<entt::id_type> thing_model::OutlineModelOf(const Registry& registry, entt::entity thing)
{
	if (!registry.Valid(thing))
	{
		return std::nullopt;
	}
	if (registry.AllOf<Vortex>(thing))
	{
		return resources::HashIdentifier(MeshId::SpellVortexCylinder);
	}
	// A town centre's totem gets its outline from the icon on its plinth, with the icon that it has now
	if (const auto* totem = registry.TryGet<const VillageTotem>(thing))
	{
		return ModelOf(registry, totem->icon);
	}
	// A teleport stone is a standing thing of the teleport's kind: it has that kind's model, which only gives it its
	// outline
	if (registry.AllOf<TeleportStone>(thing))
	{
		if (!Locator::infoConstants::has_value())
		{
			return std::nullopt;
		}
		const auto& info = Locator::infoConstants::value().mobileStatic.at(static_cast<size_t>(MobileStaticInfo::Teleport));
		return resources::HashIdentifier(info.meshId);
	}
	return ModelOf(registry, thing);
}
