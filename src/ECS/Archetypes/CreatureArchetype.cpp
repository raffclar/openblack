/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureArchetype.h"

#include "3D/CreatureBody.h"
#include "3D/L3DMesh.h"
#include "3D/ObjectMatrix.h"
#include "Creature/CreatureMorph.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureDrawPose.h"
#include "ECS/Components/CreatureHair.h"
#include "ECS/Components/CreatureLocomotion.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/CreatureNeeds.h"
#include "ECS/Components/CreatureSkin.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Transform.h"
#include "ECS/MobileDrawing.h"
#include "ECS/ObjectCreationIndex.h"
#include "ECS/Registry.h"
#include "ECS/Systems/LeashSystemInterface.h"
#include "Enums.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs::archetypes;
using namespace openblack::ecs::components;
using namespace openblack::creature;

namespace
{
// What a new creature of a species starts at without the game's creature tables (a stand-in)
constexpr float k_UnknownStartScale = 0.22f;

const GCreatureInfo* SpeciesInfo(CreatureType species)
{
	if (!Locator::infoConstants::has_value())
	{
		return nullptr;
	}
	const auto& creatures = Locator::infoConstants::value().creature;
	const auto row = creature::InfoRow(species);
	return row < creatures.size() ? &creatures.at(row) : nullptr;
}

} // namespace

float CreatureArchetype::DrawnScale(CreatureType species, float size)
{
	// Without the meshes the rest height is unknown, and the body is drawn at its size
	float restHeight = 0.0f;
	if (Locator::resources::has_value())
	{
		const auto meshId = creature::GetIdFromType(species, CreatureBody::Appearance::Base);
		const auto& meshes = Locator::resources::value().GetMeshes();
		if (meshes.Contains(meshId))
		{
			restHeight = creature_morph::RestHeight(meshes.Handle(meshId)->GetBoneMatrices());
		}
	}
	return creature_morph::DrawnScale(size, restHeight);
}

float CreatureArchetype::SpeciesStrength(CreatureType species)
{
	const auto* info = SpeciesInfo(species);
	return info != nullptr ? info->strength : creature_morph::k_UnknownSpeciesStrength;
}

float CreatureArchetype::StartScale(CreatureType species)
{
	const auto* info = SpeciesInfo(species);
	return creature_morph::ClampScale(info != nullptr ? info->startScale : k_UnknownStartScale);
}

CreatureArchetype::Body CreatureArchetype::StartBody(CreatureType species)
{
	const auto* info = SpeciesInfo(species);
	if (info == nullptr)
	{
		return {};
	}
	return {.alignment = 0.0f, .fatness = info->startFatness, .strength = info->strength};
}

entt::entity CreatureArchetype::Create(const glm::vec3& position, PlayerNames playerName, CreatureType creatureType,
                                       entt::id_type creatureMindId, float yAngleRadians, float scale, const Body& body)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = registry.Create();
	ecs::object_index::Assign(entity);
	const auto morph =
	    creature_morph::FromAttributes(body.alignment, body.fatness, body.strength, SpeciesStrength(creatureType));
	const auto size = creature_morph::ClampScale(scale);
	registry.Assign<Creature>(entity, Creature {
	                                      .owner = playerName,
	                                      .leashable = false,
	                                      .species = creatureType,
	                                      .mind = creatureMindId,
	                                      .alignment = body.alignment,
	                                      .fatness = body.fatness,
	                                      .strength = body.strength,
	                                      .size = size,
	                                  });
	// The body is drawn with the base mesh's skins, its shape blended towards the other meshes
	registry.Assign<Mesh>(entity, creature::GetIdFromType(creatureType, CreatureBody::Appearance::Base));
	registry.AssignState<CreatureMorph>(entity, CreatureMorph {.shownFatness = body.fatness, .drawn = morph, .revision = 0});
	registry.AssignState<CreatureAnimation>(entity);
	registry.AssignState<CreatureEyes>(entity);
	registry.AssignState<CreatureMindState>(entity);
	registry.AssignState<CreatureNeeds>(entity);
	registry.AssignState<CreatureHair>(entity);
	registry.AssignState<CreatureTattoos>(entity);
	registry.AssignState<CreatureMarks>(entity);
	registry.AssignState<CreatureSkin>(entity);
	registry.AssignState<CreatureLocomotion>(entity);
	registry.AssignState<CreatureDrawPose>(entity);
	// the creature's world matrix is the generic object one: Ry(-a), not Ry(+a)
	registry.Assign<Transform>(entity, position, affine::AngleY(yAngleRadians), glm::vec3(DrawnScale(creatureType, size)));
	// the first creature of its owner is the one they lead
	if (Locator::leashSystem::has_value())
	{
		Locator::leashSystem::value().ClaimOnArrival(entity);
	}
	// it appears where it is: drawn there from the next frame, with no slide from anywhere
	ecs::NotifyTeleported(entity);
	return entity;
}
