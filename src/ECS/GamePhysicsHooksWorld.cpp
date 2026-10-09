/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#include "GamePhysicsHooksWorld.h"

#include "3D/LandIslandInterface.h"
#include "Audio/AudioManagerInterface.h"
#include "Common/GameRandom.h"
#include "Creature/CreatureRig.h"
#include "ECS/Components/Creature.h"
#include "ECS/Components/LivingAction.h"
#include "ECS/ObjectPhysics.h"
#include "ECS/PhysicsClasses.h"
#include "ECS/Registry.h"
#include "ECS/Systems/AnimalSystemInterface.h"
#include "ECS/Systems/BuildingDamageSystemInterface.h"
#include "ECS/Systems/CreatureAnimationSystemInterface.h"
#include "ECS/Systems/CreatureHandSystemInterface.h"
#include "ECS/Systems/CreatureLocomotionSystemInterface.h"
#include "ECS/Systems/CreatureMindSystemInterface.h"
#include "ECS/Systems/CreatureObjectActionSystemInterface.h"
#include "ECS/Systems/DynamicsSystemInterface.h"
#include "ECS/Systems/ForestSystemInterface.h"
#include "ECS/Systems/LivingActionSystemInterface.h"
#include "ECS/Systems/MagicShieldSystemInterface.h"
#include "ECS/Systems/MagicSystemInterface.h"
#include "ECS/Systems/ParticleSystemInterface.h"
#include "ECS/Systems/PlayerSystemInterface.h"
#include "ECS/Systems/TimeSystemInterface.h"
#include "ECS/WorldObjects.h"
#include "InfoConstants.h"
#include "Locator.h"
#include "Resources/ResourcesInterface.h"

using namespace openblack;
using namespace openblack::ecs;
using namespace openblack::ecs::components;
using namespace openblack::ecs::systems;

namespace
{
/// The game's crush, which hard knocks apply
constexpr size_t k_CrushPreset = 3;
} // namespace

Registry& GamePhysicsHooksWorld::Entities()
{
	return Locator::entitiesRegistry::value();
}

std::optional<float> GamePhysicsHooksWorld::LandHeight(glm::vec2 point) const
{
	if (!Locator::terrainSystem::has_value())
	{
		return std::nullopt;
	}
	return Locator::terrainSystem::value().GetHeightAt(point);
}

uint32_t GamePhysicsHooksWorld::Turn() const
{
	return Locator::time::has_value() ? Locator::time::value().GetTurn() : 0;
}

GameRandomInterface* GamePhysicsHooksWorld::Random()
{
	return Locator::gameRandom::has_value() ? &Locator::gameRandom::value() : nullptr;
}

std::optional<PlayerNames> GamePhysicsHooksWorld::LocalPlayer() const
{
	if (!Locator::playerSystem::has_value())
	{
		return std::nullopt;
	}
	return Locator::playerSystem::value().GetLocalPlayer();
}

float GamePhysicsHooksWorld::LifeOf(entt::entity object) const
{
	return world_objects::LifeOf(object);
}

float GamePhysicsHooksWorld::HeightOf(entt::entity object) const
{
	return world_objects::SizeOf(object).height;
}

const GAbodeInfo* GamePhysicsHooksWorld::AbodeInfoOf(entt::entity object) const
{
	return world_objects::AbodeInfoOf(object);
}

bool GamePhysicsHooksWorld::IsToy(entt::entity object) const
{
	return Locator::infoConstants::has_value() &&
	       physics_classes::IsToy(Locator::entitiesRegistry::value(), object, Locator::infoConstants::value());
}

bool GamePhysicsHooksWorld::IsRock(entt::entity object) const
{
	return object_physics::IsRock(object);
}

float GamePhysicsHooksWorld::WeightOf(entt::entity object) const
{
	return Locator::dynamicsSystem::has_value() ? Locator::dynamicsSystem::value().WeightOf(object) : 0.0f;
}

void GamePhysicsHooksWorld::LeaveGhost(entt::entity object)
{
	world_objects::LeaveGhost(object);
}

void GamePhysicsHooksWorld::Remove(entt::entity object)
{
	world_objects::Remove(object);
}

std::optional<magic::EffectValues> GamePhysicsHooksWorld::Crush() const
{
	if (!Locator::magicSystem::has_value() || !Locator::infoConstants::has_value())
	{
		return std::nullopt;
	}
	return magic::EffectValues::From(Locator::infoConstants::value().effect.at(k_CrushPreset));
}

bool GamePhysicsHooksWorld::HasMagic() const
{
	return Locator::magicSystem::has_value();
}

void GamePhysicsHooksWorld::ApplyEffect(entt::entity object, const magic::EffectValues& values,
                                        const magic::EffectSource& source)
{
	if (Locator::magicSystem::has_value())
	{
		Locator::magicSystem::value().ApplyEffectToObject(object, values, source);
	}
}

