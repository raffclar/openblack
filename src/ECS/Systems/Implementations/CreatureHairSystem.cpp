/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#define LOCATOR_IMPLEMENTATIONS

#include "CreatureHairSystem.h"

#include <algorithm>
#include <optional>

#include <glm/geometric.hpp>
#include <glm/gtx/transform.hpp>

#include "3D/CreatureBody.h"
#include "Creature/CreatureEyes.h"
#include "Creature/CreatureHair.h"
#include "Creature/CreatureMorph.h"
#include "Creature/CreatureRig.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureCopy.h"
#include "ECS/Components/CreatureHair.h"
#include "ECS/Components/Transform.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CreatureHairStep.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::components;
using openblack::creature::CreatureRig;

namespace
{
/// Where a strand is rooted this frame and which way it grows out, or none where its triangle has no area. The root
/// is sunk along the triangle's normal as it comes, unnormalised.
std::optional<creature_hair::Root> RootOf(const CreatureRig::HairStrand& strand, const creature_morph::Morph& morph,
                                          const std::vector<glm::mat4>& bones, const glm::mat4& model)
{
	const auto triangle = creature::PosedTriangle(strand.vertices, strand.bones, morph, bones, model,
	                                              strand.turned ? creature::k_TurnedHairOrder : creature::k_SkinPointOrder);
	const auto point = creature_eyes::PointOnTriangle(triangle[0], triangle[1], triangle[2], strand.u, strand.v);
	if (glm::length(point.normal) <= 0.0f)
	{
		return std::nullopt;
	}
	auto direction = creature_hair::GrowthDirection(point.normal);
	if (strand.turned)
	{
		// Turned in the frame of the bones that move the triangle
		std::array<glm::mat3, 3> frames {};
		for (size_t i = 0; i < frames.size(); ++i)
		{
			frames.at(i) = glm::mat3(creature::PosedBone(strand.bones.at(i), bones, model));
		}
		// Each angle moves with the alignment the body is drawn with
		const auto angle = [&strand, &morph](glm::length_t axis) {
			return creature_hair::ByAlignment(
			    std::array {strand.angles[0][axis], strand.angles[1][axis], strand.angles[2][axis]}, morph.evilGood);
		};
		const glm::vec3 turn {angle(0), angle(1), angle(2)};
		direction = creature_hair::GrowthDirection(point.normal, creature_hair::BoneFrame(frames), turn);
	}
	return creature_hair::Root {.position = point.position, .inwardNormal = point.normal, .direction = direction};
}

/// Moves the hair of a body for a number of seconds: the body has a species, a shape, a pose, a place and a size
void StepBodyHair(const resources::CreatureRigManager& rigs, CreatureType species, const creature_morph::Morph& morph,
                  const CreatureAnimation& animation, const Transform& transform, float size, CreatureHair& hair, float seconds)
{
	const auto rigId = creature::GetRigId(species);
	if (!rigs.Contains(rigId) || animation.boneMatrices.empty())
	{
		hair.groups.clear();
		return;
	}
	const auto& rig = *rigs.Handle(rigId);
	// The hair takes the look of the alignment the body is drawn with
	const auto alignment = morph.evilGood;
	const auto scale = creature_hair::HairScale(size);
	const auto model = creature::PlacementMatrix(transform.position, transform.rotation, transform.scale);
	StepHair(
	    hair, rig.hairGroups.size(),
	    [&](size_t g) {
		    const auto& source = rig.hairGroups[g];
		    const auto look = creature_hair::LookFor(source.looks, alignment);
		    return HairGroupShape {.strandCount = source.strands.size(),
		                           .segmentCount = source.segmentCount,
		                           .physics = creature_hair::PhysicsFor(look, scale, source.segmentCount),
		                           .colour = look.colour,
		                           .textured = source.textured};
	    },
	    [&](size_t g, size_t s) { return RootOf(rig.hairGroups[g].strands[s], morph, animation.boneMatrices, model); },
	    seconds);
}
} // namespace

void CreatureHairSystem::SetShown(bool shown)
{
	_shown = shown;
	if (!shown && Locator::entitiesRegistry::has_value())
	{
		Locator::entitiesRegistry::value().Each<CreatureHair>([](CreatureHair& hair) { hair = {}; });
	}
}

void CreatureHairSystem::Update(std::chrono::duration<float, std::milli> gameTime)
{
	if (!_shown)
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto& rigs = Locator::resources::value().GetCreatureRigs();
	const auto seconds = creature_hair::StepSeconds(std::chrono::duration<float, std::milli>(gameTime).count());

	registry.Each<const Creature, const CreatureMorph, const CreatureAnimation, const Transform, CreatureHair>(
	    [&](const Creature& creature, const CreatureMorph& morph, const CreatureAnimation& animation,
	        const Transform& transform, CreatureHair& hair) {
		    StepBodyHair(rigs, creature.species, morph.drawn, animation, transform, ShownSize(creature), hair, seconds);
	    });
}

void CreatureHairSystem::StepCopy(Registry& copies, entt::entity copy, std::chrono::duration<float, std::milli> frameTime)
{
	if (!_shown || !copies.Valid(copy))
	{
		return;
	}
	const auto& land = Locator::entitiesRegistry::value();
	const auto* body = copies.TryGet<const CreatureCopy>(copy);
	const auto* animation = copies.TryGet<const CreatureAnimation>(copy);
	const auto* transform = copies.TryGet<const Transform>(copy);
	auto* hair = copies.TryGet<CreatureHair>(copy);
	if (body == nullptr || animation == nullptr || transform == nullptr || hair == nullptr || !land.Valid(body->creature))
	{
		return;
	}
	const auto* creature = land.TryGet<const Creature>(body->creature);
	const auto* morph = land.TryGet<const CreatureMorph>(body->creature);
	if (creature == nullptr || morph == nullptr)
	{
		return;
	}
	// A copy's hair moves by the time of the frame, as the game is paused while the copy is shown
	const auto seconds = creature_hair::StepSeconds(frameTime.count());
	StepBodyHair(Locator::resources::value().GetCreatureRigs(), creature->species, morph->drawn, *animation, *transform,
	             body->size, *hair, seconds);
}
