/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "SpellIconArchetype.h"

#include <cmath>

#include <algorithm>
#include <optional>

#include <entt/core/hashed_string.hpp>
#include <glm/glm.hpp>
#include <glm/gtx/euler_angles.hpp>

#include "3D/L3DMesh.h"
#include "3D/LandIslandInterface.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/SpellIcon.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/Registry.h"
#include "ECS/WorshipSites.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Magic/SpellIcons.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs::archetypes;
using namespace openblack::ecs::components;
namespace icons = openblack::magic::spell_icons;

namespace
{
/// The worship site's model, whose points place its icons
constexpr auto k_SiteMesh = entt::hashed_string("temple/b_worship_l3d");
/// Icons are pushed out at most this far past their places
constexpr float k_MostPush = 30.0f;

[[nodiscard]] float LandHeightAt(glm::vec2 point)
{
	return Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(point) : 0.0f;
}

[[nodiscard]] const graphics::L3DMesh* MeshNamed(entt::id_type id)
{
	if (!Locator::resources::has_value())
	{
		return nullptr;
	}
	auto& meshes = Locator::resources::value().GetMeshes();
	return meshes.Contains(id) ? &*meshes.Handle(id) : nullptr;
}

[[nodiscard]] const graphics::L3DMesh* CentreMesh(entt::entity centre)
{
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* mesh = registry.TryGet<const Mesh>(centre);
	return mesh != nullptr ? MeshNamed(mesh->id) : nullptr;
}

[[nodiscard]] MeshId IconMesh(SpellIconInfo kind)
{
	return Locator::infoConstants::value().spellIcon.at(static_cast<size_t>(kind)).meshId;
}

/// The icon's model, standing at a point on the land turned and scaled; the seed spinning above its palm
entt::entity MakeIcon(glm::vec3 position, const glm::mat3& rotation, float scale, SpellIconInfo kind, SpellSeedType seed)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = registry.Create();
	registry.Assign<Transform>(entity, position, rotation, glm::vec3(scale));
	const auto meshId = resources::HashIdentifier(IconMesh(kind));
	registry.Assign<Mesh>(entity, meshId, static_cast<int8_t>(0), static_cast<int8_t>(0));
	// The seed spins above the first point of the icon's model, which is its palm
	auto middle = position;
	if (const auto* model = MeshNamed(meshId); model != nullptr && !model->GetExtraMetrics().empty())
	{
		middle += rotation * (glm::vec3(model->GetExtraMetrics().front()[3]) * scale);
	}
	middle.y += icons::k_SeedAbovePalm;
	registry.Assign<IconSeed>(entity, IconSeed {.seed = seed, .middle = middle});
	return entity;
}
} // namespace

size_t SpellIconArchetype::CentreHandPlaces(entt::entity centre)
{
	const auto* mesh = CentreMesh(centre);
	return mesh != nullptr ? std::min(mesh->GetExtraMetrics().size(), icons::k_CentreHands) : 0;
}

entt::entity SpellIconArchetype::CreateCentreHand(entt::entity centre, entt::entity town, size_t hand, SpellSeedType seed)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* mesh = CentreMesh(centre);
	if (mesh == nullptr || hand >= mesh->GetExtraMetrics().size() || !registry.AllOf<Transform>(centre) ||
	    !Locator::infoConstants::has_value())
	{
		return entt::null;
	}
	// The hand stands at the centre's point for it, turned as the point is turned on the centre
	const auto& placed = registry.Get<const Transform>(centre);
	const auto& point = mesh->GetExtraMetrics().at(hand);
	auto position = placed.position + placed.rotation * (glm::vec3(point[3]) * placed.scale);
	// The centre's model follows the land, its points with it: raised by how much higher the land is there than under
	// the centre
	position.y += LandHeightAt({position.x, position.z}) - LandHeightAt({placed.position.x, placed.position.z});
	const auto turned = placed.rotation * glm::mat3(point);
	const float yAngle = std::atan2(turned[0].z, turned[0].x);
	const auto entity =
	    MakeIcon(position, glm::mat3(glm::eulerAngleY(-yAngle)), placed.scale.x, SpellIconInfo::TownCentre, seed);
	registry.Assign<TownCentreIcon>(
	    entity, TownCentreIcon {.centre = centre, .town = town, .hand = static_cast<uint8_t>(hand), .seed = seed});
	return entity;
}

entt::entity SpellIconArchetype::CreateSiteIcon(entt::entity site, SpellSeedType seed)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* model = MeshNamed(k_SiteMesh.value());
	if (model == nullptr || !registry.AllOf<WorshipSite, Transform>(site) || !Locator::infoConstants::has_value())
	{
		return entt::null;
	}
	const auto& component = registry.Get<const WorshipSite>(site);
	const auto& placed = registry.Get<const Transform>(site);
	const glm::vec2 at {placed.position.x, placed.position.z};
	const auto& points = model->GetExtraMetrics();
	const auto taken = [&registry, site](glm::vec2 place) {
		bool found = false;
		registry.Each<const WorshipIcon, const Transform>([&](const WorshipIcon& icon, const Transform& transform) {
			found = found || (icon.site == site &&
			                  glm::distance(glm::vec2(transform.position.x, transform.position.z), place) < icons::k_SamePlace);
		});
		return found;
	};
	// The first of its places with no icon on it; while they are all taken, each pushed further out from the site, and
	// past the furthest the last place tried
	std::optional<glm::vec2> place;
	glm::vec2 tried = at;
	float placeHeight = 0.0f;
	for (float push = 0.0f; !place.has_value() && push <= k_MostPush; push += icons::k_SitePushStep)
	{
		for (uint32_t index = icons::k_FirstSitePoint;
		     index < icons::k_FirstSitePoint + icons::k_SitePoints && index < points.size() && !place.has_value(); ++index)
		{
			tried = ecs::worship_site::TurnedPoint(at, component.facing, glm::vec3(points.at(index)[3]));
			if (push > 0.0f && tried != at)
			{
				tried += glm::normalize(tried - at) * push;
			}
			if (!taken(tried))
			{
				place = tried;
				placeHeight = points.at(index)[3].y;
			}
		}
	}
	const auto spot = place.value_or(tried);
	// As high above the land as its place is on the site's model
	const glm::vec3 position {spot.x, LandHeightAt(spot) + placeHeight, spot.y};
	// Turned and scaled as its site is
	const auto entity = MakeIcon(position, placed.rotation, placed.scale.x, SpellIconInfo::Basic, seed);
	registry.Assign<WorshipIcon>(entity, WorshipIcon {.site = site, .seed = seed});
	return entity;
}
