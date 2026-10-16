/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "TattooSites.h"

#include <algorithm>

#include "3D/CreatureBody.h"
#include "3D/L3DMesh.h"
#include "3D/L3DSubMesh.h"
#include "Creature/CreatureRig.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs;

namespace
{
/// The first triangle of the body's skin that holds a tattoo place. The search goes through the submeshes and their
/// primitives in the order of the model, and through the triangles of each primitive that uses the skin of the place.
std::optional<posed_model::SkinPoint> FindSite(const graphics::L3DMesh& mesh, const creature_tattoo::Site& site)
{
	const auto& skins = mesh.GetSkinOrder();
	if (!site.enabled || site.skin >= skins.size())
	{
		return std::nullopt;
	}
	const auto skin = static_cast<uint32_t>(skins[site.skin]);
	const auto point = creature_tattoo_editor::TexturePoint(site);
	const auto& subMeshes = mesh.GetSubMeshes();
	for (size_t s = 0; s < subMeshes.size(); ++s)
	{
		const auto& geometry = subMeshes[s]->GetBodyGeometry();
		const auto uvOf = [&geometry](uint16_t vertex) {
			return vertex < geometry.uvs.size() ? geometry.uvs[vertex] : glm::vec2(0.0f);
		};
		for (const auto& primitive : subMeshes[s]->GetPrimitives())
		{
			if (primitive.skinID != skin)
			{
				continue;
			}
			const auto end = std::min<size_t>(primitive.indicesOffset + primitive.indicesCount, geometry.indices.size());
			for (size_t first = primitive.indicesOffset; first + 2 < end; first += 3)
			{
				const std::array corners {uvOf(geometry.indices[first]), uvOf(geometry.indices[first + 1]),
				                          uvOf(geometry.indices[first + 2])};
				if (const auto at = creature_tattoo_editor::InTriangle(point, corners); at.has_value())
				{
					return posed_model::SkinPoint {.subMesh = static_cast<uint32_t>(s),
					                               .firstIndex = static_cast<uint32_t>(first),
					                               .s = at->x,
					                               .t = at->y};
				}
			}
		}
	}
	return std::nullopt;
}

} // namespace

tattoo_sites::Found tattoo_sites::Find(const graphics::L3DMesh& mesh, CreatureType species)
{
	Found found {};
	const auto& rigs = Locator::resources::value().GetCreatureRigs();
	const auto rigId = creature::GetRigId(species);
	if (!rigs.Contains(rigId) || !rigs.Handle(rigId)->tattooSites.has_value())
	{
		return found;
	}
	const auto& sites = *rigs.Handle(rigId)->tattooSites;
	for (size_t i = 0; i < sites.size() && i < found.size(); ++i)
	{
		found.at(i) = FindSite(mesh, sites.at(i));
	}
	return found;
}

std::vector<creature_tattoo_editor::SiteOnScreen> tattoo_sites::OnScreen(const Registry& registry, entt::entity creature,
                                                                         const graphics::L3DMesh& mesh, const glm::mat4& model,
                                                                         std::span<const glm::mat4> bones, const Found& found,
                                                                         const screen_pick::View& view, glm::ivec2 origin)
{
	std::vector<creature_tattoo_editor::SiteOnScreen> onScreen;
	for (size_t i = 0; i < found.size(); ++i)
	{
		const auto& point = found.at(i);
		if (!point.has_value())
		{
			continue;
		}
		const auto corners = posed_model::CornersOnSkin(registry, creature, mesh, model, bones, *point);
		if (!corners.has_value())
		{
			continue;
		}
		const auto place = posed_model::SkinPointPosition(*corners, point->s, point->t);
		if (const auto pixel = screen_pick::PixelOf(view, place); pixel.has_value())
		{
			onScreen.push_back({.site = static_cast<uint8_t>(i),
			                    .screen = *pixel + origin,
			                    .facingCamera = creature_tattoo_editor::FacingCamera(*corners, place, view.camera)});
		}
	}
	return onScreen;
}
