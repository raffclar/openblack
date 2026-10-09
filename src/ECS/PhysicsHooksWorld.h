/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include <cstdint>

#include <optional>

#include <entt/core/fwd.hpp>
#include <entt/entity/entity.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include "ECS/Systems/ResourceStoreSystemInterface.h"
#include "Enums.h"
#include "Magic/MagicWorldInterface.h"
#include "Magic/SpellRules.h"
#include "Particles/PlasmaCommand.h"

namespace openblack
{
class GameRandomInterface;
struct GAbodeInfo;
namespace creature_mind_tables
{
struct Tables;
}
namespace ecs
{
class Registry;
struct PhysicsEntry;
struct ImpactInfo;
namespace systems
{
class DynamicsSystemInterface;
}
} // namespace ecs
} // namespace openblack

/// What the game's kinds of thing need of the rest of the game as they fly, are struck and come down: the entities, the
/// land, the game's tables, the clock, sounds and beams, magic, the villagers' states, the animals and their forests, the
/// creatures' minds, bodies and hands, the stores and the breaking of buildings. The game's own world works through the
/// game's systems; tests give a fake.
namespace openblack::ecs::physics_hooks
{

class World
{
public:
	virtual ~World() = default;

	[[nodiscard]] virtual Registry& Entities() = 0;
	/// The land's height at a point, none before there is land
	[[nodiscard]] virtual std::optional<float> LandHeight(glm::vec2 point) const = 0;
	/// The game turn it is
	[[nodiscard]] virtual uint32_t Turn() const = 0;
	/// The game's synchronised random numbers, none when there are none
	[[nodiscard]] virtual GameRandomInterface* Random() = 0;
	/// The player at this machine, none when there are no players
	[[nodiscard]] virtual std::optional<PlayerNames> LocalPlayer() const = 0;

	[[nodiscard]] virtual float LifeOf(entt::entity object) const = 0;
	/// An object's height as the game measures it
	[[nodiscard]] virtual float HeightOf(entt::entity object) const = 0;
	/// The abode row of a building's kind, none for what isn't an abode
	[[nodiscard]] virtual const GAbodeInfo* AbodeInfoOf(entt::entity object) const = 0;
	/// Whether a thing is a toy
	[[nodiscard]] virtual bool IsToy(entt::entity object) const = 0;
	/// Whether a thing is a rock, or a bonfire, which wear and break as rocks do
	[[nodiscard]] virtual bool IsRock(entt::entity object) const = 0;
	/// A thing's own weight, as the physics weighs it before it has a body
	[[nodiscard]] virtual float WeightOf(entt::entity object) const = 0;
	/// The object leaves a ghost where it was
	virtual void LeaveGhost(entt::entity object) = 0;
	/// The object goes from the world
	virtual void Remove(entt::entity object) = 0;

	/// The game's crush, none before the game's data is loaded or without magic
	[[nodiscard]] virtual std::optional<magic::EffectValues> Crush() const = 0;
	/// Whether there is magic to apply effects with
	[[nodiscard]] virtual bool HasMagic() const = 0;
	virtual void ApplyEffect(entt::entity object, const magic::EffectValues& values, const magic::EffectSource& source) = 0;
	/// A physical shield pays for a blow
	virtual void ShieldImpact(entt::entity shield, entt::entity hitter, float momentum, std::optional<PlayerNames> player) = 0;

	/// A villager's state at one of its levels, none without the villagers' states
	[[nodiscard]] virtual std::optional<VillagerStates> VillagerStateOf(entt::entity villager, bool final) const = 0;

	/// The spot visual a temple's heart beams from, at a place and given a player; none when it couldn't be made
	[[nodiscard]] virtual std::optional<uint32_t> StartHeartBeamSource(glm::vec3 position, PlayerNames player) = 0;
	virtual void AddPlasma(uint32_t source, const particles::PlasmaCommand& command) = 0;
	/// A sound effect played once at a place, belonging to an owner
	virtual void PlaySound(entt::id_type sound, glm::vec3 position, entt::entity owner) = 0;

