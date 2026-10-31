/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "WorshipChantMusic.h"

#include <cmath>

#include <array>
#include <vector>

#include <glm/gtx/vec_swizzle.hpp>

#include "3D/LandIslandInterface.h"
#include "Common/GUtilsDistance.h"
#include "Creature/CreatureDance.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/GamePlayers.h"
#include "ECS/Registry.h"
#include "ECS/Systems/WorshipSiteSystemInterface.h"
#include "Locator.h"

using namespace openblack;
using namespace openblack::ecs::components;

namespace openblack::ecs::worship_chant_music
{

std::optional<size_t> NearestTemple(std::span<const glm::vec3> temples, glm::vec3 camera)
{
	float nearest = k_TempleFurthest;
	std::optional<size_t> found;
	for (size_t i = 0; i < temples.size(); ++i)
	{
		const float distance = gutils::GetDistanceInMetres(camera, temples[i]);
		if (distance < nearest)
		{
			nearest = distance;
			found = i;
		}
	}
	return found;
}

std::optional<size_t> NearestSite(std::span<const std::optional<Site>> sites, glm::vec3 camera)
{
	float nearest = k_SiteFurthest;
	std::optional<size_t> found;
	for (size_t i = 0; i < sites.size(); ++i)
	{
		if (!sites[i].has_value() || sites[i]->dancers == 0)
		{
			continue;
		}
		const float distance = gutils::GetDistanceInMetres(sites[i]->dance, camera);
		if (distance < nearest)
		{
			nearest = distance;
			found = i;
		}
	}
	return found;
}

bool NearInHeight(float camera, float groundAtDance)
{
	return std::abs(camera - groundAtDance) < k_Highest;
}

std::optional<audio::GameMusic::TurnInputs::CreatureDance> Heard(glm::vec3 camera)
{
	if (!Locator::entitiesRegistry::has_value() || !Locator::worshipSiteSystem::has_value())
	{
		return std::nullopt;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	const auto& sites = Locator::worshipSiteSystem::value();
	// The temples of every player, the neutral player last
	std::vector<entt::entity> temples;
	std::vector<glm::vec3> places;
	for (const auto player : ecs::game_players::All())
	{
		const auto temple = sites.TempleOf(player);
		if (temple == entt::null || !registry.Valid(temple) || !registry.AllOf<Transform, CitadelWorship>(temple))
		{
			continue;
		}
		temples.push_back(temple);
		places.push_back(registry.Get<const Transform>(temple).position);
	}
	const auto temple = NearestTemple(places, camera);
	if (!temple.has_value())
	{
		return std::nullopt;
	}
	const auto& placesRound = registry.Get<const CitadelWorship>(temples.at(*temple)).sites;
	std::array<std::optional<Site>, CitadelWorship::k_Places> heard;
	for (size_t place = 0; place < placesRound.size(); ++place)
	{
		const auto site = placesRound.at(place);
		if (site == entt::null || !registry.Valid(site))
		{
			continue;
		}
		const auto dance = sites.DancePosition(site);
		if (dance.has_value())
		{
			heard.at(place) = Site {.dance = *dance, .dancers = sites.Dancers(site)};
		}
	}
	const auto found = NearestSite(heard, camera);
	if (!found.has_value())
	{
		return std::nullopt;
	}
	const auto site = placesRound.at(*found);
	const auto& dance = *heard.at(*found);
	const float ground =
	    Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(glm::xz(dance.dance)) : dance.dance.y;
	if (!NearInHeight(camera.y, ground))
	{
		return std::nullopt;
	}
	// The chant of the site's tribe, by how many dance
	const auto tribe = static_cast<int32_t>(registry.Get<const WorshipSite>(site).tribe);
	const auto type = creature_dance::MusicType(tribe, dance.dancers);
	return audio::GameMusic::TurnInputs::CreatureDance {.type = static_cast<audio::MusicType>(type), .position = dance.dance};
}

} // namespace openblack::ecs::worship_chant_music
