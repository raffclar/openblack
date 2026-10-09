/******************************************************************************
 * Copyright (c) 2018-2026 openblack developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/openblack/openblack
 *
 * openblack is licensed under the GNU General Public License version 3.
 *******************************************************************************/

#pragma once

#include "ECS/PhysicsHooksWorld.h"

namespace openblack::ecs
{

/// The game's kinds of thing in the physics, in the game: its entities, land, clock, tables and systems
class GamePhysicsHooksWorld final: public physics_hooks::World
{
public:
	[[nodiscard]] Registry& Entities() override;
	[[nodiscard]] std::optional<float> LandHeight(glm::vec2 point) const override;
	[[nodiscard]] uint32_t Turn() const override;
	[[nodiscard]] GameRandomInterface* Random() override;
	[[nodiscard]] std::optional<PlayerNames> LocalPlayer() const override;
	[[nodiscard]] float LifeOf(entt::entity object) const override;
	[[nodiscard]] float HeightOf(entt::entity object) const override;
	[[nodiscard]] const GAbodeInfo* AbodeInfoOf(entt::entity object) const override;
	[[nodiscard]] bool IsToy(entt::entity object) const override;
	[[nodiscard]] bool IsRock(entt::entity object) const override;
	[[nodiscard]] float WeightOf(entt::entity object) const override;
	void LeaveGhost(entt::entity object) override;
	void Remove(entt::entity object) override;
	[[nodiscard]] std::optional<magic::EffectValues> Crush() const override;
	[[nodiscard]] bool HasMagic() const override;
	void ApplyEffect(entt::entity object, const magic::EffectValues& values, const magic::EffectSource& source) override;
	void ShieldImpact(entt::entity shield, entt::entity hitter, float momentum, std::optional<PlayerNames> player) override;
	[[nodiscard]] std::optional<VillagerStates> VillagerStateOf(entt::entity villager, bool final) const override;
	[[nodiscard]] std::optional<uint32_t> StartHeartBeamSource(glm::vec3 position, PlayerNames player) override;
	void AddPlasma(uint32_t source, const particles::PlasmaCommand& command) override;
	[[nodiscard]] std::vector<model_surface::Triangle> DrawnTrianglesOf(entt::entity object) const override;
	[[nodiscard]] glm::mat4 PlacementOf(entt::entity object) const override;
	void PlaySound(entt::id_type sound, glm::vec3 position, entt::entity owner) override;
	void PassOnBlowToBuilding(systems::DynamicsSystemInterface& dynamics, entt::entity building, PhysicsEntry& struck,
	                          const ImpactInfo& impact) override;
	void StrikeBuilding(systems::DynamicsSystemInterface& dynamics, PhysicsEntry& entry, const ImpactInfo& impact) override;
	[[nodiscard]] std::optional<entt::entity> PieceAtRest(systems::DynamicsSystemInterface& dynamics, PhysicsEntry* entry,
	                                                      entt::entity piece, bool insert) override;
	[[nodiscard]] systems::ResourceStoreSystemInterface::ObjectResource ResourceOf(entt::entity object) const override;
	[[nodiscard]] bool HasStores() const override;
	[[nodiscard]] bool IsStore(entt::entity store, ResourceType type) const override;
	bool TakeObject(entt::entity store, entt::entity object, std::optional<PlayerNames> giver) override;
	void AddToPile(entt::entity pile, ResourceType type, uint32_t amount, bool poisoned) override;
	[[nodiscard]] bool HasAnimals() const override;
	void SetDying(entt::entity animal) override;
	[[nodiscard]] entt::entity LeaderOf(entt::entity flock) const override;
	void SetFlockCentre(entt::entity flock, glm::vec2 centre) override;
	[[nodiscard]] std::optional<glm::vec3> ForestLair(AnimalInfo kind, glm::vec3 from) const override;
	[[nodiscard]] bool HasMinds() const override;
	[[nodiscard]] const creature_mind_tables::Tables* MindTables() const override;
	void PlayerDid(size_t deed, glm::vec3 point, entt::entity object, PlayerNames player) override;
	void UpdateAttitudeFromFeedback(entt::entity creature, float feedback) override;
	void ChangeDesireSource(entt::entity creature, uint32_t source, float amount) override;
	bool ForceCatch(entt::entity creature, entt::entity object) override;
	[[nodiscard]] bool HasCreatureBodies() const override;
	void KickSway(entt::entity creature, glm::vec3 force, glm::vec3 point) override;
	[[nodiscard]] std::optional<float> AnimationDuration(entt::entity creature, size_t animation) override;
	[[nodiscard]] std::optional<glm::vec3> AnimationTravel(entt::entity creature, size_t animation) override;
	[[nodiscard]] std::optional<float> CatchMs(entt::entity creature) const override;
	[[nodiscard]] std::optional<entt::entity> HandHeldCreature() const override;
	[[nodiscard]] bool HasCreatureActions() const override;
	[[nodiscard]] bool CanPickUp(entt::entity object) const override;
	void Catch(entt::entity creature, entt::entity object) override;
	void StopCreature(entt::entity creature) override;
};

} // namespace openblack::ecs