void GamePhysicsHooksWorld::ShieldImpact(entt::entity shield, entt::entity hitter, float momentum,
                                         std::optional<PlayerNames> player)
{
	if (Locator::magicShieldSystem::has_value())
	{
		Locator::magicShieldSystem::value().Impact(shield, hitter, momentum, player);
	}
}

std::optional<VillagerStates> GamePhysicsHooksWorld::VillagerStateOf(entt::entity villager, bool final) const
{
	const auto* action = Locator::entitiesRegistry::value().TryGet<const LivingAction>(villager);
	if (action == nullptr || !Locator::livingActionSystem::has_value())
	{
		return std::nullopt;
	}
	return Locator::livingActionSystem::value().VillagerGetState(*action,
	                                                             final ? LivingAction::Index::Final : LivingAction::Index::Top);
}

std::optional<uint32_t> GamePhysicsHooksWorld::StartHeartBeamSource(glm::vec3 position, PlayerNames player)
{
	if (!Locator::particleSystem::has_value())
	{
		return std::nullopt;
	}
	auto& particles = Locator::particleSystem::value();
	const auto id = particles.StartSpotVisual(SpotVisualType::MagicBeamOnCitadel, position, -1, entt::null, 1.0f);
	if (id == ParticleSystemInterface::k_NoEffect)
	{
		return std::nullopt;
	}
	particles.SetPlayer(id, static_cast<int>(player));
	return id;
}

void GamePhysicsHooksWorld::AddPlasma(uint32_t source, const particles::PlasmaCommand& command)
{
	if (Locator::particleSystem::has_value())
	{
		Locator::particleSystem::value().AddPlasma(source, command);
	}
}

void GamePhysicsHooksWorld::PlaySound(entt::id_type sound, glm::vec3 position, entt::entity owner)
{
	if (Locator::audio::has_value())
	{
		Locator::audio::value().StartSoundEffect(sound, {.position = position, .owner = owner});
	}
}

void GamePhysicsHooksWorld::PassOnBlowToBuilding(DynamicsSystemInterface& dynamics, entt::entity building, PhysicsEntry& struck,
                                                 const ImpactInfo& impact)
{
	if (Locator::buildingDamageSystem::has_value())
	{
		Locator::buildingDamageSystem::value().ReactToPassedOnImpact(dynamics, building, struck, impact);
	}
}

void GamePhysicsHooksWorld::StrikeBuilding(DynamicsSystemInterface& dynamics, PhysicsEntry& entry, const ImpactInfo& impact)
{
	if (Locator::buildingDamageSystem::has_value())
	{
		Locator::buildingDamageSystem::value().ReactToImpact(dynamics, entry, impact);
	}
}

std::optional<entt::entity> GamePhysicsHooksWorld::PieceAtRest(DynamicsSystemInterface& dynamics, PhysicsEntry* entry,
                                                               entt::entity piece, bool insert)
{
	if (!Locator::buildingDamageSystem::has_value())
	{
		return std::nullopt;
	}
	return Locator::buildingDamageSystem::value().PieceAtRest(dynamics, entry, piece, insert);
}

ResourceStoreSystemInterface::ObjectResource GamePhysicsHooksWorld::ResourceOf(entt::entity object) const
{
	return Locator::resourceStoreSystem::has_value() ? Locator::resourceStoreSystem::value().ResourceOf(object)
	                                                 : ResourceStoreSystemInterface::ObjectResource {};
}

bool GamePhysicsHooksWorld::HasStores() const
{
	return Locator::resourceStoreSystem::has_value();
}

bool GamePhysicsHooksWorld::IsStore(entt::entity store, ResourceType type) const
{
	return Locator::resourceStoreSystem::has_value() && Locator::resourceStoreSystem::value().IsStore(store, type);
}

bool GamePhysicsHooksWorld::TakeObject(entt::entity store, entt::entity object, std::optional<PlayerNames> giver)
{
	return Locator::resourceStoreSystem::has_value() && Locator::resourceStoreSystem::value().TakeObject(store, object, giver);
}

void GamePhysicsHooksWorld::AddToPile(entt::entity pile, ResourceType type, uint32_t amount, bool poisoned)
{
	if (Locator::resourceStoreSystem::has_value())
	{
		Locator::resourceStoreSystem::value().AddToPile(pile, type, amount, poisoned);
	}
}

bool GamePhysicsHooksWorld::HasAnimals() const
{
	return Locator::animalSystem::has_value();
}

void GamePhysicsHooksWorld::SetDying(entt::entity animal)
{
	if (Locator::animalSystem::has_value())
	{
		Locator::animalSystem::value().SetDying(animal);
	}
}

entt::entity GamePhysicsHooksWorld::LeaderOf(entt::entity flock) const
{
	return Locator::animalSystem::has_value() ? Locator::animalSystem::value().LeaderOf(flock) : entt::null;
}

