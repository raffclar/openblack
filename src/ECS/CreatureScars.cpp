/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureScars.h"

#include <optional>
#include <vector>

#include <glm/geometric.hpp>

#include "3D/CreatureBody.h"
#include "3D/L3DMesh.h"
#include "Common/GameRandom.h"
#include "Common/X87Vector.h"
#include "Creature/CreatureMarks.h"
#include "Creature/CreatureRig.h"
#include "Creature/CreatureTurnPose.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Transform.h"
#include "ECS/PosedModel.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CreatureSkinSystemInterface.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;

namespace
{
/// Where a bone of the creature's action points is in the world, as its body is posed this frame
std::optional<glm::vec3> PosedPointOf(entt::entity creature, uint32_t creature::CreatureRig::ActionPoints::* point)
{
	if (!Locator::entitiesRegistry::has_value() || !Locator::resources::has_value())
	{
		return std::nullopt;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* body = registry.TryGet<const Creature>(creature);
	const auto* animation = registry.TryGet<const CreatureAnimation>(creature);
	const auto* transform = registry.TryGet<const Transform>(creature);
	const auto& rigs = Locator::resources::value().GetCreatureRigs();
	if (body == nullptr || animation == nullptr || transform == nullptr)
	{
		return std::nullopt;
	}
	const auto rigId = creature::GetRigId(body->species);
	if (!rigs.Contains(rigId) || !rigs.Handle(rigId)->actionPoints.has_value())
	{
		return std::nullopt;
	}
	const auto bone = (*rigs.Handle(rigId)->actionPoints).*point;
	if (bone >= animation->boneMatrices.size())
	{
		return std::nullopt;
	}
	const auto pose = creature::TurnPose(animation->newerTurnPose, animation->boneMatrices,
	                                     creature::PlacementMatrix(transform->position, transform->rotation, transform->scale));
	return glm::vec3(creature::PosedBone(bone, pose.bones, pose.placement)[3]);
}
} // namespace

std::optional<glm::vec3> creature_scars::GroinOf(entt::entity creature)
{
	return PosedPointOf(creature, &creature::CreatureRig::ActionPoints::groin);
}

std::optional<glm::vec3> creature_scars::BellyOf(entt::entity creature)
{
	return PosedPointOf(creature, &creature::CreatureRig::ActionPoints::belly);
}

void creature_scars::MarkAlong(entt::entity creature, glm::vec3 from, glm::vec3 direction, uint8_t kind, uint8_t column)
{
	if (!Locator::creatureSkinSystem::has_value())
	{
		return;
	}
	if (const auto wound = WoundAlong(creature, from, direction, kind, column))
	{
		Locator::creatureSkinSystem::value().AddWound(creature, *wound);
	}
}

std::optional<openblack::creature_marks::Mark> creature_scars::WoundAlong(entt::entity creature, glm::vec3 from,
                                                                          glm::vec3 direction, uint8_t kind, uint8_t column)
{
	if (!Locator::entitiesRegistry::has_value() || !Locator::resources::has_value())
	{
		return std::nullopt;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* mesh = registry.TryGet<const Mesh>(creature);
	const auto* transform = registry.TryGet<const Transform>(creature);
	auto& meshes = Locator::resources::value().GetMeshes();
	if (mesh == nullptr || transform == nullptr || !meshes.Contains(mesh->id))
	{
		return std::nullopt;
	}
	// The line meets the skin as the body is posed at the end of its newest game turn
	const auto placement = creature::PlacementMatrix(transform->position, transform->rotation, transform->scale);
	// The unit direction adds the squares from z to x. A direction of no length stays as it is.
	const auto unit = direction == glm::vec3(0.0f) ? direction : x87::Scaled<x87::SumOrder::Zyx>(direction);
	const auto& model = *meshes.Handle(mesh->id);
	const auto drawn = posed_model::BonesOf(registry, creature, model);
	std::vector<glm::mat4> bones(drawn.begin(), drawn.end());
	if (const auto* animation = registry.TryGet<const CreatureAnimation>(creature);
	    animation != nullptr && !animation->newerTurnPose.empty() && animation->newerTurnPose.size() == bones.size())
	{
		creature_turn_pose::ToMesh(animation->newerTurnPose, placement, bones);
	}
	const auto hit = posed_model::NearestSkinHit(registry, creature, model, placement, bones, from, unit);
	if (!hit.has_value())
	{
		return std::nullopt;
	}
	const auto texel = creature_marks::scar::TexelAt(hit->uvs, hit->hit.s, hit->hit.t);
	return creature_marks::Mark {
	    .u = texel.x, .v = texel.y, .skin = static_cast<uint8_t>(hit->skin & 3u), .age = 0, .type = kind, .column = column};
}

void creature_scars::BurnOnCatching(entt::entity creature)
{
	if (!Locator::gameRandom::has_value() || !Locator::entitiesRegistry::has_value())
	{
		return;
	}
	const auto& registry = Locator::entitiesRegistry::value();
	const auto* body = registry.TryGet<const Creature>(creature);
	if (body == nullptr)
	{
		return;
	}
	// The synced draws are made whether or not the posed body can be marked: without it they are drawn about where the
	// creature stands, and nothing is marked
	const auto posedGroin = GroinOf(creature);
	const auto* standing = registry.TryGet<const Transform>(creature);
	const glm::vec3 groin = posedGroin.value_or(standing != nullptr ? standing->position : glm::vec3(0.0f));
	auto& random = Locator::gameRandom::value();
	const float reach = ShownSize(*body) * creature_marks::scar::k_BurnReachPerSize;
	for (int32_t i = 0; i < creature_marks::scar::k_CatchingBurnTries; ++i)
	{
		const auto from =
		    creature_marks::scar::BurnFrom(groin, reach, [&random](float limit) { return random.GameFloatRand(limit); });
		const auto direction = groin - from;
		// The squares add y, x, then z
		if (!(x87::Dot<x87::SumOrder::Yxz>(direction, direction) > creature_marks::scar::k_LeastReachSquared))
		{
			continue;
		}
		const auto kind = creature_marks::scar::BurnKind(random.GameRand(3));
		const auto column = creature_marks::scar::CatchingBurnColumn(random.GameRand(8));
		if (posedGroin.has_value())
		{
			MarkAlong(creature, from, direction, kind, column);
		}
	}
}
