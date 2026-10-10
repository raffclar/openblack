/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "TempleConstruction.h"

#include <cstddef>

#include <vector>

#include <entt/core/hashed_string.hpp>
#include <glm/vec2.hpp>
#include <spdlog/spdlog.h>

#include "3D/L3DMesh.h"
#include "Audio/GameMusic.h"
#include "ECS/Archetypes/CitadelArchetype.h"
#include "ECS/BuildingConstruction.h"
#include "ECS/Components/Abode.h"
#include "ECS/Components/Construction.h"
#include "ECS/Components/Fire.h"
#include "ECS/Components/Physics.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Town.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CinematicDirectorSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/WorldObjects.h"
#include "Game.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;

namespace
{

constexpr auto k_TempleMesh = entt::hashed_string("temple/b_first_temple_l3d");
/// The music a temple plays as it is finished
constexpr auto k_FinishedTempleMusic = audio::MusicType::ScriptEpic01;
static_assert(static_cast<int32_t>(k_FinishedTempleMusic) == 61);

ecs::Registry& Entities()
{
	return Locator::entitiesRegistry::value();
}

/// The reach across the ground of a planned temple: its model's larger half width, scaled
float PlannedReach(const Transform& transform)
{
	const auto& meshes = Locator::resources::value().GetMeshes();
	if (!meshes.Contains(k_TempleMesh.value()))
	{
		return 0.0f;
	}
	const auto half = meshes.Handle(k_TempleMesh.value())->GetBoundingBox().Size() * 0.5f;
	return building_construction::ModelReach({half.x, half.z}, transform.scale.x);
}

/// The player a town's temple is built for: the town's own, else the one its plan names
PlayerNames OwnerOf(const PlannedTemple& planned)
{
	std::optional<PlayerNames> owner;
	Entities().Each<const Town>([&](entt::entity, const Town& town) {
		if (static_cast<int32_t>(town.id) == planned.townId)
		{
			owner = town.owner;
		}
	});
	return owner.value_or(planned.owner);
}

/// The town a building belongs to, if any
std::optional<int32_t> TownOf(entt::entity building)
{
	const auto& registry = Entities();
	if (const auto* temple = registry.TryGet<const Temple>(building))
	{
		return temple->town;
	}
	if (const auto* abode = registry.TryGet<const Abode>(building))
	{
		return static_cast<int32_t>(abode->townId);
	}
	return std::nullopt;
}

/// A temple finished plays its music for the player at this machine
void PlayFinishedMusic(PlayerNames owner)
{
	auto* music = Game::Instance() != nullptr ? Game::Instance()->GetGameMusic() : nullptr;
	if (music == nullptr)
	{
		return;
	}
	const bool local = Locator::playerSystem::has_value() && Locator::playerSystem::value().GetLocalPlayer() == owner;
	bool cutScene = false;
	if (Locator::cinematicDirectorSystem::has_value())
	{
		const auto& director = Locator::cinematicDirectorSystem::value();
		cutScene = director.IsWideScreenOn() && director.GetWideScreenOwner() != 0;
	}
	const auto temple = building_construction::FinishedTemple {
	    .localPlayers = local,
	    .scriptCutScene = cutScene,
	    .scriptMusic = music->GetScriptType() != audio::MusicType::None,
	    .landNumber = Entities().Context().mapScriptGlobals.landNumber,
	};
	if (building_construction::PlaysFinishedMusic(temple))
	{
		music->StartScriptMusic(k_FinishedTempleMusic);
	}
}

/// A building is finished: no longer under construction, its site gone. A temple's heart is whole again.
void Finish(entt::entity building)
{
	auto& registry = Entities();
	registry.Remove<BuildProgress>(building);
	registry.Remove<BuildingSite>(building);
	if (registry.AllOf<Temple>(building))
	{
		if (auto* life = registry.TryGet<ObjectLife>(building))
		{
			life->life = 1.0f;
		}
		// TODO(worship-sites): its player's worship sites are planned for each of its towns
		PlayFinishedMusic(registry.Get<const Temple>(building).owner);
	}
	else
	{
		// TODO(villager-life): a finished house takes its people in and its town reacts to the new building
		SPDLOG_LOGGER_DEBUG(spdlog::get("game"), "A building other than a temple was finished without its town's reaction");
	}
}

void Apply(entt::entity building, building_construction::Progress progress)
{
	if (progress.finished)
	{
		Finish(building);
		return;
	}
	auto& registry = Entities();
	if (auto* current = registry.TryGet<BuildProgress>(building))
	{
		current->built = progress.built;
	}
	else
	{
		registry.Assign<BuildProgress>(building, progress.built);
	}
	// It is drawn as far up as it now stands
	registry.SetDirty();
}

} // namespace

std::optional<entt::entity> construction::StartPlannedAt(glm::vec3 place, float scriptDesire)
{
	auto& registry = Entities();
	std::vector<entt::entity> plans;
	std::vector<building_construction::PlannedCandidate> candidates;
	registry.Each<const PlannedTemple, const Transform>(
	    [&](entt::entity entity, const PlannedTemple&, const Transform& transform) {
		    plans.push_back(entity);
		    candidates.push_back({.position = {transform.position.x, transform.position.z}, .reach = PlannedReach(transform)});
	    });
	const auto found = building_construction::PlannedAt(candidates, {place.x, place.z});
	if (!found.has_value())
	{
		return std::nullopt;
	}
	const auto plan = plans[*found];
	const auto planned = registry.Get<const PlannedTemple>(plan);
	const auto transform = registry.Get<const Transform>(plan);
	registry.Destroy(plan);
	// The temple's heart goes up where it was planned, with nothing of it built, and a site for its builders
	const auto temple =
	    archetypes::CitadelArchetype::Create(transform.position, OwnerOf(planned), planned.yAngle, transform.scale, 0.0f);
	registry.Get<Temple>(temple).town = planned.townId;
	registry.Assign<BuildingSite>(temple, building_construction::SiteDesire(scriptDesire));
	return temple;
}

float construction::BuiltOf(entt::entity object)
{
	const auto& registry = Entities();
	if (!registry.Valid(object))
	{
		return 1.0f;
	}
	const auto* progress = registry.TryGet<const BuildProgress>(object);
	return progress != nullptr ? progress->built : 1.0f;
}

bool construction::IsBuilt(entt::entity object)
{
	return !Entities().AllOf<BuildProgress>(object);
}

void construction::SetBuilt(entt::entity building, float built)
{
	auto& registry = Entities();
	if (!registry.Valid(building) || !(registry.AllOf<Temple>(building) || world_objects::IsBuilding(building)))
	{
		return;
	}
	const auto progress = building_construction::SetBuilt(built);
	Apply(building, progress);
	// A town's building left unfinished gets a site for its builders
	if (!progress.finished && TownOf(building).has_value() && !registry.AllOf<BuildingSite>(building))
	{
		registry.Assign<BuildingSite>(building);
	}
}

void construction::BuildBy(entt::entity building, float amount)
{
	const auto& registry = Entities();
	if (!registry.Valid(building) || IsBuilt(building))
	{
		return;
	}
	Apply(building, building_construction::BuildBy(BuiltOf(building), amount));
}
