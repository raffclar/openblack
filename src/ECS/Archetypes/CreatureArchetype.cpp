/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "CreatureArchetype.h"

#include <tuple>

#include <glm/gtx/euler_angles.hpp>

#include "3D/CreatureBody.h"
#include "3D/L3DMesh.h"
#include "3D/LandIslandInterface.h"
#include "3D/MapCoords.h"
#include "Common/GameRandom.h"
#include "Common/PortableRotations.h"
#include "Creature/CreatureMorph.h"
#include "Creature/CreaturePhysiology.h"
#include "Creature/TemplePen.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/CreatureBody.h"
#include "ECS/Components/CreatureHair.h"
#include "ECS/Components/CreatureLeash.h"
#include "ECS/Components/CreatureLocomotion.h"
#include "ECS/Components/CreatureMind.h"
#include "ECS/Components/CreatureNeeds.h"
#include "ECS/Components/CreatureSkin.h"
#include "ECS/Components/LivingListPlace.h"
#include "ECS/Components/Mesh.h"
#include "ECS/Components/Transform.h"
#include "ECS/CreatureHome.h"
#include "ECS/CreatureNaming.h"
#include "ECS/LivingBirth.h"
#include "ECS/Registry.h"
#include "ECS/Systems/CreatureMindSystemInterface.h"
#include "ECS/Systems/LeashSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
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
// What is known of a species without the game's creature tables
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
	const auto meshId = creature::GetIdFromType(species, CreatureBody::Appearance::Base);
	const auto& meshes = Locator::resources::value().GetMeshes();
	const auto restHeight =
	    meshes.Contains(meshId) ? creature_morph::RestHeight(meshes.Handle(meshId)->GetBoneMatrices()) : 0.0f;
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
	return info != nullptr ? info->startScale : k_UnknownStartScale;
}

CreatureArchetype::Body CreatureArchetype::StartBody(CreatureType species)
{
	const auto* info = SpeciesInfo(species);
	if (info == nullptr)
	{
		return {};
	}
	if (!Locator::gameRandom::has_value())
	{
		return {.alignment = 0.0f, .fatness = info->startFatness, .strength = info->strength};
	}
	// The first number of each living thing that the game makes comes before the numbers of the body
	auto& random = Locator::gameRandom::value();
	std::ignore = random.GameRand(ecs::living_birth::AgeDrawRange(info->startAge));
	// A new creature's body spreads the strength first, then the fatness, by the random numbers of the game
	const auto upTo = [&random](float most) { return random.GameFloatRand(most); };
	const auto strength = creature_physiology::SpreadStart(info->strength, upTo);
	const auto fatness = creature_physiology::SpreadStart(info->startFatness, upTo);
	return {.alignment = 0.0f, .fatness = fatness, .strength = strength};
}

void CreatureArchetype::ShowStrength(entt::entity creature)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto* body = registry.TryGet<const Creature>(creature);
	auto* morph = registry.TryGet<CreatureMorph>(creature);
	if (body != nullptr && morph != nullptr)
	{
		morph->strengthLook = creature_morph::StrengthLook(body->strength, SpeciesStrength(body->species));
	}
}

void CreatureArchetype::TakeLooks(Registry& registry, entt::entity creature)
{
	const auto* body = registry.TryGet<const Creature>(creature);
	auto* morph = registry.TryGet<CreatureMorph>(creature);
	if (body == nullptr || morph == nullptr)
	{
		return;
	}
	const auto looks = creature_morph::TakeLooks(morph->shownFatness, body->fatness, body->alignment, body->strength,
	                                             SpeciesStrength(body->species));
	morph->shownFatness = looks.shownFatness;
	morph->alignmentLook = looks.evilGood;
	morph->strengthLook = looks.weakStrong;
}

entt::entity CreatureArchetype::Create(const glm::vec3& position, PlayerNames playerName, CreatureType creatureType,
                                       entt::id_type creatureMindId, float yAngleRadians, float scale, const Body& body)
{
	auto& registry = Locator::entitiesRegistry::value();
	const auto entity = registry.Create();
	const auto morph =
	    creature_morph::FromAttributes(body.alignment, body.fatness, body.strength, SpeciesStrength(creatureType));
	registry.Assign<Creature>(entity, Creature {.owner = playerName,
	                                            .leashable = false,
	                                            .species = creatureType,
	                                            .mind = creatureMindId,
	                                            .alignment = body.alignment,
	                                            .fatness = body.fatness,
	                                            .strength = body.strength,
	                                            .size = scale,
	                                            .made = ++registry.Context().creaturesMade});
	// It goes to the front of the list of the living
	registry.Assign<LivingListPlace>(entity, LivingListPlace {.made = registry.Context().livingMade++});
	// The body is drawn with the base mesh's skins, its shape blended towards the other meshes
	registry.Assign<Mesh>(entity, creature::GetIdFromType(creatureType, CreatureBody::Appearance::Base));
	registry.Assign<CreatureMorph>(entity, CreatureMorph {.shownFatness = body.fatness,
	                                                      .drawn = morph,
	                                                      .strengthLook = morph.weakStrong,
	                                                      .alignmentLook = morph.evilGood,
	                                                      .revision = 0});
	registry.Assign<CreatureAnimation>(entity);
	registry.Assign<CreatureEyes>(entity);
	registry.Assign<CreatureMindState>(entity);
	registry.Assign<CreatureNeeds>(entity);
	registry.Assign<CreatureHair>(entity);
	registry.Assign<CreatureTattoos>(entity);
	registry.Assign<CreatureMarks>(entity);
	registry.Assign<CreatureSkin>(entity);
	registry.Assign<CreatureLocomotion>(entity);
	// It is named as it is made, by who it belongs to and how many creatures there are with it
	registry.Get<Creature>(entity).name = creature_naming::NewName(playerName, registry.Size<Creature>());
	// Its own size is kept as given; only the size it is drawn at is kept within what a creature can be drawn at
	registry.Assign<Transform>(entity, position, portable_math::EulerAngleY(yAngleRadians),
	                           glm::vec3(DrawnScale(creatureType, creature_morph::ClampScale(scale))));
	// Its owner keeps their creatures in the order they got them, the first being their primary creature
	if (Locator::playerSystem::has_value())
	{
		Locator::playerSystem::value().AddCreature(entity);
	}
	// Its home is its player's temple's pen if the temple stands, else where it was made. The leash keeps the exact map
	// position of the home: the pen's, or the map position where the creature was made, with its height above the
	// ground there.
	auto& leash = registry.Assign<CreatureLeash>(entity);
	const auto pen = ecs::creature_home::HomeOf(registry, entity);
	leash.home = pen.value_or(position);
	if (pen.has_value() || !Locator::terrainSystem::has_value())
	{
		leash.homeOnMap = temple_pen::MapPosition(*leash.home);
	}
	else
	{
		leash.homeOnMap = map_coords::FromWorld(Locator::terrainSystem::value(), position);
	}
	// A player's first creature is the one they can lead on the leash
	if (Locator::leashSystem::has_value())
	{
		Locator::leashSystem::value().ClaimOnArrival(entity);
	}
	// The creature gets its mind as it is made, after its body, as the game makes it
	if (Locator::creatureMindSystem::has_value())
	{
		Locator::creatureMindSystem::value().StartMind(entity);
	}
	return entity;
}
