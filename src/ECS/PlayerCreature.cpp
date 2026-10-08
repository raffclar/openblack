/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "PlayerCreature.h"

#include <cmath>

#include <algorithm>
#include <numbers>
#include <utility>
#include <vector>

#include <spdlog/spdlog.h>

#include "3D/L3DMesh.h"
#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "Common/GUtilsDistance.h"
#include "Creature/CreatureMind.h"
#include "Creature/CreatureMindFileBody.h"
#include "Debug/StateHash.h"
#include "ECS/Archetypes/CreatureArchetype.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureDrawPose.h"
#include "ECS/Components/CreatureLeash.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Temple.h"
#include "ECS/Components/Transform.h"
#include "ECS/Components/WorshipSite.h"
#include "ECS/Registry.h"
#include "ECS/Systems/LeashSystemInterface.h"
#include "EngineConfig.h"
#include "FileSystem/FileSystemInterface.h"
#include "Locator.h"
#include "Resources/Loaders.h"
#include "Resources/ResourcesInterface.h"

namespace openblack::ecs::player_creature
{

std::optional<std::filesystem::path> ProfileMindPath(std::string_view file, const std::filesystem::path& mindFolder)
{
	if (file.empty())
	{
		return std::nullopt;
	}
	return mindFolder / file;
}

bool ProfileHasCreature()
{
	const auto& fileSystem = Locator::filesystem::value();
	const auto path =
	    ProfileMindPath(Locator::config::value().profileCreatureFile, fileSystem.GetPath<filesystem::Path::CreatureMind>(true));
	return path.has_value() && fileSystem.Exists(*path);
}

namespace
{
/// What the two DEV_FUNCTION values the land scripts use on the player's creature are called
enum class DevFunctionId : int32_t
{
	RopeLeashEnabled = 2,
	OtherLeashesEnabled = 3,
};

bool IsCreature(const Registry& registry, entt::entity thing)
{
	return thing != entt::null && registry.Valid(thing) && registry.AllOf<components::Creature>(thing);
}
} // namespace

std::optional<LoadPlan> PlanLoad(bool playerHasCreature, const creature::CreatureMind* mind, glm::vec2 pointXZ)
{
	if (playerHasCreature || mind == nullptr || !mind->Loaded())
	{
		return std::nullopt;
	}
	const auto body = creature_mind_body::FromMindFile(mind->data);
	if (!body.has_value())
	{
		return std::nullopt;
	}
	// the middle of the point's cell, as the original's map coordinates of a cell
	const auto centre = [](float metres) {
		return map_coords::ToMetres(map_coords::CellOf(map_coords::ToFixed(metres)) * map_coords::k_FixedPerCell +
		                            map_coords::k_FixedPerCell / 2);
	};
	return LoadPlan {
	    .species = body->species,
	    .size = body->size,
	    .alignment = body->alignment,
	    .strength = body->strength,
	    .position = {centre(pointXZ.x), 0.0f, centre(pointXZ.y)},
	};
}

entt::entity LoadMyCreature(glm::vec2 pointXZ)
{
	const auto player = PlayerNames::PLAYER_ONE;
	const bool hasCreature =
	    Locator::leashSystem::has_value() && Locator::leashSystem::value().PlayersCreature(player).has_value();
	const auto& file = Locator::config::value().profileCreatureFile;
	auto& fileSystem = Locator::filesystem::value();
	const auto path = ProfileMindPath(file, fileSystem.GetPath<filesystem::Path::CreatureMind>(true));
	if (hasCreature || !path.has_value())
	{
		return entt::null;
	}
	auto& minds = Locator::resources::value().GetCreatureMinds();
	const auto loaded = minds.Load(file, resources::CreatureMindLoader::FromDiskTag {}, *path);
	const auto mindId = loaded.first->first;
	const auto handle = minds.Handle(mindId);
	const auto plan = PlanLoad(false, handle ? &*handle : nullptr, pointXZ);
	if (!plan.has_value())
	{
		SPDLOG_LOGGER_INFO(spdlog::get("game"), "LOAD_MY_CREATURE: no creature in {}", path->generic_string());
		return entt::null;
	}
	using archetypes::CreatureArchetype;
	auto body = CreatureArchetype::StartBody(plan->species);
	body.strength = plan->strength;
	if (plan->alignment.has_value())
	{
		body.alignment = *plan->alignment;
	}
	auto position = plan->position;
	position.y = Locator::terrainSystem::value().GetHeightAt(glm::vec2(position.x, position.z));
	// (inferred) the creature faces the way the script creatures do
	const float yAngle = std::numbers::pi_v<float>;
	return CreatureArchetype::Create(position, player, plan->species, mindId, yAngle,
	                                 plan->size.value_or(CreatureArchetype::StartScale(plan->species)), body);
}

void RegisterStateHash()
{
	state_hash::Register("creature", [](state_hash::Hasher& h) {
		if (Locator::entitiesRegistry::has_value())
		{
			HashCreatures(h, std::as_const(Locator::entitiesRegistry::value()));
		}
	});
}

void HashCreatures(state_hash::Hasher& h, const Registry& registry)
{
	registry.Each<const components::Creature>([&h, &registry](entt::entity entity, const components::Creature& creature) {
		h.U32(static_cast<uint32_t>(entity));
		h.U32(static_cast<uint32_t>(creature.owner));
		h.U32(static_cast<uint32_t>(creature.species));
		h.U32(creature.leashable ? 1u : 0u);
		h.U32(creature.inDevScript ? 1u : 0u);
		h.Float(creature.alignment);
		h.Float(creature.fatness);
		h.Float(creature.strength);
		h.Float(creature.size);
		const auto* mind = registry.TryGet<const components::CreatureMindState>(entity);
		h.U32(mind != nullptr ? mind->developmentPhase : 0xFFFFFFFFu);
		const auto* leash = registry.TryGet<const components::CreatureLeash>(entity);
		h.U32(leash != nullptr && leash->home.has_value() ? 1u : 0u);
		h.Vec3(leash != nullptr ? leash->home.value_or(glm::vec3(0.0f)) : glm::vec3(0.0f));
	});
}

std::optional<entt::entity> PlayersCreature(const systems::LeashSystemInterface& leash, PlayerNames player)
{
	return leash.PlayersCreature(player);
}

glm::vec3 HomeOnGround(glm::vec3 point, float groundHeight)
{
	return {map_coords::ToMetres(map_coords::ToFixed(point.x)), groundHeight,
	        map_coords::ToMetres(map_coords::ToFixed(point.z))};
}

void SetHome(systems::LeashSystemInterface& leash, const Registry& registry, entt::entity thing, glm::vec3 home)
{
	if (IsCreature(registry, thing))
	{
		leash.SetHome(thing, home);
	}
}

std::optional<glm::vec3> TemplePenPoint(const components::Transform& temple, std::span<const glm::mat4> specialPoints)
{
	if (specialPoints.size() <= k_TemplePenPoint)
	{
		return std::nullopt;
	}
	const auto local = glm::vec3(specialPoints[k_TemplePenPoint][3]);
	return temple.position + (temple.rotation * (local * temple.scale));
}

void FollowTemplePens(systems::LeashSystemInterface& leash, const Registry& registry,
                      const std::function<std::optional<glm::vec3>(PlayerNames)>& penOf,
                      const std::function<float(glm::vec2)>& groundAt)
{
	std::vector<std::pair<entt::entity, PlayerNames>> creatures;
	registry.Each<const components::Creature>([&creatures](entt::entity entity, const components::Creature& creature) {
		creatures.emplace_back(entity, creature.owner);
	});
	for (const auto& [entity, owner] : creatures)
	{
		if (const auto pen = penOf(owner); pen.has_value())
		{
			auto home = HomeOnGround(*pen, 0.0f);
			home.y = groundAt(glm::vec2(home.x, home.z));
			leash.SetHome(entity, home);
		}
	}
}

void FollowTemplePens()
{
	if (!Locator::leashSystem::has_value() || !Locator::entitiesRegistry::has_value())
	{
		return;
	}
	const auto& registry = std::as_const(Locator::entitiesRegistry::value());
	const auto penOf = [&registry](PlayerNames owner) {
		std::optional<glm::vec3> pen;
		registry.Each<const components::Temple, const components::Transform>(
		    [&registry, &pen, owner](entt::entity entity, const components::Temple& temple,
		                             const components::Transform& transform) {
			    if (pen.has_value() || temple.owner != owner)
			    {
				    return;
			    }
			    // a temple being built or rebuilt keeps no creature yet
			    if (const auto* build = registry.TryGet<const components::CitadelPartBuild>(entity);
			        build != nullptr && (build->buildFlags & components::CitadelPartBuild::k_Built) == 0)
			    {
				    return;
			    }
			    const auto* mesh = registry.TryGet<const components::Mesh>(entity);
			    if (mesh == nullptr || !Locator::resources::has_value())
			    {
				    return;
			    }
			    const auto& meshes = Locator::resources::value().GetMeshes();
			    if (meshes.Contains(mesh->id))
			    {
				    pen = TemplePenPoint(transform, meshes.Handle(mesh->id)->GetExtraMetrics());
			    }
		    });
		return pen;
	};
	const auto groundAt = [](glm::vec2 point) {
		return Locator::terrainSystem::has_value() ? Locator::terrainSystem::value().GetHeightAt(point) : 0.0f;
	};
	FollowTemplePens(Locator::leashSystem::value(), registry, penOf, groundAt);
}

bool BetweenPenWalls(glm::vec2 temple, float templeYAngle, glm::vec2 point)
{
	const float first = templeYAngle + k_PenWallAngle;
	const float second = first + k_PenWallsApart;
	const float dx = point.x - temple.x;
	const float dz = point.y - temple.y;
	return std::cos(first) * dz - std::sin(first) * dx >= 0.0f && std::sin(second) * dx - std::cos(second) * dz >= 0.0f;
}

float PenDrawnSize(float size, float distanceToPen, bool betweenWalls)
{
	if (!betweenWalls || !(distanceToPen <= k_PenOuterRadius))
	{
		return size;
	}
	const float t = std::clamp(distanceToPen, k_PenInnerRadius, k_PenOuterRadius);
	return k_PenDrawnSize + (size - k_PenDrawnSize) * (t - k_PenInnerRadius) / (k_PenOuterRadius - k_PenInnerRadius);
}

void ShrinkInPens()
{
	if (!Locator::entitiesRegistry::has_value())
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto& lookup = std::as_const(registry);
	std::vector<entt::entity> creatures;
	lookup.Each<const components::Creature>(
	    [&creatures](entt::entity entity, const components::Creature&) { creatures.push_back(entity); });
	for (const auto entity : creatures)
	{
		const auto& creature = lookup.Get<const components::Creature>(entity);
		const auto* transform = lookup.TryGet<const components::Transform>(entity);
		const auto* leash = lookup.TryGet<const components::CreatureLeash>(entity);
		if (transform == nullptr || lookup.TryGet<const components::CreatureDrawPose>(entity) == nullptr)
		{
			continue;
		}
		std::optional<glm::vec3> drawn;
		if (leash != nullptr && leash->home.has_value())
		{
			lookup.Each<const components::Temple, const components::Transform, const components::CitadelWorship>(
			    [&](entt::entity temple, const components::Temple& owner, const components::Transform& place,
			        const components::CitadelWorship& worship) {
				    if (drawn.has_value() || owner.owner != creature.owner)
				    {
					    return;
				    }
				    if (const auto* build = lookup.TryGet<const components::CitadelPartBuild>(temple);
				        build != nullptr && (build->buildFlags & components::CitadelPartBuild::k_Built) == 0)
				    {
					    return;
				    }
				    const auto at = glm::vec2(transform->position.x, transform->position.z);
				    const float size =
				        PenDrawnSize(creature.size, gutils::GetDistanceInMetres(transform->position, *leash->home),
				                     BetweenPenWalls(glm::vec2(place.position.x, place.position.z), worship.heartYAngle, at));
				    if (size != creature.size)
				    {
					    drawn = glm::vec3(archetypes::CreatureArchetype::DrawnScale(creature.species, size));
				    }
			    });
		}
		registry.Get<components::CreatureDrawPose>(entity).scale = drawn;
	}
}

void SetDevelopmentStage(Registry& registry, entt::entity thing, int32_t stage)
{
	if (!IsCreature(std::as_const(registry), thing) || stage < 0 || stage > k_LastDevelopmentStage)
	{
		return;
	}
	if (auto* mind = registry.TryGet<components::CreatureMindState>(thing))
	{
		mind->developmentPhase = static_cast<uint32_t>(stage);
	}
}

bool DevFunction(systems::LeashSystemInterface& leash, int32_t function, PlayerNames player)
{
	if (function != std::to_underlying(DevFunctionId::RopeLeashEnabled) &&
	    function != std::to_underlying(DevFunctionId::OtherLeashesEnabled))
	{
		return false;
	}
	const auto creature = leash.PlayersCreature(player);
	if (!creature.has_value())
	{
		return true;
	}
	if (function == std::to_underlying(DevFunctionId::RopeLeashEnabled))
	{
		leash.SetKnown(*creature, LeashType::Rope, true);
		return true;
	}
	leash.SetKnown(*creature, LeashType::Evil, true);
	leash.SetKnown(*creature, LeashType::Good, true);
	leash.SetLeashable(*creature, true);
	return true;
}

void SetInDevScript(Registry& registry, entt::entity thing, bool inDevScript)
{
	if (IsCreature(std::as_const(registry), thing))
	{
		registry.Get<components::Creature>(thing).inDevScript = inDevScript;
	}
}

} // namespace openblack::ecs::player_creature
