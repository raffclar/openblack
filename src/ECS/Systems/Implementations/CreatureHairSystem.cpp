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
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs::systems;
using namespace openblack::ecs::components;
using openblack::creature::CreatureRig;

namespace
{
/// The longest step the hair takes at once, in seconds
constexpr float k_MaxStepSeconds = 1.0f;

/// Where a strand is rooted this frame and which way it grows out, or none where its triangle has no area. The root
/// is sunk along the triangle's normal as it comes, unnormalised.
std::optional<creature_hair::Root> RootOf(const CreatureRig::HairStrand& strand, const creature_morph::Morph& morph,
                                          const std::vector<glm::mat4>& bones, const glm::mat4& model)
{
	const auto triangle = creature::PosedTriangle(strand.vertices, strand.bones, morph, bones, model);
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
void StepHair(const resources::CreatureRigManager& rigs, CreatureType species, const creature_morph::Morph& morph,
              const CreatureAnimation& animation, const Transform& transform, float size, CreatureHair& hair, float seconds)
{
	const auto rigId = creature::GetRigId(species);
	if (!rigs.Contains(rigId) || animation.boneMatrices.empty())
	{
		hair.groups.clear();
		return;
	}
	const auto& rig = *rigs.Handle(rigId);
	if (hair.groups.size() != rig.hairGroups.size())
	{
		hair = {};
		hair.groups.resize(rig.hairGroups.size());
	}
	// The hair takes the look of the alignment the body is drawn with
	const auto alignment = morph.evilGood;
	const auto scale = creature_hair::HairScale(size);
	const auto model = creature::PlacementMatrix(transform.position, transform.rotation, transform.scale);
	for (size_t g = 0; g < rig.hairGroups.size(); ++g)
	{
		const auto& source = rig.hairGroups[g];
		auto& group = hair.groups[g];
		const auto look = creature_hair::LookFor(source.looks, alignment);
		const auto physics = creature_hair::PhysicsFor(look, scale, source.segmentCount);
		group.colour = look.colour;
		group.halfWidth = physics.halfWidth;
		group.textured = source.textured;
		group.strands.resize(source.strands.size());
		for (size_t s = 0; s < source.strands.size(); ++s)
		{
			const auto root = RootOf(source.strands[s], morph, animation.boneMatrices, model);
			if (!root)
			{
				continue;
			}
			auto& strand = group.strands[s];
			if (!hair.started || strand.positions.size() != source.segmentCount)
			{
				strand = creature_hair::FromZero(source.segmentCount);
			}
			creature_hair::Step(strand, *root, physics, seconds);
		}
	}
	hair.started = true;
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
	const auto seconds = std::clamp(std::chrono::duration<float>(gameTime).count(), 0.0f, k_MaxStepSeconds);

	registry.Each<const Creature, const CreatureMorph, const CreatureAnimation, const Transform, CreatureHair>(
	    [&](const Creature& creature, const CreatureMorph& morph, const CreatureAnimation& animation,
	        const Transform& transform, CreatureHair& hair) {
		    StepHair(rigs, creature.species, morph.drawn, animation, transform, ShownSize(creature), hair, seconds);
	    });
}

void CreatureHairSystem::StepCopy(entt::entity copy, std::chrono::duration<float, std::milli> frameTime)
{
	if (!_shown)
	{
		return;
	}
	auto& registry = Locator::entitiesRegistry::value();
	const auto* body = registry.TryGet<const CreatureCopy>(copy);
	const auto* animation = registry.TryGet<const CreatureAnimation>(copy);
	const auto* transform = registry.TryGet<const Transform>(copy);
	auto* hair = registry.TryGet<CreatureHair>(copy);
	if (body == nullptr || animation == nullptr || transform == nullptr || hair == nullptr || !registry.Valid(body->creature))
	{
		return;
	}
	const auto* creature = registry.TryGet<const Creature>(body->creature);
	const auto* morph = registry.TryGet<const CreatureMorph>(body->creature);
	if (creature == nullptr || morph == nullptr)
	{
		return;
	}
	// A copy's hair moves by the time of the frame, as the game is paused while the copy is shown
	const auto seconds = std::clamp(std::chrono::duration<float>(frameTime).count(), 0.0f, k_MaxStepSeconds);
	StepHair(Locator::resources::value().GetCreatureRigs(), creature->species, morph->drawn, *animation, *transform, body->size,
	         *hair, seconds);
}
