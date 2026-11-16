/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "EditorLandEdits.h"

#include <cmath>

#include <algorithm>
#include <array>
#include <utility>
#include <vector>

#include <glm/vector_relational.hpp>

#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "ECS/Archetypes/StreamSegmentArchetype.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Field.h"
#include "ECS/Components/FishFarm.h"
#include "ECS/Components/Footpath.h"
#include "ECS/Components/Forest.h"
#include "ECS/Components/Hand.h"
#include "ECS/Components/HandGrab.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Stream.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/Registry.h"
#include "ECS/Systems/EditorSystemInterface.h"
#include "ECS/Systems/PlayerCommandSystemInterface.h"
#include "LNDFile.h"
#include "Locator.h"
#include "MainThreadWork.h"

namespace openblack::editor::land_edits
{

using player_commands::EditorLandStroke;
using player_commands::EditorPath;
using namespace ecs::components;

namespace
{

[[nodiscard]] bool OnTheMap(glm::vec2 point) noexcept
{
	return std::isfinite(point.x) && std::isfinite(point.y) && point.x >= 0.0f && point.y >= 0.0f && point.x < k_MapSide &&
	       point.y < k_MapSide;
}

} // namespace

std::optional<uint8_t> WorldLand::AltitudeAt(glm::u16vec2 corner) const
{
	const auto* cell = _land.FindCell(corner);
	return cell != nullptr ? std::optional<uint8_t>(cell->altitude) : std::nullopt;
}

std::optional<uint8_t> WorldLand::CountryAt(glm::u16vec2 corner) const
{
	const auto* cell = _land.FindCell(corner);
	return cell != nullptr ? std::optional<uint8_t>(cell->properties.country) : std::nullopt;
}

int WorldLand::Countries() const
{
	return static_cast<int>(_land.GetCountries().size());
}

float WorldLand::HeightAt(glm::vec2 point) const
{
	return _land.GetHeightAt(point);
}

void WorldLand::SetAltitude(glm::u16vec2 corner, uint8_t altitude)
{
	_land.SetCellAltitude(corner, altitude);
}

void WorldLand::SetCountry(glm::u16vec2 corner, uint8_t country)
{
	_land.SetCellCountry(corner, country);
}

EditorLandStroke StrokeOf(const land_brush::Brush& brush, glm::vec2 centre) noexcept
{
	return {.mode = static_cast<uint8_t>(brush.mode),
	        .innerRadius = brush.innerRadius,
	        .outerRadius = brush.outerRadius,
	        .step = static_cast<uint8_t>(std::clamp(brush.step, 0, 255)),
	        .country = brush.country,
	        .centre = centre};
}

land_brush::Brush BrushOf(const EditorLandStroke& stroke) noexcept
{
	return {.mode = static_cast<land_brush::Mode>(stroke.mode),
	        .innerRadius = stroke.innerRadius,
	        .outerRadius = stroke.outerRadius,
	        .step = static_cast<int>(stroke.step),
	        .country = stroke.country};
}

std::optional<std::string_view> LandStrokeRefusal(const EditorLandStroke& stroke, std::optional<int> countries) noexcept
{
	if (stroke.mode > static_cast<uint8_t>(land_brush::Mode::Paint))
	{
		return "the brush has no such mode";
	}
	if (!std::isfinite(stroke.innerRadius) || !std::isfinite(stroke.outerRadius) || !std::isfinite(stroke.centre.x) ||
	    !std::isfinite(stroke.centre.y))
	{
		return "a value is not a finite number";
	}
	if (stroke.innerRadius < 0.0f || stroke.outerRadius < stroke.innerRadius || stroke.outerRadius > land_brush::k_MostRadius)
	{
		return "the radii are not in the limits of the brush";
	}
	if (stroke.step < 1 || stroke.step > land_brush::k_MostStep)
	{
		return "the step is not in the limits of the brush";
	}
	if (stroke.country >= land_brush::k_MostCountries)
	{
		return "a cell cannot keep the country";
	}
	if (stroke.mode == static_cast<uint8_t>(land_brush::Mode::Paint) && countries.has_value() && stroke.country >= *countries)
	{
		return "the land has no such country";
	}
	if (!OnTheMap(stroke.centre))
	{
		return "the middle of the stroke is not on the map";
	}
	return std::nullopt;
}

size_t ApplyLandStroke(ecs::Registry& registry, EditableLand& land, const EditorLandStroke& stroke)
{
	if (LandStrokeRefusal(stroke, land.Countries()).has_value())
	{
		return 0;
	}
	const auto brush = BrushOf(stroke);
	if (brush.mode == land_brush::Mode::Paint)
	{
		const auto changes = land_brush::Paint(brush, stroke.centre, land.Countries(),
		                                       [&land](glm::u16vec2 corner) { return land.CountryAt(corner); });
		for (const auto& change : changes)
		{
			land.SetCountry(change.corner, change.country);
		}
		return changes.size();
	}
	const auto changes =
	    land_brush::Stroke(brush, stroke.centre, [&land](glm::u16vec2 corner) { return land.AltitudeAt(corner); });
	if (changes.empty())
	{
		return 0;
	}
	// The things on the land in reach keep their heights above it, so that they are not buried or left in the air
	const auto reach = glm::vec2(brush.outerRadius + land_brush::k_CornerSpacing);
	const auto lowest = stroke.centre - reach;
	const auto highest = stroke.centre + reach;
	std::vector<std::pair<entt::entity, float>> standing;
	registry.Each<const Transform>([&](entt::entity entity, const Transform& transform) {
		const glm::vec2 at {transform.position.x, transform.position.z};
		if (glm::any(glm::lessThan(at, lowest)) || glm::any(glm::greaterThan(at, highest)) ||
		    registry.AnyOf<Hand, InHand, InPhysics>(entity))
		{
			return;
		}
		standing.emplace_back(entity, land.HeightAt(at));
	});
	for (const auto& change : changes)
	{
		land.SetAltitude(change.corner, change.altitude);
	}
	for (const auto& [entity, before] : standing)
	{
		auto& transform = registry.Get<Transform>(entity);
		transform.position.y += land.HeightAt({transform.position.x, transform.position.z}) - before;
	}
	if (!standing.empty())
	{
		registry.SetDirty();
	}
	return changes.size();
}

std::optional<std::string_view> PathRefusal(const EditorPath& path) noexcept
{
	if (path.kind > static_cast<uint8_t>(paths::Kind::Stream))
	{
		return "the path has no such kind";
	}
	if (path.points.size() < 2)
	{
		return "a path needs two points or more";
	}
	if (path.points.size() > player_commands::k_MostEditorPathPoints)
	{
		return "the path has too many points";
	}
	if (!std::ranges::all_of(path.points, OnTheMap))
	{
		return "a point of the path is not on the map";
	}
	return std::nullopt;
}

bool CanHoldLink(const ecs::Registry& registry, entt::entity entity)
{
	return entity != entt::null && registry.Valid(entity) && registry.AllOf<Transform>(entity) &&
	       registry.AnyOf<Abode, Field, FishFarm, WorshipSite, BigForest, Temple>(entity);
}

std::optional<MadePath> ApplyPath(ecs::Registry& registry, const EditableLand& land, const EditorPath& path,
                                  paths::DrawnPaths* drawn)
{
	if (PathRefusal(path).has_value())
	{
		return std::nullopt;
	}
	MadePath made;
	if (path.kind == static_cast<uint8_t>(paths::Kind::Stream))
	{
		auto& streams = registry.Context().streams;
		std::vector<int> ids;
		ids.reserve(streams.size());
		for (const auto& [id, entity] : streams)
		{
			ids.push_back(id);
		}
		const auto id = paths::NextStreamId(ids);
		made.entity = registry.Create();
		made.stream = id;
		auto& stream = registry.Assign<Stream>(made.entity, id);
		streams.insert({id, made.entity});
		for (const auto point : path.points)
		{
			stream.points.emplace_back(point.x, land.HeightAt(point), point.y);
			if (stream.points.size() >= 2)
			{
				ecs::archetypes::StreamSegmentArchetype::Create(stream.points[stream.points.size() - 2], stream.points.back());
			}
		}
		if (drawn != nullptr)
		{
			drawn->streams.push_back(id);
		}
		return made;
	}
	made.entity = registry.Create();
	auto& footpath = registry.Assign<Footpath>(made.entity);
	for (const auto point : path.points)
	{
		// The node goes on the land at its place, as the land's script puts its nodes
		const auto place = paths::PlaceOf({point.x, 0.0f, point.y});
		const auto ground = map_coords::ToMetres(map_coords::MapCoords {.x = place.x, .z = place.y, .altitude = 0.0f});
		footpath.nodes.push_back({.position = {ground.x, land.HeightAt(ground), ground.y}, .place = place});
	}
	if (drawn != nullptr)
	{
		drawn->footpaths.push_back(made.entity);
	}
	const auto id = static_cast<Footpath::Id>(made.entity);
	for (const auto holder : std::array {path.firstHolder, path.lastHolder})
	{
		// A footpath with both ends on the same building leads from it one time
		if (!CanHoldLink(registry, holder) || (made.links == 1 && path.firstHolder == path.lastHolder))
		{
			continue;
		}
		// The newest link comes first, as the land's script links them
		if (auto* link = registry.TryGet<FootpathLink>(holder))
		{
			link->footpaths.insert(link->footpaths.begin(), id);
		}
		else
		{
			registry.Assign<FootpathLink>(holder, registry.Get<const Transform>(holder).position,
			                              std::vector<Footpath::Id> {id});
		}
		++made.links;
	}
	return made;
}

std::optional<size_t> RunLandStroke(const EditorLandStroke& stroke)
{
	if (!Locator::terrainSystem::has_value() || !Locator::entitiesRegistry::has_value())
	{
		return std::nullopt;
	}
	WorldLand land(Locator::terrainSystem::value());
	if (LandStrokeRefusal(stroke, land.Countries()).has_value())
	{
		return std::nullopt;
	}
	const auto changed = ApplyLandStroke(Locator::entitiesRegistry::value(), land, stroke);
	if (changed > 0)
	{
		// The new land is in the cells at once. Only its drawing goes to the main thread, and a second commit of the same
		// turn finds nothing more to draw.
		PostToMainThread(MainThreadApi::Graphics, [] {
			if (Locator::terrainSystem::has_value())
			{
				Locator::terrainSystem::value().CommitAltitudeChanges();
			}
		});
	}
	return changed;
}

std::optional<MadePath> RunPath(const EditorPath& path)
{
	if (!Locator::terrainSystem::has_value() || !Locator::entitiesRegistry::has_value())
	{
		return std::nullopt;
	}
	const WorldLand land(Locator::terrainSystem::value());
	auto* drawn = Locator::editorSystem::has_value() ? &Locator::editorSystem::value().GetDrawnPaths() : nullptr;
	return ApplyPath(Locator::entitiesRegistry::value(), land, path, drawn);
}

void SendLandStroke(const EditorLandStroke& stroke)
{
	if (Locator::playerCommandSystem::has_value())
	{
		Locator::playerCommandSystem::value().Submit(stroke);
	}
}

void SendPath(const EditorPath& path)
{
	if (Locator::playerCommandSystem::has_value())
	{
		Locator::playerCommandSystem::value().Submit(path);
	}
}

} // namespace openblack::editor::land_edits
