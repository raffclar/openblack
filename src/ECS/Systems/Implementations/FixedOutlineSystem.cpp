/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS
#include "FixedOutlineSystem.h"

#include <algorithm>

#include <LNDFile.h>

#include "3D/AllMeshes.h"
#include "3D/L3DMesh.h"
#include "3D/LandIslandInterface.h"
#include "Common/X87Angle.h"
#include "Creature/CreatureCastMoves.h"
#include "Creature/CreatureObstacles.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/AnimatedStatic.h"
#include "ECS/Components/DeadTree.h"
#include "ECS/Components/Feature.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/FixedOutline.h"
#include "ECS/Components/Flowers.h"
#include "ECS/Components/MagicShield.h"
#include "ECS/Components/Mobile.h"
#include "ECS/Components/ScriptHighlight.h"
#include "ECS/Components/SpellDispenser.h"
#include "ECS/Components/TeleportStone.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/Tree.h"
#include "ECS/Components/Vortex.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/Map.h"
#include "ECS/Registry.h"
#include "ECS/ScriptHighlightRules.h"
#include "ECS/ThingMapPlace.h"
#include "ECS/ThingModel.h"
#include "Locator.h"
#include "Resources/ResourceManager.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;

namespace
{
/// How the game gives a kind of thing its outline
enum class Shape : uint8_t
{
	/// No outline
	None,
	/// A small circle at the trunk
	Trunk,
	/// The model's box, the thing turned about the upright axis only
	UprightBox,
	/// The model's box, the thing turned about all three axes
	LeaningBox,
	/// The circles of the temple inside the circle of its model's box
	Temple,
	/// The ten balls of a worship site inside the circle of its model's box
	WorshipSite,
};

Shape ShapeOf(const Registry& registry, entt::entity entity)
{
	if (registry.AnyOf<Tree>(entity))
	{
		return Shape::Trunk;
	}
	if (registry.AnyOf<Temple>(entity))
	{
		return Shape::Temple;
	}
	if (registry.AnyOf<WorshipSite>(entity))
	{
		return Shape::WorshipSite;
	}
	// Rocks, dead trees, vortices and teleport stones lean as the land's script turns them
	if (registry.AnyOf<MobileStatic, DeadTree, Vortex, TeleportStone>(entity))
	{
		return Shape::LeaningBox;
	}
	if (registry.AnyOf<Abode, Field, Feature, AnimatedStatic, Flowers, SpellDispenser, WorshipAltar, MagicShield, ShieldDome,
	                   ScriptHighlight>(entity))
	{
		return Shape::UprightBox;
	}
	// Forests, fish farms, piles, lanterns and everything that moves have none
	return Shape::None;
}

} // namespace

std::optional<fixed_outlines::Outline> FixedOutlineSystem::OutlineOf(const Registry& registry, entt::entity entity)
{
	const auto shape = ShapeOf(registry, entity);
	const auto* transform = registry.TryGet<const Transform>(entity);
	if (shape == Shape::None || transform == nullptr)
	{
		return std::nullopt;
	}
	// The outline stands at the thing's place on the map
	const auto onMap = thing_map_place::Of(registry, entity);
	const glm::vec2 place {map_coords::ToMetres(onMap.x), map_coords::ToMetres(onMap.z)};
	if (shape == Shape::Trunk)
	{
		return fixed_outlines::OfTrunk(place);
	}
	const auto model = thing_model::OutlineModelOf(registry, entity);
	if (!model.has_value() || !Locator::resources::has_value() || !Locator::resources::value().GetMeshes().Contains(*model))
	{
		return std::nullopt;
	}
	const auto box = Locator::resources::value().GetMeshes().Handle(*model)->GetBoundingBox();
	// The world keeps a highlight upright at its full size. Only its draw turns it and changes its size for the camera.
	const bool upright = registry.AllOf<ScriptHighlight>(entity);
	const float scale = upright ? script_highlights::k_WorldScale : transform->scale.x;
	const auto angles = upright ? glm::vec3(0.0f) : x87::GameAnglesYxz(transform->rotation[0], transform->rotation[2]);
	const auto rows = shape == Shape::LeaningBox ? creature_obstacles::LeaningPlacement(angles.y, angles.x, angles.z, scale)
	                                             : creature_obstacles::UprightPlacement(angles.y, scale);
	auto outline = fixed_outlines::OfModelBox(rows, place, angles.y, scale, box.Center(), box.Size() * 0.5f);
	if (shape == Shape::Temple)
	{
		outline.parts = fixed_outlines::TempleRing(place, angles.y);
	}
	else if (shape == Shape::WorshipSite)
	{
		// The balls keep only their places on the ground and their widths
		const auto spheres =
		    creature_cast_moves::WorshipSiteSpheres({place.x, transform->position.y, place.y}, angles.y, scale);
		outline.parts.clear();
		std::ranges::transform(spheres, std::back_inserter(outline.parts),
		                       [](const creature_cast_moves::CollideSphere& sphere) {
			                       return fixed_outlines::CircleOf({sphere.centre.x, sphere.centre.z}, sphere.radius);
		                       });
	}
	return outline;
}

void FixedOutlineSystem::Insert(entt::entity entity)
{
	auto& registry = Locator::entitiesRegistry::value();
	if (auto outline = OutlineOf(registry, entity))
	{
		registry.AssignOrReplace<FixedOutline>(entity, FixedOutline {.outline = std::move(*outline)});
	}
	else if (registry.AllOf<FixedOutline>(entity))
	{
		registry.Remove<FixedOutline>(entity);
	}
}

bool FixedOutlineSystem::CollidesWithFixed(const map_coords::MapCoords& place) const
{
	if (!map_coords::InBounds(place))
	{
		return true;
	}
	if (!Locator::entitiesMap::has_value() || !Locator::entitiesRegistry::has_value())
	{
		return false;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	const auto probe = fixed_outlines::Probe({map_coords::ToMetres(place.x), map_coords::ToMetres(place.z)});
	const auto cell = map_coords::Cell(place);
	const auto fixed = Locator::entitiesMap::value().GetFixedInGridCell(MapInterface::CellId(cell.x, cell.y));
	return std::ranges::any_of(fixed, [&registry, &probe](entt::entity thing) {
		const auto* outline = registry.TryGet<const FixedOutline>(thing);
		return outline != nullptr && fixed_outlines::Collides(outline->outline, probe);
	});
}

bool FixedOutlineSystem::IsWater(const map_coords::MapCoords& place) const
{
	if (!map_coords::InBounds(place) || !Locator::terrainSystem::has_value())
	{
		return true;
	}
	const auto* land =
	    Locator::terrainSystem::value().FindCell(glm::u16vec2(map_coords::CellX(place), map_coords::CellZ(place)));
	return land == nullptr || land->properties.hasWater != 0;
}

bool FixedOutlineSystem::IsOkToCreateAt(const map_coords::MapCoords& place) const
{
	return fixed_outlines::OkToCreate(CollidesWithFixed(place), IsWater(place));
}