	/// A building breaks under a blow passed on to it from another's body
	virtual void PassOnBlowToBuilding(systems::DynamicsSystemInterface& dynamics, entt::entity building, PhysicsEntry& struck,
	                                  const ImpactInfo& impact) = 0;
	/// A building breaks under a blow on its own body
	virtual void StrikeBuilding(systems::DynamicsSystemInterface& dynamics, PhysicsEntry& entry, const ImpactInfo& impact) = 0;
	/// A piece of a broken building comes to rest: the object that stays, none when nothing does; none at all when there
	/// is nothing that breaks buildings
	[[nodiscard]] virtual std::optional<entt::entity> PieceAtRest(systems::DynamicsSystemInterface& dynamics,
	                                                              PhysicsEntry* entry, entt::entity piece, bool insert) = 0;

	/// The resources a thing would give a store, nothing without stores
	[[nodiscard]] virtual systems::ResourceStoreSystemInterface::ObjectResource ResourceOf(entt::entity object) const = 0;
	[[nodiscard]] virtual bool HasStores() const = 0;
	[[nodiscard]] virtual bool IsStore(entt::entity store, ResourceType type) const = 0;
	virtual bool TakeObject(entt::entity store, entt::entity object, std::optional<PlayerNames> giver) = 0;
	virtual void AddToPile(entt::entity pile, ResourceType type, uint32_t amount, bool poisoned) = 0;

	/// Whether there are animals
	[[nodiscard]] virtual bool HasAnimals() const = 0;
	virtual void SetDying(entt::entity animal) = 0;
	[[nodiscard]] virtual entt::entity LeaderOf(entt::entity flock) const = 0;
	virtual void SetFlockCentre(entt::entity flock, glm::vec2 centre) = 0;
	/// A forest an animal of a kind would make its lair in, none when it finds none or there are no forests
	[[nodiscard]] virtual std::optional<glm::vec3> ForestLair(AnimalInfo kind, glm::vec3 from) const = 0;

	/// Whether creatures have minds
	[[nodiscard]] virtual bool HasMinds() const = 0;
	[[nodiscard]] virtual const creature_mind_tables::Tables* MindTables() const = 0;
	virtual void PlayerDid(size_t deed, glm::vec3 point, entt::entity object, PlayerNames player) = 0;
	virtual void UpdateAttitudeFromFeedback(entt::entity creature, float feedback) = 0;
	virtual void ChangeDesireSource(entt::entity creature, uint32_t source, float amount) = 0;
	/// The creature is made to catch a thing; whether it could be: false when creatures have no minds
	virtual bool ForceCatch(entt::entity creature, entt::entity object) = 0;

	/// Whether creatures' bodies can be swayed, timed and moved
	[[nodiscard]] virtual bool HasCreatureBodies() const = 0;
	virtual void KickSway(entt::entity creature, glm::vec3 force, glm::vec3 point) = 0;
	[[nodiscard]] virtual std::optional<float> AnimationDuration(entt::entity creature, size_t animation) = 0;
	[[nodiscard]] virtual std::optional<glm::vec3> AnimationTravel(entt::entity creature, size_t animation) = 0;
	/// When a creature's catch takes hold, in milliseconds from its start, none for a species without it
	[[nodiscard]] virtual std::optional<float> CatchMs(entt::entity creature) const = 0;
	/// The creature the hand holds, none when it holds none
	[[nodiscard]] virtual std::optional<entt::entity> HandHeldCreature() const = 0;
	/// Whether creatures act on objects at all, and whether one can pick a thing up
	[[nodiscard]] virtual bool HasCreatureActions() const = 0;
	[[nodiscard]] virtual bool CanPickUp(entt::entity object) const = 0;
	/// A creature's action catches a thing, with no mind to choose it
	virtual void Catch(entt::entity creature, entt::entity object) = 0;
	/// The creature stops where it is
	virtual void StopCreature(entt::entity creature) = 0;
};

} // namespace openblack::ecs::physics_hooks