void GamePhysicsHooksWorld::SetFlockCentre(entt::entity flock, glm::vec2 centre)
{
	if (Locator::animalSystem::has_value())
	{
		Locator::animalSystem::value().SetFlockCentre(flock, centre);
	}
}

std::optional<glm::vec3> GamePhysicsHooksWorld::ForestLair(AnimalInfo kind, glm::vec3 from) const
{
	return Locator::forestSystem::has_value() ? Locator::forestSystem::value().ForestLair(kind, from) : std::nullopt;
}

bool GamePhysicsHooksWorld::HasMinds() const
{
	return Locator::creatureMindSystem::has_value();
}

const creature_mind_tables::Tables* GamePhysicsHooksWorld::MindTables() const
{
	return Locator::creatureMindSystem::has_value() ? Locator::creatureMindSystem::value().GetTables() : nullptr;
}

void GamePhysicsHooksWorld::PlayerDid(size_t deed, glm::vec3 point, entt::entity object, PlayerNames player)
{
	if (Locator::creatureMindSystem::has_value())
	{
		Locator::creatureMindSystem::value().PlayerDid(deed, point, object, player);
	}
}

void GamePhysicsHooksWorld::UpdateAttitudeFromFeedback(entt::entity creature, float feedback)
{
	if (Locator::creatureMindSystem::has_value())
	{
		Locator::creatureMindSystem::value().UpdateAttitudeFromFeedback(creature, feedback);
	}
}

void GamePhysicsHooksWorld::ChangeDesireSource(entt::entity creature, uint32_t source, float amount)
{
	if (Locator::creatureMindSystem::has_value())
	{
		Locator::creatureMindSystem::value().ChangeDesireSource(creature, source, amount);
	}
}

bool GamePhysicsHooksWorld::ForceCatch(entt::entity creature, entt::entity object)
{
	if (!Locator::creatureMindSystem::has_value())
	{
		return false;
	}
	Locator::creatureMindSystem::value().ForceCatch(creature, object);
	return true;
}

bool GamePhysicsHooksWorld::HasCreatureBodies() const
{
	return Locator::creatureAnimationSystem::has_value();
}

void GamePhysicsHooksWorld::KickSway(entt::entity creature, glm::vec3 force, glm::vec3 point)
{
	if (Locator::creatureAnimationSystem::has_value())
	{
		Locator::creatureAnimationSystem::value().KickSway(creature, force, point);
	}
}

std::optional<float> GamePhysicsHooksWorld::AnimationDuration(entt::entity creature, size_t animation)
{
	if (!Locator::creatureAnimationSystem::has_value())
	{
		return std::nullopt;
	}
	return Locator::creatureAnimationSystem::value().AnimationDuration(creature, animation);
}

std::optional<glm::vec3> GamePhysicsHooksWorld::AnimationTravel(entt::entity creature, size_t animation)
{
	if (!Locator::creatureAnimationSystem::has_value())
	{
		return std::nullopt;
	}
	return Locator::creatureAnimationSystem::value().AnimationTravel(creature, animation);
}

std::optional<float> GamePhysicsHooksWorld::CatchMs(entt::entity creature) const
{
	const auto* body = Locator::entitiesRegistry::value().TryGet<const Creature>(creature);
	if (body == nullptr || !Locator::resources::has_value())
	{
		return std::nullopt;
	}
	const auto& rigs = Locator::resources::value().GetCreatureRigs();
	const auto id = creature::GetRigId(body->species);
	if (!rigs.Contains(id) || !rigs.Handle(id)->actionPoints.has_value())
	{
		return std::nullopt;
	}
	return rigs.Handle(id)->actionPoints->catchMs;
}

std::optional<entt::entity> GamePhysicsHooksWorld::HandHeldCreature() const
{
	return Locator::creatureHandSystem::has_value() ? Locator::creatureHandSystem::value().GetCreature() : std::nullopt;
}

bool GamePhysicsHooksWorld::HasCreatureActions() const
{
	return Locator::creatureObjectActionSystem::has_value();
}

bool GamePhysicsHooksWorld::CanPickUp(entt::entity object) const
{
	return Locator::creatureObjectActionSystem::has_value() && Locator::creatureObjectActionSystem::value().CanPickUp(object);
}

void GamePhysicsHooksWorld::Catch(entt::entity creature, entt::entity object)
{
	if (Locator::creatureObjectActionSystem::has_value())
	{
		Locator::creatureObjectActionSystem::value().Catch(creature, object);
	}
}

void GamePhysicsHooksWorld::StopCreature(entt::entity creature)
{
	if (Locator::creatureLocomotionSystem::has_value())
	{
		Locator::creatureLocomotionSystem::value().Stop(creature);
	}
}
